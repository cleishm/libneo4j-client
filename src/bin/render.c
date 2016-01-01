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
#include <neo4j-client.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>


struct renderer
{
    const char *name;
    int (*handler)(shell_state_t *state, neo4j_result_stream_t *results);
};


static int terminal_width(shell_state_t *state);

static struct renderer renderers[] =
    { { "table", render_table },
      { "csv", render_csv },
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


int render_csv(shell_state_t *state, neo4j_result_stream_t *results)
{
    return neo4j_render_csv(state->out, results, state->render_flags);
}


int render_table(shell_state_t *state, neo4j_result_stream_t *results)
{
    int width = terminal_width(state);
    if (width < 2)
    {
        fprintf(state->err, "ERROR: terminal width of %d too narrow "
                "(use :output csv?)\n", width);
        return -1;
    }
    return neo4j_render_table(state->out, results, width, state->render_flags);
}


int terminal_width(shell_state_t *state)
{
    if (state->width > 0)
    {
        return state->width;
    }
    struct winsize w;
    if (ioctl(fileno(state->out), TIOCGWINSZ, &w))
    {
        return -1;
    }
    return w.ws_col;
}
