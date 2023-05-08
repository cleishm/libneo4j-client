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
#include "state.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define NEO4J_MAX_AUTHENTICATION_ATTEMPTS 3


static int attempt_db_connect(struct auth_state *auth_state,
        struct cypher_input_position pos, const char *connect_string,
        const char *port_string);
static int basic_auth_callback(void *userdata, const char *host,
        char *username, size_t usize, char *password, size_t psize);
static int check_url(shell_state_t *state, struct cypher_input_position pos,
        const char *url_string);
static neo4j_connection_t *update_password_and_reconnect(shell_state_t *state,
        neo4j_connection_t *connection,
        struct cypher_input_position pos);

int db_reconnect(shell_state_t *state, struct cypher_input_position pos) {
    if (state->connection != NULL) {
	db_disconnect(state,pos);
    }
    fprintf(stderr,"%s : %s\n", state->connect_string, state->port_string);
    char cs[BUFLEN];
    char ps[BUFLEN];
    strncpy(cs, state->connect_string,BUFLEN-1);
    strncpy(ps, state->port_string,BUFLEN-1);    
    return db_connect(state, pos, cs, ps);
}
    

int db_connect(shell_state_t *state, struct cypher_input_position pos,
        const char *connect_string, const char *port_string)
{
    if (state->connection != NULL && db_disconnect(state, pos))
    {
        return -1;
    }
    assert(state->connection == NULL);

    struct auth_state auth_state = { .attempt = 0, .state = state };
    if (state->password_prompt)
    {
        neo4j_config_set_basic_auth_callback(state->config,
                basic_auth_callback, &auth_state);
    }

    int result = attempt_db_connect(&auth_state, pos,
            connect_string, port_string);
    if (state->password_prompt)
    {
        ignore_unused_result(neo4j_config_set_basic_auth_callback(
                state->config, NULL, NULL));
    }
    if (connect_string)
    {
	strncpy(state->connect_string, connect_string, BUFLEN-1);
    }
    if (port_string)
    {
	strncpy(state->port_string, port_string, BUFLEN-1);
    }
    return result;
}


int attempt_db_connect(struct auth_state *auth_state,
        struct cypher_input_position pos, const char *connect_string,
        const char *port_string)
{
    shell_state_t *state = auth_state->state;
    uint_fast32_t connect_flags = state->connect_flags;
    if (auth_state->attempt > 0)
    {
        connect_flags |= NEO4J_NO_URI_PASSWORD;
        ignore_unused_result(neo4j_config_set_password(state->config, NULL));
    }
    ++(auth_state->attempt);

    neo4j_connection_t *connection;

    if (port_string != NULL)
    {
        char *port_end;
        long port = strtol(port_string, &port_end, 10);
        if (*port_end != '\0' || port <= 0 || port > UINT16_MAX)
        {
            print_error(state, pos, "Invalid port '%s'", port_string);
            return -1;
        }
        connection = neo4j_tcp_connect(connect_string, port, state->config,
                connect_flags);
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
                    connect_flags);
        }
        else
        {
            connection = neo4j_tcp_connect(connect_string, 0, state->config,
                    connect_flags);
        }
    }

    if (connection == NULL)
    {
        switch (errno)
        {
        case NEO4J_NO_SERVER_TLS_SUPPORT:
            print_error(state, pos, "A secure connection could not"
                    " be established (try --insecure)");
            break;
        case NEO4J_INVALID_URI:
            print_error(state, pos, "Invalid URL '%s'", connect_string);
            break;
        case NEO4J_INVALID_CREDENTIALS:
            if (state->password_prompt)
            {
                assert(state->tty != NULL);
                FILE *err = state->err;
                state->err = state->tty; // send message to TTY
                print_errno(state, pos, errno);
                state->err = err;
                if (auth_state->attempt <= NEO4J_MAX_AUTHENTICATION_ATTEMPTS)
                {
                    return attempt_db_connect(auth_state, pos, connect_string,
                            port_string);
                }
                break;
            }
            // fall through
        default:
            print_errno(state, pos, errno);
            break;
        }
        return -1;
    }

    if (state->password_prompt && neo4j_credentials_expired(connection))
    {
        connection = update_password_and_reconnect(state, connection, pos);
        if (connection == NULL)
        {
            return -1;
        }
    }

    if ((neo4j_config_get_username(state->config) == NULL))
    {
        const char *username = neo4j_connection_username(connection);
        if (neo4j_config_set_username(state->config, username))
        {
            print_errno(state, pos, errno);
            neo4j_close(connection);
            return -1;
        }
    }

    state->connection = connection;
    return 0;
}


int basic_auth_callback(void *userdata, const char *host,
        char *username, size_t usize, char *password, size_t psize)
{
    return basic_auth((struct auth_state *)userdata, host,
            username, usize, password, psize);
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
        print_error(state, pos, "Invalid URL '%s' "
                "(you may need to put quotes around the whole URL)",
                url_string);
        return -1;
    }
    else
    {
        return 1;
    }
}


neo4j_connection_t *update_password_and_reconnect(shell_state_t *state,
        neo4j_connection_t *connection,
        struct cypher_input_position pos)
{
    neo4j_config_t *config = NULL;

