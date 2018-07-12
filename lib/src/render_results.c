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


#define NEO4J_RENDER_AUTO_COLWIDTH_INSPECT 100


static int peek_widths(const neo4j_config_t *config,
        neo4j_result_stream_t *results, unsigned int nfields,
        unsigned int widths[]);

struct obtain_fieldname_cb_data
{
    neo4j_result_stream_t *results;
    uint_fast32_t flags;
};

struct obtain_result_field_cb_data
{
    neo4j_result_t *result;
    char **buffer;
    size_t *bufcap;
    uint_fast32_t flags;
};

static ssize_t obtain_fieldname(void *data, unsigned int n, const char **s,
        bool *duplicate);
ssize_t obtain_result_field(void *data, unsigned int n, const char **s,
        bool *duplicate);

static ssize_t render_field_value(neo4j_value_t value, const char **s,
        char **buf, size_t *bufcap, uint_fast32_t flags);
static size_t value_tostring(neo4j_value_t *value, char *buf, size_t n,
        uint_fast32_t flags);
static int write_csv_quoted_string(FILE *stream, const char *s, size_t n);
static int write_value(FILE *stream, const neo4j_value_t *value,
        char **buf, size_t *bufcap, uint_fast32_t flags);


int neo4j_render_table(FILE *stream, neo4j_result_stream_t *results,
        unsigned int width, uint_fast32_t flags)
{
    neo4j_config_t *config = neo4j_new_config();
    config->render_flags |= flags;
    neo4j_config_set_results_table_colors(config,
            (flags & NEO4J_RENDER_ANSI_COLOR)? neo4j_results_table_ansi_colors :
            neo4j_results_table_no_colors);
    int err = neo4j_render_results_table(config, stream, results, width);
    neo4j_config_free(config);
    return err;
}


int neo4j_render_results_table(const neo4j_config_t *config, FILE *stream,
        neo4j_result_stream_t *results, unsigned int width)
{
    REQUIRE(config != NULL, -1);
    REQUIRE(stream != NULL, -1);
    REQUIRE(results != NULL, -1);
    REQUIRE(width > 1 && width < NEO4J_RENDER_MAX_WIDTH, -1);

    int err = neo4j_check_failure(results);
    if (err != 0)
    {
        errno = err;
        return -1;
    }

    unsigned int nfields = neo4j_nfields(results);
    if (nfields == 0)
    {
        return 0;
    }

    uint_fast32_t flags = normalize_render_flags(config->render_flags);
    const struct neo4j_results_table_colors *colors =
            config->results_table_colors;

    // create array of column widths
    unsigned int *widths = NULL;
    if (nfields > 0 &&
            (widths = malloc(nfields * sizeof(unsigned int))) == NULL)
    {
        return -1;
    }

    size_t bufcap = 0;
    char *buffer = NULL;

    unsigned int min_col_width = (flags & NEO4J_RENDER_WRAP_VALUES)? 5 : 3;

    for (unsigned int i = 0; i < nfields; ++i)
    {
        widths[i] = min_col_width;
    }
    if (peek_widths(config, results, nfields, widths))
    {
        goto failure;
    }
    if (fit_column_widths(nfields, widths, min_col_width,
            width - ((nfields > ((width - 1) / min_col_width))? 2 : 1)))
    {
        goto failure;
    }
    for (unsigned int i = 0; i < nfields; ++i)
    {
        if (widths[i] > 0)
        {
            --(widths[i]);
        }
    }
    bool undersize = (widths[nfields - 1] == 0);

    // render head
    if (render_hrule(stream, nfields, widths, HLINE_TOP, undersize, flags,
                colors))
    {
        goto failure;
    }

    struct obtain_fieldname_cb_data data =
            { .results = results, .flags = flags };
    if (render_row(stream, nfields, widths, undersize, flags, colors,
            colors->header, obtain_fieldname, &data))
    {
        goto failure;
    }

    if (render_hrule(stream, nfields, widths, HLINE_HEAD, undersize, flags,
                colors))
    {
        goto failure;
    }

    // render body
    neo4j_result_t *result;
    for (bool first = true; (result = neo4j_fetch_next(results)) != NULL;
            first = false)
    {
        if (!first && (flags & NEO4J_RENDER_ROWLINES) &&
                render_hrule(stream, nfields, widths, HLINE_MIDDLE, undersize,
                    flags, colors))
        {
            goto failure;
        }

        struct obtain_result_field_cb_data data =
            { .result = result, .flags = flags,
              .buffer = &buffer, .bufcap = &bufcap };
        if (render_row(stream, nfields, widths, undersize, flags, colors,
                colors->cells, obtain_result_field, &data))
        {
            goto failure;
        }
    }

    err = neo4j_check_failure(results);
    if (err != 0)
    {
        errno = err;
        goto failure;
    }

    if (render_hrule(stream, nfields, widths, HLINE_BOTTOM, undersize, flags,
                colors))
    {
        goto failure;
    }

    if (fflush(stream) == EOF)
    {
        return -1;
    }

    free(widths);
    free(buffer);
    return 0;

    int errsv;
failure:
    errsv = errno;
    fflush(stream);
    free(widths);
    free(buffer);
    errno = errsv;
    return -1;
}


