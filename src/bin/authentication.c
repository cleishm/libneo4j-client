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


int basic_auth(struct auth_state *auth_state, const char *host,
        char *username, size_t usize, char *password, size_t psize)
{
    if (auth_state->attempt > 1 || username[0] == '\0')
    {
        assert(usize > 1);

        char default_username[NEO4J_MAXUSERNAMELEN + 1];
        assert(strlen(username) <= NEO4J_MAXUSERNAMELEN);
        strncpy(default_username, username, sizeof(default_username));

        char uprompt[NEO4J_MAXUSERNAMELEN + 14];
        if (username[0] != '\0')
        {
            snprintf(uprompt, sizeof(uprompt),
                    "Username [%s]: ", default_username);
        }
        else
        {
            strncpy(uprompt, "Username: ", sizeof(uprompt));
        }

        if (readpassphrase(uprompt, username, usize,
                RPP_REQUIRE_TTY | RPP_ECHO_ON) == NULL)
        {
            return -1;
        }

        if (username[0] == '\0')
        {
            strncpy(username, default_username, usize);
        }
    }

    assert(psize > 1);
    if (readpassphrase("Password: ", password, psize, RPP_REQUIRE_TTY) == NULL)
    {
        return -1;
    }
    return 0;
}


int change_password(shell_state_t *state, neo4j_connection_t *connection,
        char *password, size_t pwlen)
{
    assert(state->tty != NULL);

    char confirm[NEO4J_MAXPASSWORDLEN];
    assert(sizeof(confirm) >= pwlen);

    do
    {
        if (readpassphrase("New Password: ", password, pwlen,
                    RPP_REQUIRE_TTY) == NULL)
        {
            return -1;
        }

        if (readpassphrase("Retype Password: ", confirm, sizeof(confirm),
                    RPP_REQUIRE_TTY) == NULL)
        {
            return -1;
        }

        if (strcmp(password, confirm) == 0)
        {
            break;
        }

        fprintf(state->tty, "Password does not match. Try again.\n");
    } while (strcmp(password, confirm) != 0);

    neo4j_map_entry_t param =
            neo4j_map_entry("password", neo4j_string(password));
    // FIXME: should use neo4j_send, but that results in a failure.
    neo4j_result_stream_t *results = neo4j_run(connection,
            "CALL dbms.changePassword({password})", neo4j_map(&param, 1));
    if (results == NULL)
    {
        return -1;
    }

    int err = neo4j_check_failure(results);

    int result = -1;
    switch (err)
    {
    case 0:
        result = 0;
        break;

    case NEO4J_STATEMENT_EVALUATION_FAILED:
        fprintf(state->err, "%s\n", neo4j_error_message(results));
        break;

    default:
        neo4j_perror(state->err, err, "Password change failed");
        break;
    }

    neo4j_close_results(results);
    return result;
}
