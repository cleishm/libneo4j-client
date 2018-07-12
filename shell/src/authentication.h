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
#ifndef NEO4J_AUTHENTICATION_H
#define NEO4J_AUTHENTICATION_H

#include <neo4j-client.h>
#include "state.h"


struct auth_state
{
    shell_state_t *state;
    unsigned int attempt;
};


int basic_auth(struct auth_state *auth_state, const char *host,
        char *username, size_t usize, char *password, size_t psize);

int change_password(shell_state_t *state, neo4j_connection_t *connection,
        char *password, size_t pwlen);

#endif/*NEO4J_AUTHENTICATION_H*/