int peek_widths(const neo4j_config_t *config, neo4j_result_stream_t *results,
        unsigned int nfields, unsigned int widths[])
{
    if (config->render_inspect_rows == 0)
    {
        return 0;
    }

    for (unsigned int i = 0; i < nfields; ++i)
    {
        const char *s = neo4j_fieldname(results, i);
        size_t l = (s == NULL)? 0 : strlen(s);
        if (l > UINT_MAX-3)
        {
            l = UINT_MAX-3;
        }
        l += 3;
        if (widths[i] < l)
        {
            widths[i] = l;
        }
    }

    for (unsigned int depth = 0; depth < config->render_inspect_rows - 1;
            ++depth)
    {
        neo4j_result_t *result = neo4j_peek(results, depth);
        if (result == NULL)
        {
            break;
        }
        for (unsigned int i = 0; i < nfields; ++i)
        {
            neo4j_value_t value = neo4j_result_field(result, i);
            size_t l = 0;

            if (neo4j_type(value) == NEO4J_STRING &&
                    !(config->render_flags & NEO4J_RENDER_QUOTE_STRINGS))
            {
                l = neo4j_string_length(value);
            }
            else
            {
                l = value_tostring(&value, NULL, 0, config->render_flags);
            }
            if (l > UINT_MAX-3)
            {
                l = UINT_MAX-3;
            }
            l += 3;

            if (widths[i] < l)
            {
                widths[i] = l;
            }
        }
    }

    return 0;
}


ssize_t obtain_fieldname(void *data, unsigned int n, const char **s,
        bool *duplicate)
{
    struct obtain_fieldname_cb_data *cdata =
            (struct obtain_fieldname_cb_data *)data;
    *s = neo4j_fieldname(cdata->results, n);
    *duplicate = false;
    return (*s != NULL)? strlen(*s) : 0;
}


ssize_t obtain_result_field(void *data, unsigned int n, const char **s,
        bool *duplicate)
{
    struct obtain_result_field_cb_data *cdata =
            (struct obtain_result_field_cb_data *)data;
    neo4j_value_t value = neo4j_result_field(cdata->result, n);

    if (neo4j_type(value) == NEO4J_STRING &&
            !(cdata->flags & NEO4J_RENDER_QUOTE_STRINGS))
    {
        *s = neo4j_ustring_value(value);
        *duplicate = false;
        return neo4j_string_length(value);
    }

    *duplicate = true;
    return render_field_value(value, s, cdata->buffer, cdata->bufcap,
            cdata->flags);
}


ssize_t render_field_value(neo4j_value_t value, const char **s,
        char **buf, size_t *bufcap, uint_fast32_t flags)
{
    size_t length;
    do
    {
        length = value_tostring(&value, *buf, *bufcap, flags);
        if (length < *bufcap)
        {
            break;
        }

        size_t newcap = length + 80;
        char *newbuf = realloc(*buf, newcap);
        if (newbuf == NULL)
        {
            return -1;
        }
        *bufcap = newcap;
        *buf = newbuf;
    } while (true);

    *s = *buf;
    return length;
}


size_t value_tostring(neo4j_value_t *value, char *buf, size_t n,
        uint_fast32_t flags)
{
    if (!(flags & NEO4J_RENDER_SHOW_NULLS) && neo4j_is_null(*value))
    {
        if (n > 0)
        {
            assert(buf != NULL);
            buf[0] = '\0';
        }
        return 0;
    }
    return neo4j_ntostring(*value, buf, n);
}


int neo4j_render_csv(FILE *stream, neo4j_result_stream_t *results,
        uint_fast32_t flags)
{
    neo4j_config_t *config = neo4j_new_config();
    config->render_flags |= flags;
    int err = neo4j_render_results_csv(config, stream, results);
    neo4j_config_free(config);
    return err;
}


