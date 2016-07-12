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
#ifndef NEO4J_EVALUATE_H
#define NEO4J_EVALUATE_H

#include "state.h"


typedef struct evaluation_continuation evaluation_continuation_t;
struct evaluation_continuation
{
    int (*complete)(evaluation_continuation_t *self, shell_state_t *state);
    struct cypher_input_position pos;
    void *data;
};


static inline bool is_command(const char *directive)
{
    return (directive[0] == ':');
}

int evaluate_command(shell_state_t *state, const cypher_astnode_t *command);
int evaluate_command_string(shell_state_t *state, const char *command);
evaluation_continuation_t evaluate_statement(shell_state_t *state,
        const char *statement, struct cypher_input_position pos);

#endif/*NEO4J_EVALUATE_H*/
