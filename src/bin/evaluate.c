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
#include "commands.h"
#include "options.h"
#include "render.h"
#include <assert.h>
#include <errno.h>
#include <sys/time.h>


static int not_connected_error(evaluation_continuation_t *self,
        shell_state_t *state);
static int run_failure(evaluation_continuation_t *self, shell_state_t *state);
static int render_result(evaluation_continuation_t *self, shell_state_t *state);
static void render_evaluation_failure(evaluation_continuation_t *self,
        shell_state_t *state);
static void echo(shell_state_t *state, const char *s, size_t n,
        const char *postfix);


int evaluate_command(shell_state_t *state, const char *command, size_t n,
        struct cypher_input_position pos)
{
    echo(state, command, n, "");

    cypher_parse_result_t *result = cypher_uparse(command, n, NULL, NULL,
            CYPHER_PARSE_SINGLE);
    if (result == NULL)
    {
        return -1;
    }

    assert(cypher_parse_result_ndirectives(result) == 1);
    const cypher_astnode_t *directive = cypher_parse_result_get_directive(result, 0);
    int r = run_command(state, directive, pos);
    cypher_parse_result_free(result);
    return r;
}


struct evaluation_continuation
{
    int (*complete)(evaluation_continuation_t *self, shell_state_t *state);
    struct cypher_input_position pos;
    struct timeval start_time;
    neo4j_result_stream_t *results;
    int err;
    char statement[];
};


int evaluate_statement(shell_state_t *state, const char *statement, size_t n,
        struct cypher_input_position pos)
{
    evaluation_continuation_t *continuation =
            prepare_statement(state, statement, n, pos);
    if (continuation == NULL)
    {
        return -1;
    }
    return complete_evaluation(continuation, state);
}


evaluation_continuation_t *prepare_statement(shell_state_t *state,
        const char *statement, size_t n, struct cypher_input_position pos)
{
    evaluation_continuation_t *continuation = calloc(1,
            sizeof(evaluation_continuation_t) + n + 1);
    if (continuation == NULL)
    {
        print_error_errno(state, pos, errno, "calloc");
        return NULL;
    }

    if (state->show_timing && gettimeofday(&(continuation->start_time), NULL))
    {
        free(continuation);
        print_error_errno(state, pos, errno, "gettimeofday");
        return NULL;
    }

    memcpy(continuation->statement, statement, n);
    continuation->pos = pos;

    if (state->connection == NULL)
    {
        continuation->complete = not_connected_error;
        return continuation;
    }

    continuation->results = neo4j_run(state->connection,
            continuation->statement, shell_state_get_exports(state));
    if (continuation->results == NULL)
    {
        continuation->complete = run_failure;
        continuation->err = errno;
        return continuation;
    }

    continuation->complete = render_result;
    return continuation;
}


int complete_evaluation(evaluation_continuation_t *continuation,
        shell_state_t *state)
{
    echo(state, continuation->statement, SIZE_MAX, ";");
    int res = continuation->complete(continuation, state);
    free(continuation);
    return res;
}


int abort_evaluation(evaluation_continuation_t *continuation,
        shell_state_t *state)
{
    int res = 0;
    if (continuation->results != NULL &&
            neo4j_close_results(continuation->results))
    {
        print_error_errno(state, continuation->pos, errno,
                "Failed to close results");
        res = -1;
    }
    free(continuation);
    return res;
}


int not_connected_error(evaluation_continuation_t *self, shell_state_t *state)
{
    print_error(state, self->pos,
            "Not connected (try `:connect <URL>`, or `:help`)");
    return -1;
}


int run_failure(evaluation_continuation_t *self, shell_state_t *state)
{
    print_error_errno(state, self->pos, self->err, "Failed to run statement");
    return -1;
}


int render_result(evaluation_continuation_t *self, shell_state_t *state)
{
    int result = -1;
    if (state->render(state, self->pos, self->results))
    {
        if (errno == NEO4J_SESSION_RESET)
        {
            fprintf(state->err, "Interrupted"
                    " (any open transaction has been rolled back)\n");
            goto cleanup;
        }
        else if (errno == NEO4J_STATEMENT_EVALUATION_FAILED)
        {
            render_evaluation_failure(self, state);
            goto cleanup;
        }

        print_error_errno(state, self->pos, errno, "Rendering results");
        goto cleanup;
    }

    if (state->interactive && state->outfile != NULL)
    {
        fprintf(state->out, "<Output redirected to '%s'>\n", state->outfile);
    }

    if (render_update_counts(state, self->pos, self->results))
    {
        goto cleanup;
    }

    struct neo4j_statement_plan *plan = neo4j_statement_plan(self->results);
    if (plan != NULL)
    {
        int err = render_plan_table(state, self->pos, plan);
        neo4j_statement_plan_release(plan);
        if (err)
        {
            goto cleanup;
        }
    }
    else if (errno == NEO4J_STATEMENT_EVALUATION_FAILED)
    {
        render_evaluation_failure(self, state);
        goto cleanup;
    }
    else if (errno != NEO4J_NO_PLAN_AVAILABLE)
    {
        print_error_errno(state, self->pos, errno, "Rendering plan");
        goto cleanup;
    }

    if (state->show_timing)
    {
        struct timeval end_time;
        if (gettimeofday(&end_time, NULL))
        {
            print_error_errno(state, self->pos, errno, "gettimeofday");
            goto cleanup;
        }
        unsigned long long total =
            (end_time.tv_sec - self->start_time.tv_sec) * 1000 +
            (end_time.tv_usec - self->start_time.tv_usec) / 1000;
        if (render_timing(state, self->pos, self->results, total))
        {
            goto cleanup;
        }
    }

    result = 0;

cleanup:
    if (neo4j_close_results(self->results) && result == 0)
    {
        print_error_errno(state, self->pos, errno, "Failed to close results");
        return -1;
    }
    return result;
}


