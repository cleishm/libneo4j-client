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
#include "evaluate.h"
#include "render.h"
#include <cypher-parser.h>
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
    int (*action)(shell_state_t *state, const cypher_astnode_t *command);
};


static int eval_connect(shell_state_t *state, const cypher_astnode_t *command);
static int eval_disconnect(shell_state_t *state, const cypher_astnode_t *command);
static int eval_help(shell_state_t *state, const cypher_astnode_t *command);
static int eval_output(shell_state_t *state, const cypher_astnode_t *command);
static int eval_quit(shell_state_t *state, const cypher_astnode_t *command);
static int eval_reset(shell_state_t *state, const cypher_astnode_t *command);
static int eval_width(shell_state_t *state, const cypher_astnode_t *command);

static int not_connected_error(evaluation_continuation_t *self,
        shell_state_t *state);
static int run_failure(evaluation_continuation_t *self, shell_state_t *state);
static int render_result(evaluation_continuation_t *self, shell_state_t *state);

static shell_command_t shell_commands[] =
    { { "connect", eval_connect },
      { "disconnect", eval_disconnect },
      { "exit", eval_quit },
      { "help", eval_help },
      { "output", eval_output },
      { "quit", eval_quit },
      { "reset", eval_reset },
      { "width", eval_width },
      { NULL, NULL } };


int evaluate_command_string(shell_state_t *state, const char *command)
{
    cypher_parse_result_t *result = cypher_parse(command, NULL, NULL,
            CYPHER_PARSE_SINGLE);
    if (result == NULL)
    {
        return -1;
    }

    assert(cypher_parse_result_ndirectives(result) == 1);
    const cypher_astnode_t *directive = cypher_parse_result_get_directive(result, 0);
    int r = evaluate_command(state, directive);
    cypher_parse_result_free(result);
    return r;
}


int evaluate_command(shell_state_t *state, const cypher_astnode_t *command)
{
    assert(cypher_astnode_instanceof(command, CYPHER_AST_COMMAND));
    const cypher_astnode_t *node = cypher_ast_command_get_name(command);
    assert(node != NULL);
    assert(cypher_astnode_instanceof(node, CYPHER_AST_STRING));
    const char *name = cypher_ast_string_get_value(node);

    for (unsigned int i = 0; shell_commands[i].name != NULL; ++i)
    {
        if (strcmp(shell_commands[i].name, name) == 0)
        {
            return shell_commands[i].action(state, command);
        }
    }

    fprintf(state->err, "Unknown command '%s'\n", name);
    return -1;
}


int eval_connect(shell_state_t *state, const cypher_astnode_t *command)
{
    const cypher_astnode_t *arg =
            cypher_ast_command_get_argument(command, 0);

    if (arg == NULL)
    {
        fprintf(state->err, ":connect requires a URI to connect to\n");
        return -1;
    }

    assert(cypher_astnode_instanceof(arg, CYPHER_AST_STRING));
    const char *uri_string = cypher_ast_string_get_value(arg);

    return db_connect(state, uri_string);
}


