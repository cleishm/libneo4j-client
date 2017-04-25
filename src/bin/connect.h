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
#ifndef NEO4J_CONNECT_H
#define NEO4J_CONNECT_H

#include "state.h"

int db_connect(shell_state_t *state, struct cypher_input_position pos,
        const char *connect_string, const char *port_string);
int db_disconnect(shell_state_t *state, struct cypher_input_position pos);

#endif/*NEO4J_CONNECT_H*/