// deprecated
int neo4j_render_ccsv(const neo4j_config_t *config, FILE *stream,
        neo4j_result_stream_t *results)
{
    return neo4j_render_results_csv(config, stream, results);
}


int neo4j_render_results_csv(const neo4j_config_t *config, FILE *stream,
        neo4j_result_stream_t *results)
{
    REQUIRE(config != NULL, -1);
    REQUIRE(stream != NULL, -1);
    REQUIRE(results != NULL, -1);

    uint_fast32_t flags = normalize_render_flags(config->render_flags);

    size_t bufcap = NEO4J_FIELD_BUFFER_INITIAL_CAPACITY;
    char *buffer = malloc(bufcap);
    if (buffer == NULL)
    {
        return -1;
    }

    int err = neo4j_check_failure(results);
    if (err != 0)
    {
        errno = err;
        goto failure;
    }

    unsigned int nfields = neo4j_nfields(results);
    if (nfields == 0)
    {
        free(buffer);
        return 0;
    }

    for (unsigned int i = 0; i < nfields; ++i)
    {
        const char *fieldname = neo4j_fieldname(results, i);
        if (write_csv_quoted_string(stream, fieldname, strlen(fieldname)))
        {
            goto failure;
        }
        if ((i + 1) < nfields && fputc(',', stream) == EOF)
        {
            goto failure;
        }
    }
    if (fputc('\n', stream) == EOF)
    {
        goto failure;
    }

    neo4j_result_t *result;
    while ((result = neo4j_fetch_next(results)) != NULL)
    {
        for (unsigned int i = 0; i < nfields; ++i)
        {
            neo4j_value_t value = neo4j_result_field(result, i);
            if (write_value(stream, &value, &buffer, &bufcap, flags))
            {
                goto failure;
            }
            if ((i + 1) < nfields && fputc(',', stream) == EOF)
            {
                goto failure;
            }
        }
        if (fputc('\n', stream) == EOF)
        {
            goto failure;
        }
    }

    err = neo4j_check_failure(results);
    if (err != 0)
    {
        errno = err;
        goto failure;
    }

    if (fflush(stream) == EOF)
    {
        goto failure;
    }

    free(buffer);
    return 0;

    int errsv;
failure:
    errsv = errno;
    if (buffer != NULL)
    {
        free(buffer);
    }
    fflush(stream);
    errno = errsv;
    return -1;
}


int write_csv_quoted_string(FILE *stream, const char *s, size_t n)
{
    if (fputc('"', stream) == EOF)
    {
        return -1;
    }

    const char *end = s + n;
    while (s < end)
    {
        const char *c = (const char *)memchr((void *)(intptr_t)s, '"', n);
        if (c == NULL)
        {
            if (fwrite(s, sizeof(char), n, stream) < n)
            {
                return -1;
            }
            break;
        }

        assert(c >= s && c < end);
        assert(*c == '"');
        size_t l = c - s;
        if (fwrite(s, sizeof(char), l, stream) < l)
        {
            return -1;
        }
        if (fputs("\"\"", stream) == EOF)
        {
            return -1;
        }
        n -= l+1;
        s = c+1;
    }
    if (fputc('"', stream) == EOF)
    {
        return -1;
    }
    return 0;
}


int write_value(FILE *stream, const neo4j_value_t *value,
        char **buf, size_t *bufcap, uint_fast32_t flags)
{
    neo4j_type_t type = neo4j_type(*value);

    if (type == NEO4J_STRING)
    {
        return write_csv_quoted_string(stream, neo4j_ustring_value(*value),
                neo4j_string_length(*value));
    }

    if (!(flags & NEO4J_RENDER_SHOW_NULLS) && type == NEO4J_NULL)
    {
        return 0;
    }

    assert(*bufcap >= 2);
    do
    {
        size_t required = neo4j_ntostring(*value, *buf, *bufcap);
        if (required < *bufcap)
        {
            break;
        }

        char *newbuf = realloc(*buf, required);
        if (newbuf == NULL)
        {
            return -1;
        }
        *bufcap = required;
        *buf = newbuf;
    } while (true);

    if (type == NEO4J_NULL || type == NEO4J_BOOL || type == NEO4J_INT ||
            type == NEO4J_FLOAT)
    {
        if (fputs(*buf, stream) == EOF)
        {
            return -1;
        }
        return 0;
    }
    return write_csv_quoted_string(stream, *buf, strlen(*buf));
}
