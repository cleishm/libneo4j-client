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
#include "authentication.h"
#include "batch.h"
#include "connect.h"
#include "render.h"
#include <cypher-parser.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>


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


static int option_set(shell_state_t *state, const char *name,
        const char *value);
static int option_unset(shell_state_t *state, const char *name);

static int set_echo(shell_state_t *state, const char *value);
static int unset_echo(shell_state_t *state);
static const char *get_echo(shell_state_t *state, char *buf, size_t n);

static int set_insecure(shell_state_t *state, const char *value);
static int unset_insecure(shell_state_t *state);
static const char *get_insecure(shell_state_t *state, char *buf, size_t n);

static int set_format(shell_state_t *state, const char *value);
static int set_output(shell_state_t *state, const char *value);
static const char *get_format(shell_state_t *state, char *buf, size_t n);

static int set_outfile(shell_state_t *state, const char *value);
static int unset_outfile(shell_state_t *state);
static const char *get_outfile(shell_state_t *state, char *buf, size_t n);

static int set_username(shell_state_t *state, const char *value);
static int unset_username(shell_state_t *state);
static const char *get_username(shell_state_t *state, char *buf, size_t n);

static int set_width(shell_state_t *state, const char *value);
static int unset_width(shell_state_t *state);
static const char * get_width(shell_state_t *state, char *buf, size_t n);

struct options
{
    const char *name;
    int (*set)(shell_state_t *state, const char *value);
    bool allow_null;
    int (*unset)(shell_state_t *state);
    const char *(*get)(shell_state_t *state, char *buf, size_t n);
    const char *description;
};

static struct options options[] =
    { { "echo", set_echo, true, unset_echo, get_echo,
          "echo non-interactive commands before rendering results" },
      { "insecure", set_insecure, true, unset_insecure, get_insecure,
          "do not attempt to establish secure connections" },
      { "format", set_format, false, NULL, get_format,
          "set the output format (`table` or `csv`)." },
      { "output", set_output, false, NULL, NULL, NULL },
      { "outfile", set_outfile, false, unset_outfile, get_outfile,
          "redirect output to a file" },
      { "username", set_username, false, unset_username, get_username,
          "the default username for connections" },
      { "width", set_width, false, unset_width, get_width,
          "the width to render tables (`auto` for term width)" },
      { NULL, false, NULL } };


static int not_connected_error(evaluation_continuation_t *self,
        shell_state_t *state);
static int run_failure(evaluation_continuation_t *self, shell_state_t *state);
static int render_result(evaluation_continuation_t *self, shell_state_t *state);


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
        char buf[64];
        for (unsigned int i = 0; options[i].name != NULL; ++i)
        {
            if (options[i].get != NULL)
            {
                const char *name = options[i].name;
                const char *val = options[i].get(state, buf, sizeof(buf));
                unsigned int end_offset = strlen(name) + strlen(val) + 3;
                fprintf(state->out, " %s=%s %*s// %s\n", name, val,
                        (end_offset < 20)? 20 - end_offset : 0, "",
                        options[i].description);
            }
        }
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


int option_set(shell_state_t *state, const char *name,
        const char *value)
{
    for (unsigned int i = 0; options[i].name != NULL; ++i)
    {
        if (strcmp(options[i].name, name) == 0)
        {
            if (value != NULL && *value == '\0')
            {
                value = NULL;
            }
            if (value == NULL && !options[i].allow_null)
            {
                fprintf(state->err, "Option '%s' requires a value\n",
                        name);
                return -1;
            }
            return options[i].set(state, value);
        }
    }

    fprintf(state->err, "Unknown option '%s'\n", name);
    return -1;
}


int option_unset(shell_state_t *state, const char *name)
{
    for (unsigned int i = 0; options[i].name != NULL; ++i)
    {
        if (strcmp(options[i].name, name) == 0)
        {
            if (options[i].unset != NULL)
            {
                return options[i].unset(state);
            }
            else
            {
                fprintf(state->err, "Cannot unset option '%s'\n", name);
                return -1;
            }
        }
    }

    fprintf(state->err, "Unknown option '%s'\n", name);
    return -1;
}


int set_echo(shell_state_t *state, const char *value)
{
    if (value == NULL || strcmp(value, "on") == 0)
    {
        state->batch_echo = true;
    }
    else if (strcmp(value, "off") == 0)
    {
        state->batch_echo = false;
    }
    else
    {
        fprintf(state->err, "Must set echo to 'on' or 'off'\n");
        return -1;
    }
    return 0;
}


int unset_echo(shell_state_t *state)
{
    state->batch_echo = false;
    return 0;
}


const char *get_echo(shell_state_t *state, char *buf, size_t n)
{
    return (state->batch_echo)? "on" : "off";
}


