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
#include "batch.h"
#include "connect.h"
#include "options.h"
#include <assert.h>
#include <ctype.h>


struct shell_command
{
    const char *name;
    int (*action)(shell_state_t *state, const cypher_astnode_t *command);
};

static int eval_connect(shell_state_t *state, const cypher_astnode_t *command);
static int eval_disconnect(shell_state_t *state, const cypher_astnode_t *command);
static int eval_export(shell_state_t *state, const cypher_astnode_t *command);
static int eval_help(shell_state_t *state, const cypher_astnode_t *command);
static int eval_format(shell_state_t *state, const cypher_astnode_t *command);
static int eval_output(shell_state_t *state, const cypher_astnode_t *command);
static int eval_quit(shell_state_t *state, const cypher_astnode_t *command);
static int eval_reset(shell_state_t *state, const cypher_astnode_t *command);
static int eval_set(shell_state_t *state, const cypher_astnode_t *command);
static int eval_unset(shell_state_t *state, const cypher_astnode_t *command);
static int eval_source(shell_state_t *state, const cypher_astnode_t *command);
static int eval_status(shell_state_t *state, const cypher_astnode_t *command);
static int eval_unexport(shell_state_t *state, const cypher_astnode_t *command);
static int eval_width(shell_state_t *state, const cypher_astnode_t *command);

static struct shell_command shell_commands[] =
    { { "connect", eval_connect },
      { "disconnect", eval_disconnect },
      { "exit", eval_quit },
      { "export", eval_export },
      { "help", eval_help },
      { "format", eval_format },
      { "output", eval_output },
      { "quit", eval_quit },
      { "reset", eval_reset },
      { "set", eval_set },
      { "unset", eval_unset },
      { "source", eval_source },
      { "status", eval_status },
      { "unexport", eval_unexport },
      { "width", eval_width },
      { NULL, NULL } };


int run_command(shell_state_t *state, const cypher_astnode_t *command)
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

    fprintf(state->err, "Unknown command '%s' (for usage, enter `:help`)\n",
            name);
    return -1;
}


int eval_connect(shell_state_t *state, const cypher_astnode_t *command)
{
    const cypher_astnode_t *arg = cypher_ast_command_get_argument(command, 0);

    if (arg == NULL)
    {
        fprintf(state->err,
                ":connect requires a URL or host:port to connect to\n");
        return -1;
    }
    if (cypher_ast_command_narguments(command) > 1)
    {
        fprintf(state->err, ":connect requires a single argument\n");
        return -1;
    }

    assert(cypher_astnode_instanceof(arg, CYPHER_AST_STRING));
    const char *connect_string = cypher_ast_string_get_value(arg);

    return db_connect(state, connect_string);
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


int eval_export(shell_state_t *state, const cypher_astnode_t *command)
{
    if (cypher_ast_command_narguments(command) == 0)
    {
        for (unsigned int i = 0; i < state->nexports; ++i)
        {
            fprintf(state->out, " %.*s=",
                neo4j_string_length(state->exports[i].key),
                neo4j_ustring_value(state->exports[i].key));
            neo4j_fprint(state->exports[i].value, state->out);
            fputc('\n', state->out);
        }
        return 0;
    }

    const cypher_astnode_t *arg;
    for (unsigned int i = 0;
        (arg = cypher_ast_command_get_argument(command, i)) != NULL; ++i)
    {
        assert(cypher_astnode_instanceof(arg, CYPHER_AST_STRING));
        const char *argvalue = cypher_ast_string_get_value(arg);
        for (; isspace(*argvalue); ++argvalue)
            ;
        char *export = strdup(argvalue);
        if (export == NULL)
        {
            return -1;
        }
        const char *eq = strchr(export, '=');
        size_t elen = eq - export;
        for (; elen > 0 && isspace(export[elen-1]); --elen)
            ;
        neo4j_value_t name = neo4j_ustring(export, elen);
        neo4j_value_t value = neo4j_string(eq + 1);
        if (shell_state_add_export(state, name, value, export))
        {
            return -1;
        }
    }
    return 0;
}


int eval_unexport(shell_state_t *state, const cypher_astnode_t *command)
{
    if (cypher_ast_command_narguments(command) == 0)
    {
        fprintf(state->err, ":unexport requires parameter name(s) "
                "to stop exporting\n");
        return -1;
    }

    const cypher_astnode_t *arg;
    for (unsigned int i = 0;
        (arg = cypher_ast_command_get_argument(command, i)) != NULL; ++i)
    {
        assert(cypher_astnode_instanceof(arg, CYPHER_AST_STRING));
        const char *argvalue = cypher_ast_string_get_value(arg);
        for (; isspace(*argvalue); ++argvalue)
            ;
        size_t len = strlen(argvalue);
        for (; isspace(argvalue[len-1]); --len)
            ;
        neo4j_value_t name = neo4j_ustring(argvalue, len);
        shell_state_unexport(state, name);
    }
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
"Enter commands or cypher statements at the prompt.\n"
"\n"
"Commands always begin with a colon (:) and conclude at the end of the line,\n"
"for example `:help`. Statements do not begin with a colon (:), may span\n"
"multiple lines, are terminated with a semi-colon (;) and will be sent to\n"
"the Neo4j server for evaluation.\n"
"\n"
"Available commands:\n"
":quit                  Exit the shell\n"
":connect '<url>'       Connect to the specified URL\n"
":connect host[:port]   Connect to the specified host (and optional port)\n"
":disconnect            Disconnect the client from the server\n"
":export                Display currently exported parameters\n"
":export name=val ...   Export parameters for queries\n"
":unexport name ...     Unexport parameters for queries\n"
":reset                 Reset the session with the server\n"
":set                   Display current option values\n"
":set option=value ...  Set shell options\n"
":unset option ...      Unset shell options\n"
":status                Show the client connection status\n"
":help                  Show usage information\n"
":format (table|csv)    Set the output format\n"
":width (<n>|auto)      Set the number of columns in the table output\n"
"\n"
"For more information, see the neo4j-client(1) manpage.\n");
    fflush(state->out);
    return 0;
}


