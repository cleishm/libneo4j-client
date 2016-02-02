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
#include "state.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>


int shell_state_init(shell_state_t *state, const char *prog_name,
        FILE *in, FILE *out, FILE *err, FILE *tty)
{
    memset(state, 0, sizeof(shell_state_t));
    state->prog_name = prog_name;
    state->in = in;
    state->out = out;
    state->err = err;
    state->tty = tty;
    return 0;
}


void shell_state_destroy(shell_state_t *state)
{
    assert(state != NULL);

    if (state->temp_buffer != NULL)
    {
        free(state->temp_buffer);
    }
    if (state->session != NULL)
    {
        neo4j_end_session(state->session);
    }
    if (state->connection != NULL)
    {
        neo4j_close(state->connection);
    }
}


char *temp_copy(shell_state_t *state, const char *s, size_t n)
{
    if (state->temp_buffer_size < n+1)
    {
        char *updated = realloc(state->temp_buffer, n+1);
        if (updated == NULL)
        {
            return NULL;
        }
        state->temp_buffer = updated;
        state->temp_buffer_size = n+1;
    }
    memcpy(state->temp_buffer, s, n+1);
    state->temp_buffer[n] = '\0';
    return state->temp_buffer;
}
