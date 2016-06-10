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
    state->pipeline_max = NEO4J_DEFAULT_MAX_PIPELINED_REQUESTS / 2;
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


static int neo4j_string_cmp(neo4j_value_t s1, neo4j_value_t s2)
{
    size_t s1len = neo4j_string_length(s1);
    size_t s2len = neo4j_string_length(s2);
    size_t n = (s1len < s2len)? s1len : s2len;
    int cmp = strncmp(neo4j_ustring_value(s1), neo4j_ustring_value(s2), n);
    return cmp? cmp : (s1len < s2len)? -1 : (s1len == s2len)? 0 : 1;
}


int shell_state_add_export(shell_state_t *state, neo4j_value_t name,
        neo4j_value_t value, void *storage)
{
    int cmp;
    unsigned int i;
    for (i = 0; i < state->nexports &&
            (cmp = neo4j_string_cmp(state->exports[i].key, name)) < 0; ++i)
        ;
    if (cmp == 0)
    {
        state->exports[i].key = name;
        state->exports[i].value = value;
        free(state->exports_storage[i]);
        state->exports_storage[i] = storage;
        return 0;
    }

    assert(state->nexports <= state->exports_cap);
    if (state->nexports >= state->exports_cap)
    {
        size_t cap = (state->exports_cap == 0)? 8 : state->exports_cap * 2;
        neo4j_map_entry_t *exports = realloc(state->exports,
                cap * sizeof(neo4j_map_entry_t));
        if (exports == NULL)
        {
            return -1;
        }

        void **storage = realloc(state->exports_storage, cap * sizeof(void *));
        if (storage == NULL)
        {
            free(exports);
            return -1;
        }

        state->exports = exports;
        state->exports_storage = storage;
        state->exports_cap = cap;
    }

    if (i < state->nexports)
    {
        memmove(state->exports + i + 1, state->exports + i,
                (state->nexports - i) * sizeof(neo4j_map_entry_t));
        memmove(state->exports_storage + i + 1, state->exports_storage + i,
                (state->nexports - i) * sizeof(void *));
    }
    state->exports[i].key = name;
    state->exports[i].value = value;
    state->exports_storage[i] = storage;
    ++(state->nexports);
    return 0;
}


void shell_state_unexport(shell_state_t *state, neo4j_value_t name)
{
    for (unsigned int i = 0; i < state->nexports; ++i)
    {
        if (neo4j_eq(state->exports[i].key, name))
        {
            free(state->exports_storage[i]);
            state->exports[i] = state->exports[state->nexports-1];
            state->exports_storage[i] =
                    state->exports_storage[state->nexports-1];
            --(state->nexports);
            return;
        }
    }
}