int eval_format(shell_state_t *state, const cypher_astnode_t *command)
{
    const cypher_astnode_t *arg = cypher_ast_command_get_argument(command, 0);
    if (arg == NULL)
    {
        fprintf(state->err, ":format requires a rendering format "
                "(table or csv)\n");
        return -1;
    }

    assert(cypher_astnode_instanceof(arg, CYPHER_AST_STRING));
    const char *value = cypher_ast_string_get_value(arg);
    return set_format(state, value);
}


int eval_output(shell_state_t *state, const cypher_astnode_t *command)
{
    fprintf(state->err,
            "WARNING: `:output` is deprecated. "
            "Use `:format` (or `:set format=value`) instead.\n");
    return eval_format(state, command);
}


int eval_set(shell_state_t *state, const cypher_astnode_t *command)
{
    if (cypher_ast_command_narguments(command) == 0)
    {
        options_display(state, state->output);
        return 0;
    }

    const cypher_astnode_t *arg;
    for (unsigned int i = 0;
        (arg = cypher_ast_command_get_argument(command, i)) != NULL; ++i)
    {
        assert(cypher_astnode_instanceof(arg, CYPHER_AST_STRING));
        const char *str = cypher_ast_string_get_value(arg);
        const char *eq = strchr(str, '=');
        if (eq == NULL)
        {
            if (option_set(state, str, NULL))
            {
                return -1;
            }
        }
        else
        {
            char name[32];
            size_t varlen = eq - str;
            if (varlen > sizeof(name))
            {
                varlen = sizeof(name) - 1;
            }
            strncpy(name, str, varlen);
            name[varlen] = '\0';
            if (option_set(state, name, eq + 1))
            {
                return -1;
            }
        }
    }

    return 0;
}


int eval_source(shell_state_t *state, const cypher_astnode_t *command)
{
    const cypher_astnode_t *arg = cypher_ast_command_get_argument(command, 0);
    if (arg == NULL)
    {
        fprintf(state->err, ":source requires a filename\n");
        return -1;
    }

    return source(state, cypher_ast_string_get_value(arg));
}


int eval_unset(shell_state_t *state, const cypher_astnode_t *command)
{
    if (cypher_ast_command_narguments(command) == 0)
    {
        fprintf(state->err, ":unset requires at least one option name\n");
        return -1;
    }

    char name[32];
    const cypher_astnode_t *arg;
    for (unsigned int i = 0;
        (arg = cypher_ast_command_get_argument(command, i)) != NULL; ++i)
    {
        assert(cypher_astnode_instanceof(arg, CYPHER_AST_STRING));
        const char *value = cypher_ast_string_get_value(arg);
        for (; isspace(*value); ++value)
            ;
        size_t varlen = strlen(value);
        for (; isspace(value[varlen-1]); --varlen)
            ;
        strncpy(name, value, varlen);
        name[varlen] = '\0';

        if (option_unset(state, name))
        {
            return -1;
        }
    }
    return 0;
}


int eval_status(shell_state_t *state, const cypher_astnode_t *command)
{
    if (cypher_ast_command_narguments(command) != 0)
    {
        fprintf(state->err, ":status does not take any arguments\n");
        return -1;
    }
    display_status(state->out, state);
    return 0;
}


int eval_width(shell_state_t *state, const cypher_astnode_t *command)
{
    const cypher_astnode_t *arg = cypher_ast_command_get_argument(command, 0);
    if (arg == NULL)
    {
        fprintf(state->err, ":width requires an integer value, or 'auto'\n");
        return -1;
    }

    assert(cypher_astnode_instanceof(arg, CYPHER_AST_STRING));
    const char *value = cypher_ast_string_get_value(arg);
    return set_width(state, value);
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