int set_insecure(shell_state_t *state, const char *value)
{
    if (value == NULL || strcmp(value, "yes") == 0)
    {
        state->connect_flags |= NEO4J_INSECURE;
    }
    else if (strcmp(value, "no") == 0)
    {
        state->connect_flags &= ~NEO4J_INSECURE;
    }
    else
    {
        fprintf(state->err, "Must set insecure to 'yes' or 'no'\n");
        return -1;
    }
    return 0;
}


int unset_insecure(shell_state_t *state)
{
    state->connect_flags &= ~NEO4J_INSECURE;
    return 0;
}


const char *get_insecure(shell_state_t *state, char *buf, size_t n)
{
    return (state->connect_flags & NEO4J_INSECURE)? "yes" : "no";
}


int set_format(shell_state_t *state, const char *value)
{
    renderer_t renderer = find_renderer(value);
    if (renderer == NULL)
    {
        fprintf(state->err, "Unknown output format '%s'\n", value);
        return -1;
    }
    state->render = renderer;
    return 0;
}


int set_output(shell_state_t *state, const char *value)
{
    fprintf(state->err,
            "WARNING: `:set output=value` is deprecated. "
            "Use `:set format=value` instead.\n");
    return set_format(state, value);
}


const char *get_format(shell_state_t *state, char *buf, size_t n)
{
    const char *name = renderer_name(state->render);
    return (name != NULL)? name : "unknown";
}


int set_outfile(shell_state_t *state, const char *value)
{
    return redirect_output(state, value);
}


int unset_outfile(shell_state_t *state)
{
    return set_outfile(state, NULL);
}


const char *get_outfile(shell_state_t *state, char *buf, size_t n)
{
    if (state->outfile == NULL)
    {
        return "";
    }
    snprintf(buf, n, "\"%s\"", state->outfile);
    return buf;
}


int set_username(shell_state_t *state, const char *value)
{
    return neo4j_config_set_username(state->config,
            (*value != '\0')? value : NULL);
}


int unset_username(shell_state_t *state)
{
    return neo4j_config_set_username(state->config, NULL);
}


const char *get_username(shell_state_t *state, char *buf, size_t n)
{
    const char *username = neo4j_config_get_username(state->config);
    if (username == NULL)
    {
        return "";
    }
    snprintf(buf, n, "\"%s\"", username);
    return buf;
}


int set_width(shell_state_t *state, const char *value)
{
    if (strcmp(value, "auto") == 0)
    {
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


int unset_width(shell_state_t *state)
{
    state->width = 0;
    return 0;
}


const char *get_width(shell_state_t *state, char *buf, size_t n)
{
    if (state->width == 0)
    {
        return "auto";
    }
    snprintf(buf, n, "%d", state->width);
    return buf;
}


evaluation_continuation_t evaluate_statement(shell_state_t *state,
        const char *statement, struct cypher_input_position pos)
{
    if (state->session == NULL)
    {
        evaluation_continuation_t continuation =
                { .complete = not_connected_error, .pos = pos, .data = NULL };
        return continuation;
    }

    neo4j_result_stream_t *results = neo4j_run(state->session,
            statement, shell_state_get_exports(state));
    if (results == NULL)
    {
        evaluation_continuation_t continuation =
                { .complete = run_failure,
                  .pos = pos,
                  .data = (void *)(long)errno };
        return continuation;
    }

    evaluation_continuation_t continuation =
            { .complete = render_result, .pos = pos, .data = results };
    return continuation;
}


int not_connected_error(evaluation_continuation_t *self, shell_state_t *state)
{
    fprintf(state->err, "Not connected (try `:connect <URL>`, or `:help`)\n");
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
        if (errno != NEO4J_STATEMENT_EVALUATION_FAILED)
        {
            neo4j_perror(state->err, errno, "unexpected error");
            goto cleanup;
        }

        const struct neo4j_failure_details *details =
                neo4j_failure_details(results);

        struct cypher_input_position pos = self->pos;
        bool is_indented = (details->line == 1 && pos.column > 1);
        pos.offset += details->offset;
        pos.column = (details->line == 1)?
                pos.column + details->column - 1 : details->column;
        pos.line += (details->line - 1);

        fprintf(state->err, "%s%s:%u:%u:%s %serror:%s %s%s%s\n",
                state->error_colorize->pos[0], state->infile, pos.line,
                pos.column, state->error_colorize->pos[1],
                state->error_colorize->def[0], state->error_colorize->def[1],
                state->error_colorize->msg[0], details->description,
                state->error_colorize->msg[1]);
        if (details->context != NULL)
        {
            unsigned int offset = is_indented?
                    details->context_offset + 3 : details->context_offset;
            fprintf(state->err, "%s%s%s\n%*s^%s\n",
                    state->error_colorize->ctx[0],
                    is_indented? "..." : "", details->context, offset, "",
                    state->error_colorize->ctx[1]);
        }
        goto cleanup;
    }

    if (state->interactive && state->outfile != NULL)
    {
        fprintf(state->out, "<Output redirected to '%s'>\n", state->outfile);
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
