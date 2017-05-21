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
#ifndef NEO4J_RENDER_H
#define NEO4J_RENDER_H

#include "state.h"

renderer_t find_renderer(const char *name);
const char *renderer_name(renderer_t renderer);

int render_results_csv(shell_state_t *state,
        struct cypher_input_position pos,
        neo4j_result_stream_t *results);

int render_results_table(shell_state_t *state,
        struct cypher_input_position pos,
        neo4j_result_stream_t *results);

int render_update_counts(shell_state_t *state,
        struct cypher_input_position pos,
        neo4j_result_stream_t *results);

int render_plan_table(shell_state_t *state,
        struct cypher_input_position pos,
        struct neo4j_statement_plan *plan);

int render_timing(shell_state_t *state,
        struct cypher_input_position pos,
        neo4j_result_stream_t *results,
        unsigned long long client_time);

#endif/*NEO4J_RENDER_H*/