    char *hostname = strdup(neo4j_connection_hostname(connection));
    if (hostname == NULL)
    {
        print_error_errno(state, pos, errno, "strdup");
        goto failure;
    }

    unsigned int port = neo4j_connection_port(connection);

    const char *username = neo4j_connection_username(connection);
    if (username == NULL)
    {
        print_error(state, pos, "Unexpected error: "
                "credentials have expired, yet no username was provided.");
        goto failure;
    }

    assert(state->tty != NULL);
    fprintf(state->tty,
            "The current password has expired and must be changed.\n");
    char password[NEO4J_MAXPASSWORDLEN];
    if (change_password(state, connection, password, sizeof(password)))
    {
        goto failure;
    }

    // prepare new config
    config = neo4j_config_dup(state->config);
    if (config == NULL || neo4j_config_set_username(config, username) ||
            neo4j_config_set_password(config, password))
    {
        print_errno(state, pos, errno);
        goto failure;
    }

    neo4j_close(connection);

    connection = neo4j_tcp_connect(hostname, port, config,
            state->connect_flags);
    if (connection == NULL)
    {
        print_errno(state, pos, errno);
        goto failure;
    }

    free(hostname);
    neo4j_config_free(config);
    return connection;

failure:
    if (connection != NULL)
    {
        neo4j_close(connection);
    }
    free(hostname);
    neo4j_config_free(config);
    return NULL;
}


int db_disconnect(shell_state_t *state, struct cypher_input_position pos)
{
    if (state->connection == NULL)
    {
        print_error(state, pos, "Not connected");
        return -1;
    }
    neo4j_close(state->connection);
    state->connection = NULL;
    return 0;
}

int db_begin_tx(shell_state_t *state, struct cypher_input_position pos,
                int timeout, const char *mode)
{
  if (state->connection == NULL)
    {
      print_error(state, pos, "Not connected");
      return -1;
    }
  if (state->tx != NULL) {
      if (neo4j_tx_defunct(state->tx) ||
	  neo4j_tx_is_open(state->tx) == 0)
      {
        neo4j_free_tx(state->tx);
	state->tx = NULL;
      }
    else
      {
        print_error(state, pos, "A transaction is already open");
        return -1;
      }
  }

  neo4j_transaction_t *new_tx = neo4j_begin_tx(state->connection, timeout, mode, state->dbname);
  if (new_tx == NULL)
    {
      print_error_errno(state, pos, errno, "Cannot create transaction");
      return -1;
    }
  if (neo4j_tx_failure(new_tx) != 0)
    {
      char msg[256];
      if (neo4j_tx_failure_code(new_tx) != NULL)
      {
	  snprintf(msg, sizeof(msg), "Transaction failed with %s",
		   neo4j_tx_failure_code(new_tx));
      }
      else
      {
	  snprintf(msg, sizeof(msg), "Transaction failed");
      }
      print_error_errno(state, pos, errno, (const char *)msg);
      return -1;
    }
  state->tx = new_tx;
  return 0;
}

int db_commit_tx(shell_state_t *state, struct cypher_input_position pos)
{
  if (state->connection == NULL)
    {
      print_error(state, pos, "Not connected");
      return -1;
    }
  if (state->tx == NULL)
    {
      print_error(state, pos, "No transaction is open");
      return -1;
    }
  int err = 0;
  if (state->tx != NULL)
    {
      if (neo4j_tx_is_open(state->tx) == 0)
        {
          print_error(state, pos, "No transaction is open");
          return -1;
        }
    }
  if (neo4j_commit(state->tx) < 0)
    {
      if (neo4j_tx_defunct(state->tx)) {
        print_error(state, pos, "Transaction timed out or connection reset");
      }
      else {
        char msg[256];
        snprintf(msg, sizeof(msg), "Transaction failed on commit with %s",
                 neo4j_tx_failure_code(state->tx));
        print_error_errno(state, pos, errno, (const char *)msg);
      }
      err = -1;
    }
  neo4j_free_tx(state->tx);
  state->tx = NULL;
  return err;
}

int db_rollback_tx(shell_state_t *state, struct cypher_input_position pos)
{
  if (state->connection == NULL)
    {
      print_error(state, pos, "Not connected");
      return -1;
    }
  if (state->tx == NULL)
    {
      print_error(state, pos, "No transaction is open");
      return -1;
    }
  int err = 0;
  if (state->tx != NULL)
    {
      if (neo4j_tx_is_open(state->tx) == 0)
        {
          print_error(state, pos, "No transaction is open");
          err = -1;
        }
    }
  if (neo4j_rollback(state->tx) < 0)
    {
      if (neo4j_tx_defunct(state->tx)) {
        print_error(state, pos, "Transaction timed out or connection reset");
      }
      else {
        char msg[256];
        snprintf(msg, sizeof(msg), "Transaction failed on rollback with %s",
                 neo4j_tx_failure_code(state->tx));
        print_error_errno(state, pos, errno, (const char *)msg);
      }
      err = -1;
    }
  neo4j_free_tx(state->tx);
  state->tx = NULL;
  return err;
}
