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
#include "commands.h"
#include "render.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>


typedef struct shell_command shell_command_t;
struct shell_command
{
    const char *name;
    int (*action)(shell_state_t *state, const char *args);
};


static int help(shell_state_t *state, const char *args);
static int output(shell_state_t *state, const char *args);
static int width(shell_state_t *state, const char *args);
static int quit(shell_state_t *state, const char *args);

static shell_command_t shell_commands[] =
    { { "exit", quit },
      { "quit", quit },
      { "connect", db_connect },
      { "disconnect", db_disconnect },
      { "help", help },
      { "output", output },
      { "width", width },
      { NULL, NULL } };


int evaluate_command(shell_state_t *state, const char *command)
{
    assert(command[0] == ':');
    command++;
    size_t wlen = strcspn(command, " ");
    const char *args = command + wlen;
    while (isspace(*args))
    {
        ++args;
    }

    for (unsigned int i = 0; shell_commands[i].name != NULL; ++i)
    {
        if (strlen(shell_commands[i].name) == wlen &&
            strncmp(shell_commands[i].name, command, wlen) == 0)
        {
            return shell_commands[i].action(state, args);
        }
    }
    fprintf(state->err, "Unknown command '%s'\n", command);
    return 0;
}


int db_connect(shell_state_t *state, const char *args)
{
    const char *s;
    size_t n;
    bool complete;
    if (neo4j_cli_arg_parse(args, &s, &n, &complete) < 0 || !complete)
    {
        fprintf(state->err, ":connect requires a URI to connect to\n");
        return -1;
    }

    assert(n > 0);
    char *uri_string = strndup(s, n);
    if (uri_string == NULL)
    {
        neo4j_perror(state->err, errno, "unexpected error");
        return -1;
    }

    int result = -1;

    if (state->session != NULL)
    {
        if (db_disconnect(state, NULL))
        {
            goto cleanup;
        }
    }
    assert(state->session == NULL);

    neo4j_connection_t *connection =
        neo4j_connect(uri_string, state->config, state->connect_flags);
    if (connection == NULL)
    {
        if (errno == NEO4J_NO_SERVER_TLS_SUPPORT)
        {
            fprintf(state->err, "connection to '%s' failed: A secure"
                    " connection could not be esablished (try --insecure)\n",
                    uri_string);
        }
        else
        {
            char ebuf[512];
            fprintf(state->err, "connection to '%s' failed: %s\n", uri_string,
                    neo4j_strerror(errno, ebuf, sizeof(ebuf)));
        }
        goto cleanup;
    }

    neo4j_session_t *session = neo4j_new_session(connection);
    if (session == NULL)
    {
        char ebuf[512];
        fprintf(state->err, "connection to '%s' failed: %s\n", uri_string,
                neo4j_strerror(errno, ebuf, sizeof(ebuf)));
        neo4j_close(connection);
        goto cleanup;
    }

    state->connection = connection;
    state->session = session;
    result = 0;

cleanup:
    free(uri_string);
    return result;
}


int db_disconnect(shell_state_t *state, const char *args)
{
    if (state->session == NULL)
    {
        fprintf(state->err, "ERROR: not connected\n");
        return -1;
    }
    neo4j_end_session(state->session);
    state->session = NULL;
    neo4j_close(state->connection);
    state->connection = NULL;
    return 0;
}


int help(shell_state_t *state, const char *args)
{
    fprintf(state->out,
":quit                  Exit the shell\n"
":connect '<url>'       Connect to the specified URL\n"
":disconnect            Disconnect the client from the server\n"
":help                  Show usage information\n"
":output (table|csv)    Set the output format\n"
":width <n>             Set the number of columns in the table output\n");
    fflush(state->out);
    return 0;
}


int output(shell_state_t *state, const char *args)
{
    const char *s;
    size_t n;
    bool complete;
    if (neo4j_cli_arg_parse(args, &s, &n, &complete) < 0 || !complete)
    {
        fprintf(state->err, ":output requires a rendering format "
                "(table or csv)\n");
        return -1;
    }

    const char *name = temp_copy(state, s, n);
    renderer_t renderer = find_renderer(name);
    if (renderer == NULL)
    {
        fprintf(state->err, "Unknown output format '%s'\n", args);
        return -1;
    }
    state->render = renderer;
    return 0;
}


int width(shell_state_t *state, const char *args)
{
    const char *s;
    size_t n;
    bool complete;
    if (neo4j_cli_arg_parse(args, &s, &n, &complete) < 0 || !complete)
    {
        fprintf(state->err, ":width requires an integer value, or 'auto'\n");
        return -1;
    }

    const char *arg = temp_copy(state, s, n);
    if (strcmp(arg, "auto") == 0)
    {
        if (!isatty(fileno(state->out)))
        {
            fprintf(state->err, "Setting width to auto is only possible"
                    " when outputing to a tty\n");
            return -1;
        }
        state->width = 0;
        return 0;
    }

    long width = strtol(arg, NULL, 10);
    if (width < 2 || width >= NEO4J_RENDER_MAX_WIDTH)
    {
        fprintf(state->err, "Width value (%ld) out of range [1,%d)\n",
                width, NEO4J_RENDER_MAX_WIDTH);
        return -1;
    }

    state->width = (unsigned int)width;
    return 0;
}


int quit(shell_state_t *state, const char *args)
{
    return 1;
}
