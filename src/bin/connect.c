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
#include "connect.h"
#include "authentication.h"
#include <assert.h>
#include <errno.h>


static int check_url(shell_state_t *state, struct cypher_input_position pos,
        const char *url_string);
static int update_password_and_reconnect(shell_state_t *state,
        struct cypher_input_position pos);


int db_connect(shell_state_t *state, struct cypher_input_position pos,
        const char *connect_string, const char *port_string)
{
    if (state->connection != NULL && db_disconnect(state, pos))
    {
        return -1;
    }
    assert(state->connection == NULL);

    neo4j_connection_t *connection;

    if (port_string != NULL)
    {
        char *port_end;
        long port = strtol(port_string, &port_end, 10);
        if (*port_end != '\0' || port <= 0 || port > UINT16_MAX)
        {
            print_error(state, pos, "invalid port '%s'", port_string);
            return -1;
        }
        connection = neo4j_tcp_connect(connect_string, port, state->config,
                state->connect_flags);
    }
    else
    {
        int r = check_url(state, pos, connect_string);
        if (r < 0)
        {
            return -1;
        }
        if (r == 0)
        {
            connection = neo4j_connect(connect_string, state->config,
                    state->connect_flags);
        }
        else
        {
            connection = neo4j_tcp_connect(connect_string, 0, state->config,
                    state->connect_flags);
        }
    }

    if (connection == NULL)
    {
        switch (errno)
        {
        case NEO4J_NO_SERVER_TLS_SUPPORT:
            print_error(state, pos, "connection failed: A secure"
                    " connection could not be esablished (try --insecure)");
            break;
        case NEO4J_INVALID_URI:
            print_error(state, pos, "invalid URL '%s'", connect_string);
            break;
        default:
            print_error_errno(state, pos, errno, "connection failed");
            break;
        }
        return -1;
    }

    state->connection = connection;

    if (state->password_prompt && neo4j_credentials_expired(connection) &&
            update_password_and_reconnect(state, pos))
    {
        assert(state->connection == NULL);
        return -1;
    }

    return 0;
}


int check_url(shell_state_t *state, struct cypher_input_position pos,
        const char *url_string)
{
    const char *colon = strchr(url_string, ':');
    if (colon == NULL)
    {
        return 1;
    }
    else if (*(colon + 1) == '/' && *(colon + 2) == '/')
    {
        return 0;
    }
    else if (*(colon + 1) == '\0')
    {
        print_error(state, pos, "invalid URL '%s' "
                "(you may need to put quotes around the whole URL)",
                url_string);
        return -1;
    }
    else
    {
        return 1;
    }
}


int update_password_and_reconnect(shell_state_t *state,
        struct cypher_input_position pos)
{
    neo4j_connection_t *connection = state->connection;
    state->connection = NULL;

    neo4j_config_t *config = NULL;

    char *hostname = strdup(neo4j_connection_hostname(connection));
    if (hostname == NULL)
    {
        goto failure;
    }

    unsigned int port = neo4j_connection_port(connection);

    const char *username = neo4j_connection_username(connection);
    if (username == NULL)
    {
        print_error(state, pos, "connection failed: "
                "credentials have expired, yet no username was provided.");
        goto failure_cleanup;
    }

    config = neo4j_config_dup(state->config);
    if (config == NULL)
    {
        goto failure;
    }

    assert(state->tty != NULL);
    fprintf(state->tty,
            "The current password has expired and must be changed.\n");
    char password[NEO4J_MAXPASSWORDLEN];
    if (change_password(state, connection, password, sizeof(password)))
    {
        goto failure_cleanup;
    }

    if (neo4j_config_set_username(config, username))
    {
        goto failure;
    }

    if (neo4j_config_set_password(config, password))
    {
        goto failure;
    }

    neo4j_close(connection);

    connection = neo4j_tcp_connect(hostname, port, config,
            state->connect_flags);
    if (connection == NULL)
    {
        goto failure;
    }

    state->connection = connection;
    free(hostname);
    neo4j_config_free(config);
    return 0;

failure:
    print_error_errno(state, pos, errno, "connection failed");
failure_cleanup:
    if (connection != NULL)
    {
        neo4j_close(connection);
    }
    free(hostname);
    neo4j_config_free(config);
    return -1;
}


int db_disconnect(shell_state_t *state, struct cypher_input_position pos)
{
    if (state->connection == NULL)
    {
        print_error(state, pos, "not connected");
        return -1;
    }
    neo4j_close(state->connection);
    state->connection = NULL;
    return 0;
}