int db_connect(shell_state_t *state, const char *uri_string)
{
    if (state->session != NULL)
    {
        if (db_disconnect(state))
        {
            return -1;
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
        return -1;
    }

    neo4j_session_t *session = neo4j_new_session(connection);
    if (session == NULL)
    {
        char ebuf[512];
        fprintf(state->err, "connection to '%s' failed: %s\n", uri_string,
                neo4j_strerror(errno, ebuf, sizeof(ebuf)));
        neo4j_close(connection);
        return -1;
    }

    state->connection = connection;
    state->session = session;
    return 0;
}


int eval_disconnect(shell_state_t *state, const cypher_astnode_t *command)
{
    if (cypher_ast_command_narguments(command) != 0)
    {
        fprintf(state->err, ":disconnect does not take any arguments\n");
        return -1;
    }

    return db_disconnect(state);
}


int db_disconnect(shell_state_t *state)
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


int eval_reset(shell_state_t *state, const cypher_astnode_t *command)
{
    if (cypher_ast_command_narguments(command) != 0)
    {
        fprintf(state->err, ":reset does not take any arguments\n");
        return -1;
    }

    if (state->session == NULL)
    {
        fprintf(state->err, "ERROR: not connected\n");
        return -1;
    }
    neo4j_reset_session(state->session);
    return 0;
}


int eval_help(shell_state_t *state, const cypher_astnode_t *command)
{
    if (cypher_ast_command_narguments(command) != 0)
    {
        fprintf(state->err, ":help does not take any arguments\n");
        return -1;
    }

    fprintf(state->out,
":quit                  Exit the shell\n"
":connect '<url>'       Connect to the specified URL\n"
":disconnect            Disconnect the client from the server\n"
":reset                 Reset the session with the server\n"
":help                  Show usage information\n"
":output (table|csv)    Set the output format\n"
":width (<n>|auto)      Set the number of columns in the table output\n");
    fflush(state->out);
    return 0;
}


int eval_output(shell_state_t *state, const cypher_astnode_t *command)
{
    const cypher_astnode_t *arg =
            cypher_ast_command_get_argument(command, 0);
    if (arg == NULL)
    {
        fprintf(state->err, ":connect requires a rendering format "
                "(table or csv)\n");
        return -1;
    }

    assert(cypher_astnode_instanceof(arg, CYPHER_AST_STRING));
    const char *name = cypher_ast_string_get_value(arg);
    renderer_t renderer = find_renderer(name);
    if (renderer == NULL)
    {
        fprintf(state->err, "Unknown output format '%s'\n", name);
        return -1;
    }
    state->render = renderer;
    return 0;
}


int eval_width(shell_state_t *state, const cypher_astnode_t *command)
{
    const cypher_astnode_t *arg =
            cypher_ast_command_get_argument(command, 0);
    if (arg == NULL)
    {
        fprintf(state->err, ":width requires an integer value, or 'auto'\n");
        return -1;
    }

    assert(cypher_astnode_instanceof(arg, CYPHER_AST_STRING));
    const char *value = cypher_ast_string_get_value(arg);

    if (strcmp(value, "auto") == 0)
    {
        if (!isatty(fileno(state->out)))
        {
            fprintf(state->err, "Setting width to auto is only possible"
                    " when outputting to a tty\n");
            return -1;
        }
        state->width = 0;
        return 0;
    }

    long width = strtol(value, NULL, 10);
    if (width < 2 || width >= NEO4J_RENDER_MAX_WIDTH)
    {
        fprintf(state->err, "Width value (%ld) out of range [1,%d)\n",
                width, NEO4J_RENDER_MAX_WIDTH);
        return -1;
    }

    state->width = (unsigned int)width;
    return 0;
}


int eval_quit(shell_state_t *state, const cypher_astnode_t *command)
{
    if (cypher_ast_command_narguments(command) != 0)
    {
        fprintf(state->err, ":quit does not take any arguments\n");
        return -1;
    }

    return 1;
}


evaluation_continuation_t evaluate_statement(shell_state_t *state,
        const char *statement)
{
    if (state->session == NULL)
    {
        evaluation_continuation_t continuation =
            { .complete = not_connected_error, .data = NULL };
        return continuation;
    }

    neo4j_result_stream_t *results = neo4j_run(state->session,
            statement, neo4j_map(NULL, 0));
    if (results == NULL)
    {
        evaluation_continuation_t continuation =
            { .complete = run_failure, .data = (void *)(long)errno };
        return continuation;
    }

    evaluation_continuation_t continuation =
        { .complete = render_result, .data = results };
    return continuation;
}


int not_connected_error(evaluation_continuation_t *self, shell_state_t *state)
{
    fprintf(state->err, "ERROR: not connected\n");
    return -1;
}


int run_failure(evaluation_continuation_t *self, shell_state_t *state)
{
    int error = (int)(long)(self->data);
    neo4j_perror(state->err, error, "failed to run statement");
    return -1;
}


int render_result(evaluation_continuation_t *self, shell_state_t *state)
{
    neo4j_result_stream_t *results = self->data;
    int result = -1;
    if (state->render(state, results))
    {
        int error = errno;
        if (error == NEO4J_STATEMENT_EVALUATION_FAILED)
        {
            fprintf(state->err, "%s\n", neo4j_error_message(results));
        }
        else
        {
            neo4j_perror(state->err, errno, "unexpected error");
        }
        goto cleanup;
    }

    if (render_update_counts(state, results))
    {
        goto cleanup;
    }

    struct neo4j_statement_plan *plan = neo4j_statement_plan(results);
    if (plan != NULL)
    {
        int err = render_plan_table(state, plan);
        int errsv = errno;
        neo4j_statement_plan_release(plan);
        errno = errsv;
        if (err)
        {
            goto cleanup;
        }
    }
    else if (errno != NEO4J_NO_PLAN_AVAILABLE)
    {
        neo4j_perror(state->err, errno, "unexpected error");
        goto cleanup;
    }

    result = 0;

cleanup:
    if (neo4j_close_results(results) && result == 0)
    {
        neo4j_perror(state->err, errno, "failed to close results");
        return -1;
    }
    return result;
}