void render_evaluation_failure(evaluation_continuation_t *self,
        shell_state_t *state)
{
    const struct neo4j_failure_details *details =
            neo4j_failure_details(self->results);

    struct cypher_input_position pos = self->pos;
    bool is_indented = (details->line == 1 && pos.column > 1);
    pos.offset += details->offset;
    pos.column = (details->line == 1)?
            pos.column + details->column - 1 : details->column;
    pos.line += (details->line - 1);

    print_error(state, pos, details->description);
    if (details->context != NULL)
    {
        unsigned int offset = is_indented?
                details->context_offset + 3 : details->context_offset;
        fprintf(state->err, "%s%s%s%s\n%*s%s%s%s\n",
                state->colorize->error->ctx[0],
                is_indented? "..." : "", details->context,
                state->colorize->error->ctx[1],
                offset, "",
                state->colorize->error->ptr[0],
                neo4j_config_get_render_ascii(state->config)? "^" : u8"\u25B2",
                state->colorize->error->ptr[1]);
    }
}


void echo(shell_state_t *state, const char *s, size_t n, const char *postfix)
{
    if (!state->echo)
    {
        return;
    }

    char indent_with = '+';
    while (n > 0 && *s != '\0')
    {
        for (unsigned int i = state->source_depth; i > 0; --i)
        {
            fputc(indent_with, state->output);
        }

        for (; n > 0 && *s != '\0'; ++s, --n)
        {
            fputc(*s, state->output);
            if (*s == '\n')
            {
                ++s;
                --n;
                break;
            }
        }

        indent_with = ' ';
    }

    fputs(postfix, state->output);
    fputc('\n', state->output);
}


int display_schema(shell_state_t* state, struct cypher_input_position pos)
{
    if (state->connection == NULL)
    {
        print_error(state, pos, "Not connected\n");
        return -1;
    }

    neo4j_result_stream_t *indexes_result = NULL;
    neo4j_result_stream_t *constraints_result = NULL;

    indexes_result = neo4j_run(state->connection,
            "CALL db.indexes()", neo4j_null);
    if (indexes_result == NULL)
    {
        print_error_errno(state, pos, errno, "db.indexes() failed");
        goto failure;
    }
    constraints_result = neo4j_run(state->connection,
            "CALL db.constraints()", neo4j_null);
    if (constraints_result == NULL)
    {
        print_error_errno(state, pos, errno, "db.constraints() failed");
        goto failure;
    }

    fprintf(state->output, "Indexes\n");
    neo4j_result_t *result;
    while ((result = neo4j_fetch_next(indexes_result)) != NULL)
    {
        neo4j_value_t index_desc = neo4j_result_field(result, 0);
        neo4j_value_t index_state = neo4j_result_field(result, 1);
        if (!neo4j_instanceof(index_desc, NEO4J_STRING) ||
            !neo4j_instanceof(index_state, NEO4J_STRING))
        {
            print_error(state, pos, "Invalid result from db.indexes()\n");
            goto failure;
        }
        fprintf(state->output, "   %.*s %.*s\n",
                neo4j_string_length(index_desc),
                neo4j_ustring_value(index_desc),
                neo4j_string_length(index_state),
                neo4j_ustring_value(index_state));
    }

    int err;
    if ((err = neo4j_check_failure(indexes_result)) != 0)
    {
        print_error_errno(state, pos, err, "db.indexes() failed");
        goto failure;
    }

    if (neo4j_close_results(indexes_result))
    {
        print_error_errno(state, pos, errno, "Unexpected error");
        goto failure;
    }

    fprintf(state->output, "\nConstraints\n");
    while ((result = neo4j_fetch_next(constraints_result)) != NULL)
    {
        neo4j_value_t desc = neo4j_result_field(result, 0);
        if (!neo4j_instanceof(desc, NEO4J_STRING))
        {
            print_error(state, pos, "Invalid result from db.constraints()\n");
            goto failure;
        }
        fprintf(state->output, "   %.*s\n", neo4j_string_length(desc),
                neo4j_ustring_value(desc));
    }

    if ((err = neo4j_check_failure(constraints_result)) != 0)
    {
        print_error_errno(state, pos, err, "db.constraints() failed");
        goto failure;
    }

    if (neo4j_close_results(constraints_result))
    {
        print_error_errno(state, pos, errno, "Failed to close results");
        goto failure;
    }

    return 0;

    int errsv;
failure:
    errsv = errno;
    neo4j_close_results(indexes_result);
    neo4j_close_results(constraints_result);
    errno = errsv;
    return -1;
}
