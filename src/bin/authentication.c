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
#include "authentication.h"
#include "readpass.h"
#include "state.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>


int auth_reattempt(void *userdata, const char *host, unsigned int attempts,
        int error, char *username, size_t usize, char *password, size_t psize)
{
    assert(psize > 1);
    shell_state_t *state = (shell_state_t *)userdata;
    if (error != 0)
    {
        neo4j_perror(state->tty, error, "Authentication failed");
    }
    if (attempts > 3)
    {
        return NEO4J_AUTHENTICATION_FAIL;
    }

    if (username[0] == '\0' && readpassphrase("Username: ", username, usize,
            RPP_REQUIRE_TTY | RPP_ECHO_ON) == NULL)
    {
        return -1;
    }

    if (readpassphrase("Password: ", password, psize, RPP_REQUIRE_TTY) == NULL)
    {
        return -1;
    }
    return NEO4J_AUTHENTICATION_REATTEMPT;
}
