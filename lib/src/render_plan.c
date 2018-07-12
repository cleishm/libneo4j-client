/* vi:set ts=4 sw=4 expandtab:
 *
 * Copyright 2016, Chris Leishman (http://github.com/cleishm)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "../../config.h"
#include "neo4j-client.h"
#include "client_config.h"
#include "render.h"
#include "util.h"
#include <assert.h>
#include <math.h>


static ssize_t obtain_header(void *data, unsigned int n, const char **s,
        bool *duplicate);

static int render_steps(FILE *stream,
        struct neo4j_statement_execution_step *step, unsigned int depth,
        bool last, char **ids_buffer, size_t *ids_bufcap, char **args_buffer,
        size_t *args_bufcap, unsigned int widths[6], uint_fast32_t flags,
        const struct neo4j_plan_table_colors *colors);
static int render_op(FILE *stream, const char *operator_type,
        unsigned int op_depth, unsigned int width, uint_fast32_t flags,
        const struct neo4j_plan_table_colors *colors);
static ssize_t build_str_list(const char * const *list, unsigned int n,
        char **buffer, size_t *bufcap);
static ssize_t build_args_value(neo4j_value_t args, char **buffer,
        size_t *bufcap);
static int render_wrap(FILE *stream, unsigned int op_depth,
        unsigned int widths[4], uint_fast32_t flags,
        const struct neo4j_plan_table_colors *colors);
static int render_tr(FILE *stream, unsigned int op_depth, bool branch,
        unsigned int widths[6], uint_fast32_t flags,
        const struct neo4j_plan_table_colors *colors);
static void calculate_widths(unsigned int widths[6],
        struct neo4j_statement_plan *plan, unsigned int render_width);
static unsigned int operators_width(
        struct neo4j_statement_execution_step *step);
static unsigned int identifiers_width(
        struct neo4j_statement_execution_step *step);
static unsigned int str_list_len(const char * const *list, unsigned int n);


static const char *HEADERS[6] =
    { "Operator", "Estimated Rows", "Rows", "DB Hits", "Identifiers", "Other" };
static unsigned int MIN_OPR_WIDTH = 10; // strlen(HEADERS[0]);
static unsigned int EST_WIDTH = 14; // strlen(HEADERS[1]);
static unsigned int RWS_WIDTH = 4; // strlen(HEADERS[2]);
static unsigned int DBH_WIDTH = 7; // strlen(HEADERS[3]);
static unsigned int MIN_IDS_WIDTH = 11; // strlen(HEADERS[4]);
static unsigned int MIN_OTH_WIDTH = 5; // strlen(HEADERS[5]);


static const struct neo4j_results_table_colors *ctable_colors(
    const struct neo4j_plan_table_colors *plan_colors)
{
    return (const struct neo4j_results_table_colors *)plan_colors;
}


int neo4j_render_plan_table(FILE *stream, struct neo4j_statement_plan *plan,
        unsigned int width, uint_fast32_t flags)
{
    neo4j_config_t *config = neo4j_new_config();
    config->render_flags |= flags;
    neo4j_config_set_plan_table_colors(config,
            (flags & NEO4J_RENDER_ANSI_COLOR)? neo4j_plan_table_ansi_colors :
            neo4j_plan_table_no_colors);
    int err = neo4j_render_plan_ctable(config, stream, plan, width);
    neo4j_config_free(config);
    return err;
}


int neo4j_render_plan_ctable(const neo4j_config_t *config, FILE *stream,
        struct neo4j_statement_plan *plan, unsigned int width)
{
    REQUIRE(stream != NULL, -1);
    REQUIRE(plan != NULL, -1);
    REQUIRE(width > 1 && width < NEO4J_RENDER_MAX_WIDTH, -1);

    uint_fast32_t flags = normalize_render_flags(config->render_flags);
    const struct neo4j_plan_table_colors *colors = config->plan_table_colors;

    size_t ids_bufcap = NEO4J_FIELD_BUFFER_INITIAL_CAPACITY;
    char *ids_buffer = malloc(ids_bufcap);
    if (ids_buffer == NULL)
    {
        return -1;
    }
    size_t args_bufcap = NEO4J_FIELD_BUFFER_INITIAL_CAPACITY;
    char *args_buffer = malloc(args_bufcap);
    if (args_buffer == NULL)
    {
        free(ids_buffer);
        return -1;
    }

    unsigned int widths[6];
    calculate_widths(widths, plan, width);
    bool undersize = (widths[5] == 0);

    if (render_hrule(stream, 6, widths, HLINE_TOP, undersize, flags,
            ctable_colors(colors)))
    {
        goto failure;
    }

    if (render_row(stream, 6, widths, undersize, flags, ctable_colors(colors),
                colors->header, obtain_header, NULL))
    {
        goto failure;
    }

    if (render_hrule(stream, 6, widths, HLINE_HEAD, undersize, flags,
                ctable_colors(colors)))
    {
        goto failure;
    }

    if (render_steps(stream, plan->output_step, 0, true, &ids_buffer,
                &ids_bufcap, &args_buffer, &args_bufcap, widths, flags, colors))
    {
        goto failure;
    }

    if (render_hrule(stream, 6, widths, HLINE_BOTTOM, undersize, flags,
                ctable_colors(colors)))
    {
        goto failure;
    }

    free(args_buffer);
    free(ids_buffer);
    return 0;

    int errsv;
failure:
    errsv = errno;
    fflush(stream);
    free(args_buffer);
    free(ids_buffer);
    errno = errsv;
    return -1;
}


ssize_t obtain_header(void *data, unsigned int n, const char **s,
        bool *duplicate)
{
    *s = HEADERS[n];
    *duplicate = false;
    return strlen(HEADERS[n]);
}


int render_steps(FILE *stream, struct neo4j_statement_execution_step *step,
        unsigned int depth, bool last, char **ids_buffer, size_t *ids_bufcap,
        char **args_buffer, size_t *args_bufcap, unsigned int widths[6],
        uint_fast32_t flags,
        const struct neo4j_plan_table_colors *plan_colors)
{
    const struct neo4j_results_table_colors *colors = ctable_colors(plan_colors);
    struct neo4j_statement_execution_step **sources = step->sources;
    for (unsigned int i = 0; i < step->nsources; ++i)
    {
        bool branch = false;
        unsigned int d = depth;
        if (i > 0)
        {
            branch = true;
            d = depth + 1;
        }
        if (render_steps(stream, sources[i], d, false, ids_buffer, ids_bufcap,
                    args_buffer, args_bufcap, widths, flags, plan_colors))
        {
            return -1;
        }
        if (render_tr(stream, depth+1, branch, widths, flags, plan_colors))
        {
            return -1;
        }
    }

    if (widths[0] > 0 && render_op(stream, step->operator_type,
                depth+1, widths[0], flags, plan_colors))
    {
        return -1;
    }

    if (widths[1] > 0 && (
            render_border_line(stream, VERTICAL_LINE, flags, colors) ||
            fprintf(stream, " %*lld ", widths[1] - 2,
                 llround(step->estimated_rows)) < 0))
    {
        return -1;
    }

    if (widths[2] > 0 && (
            render_border_line(stream, VERTICAL_LINE, flags, colors) ||
            fprintf(stream, " %*lld ", widths[2] - 2, step->rows) < 0))
    {
        return -1;
    }

    if (widths[3] > 0 && (
            render_border_line(stream, VERTICAL_LINE, flags, colors) ||
            fprintf(stream, " %*lld ", widths[3] - 2, step->db_hits) < 0))
    {
        return -1;
    }

    ssize_t ids_len = (widths[4] == 0)? 0 : build_str_list(
            step->identifiers, step->nidentifiers, ids_buffer, ids_bufcap);
    if (ids_len < 0)
    {
        return -1;
    }

    ssize_t args_len = (widths[5] == 0)? 0 : build_args_value(
            step->arguments, args_buffer, args_bufcap);
    if (args_len < 0)
    {
        return -1;
    }

    unsigned int ids_width = (widths[4] > 0)? widths[4] - 2 : 0;
    unsigned int args_width = (widths[5] > 0)? widths[5] - 2 : 0;
    const char *ids = *ids_buffer;
    char *ids_end = (*ids_buffer)+ids_len;
    assert(ids_end < (*ids_buffer)+(*ids_bufcap));
    *ids_end = '\0';
    const char *args = *args_buffer;
    char *args_end = (*args_buffer)+args_len;
    assert(args_end < (*args_buffer)+(*args_bufcap));
    *args_end = '\0';
    for (;;)
    {
        if (widths[4] > 0)
        {
            if (render_border_line(stream, VERTICAL_LINE, flags, colors) ||
                    fprintf(stream, " %-*.*s ", ids_width, ids_width, ids) < 0)
            {
                return -1;
            }
            ids += ids_width;
            if (ids >= ids_end)
            {
                ids = ids_end;
            }
        }

        if (widths[5] > 0)
        {
            if (render_border_line(stream, VERTICAL_LINE, flags, colors) ||
                    fprintf(stream, " %-*.*s ", args_width,
                        args_width, args) < 0 ||
                    render_border_line(stream, VERTICAL_LINE, flags, colors))
            {
                return -1;
            }
            args += args_width;
            if (args >= args_end)
            {
                args = args_end;
            }
        }
        else if (render_border_line(stream, VERTICAL_LINE, flags, colors) ||
                render_overflow(stream, flags, colors->border))
        {
            return -1;
        }

        if (fputc('\n', stream) == EOF)
        {
            return -1;
        }

        if (ids == ids_end && args == args_end)
        {
            break;
        }
        if (render_wrap(stream, last? 0 : depth+1, widths, flags, plan_colors))
        {
            return -1;
        }
    }

    return 0;
}


int render_op(FILE *stream, const char *operator_type, unsigned int op_depth,
        unsigned int width, uint_fast32_t flags,
        const struct neo4j_plan_table_colors *colors)
{
    if (render_border_line(stream, VERTICAL_LINE, flags, ctable_colors(colors)))
    {
        return -1;
    }

    if (fputs(colors->graph[0], stream) == EOF)
    {
        return -1;
    }

    unsigned int offset = 0;
    for (;;)
    {
        offset += 2;
        if (fputc(' ', stream) == EOF)
        {
            return -1;
        }
        if (offset >= op_depth*2)
        {
            if (fputs((flags & NEO4J_RENDER_ASCII_ART)?
                        "*" : u8"\u25B8", stream) == EOF)
            {
                return -1;
            }
            break;
        }
        if (fputs((flags & NEO4J_RENDER_ASCII_ART)?
                    "|" : u8"\u2502", stream) == EOF)
        {
            return -1;
        }
    } while (offset < op_depth*2);

    if (fputs(colors->graph[1], stream) == EOF)
    {
        return -1;
    }

    if (fprintf(stream, "%-*s ", width - offset - 1, operator_type) < 0)
    {
        return -1;
    }
    return 0;
}


ssize_t build_str_list(const char * const *list, unsigned int n,
        char **buffer, size_t *bufcap)
{
    size_t used = 0;
    for (unsigned int i = 0; i < n; ++i)
    {
        assert(used + 2 < *bufcap);
        if (i > 0)
        {
            memcpy((*buffer)+used, ", ", 2);
            used += 2;
        }

        size_t l = strlen(list[i]);
        if ((used + l) >= *bufcap)
        {
            char *newbuf = realloc(*buffer, used + l + 3);
            if (newbuf == NULL)
            {
                return -1;
            }
            *bufcap = used + l + 3;
            *buffer = newbuf;
        }
        memcpy((*buffer)+used, list[i], l);
        used += l;
    }

    assert(used < *bufcap);
    (*buffer)[used] = '\0';
    return used;
}


ssize_t build_args_value(neo4j_value_t args, char **buffer, size_t *bufcap)
{
    if (neo4j_type(args) != NEO4J_MAP)
    {
        return 0;
    }

    const neo4j_value_t skip_keys[] =
        {
            neo4j_string("version"),
            neo4j_string("planner"),
            neo4j_string("planner-impl"),
            neo4j_string("runtime"),
            neo4j_string("runtime-impl"),
            neo4j_string("EstimatedRows"),
            neo4j_string("DbHits"),
            neo4j_string("PageCacheHits"),
            neo4j_string("PageCacheMisses"),
            neo4j_string("Rows"),
            neo4j_string("Time")
        };
    const unsigned int nskip_keys = sizeof(skip_keys) / sizeof(neo4j_value_t);

    size_t used = 0;
    unsigned int nargs = neo4j_map_size(args);
    for (unsigned int i = 0; i < nargs; ++i)
    {
        const neo4j_map_entry_t *entry = neo4j_map_getentry(args, i);
        bool skip = false;
        for (unsigned int k = 0; k < nskip_keys; ++k)
        {
            if (neo4j_eq(entry->key, skip_keys[k]))
            {
                skip = true;
                break;
            }
        }
        if (skip)
        {
            continue;
        }

        assert(used + 2 < *bufcap);
        if (used > 0)
        {
            memcpy((*buffer) + used, "; ", 2);
            used += 2;
        }

        for (;;)
        {
            size_t l;
            if (neo4j_type(entry->value) == NEO4J_STRING)
            {
                neo4j_string_value(entry->value, (*buffer + used),
                        (*bufcap) - used);
                l = neo4j_string_length(entry->value);
            }
            else
            {
                l = neo4j_ntostring(entry->value, (*buffer) + used,
                        (*bufcap) - used);
            }
            if ((used + l) <= *bufcap)
            {
                used += l;
                break;
            }
            char *newbuf = realloc(*buffer, used + l + 3);
            if (newbuf == NULL)
            {
                return -1;
            }
            *bufcap = used + l + 3;
            *buffer = newbuf;
        }
    }

    assert(used < *bufcap);
    (*buffer)[used] = '\0';
    return used;
}


int render_wrap(FILE *stream, unsigned int op_depth, unsigned int widths[4],
        uint_fast32_t flags,
        const struct neo4j_plan_table_colors *plan_colors)
{
    const struct neo4j_results_table_colors *colors = ctable_colors(plan_colors);
    if (render_border_line(stream, VERTICAL_LINE, flags, colors))
    {
        return -1;
    }

    if (fputs(plan_colors->graph[0], stream) == EOF)
    {
        return -1;
    }
    size_t width = 0;
    while (width < op_depth*2)
    {
        width += 2;
        if (fputs((flags & NEO4J_RENDER_ASCII_ART)?
                " |" : u8" \u2502", stream) == EOF)
        {
            return -1;
        }
    }

    if (fputs(plan_colors->graph[1], stream) == EOF)
    {
        return -1;
    }

    width = widths[0] - width;
    for (unsigned int w = width; w > 0; --w)
    {
        if (fputc(' ', stream) == EOF)
        {
            return -1;
        }
    }

    for (unsigned int i = 1; i < 4; ++i)
    {
        if (widths[i] == 0)
        {
            continue;
        }
        if (render_border_line(stream, VERTICAL_LINE, flags, colors))
        {
            return -1;
        }
        for (unsigned int w = widths[i]; w > 0; --w)
        {
            if (fputc(' ', stream) == EOF)
            {
                return -1;
            }
        }
    }

    return 0;
}


int render_tr(FILE *stream, unsigned int op_depth, bool branch,
        unsigned int widths[6], uint_fast32_t flags,
        const struct neo4j_plan_table_colors *plan_colors)
{
    const struct neo4j_results_table_colors *colors = ctable_colors(plan_colors);

    if (widths[0] == 0)
    {
        if (render_row(stream, 0, NULL, true, flags, colors,
                    NULL, NULL, NULL))
        {
            return -1;
        }
        return 0;
    }

    if (render_border_line(stream, VERTICAL_LINE, flags, colors))
    {
        return -1;
    }

    if (fputs(plan_colors->graph[0], stream) == EOF)
    {
        return -1;
    }

    size_t width = 0;
    do
    {
        width += 2;
        if (fputs((flags & NEO4J_RENDER_ASCII_ART)?
                    " |" : u8" \u2502", stream) == EOF)
        {
            return -1;
        }
    } while (width < op_depth*2);

    if (branch && fputc('/', stream) == EOF)
    {
        return -1;
    }

    if (fputs(plan_colors->graph[1], stream) == EOF)
    {
        return -1;
    }

    width = widths[0] - width - (branch? 1 : 0);
    for (unsigned int w = width; w > 0; --w)
    {
        if (fputc(' ', stream) == EOF)
        {
            return -1;
        }
    }

    if (render_hrule(stream, 5, widths+1, HLINE_MIDDLE, (widths[5]==0),
                flags, colors))
    {
        return -1;
    }
    return 0;
}


void calculate_widths(unsigned int widths[6],
        struct neo4j_statement_plan *plan, unsigned int render_width)
{
    unsigned int opr_width = operators_width(plan->output_step);

    unsigned int accum = 1;
    widths[0] = maxu(opr_width, MIN_OPR_WIDTH) + 2;
    widths[1] = EST_WIDTH + 2;
    accum += widths[0] + widths[1] + 2;
    if (plan->is_profile)
    {
        widths[2] = RWS_WIDTH + 2;
        widths[3] = DBH_WIDTH + 2;
        accum += widths[2] + widths[3] + 2;
    }
    else
    {
        widths[2] = 0;
        widths[3] = 0;
    }

    widths[4] = MIN_IDS_WIDTH + 2;
    widths[5] = MIN_OTH_WIDTH + 2;
    if ((accum + widths[4] + widths[5] + 2) < render_width)
    {
        unsigned int half = ((render_width - accum) / 2) - 1;
        if (half > widths[4])
        {
            unsigned int max = maxu(widths[4],
                    identifiers_width(plan->output_step) + 2);
            widths[4] = minu(half, max);
        }
        accum += widths[4] + 1;
        widths[5] = (render_width - accum) - 1;
    }

    accum = 1;
    for (unsigned int i = 0; i < 6; ++i)
    {
        if (widths[i] == 0)
        {
            continue;
        }
        accum += widths[i] + 1;
        if (accum > render_width)
        {
            widths[i] = 0;
        }
    }
}


unsigned int operators_width(struct neo4j_statement_execution_step *step)
{
    assert(step != NULL);
    assert(step->operator_type != NULL);

    unsigned int width = 1+neo4j_u8cswidth(step->operator_type, SIZE_MAX);
    for (unsigned int i = step->nsources; i-- > 0;)
    {
        unsigned int swidth = operators_width(step->sources[i]);
        width = maxu(width, (i > 0)? 2 + swidth : swidth);
    }
    return width;
}


unsigned int identifiers_width(struct neo4j_statement_execution_step *step)
{
    assert(step != NULL);

    unsigned int width = 1+str_list_len(step->identifiers, step->nidentifiers);
    for (unsigned int i = step->nsources; i-- > 0;)
    {
        width = maxu(width, identifiers_width(step->sources[i]));
    }
    return width;
}


unsigned int str_list_len(const char * const *list, unsigned int n)
{
    unsigned int w = 0;
    for (unsigned int i = 0; i < n; ++i)
    {
        w += neo4j_u8cswidth(list[i], SIZE_MAX) + 2;
    }
    return w-2;
}
