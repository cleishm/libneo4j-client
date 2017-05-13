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
#include "evaluate.h"
#include "options.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>


struct shell_command
{
    const char *name;
    int (*action)(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
};

static int eval_begin(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_commit(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_connect(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_disconnect(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_export(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_params(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_param(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_help(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_format(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_output(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_quit(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_reset(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_rollback(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_set(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_unset(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_source(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_status(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_schema(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_unexport(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);
static int eval_width(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos);

static struct shell_command shell_commands[] =
    { { "begin", eval_begin },
      { "commit", eval_commit },
      { "connect", eval_connect },
      { "disconnect", eval_disconnect },
      { "exit", eval_quit },
      { "param", eval_param },
      { "params", eval_params },
      { "export", eval_export },
      { "help", eval_help },
      { "format", eval_format },
      { "output", eval_output },
      { "quit", eval_quit },
      { "reset", eval_reset },
      { "rollback", eval_rollback },
      { "set", eval_set },
      { "unset", eval_unset },
      { "source", eval_source },
      { "status", eval_status },
      { "schema", eval_schema },
      { "unexport", eval_unexport },
      { "width", eval_width },
      { NULL, NULL } };


int run_command(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
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
            return shell_commands[i].action(state, command, pos);
        }
    }

    print_error(state, pos,
            "Unknown command '%s' (for usage, enter `:help`)", name);
    return -1;
}


int eval_begin(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    if (cypher_ast_command_narguments(command) != 0)
    {
        print_error(state, pos, ":begin does not take any arguments");
        return -1;
    }

    bool echo = state->echo;
    state->echo = false;
    unsigned int nexports = state->nexports;
    state->nexports = 0;
    int err = evaluate_statement(state, "begin", 5, pos);
    state->echo = echo;
    state->nexports = nexports;
    return err;
}


int eval_commit(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    if (cypher_ast_command_narguments(command) != 0)
    {
        print_error(state, pos, ":commit does not take any arguments");
        return -1;
    }

    bool echo = state->echo;
    state->echo = false;
    unsigned int nexports = state->nexports;
    state->nexports = 0;
    int err = evaluate_statement(state, "commit", 6, pos);
    state->echo = echo;
    state->nexports = nexports;
    return err;
}


int eval_connect(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    if (cypher_ast_command_narguments(command) == 0)
    {
        print_error(state, pos,
                ":connect requires a URL or a host and port to connect to");
        return -1;
    }
    if (cypher_ast_command_narguments(command) > 2)
    {
        print_error(state, pos, ":connect requires two arguments at most");
        return -1;
    }

    const cypher_astnode_t *arg = cypher_ast_command_get_argument(command, 0);
    assert(cypher_astnode_instanceof(arg, CYPHER_AST_STRING));
    const char *connect_string = cypher_ast_string_get_value(arg);

    const cypher_astnode_t *arg2 = cypher_ast_command_get_argument(command, 1);
    assert(arg2 == NULL || cypher_astnode_instanceof(arg2, CYPHER_AST_STRING));
    const char *port_string = (arg2 != NULL)?
            cypher_ast_string_get_value(arg2) : NULL;

    return db_connect(state, pos, connect_string, port_string);
}


int eval_disconnect(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    if (cypher_ast_command_narguments(command) != 0)
    {
        print_error(state, pos, ":disconnect does not take any arguments");
        return -1;
    }

    return db_disconnect(state, pos);
}


int eval_export(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
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
        if (eq == NULL)
        {
            free(export);
            continue;
        }
        size_t elen = eq - export;
        for (; elen > 0 && isspace(export[elen-1]); --elen)
            ;
        neo4j_value_t name = neo4j_ustring(export, elen);
        neo4j_value_t value = neo4j_string(eq + 1);
        if (shell_state_add_export(state, name, value, export))
        {
            free(export);
            return -1;
        }
    }
    return 0;
}


int eval_unexport(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    if (cypher_ast_command_narguments(command) == 0)
    {
        print_error(state, pos, ":unexport requires parameter name(s) "
                "to stop exporting");
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


int eval_params(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    if (cypher_ast_command_narguments(command) != 0)
    {
        print_error(state, pos, ":params does not take any arguments");
        return -1;
    }

    return eval_export(state, command, pos);
}


int eval_param(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    if (cypher_ast_command_narguments(command) != 2)
    {
        print_error(state, pos, ":param requires a parameter name and value");
        return -1;
    }

    const cypher_astnode_t *name = cypher_ast_command_get_argument(command, 0);
    const cypher_astnode_t *value = cypher_ast_command_get_argument(command, 1);
    assert(name != NULL && value != NULL);
    assert(cypher_astnode_instanceof(name, CYPHER_AST_STRING));
    assert(cypher_astnode_instanceof(value, CYPHER_AST_STRING));

    const char *namestr = cypher_ast_string_get_value(name);
    for (; isspace(*namestr); ++namestr)
        ;
    size_t namelen = strlen(namestr);
    const char *valuestr = cypher_ast_string_get_value(value);
    for (; isspace(*valuestr); ++valuestr)
        ;
    size_t valuelen = strlen(valuestr);

    char *storage = malloc(namelen + valuelen);
    if (storage == NULL)
    {
        return -1;
    }
    memcpy(storage, namestr, namelen);
    memcpy(storage + namelen, valuestr, valuelen);

    if (shell_state_add_export(state,
            neo4j_ustring(storage, namelen),
            neo4j_ustring(storage + namelen, valuelen), storage))
    {
        free(storage);
        return -1;
    }
    return 0;
}


int eval_reset(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    if (cypher_ast_command_narguments(command) != 0)
    {
        print_error(state, pos, ":reset does not take any arguments");
        return -1;
    }

    if (state->connection == NULL)
    {
        print_error(state, pos, "Not connected");
        return -1;
    }
    neo4j_reset(state->connection);
    return 0;
}


int eval_rollback(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    if (cypher_ast_command_narguments(command) != 0)
    {
        print_error(state, pos, ":rollback does not take any arguments");
        return -1;
    }

    bool echo = state->echo;
    state->echo = false;
    unsigned int nexports = state->nexports;
    state->nexports = 0;
    int err = evaluate_statement(state, "rollback", 8, pos);
    state->echo = echo;
    state->nexports = nexports;
    return err;
}


int eval_help(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    if (cypher_ast_command_narguments(command) != 0)
    {
        print_error(state, pos, ":help does not take any arguments");
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
":connect host [port]   Connect to the specified host (and optional port)\n"
":disconnect            Disconnect the client from the server\n"
":export                Display currently exported parameters\n"
":export name=val ...   Export parameters for queries\n"
":unexport name ...     Unexport parameters for queries\n"
":reset                 Reset the session with the server\n"
":set                   Display current option values\n"
":set option=value ...  Set shell options\n"
":unset option ...      Unset shell options\n"
":source file           Evaluate statements from the specified input file\n"
":status                Show the client connection status\n"
":schema                Show database schema indexes and constraints\n"
":help                  Show usage information\n"
":format (table|csv)    Set the output format\n"
":width (<n>|auto)      Set the number of columns in the table output\n"
"\n"
"For more information, see the neo4j-client(1) manpage.\n");
    fflush(state->out);
    return 0;
}


int eval_format(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    const cypher_astnode_t *arg = cypher_ast_command_get_argument(command, 0);
    if (arg == NULL)
    {
        print_error(state, pos, ":format requires a rendering format "
                "(table or csv)");
        return -1;
    }

    assert(cypher_astnode_instanceof(arg, CYPHER_AST_STRING));
    const char *value = cypher_ast_string_get_value(arg);
    return set_format(state, value);
}


int eval_output(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    print_warning(state, pos,
            "`:output` is deprecated. "
            "Use `:format` (or `:set format=value`) instead.");
    return eval_format(state, command, pos);
}


int eval_set(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
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


int eval_source(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    const cypher_astnode_t *arg = cypher_ast_command_get_argument(command, 0);
    if (arg == NULL)
    {
        print_error(state, pos, ":source requires a filename");
        return -1;
    }

    return source(state, cypher_ast_string_get_value(arg));
}


int eval_unset(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    if (cypher_ast_command_narguments(command) == 0)
    {
        print_error(state, pos, ":unset requires at least one option name");
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


int eval_status(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    if (cypher_ast_command_narguments(command) != 0)
    {
        print_error(state, pos, ":status does not take any arguments");
        return -1;
    }
    display_status(state->out, state);
    return 0;
}


int eval_schema(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    if (cypher_ast_command_narguments(command) != 0)
    {
        print_error(state, pos, ":schema does not take any arguments");
        return -1;
    }

    return display_schema(state, pos);
}


int eval_width(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    const cypher_astnode_t *arg = cypher_ast_command_get_argument(command, 0);
    if (arg == NULL)
    {
        print_error(state, pos, ":width requires an integer value, or 'auto'");
        return -1;
    }

    assert(cypher_astnode_instanceof(arg, CYPHER_AST_STRING));
    const char *value = cypher_ast_string_get_value(arg);
    return set_width(state, value);
}


int eval_quit(shell_state_t *state, const cypher_astnode_t *command,
        struct cypher_input_position pos)
{
    if (cypher_ast_command_narguments(command) != 0)
    {
        print_error(state, pos, ":quit does not take any arguments");
        return -1;
    }

    return 1;
}
