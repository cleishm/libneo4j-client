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
#ifndef NEO4J_STATE_H
#define NEO4J_STATE_H

#include "util.h"
#include <neo4j-client.h>
#include <cypher-parser.h>
#include <stdint.h>
#include <stdio.h>


typedef struct shell_state shell_state_t;
struct shell_state
{
    const char *prog_name;
    FILE *in;
    FILE *out;
    FILE *err;
    FILE *tty;
    bool interactive;
    const char *histfile;
    unsigned int pipeline_max;
    neo4j_config_t *config;
    uint_fast32_t connect_flags;
    neo4j_connection_t *connection;
    neo4j_session_t *session;
    char *temp_buffer;
    size_t temp_buffer_capacity;
    int (*render)(shell_state_t *state, neo4j_result_stream_t *results);
    int width;
    uint_fast16_t render_flags;

    neo4j_map_entry_t *exports;
    void **exports_storage;
    size_t exports_cap;
    unsigned int nexports;
};


int shell_state_init(shell_state_t *state, const char *prog_name,
        FILE *in, FILE *out, FILE *err, FILE *tty);
void shell_state_destroy(shell_state_t *state);


int shell_state_add_export(shell_state_t *state, neo4j_value_t name,
        neo4j_value_t value, void *storage);

void shell_state_unexport(shell_state_t *state, neo4j_value_t name);

static inline neo4j_value_t shell_state_get_exports(shell_state_t *state)
{
    return neo4j_map(state->exports, state->nexports);
}


static inline char *temp_copy(shell_state_t *state, const char *s, size_t n)
{
    return strncpy_alloc(&(state->temp_buffer), &(state->temp_buffer_capacity),
            s, n);
}


#endif/*NEO4J_STATE_H*/
