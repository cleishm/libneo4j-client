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
#include "render.h"
#include <assert.h>
#include <ctype.h>
#include <neo4j-client.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>


struct renderer
{
    const char *name;
    int (*handler)(shell_state_t *state,
            struct cypher_input_position pos,
            neo4j_result_stream_t *results);
};


static int terminal_width(shell_state_t *state);

static struct renderer renderers[] =
    { { "table", render_results_table },
      { "csv", render_results_csv },
      { NULL, NULL } };


renderer_t find_renderer(const char *name)
{
    for (unsigned int i = 0; renderers[i].name != NULL; ++i)
    {
        if (strcmp(renderers[i].name, name) == 0)
        {
            return renderers[i].handler;
        }
    }
    return NULL;
}


const char *renderer_name(renderer_t renderer)
{
    for (unsigned int i = 0; renderers[i].name != NULL; ++i)
    {
        if (renderers[i].handler == renderer)
        {
            return renderers[i].name;
        }
    }
    return NULL;
}


int render_results_csv(shell_state_t *state,
        struct cypher_input_position pos,
        neo4j_result_stream_t *results)
{
    return neo4j_render_results_csv(state->config, state->output, results);
}


int render_results_table(shell_state_t *state,
        struct cypher_input_position pos,
        neo4j_result_stream_t *results)
{
    int width = terminal_width(state);
    if (width < 0)
    {
        return -1;
    }
    if (width < 2)
    {
        width = 2;
    }
    return neo4j_render_results_table(state->config, state->output, results,
            width);
}


int terminal_width(shell_state_t *state)
{
    if (state->width > 0)
    {
        return state->width;
    }
    struct winsize w;
    int fd = fileno(state->output);
    if (!isatty(fd))
    {
        return 70;
    }
    if (ioctl(fd, TIOCGWINSZ, &w))
    {
        return -1;
    }
    return w.ws_col;
}


struct update_formats
{
    const char *action;
    const char *singular;
    const char *plural;
};


static struct update_formats update_formats[] = {
    { "created", "node", "nodes" },
    { "deleted", "node", "nodes" },
    { "created", "relationship", "relationships" },
    { "deleted", "relationship", "relationships" },
    { "set", "property", "properties" },
    { "added", "label", "labels" },
    { "removed", "label", "labels" },
    { "created", "index", "indexes" },
    { "dropped", "index", "indexes" },
    { "added", "constraint", "constraints" },
    { "dropped", "constraint", "constraints" },
    { NULL, NULL, NULL }
};


int render_update_counts(shell_state_t *state,
        struct cypher_input_position pos,
        neo4j_result_stream_t *results)
{
    assert(results != NULL);
    struct neo4j_update_counts update_counts = neo4j_update_counts(results);

    unsigned long long counts[] = {
        update_counts.nodes_created,
        update_counts.nodes_deleted,
        update_counts.relationships_created,
        update_counts.relationships_deleted,
        update_counts.properties_set,
        update_counts.labels_added,
        update_counts.labels_removed,
        update_counts.indexes_added,
        update_counts.indexes_removed,
        update_counts.constraints_added,
        update_counts.constraints_removed
    };

    bool first = true;
    struct update_formats *format = update_formats;
    unsigned long long *count = counts;
    for (; format->action != NULL; count++, format++)
    {
        if (*count == 0)
        {
            continue;
        }
        if (first && fputc(toupper(format->action[0]), state->output) == EOF)
        {
            return -1;
        }
        if (!first && fputs(", ", state->output) == EOF)
        {
            return -1;
        }
        if (fprintf(state->output, "%s %llu %s",
                first? format->action + 1 : format->action, *count,
                (*count == 1)? format->singular : format->plural) < 0)
        {
            return -1;
        }
        first = false;
    }

    if (!first && fputc('\n', state->output) == EOF)
    {
        return -1;
    }

    return 0;
}


int render_plan_table(shell_state_t *state,
        struct cypher_input_position pos,
        struct neo4j_statement_plan *plan)
{
    int width = terminal_width(state);
    if (width < 0)
    {
        return -1;
    }
    if (width < 2)
    {
        width = 2;
    }
    if (fprintf(state->output, "Compiler: %s\nPlanner: %s\nRuntime: %s\n%s:\n",
                plan->version, plan->planner, plan->runtime,
                plan->is_profile? "Profile":"Plan") < 0)
    {
        return -1;
    }
    return neo4j_render_plan_ctable(state->config, state->output, plan, width);
}


int render_timing(shell_state_t *state,
        struct cypher_input_position pos,
        neo4j_result_stream_t *results,
        unsigned long long client_time)
{
    assert(results != NULL);
    unsigned long long count = neo4j_result_count(results);
    unsigned long long available = neo4j_results_available_after(results);
    unsigned long long consumed = neo4j_results_consumed_after(results);

    if (fprintf(state->output, "%lld %s returned in %lldms (",
            count, (count != 1)? "rows" : "row", available + consumed) < 0)
    {
        return -1;
    }

    if (count > 0 && fprintf(state->output, "first row after %lldms, ",
            available) < 0)
    {
        return -1;
    }

    if (fprintf(state->output, "rendered after %lldms)\n", client_time) < 0)
    {
        return -1;
    }

    return 0;
}
