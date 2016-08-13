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


static int not_connected_error(evaluation_continuation_t *self,
        shell_state_t *state);
static int run_failure(evaluation_continuation_t *self, shell_state_t *state);
static int render_result(evaluation_continuation_t *self, shell_state_t *state);
static void echo(shell_state_t *state, const char *s, size_t n,
        const char *postfix);


int evaluate_command(shell_state_t *state, const char *command, size_t n)
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
    int r = run_command(state, directive);
    cypher_parse_result_free(result);
    return r;
}


struct evaluation_continuation
{
    int (*complete)(evaluation_continuation_t *self, shell_state_t *state);
    struct cypher_input_position pos;
    neo4j_result_stream_t *results;
    int err;
    char statement[];
};


evaluation_continuation_t *evaluate_statement(shell_state_t *state,
        const char *statement, size_t n, struct cypher_input_position pos)
{
    evaluation_continuation_t *continuation = calloc(1,
            sizeof(evaluation_continuation_t) + n + 1);
    if (continuation == NULL)
    {
        return NULL;
    }

    memcpy(continuation->statement, statement, n);
    continuation->pos = pos;

    if (state->session == NULL)
    {
        continuation->complete = not_connected_error;
        return continuation;
    }

    continuation->results = neo4j_run(state->session,
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
        neo4j_perror(state->err, errno, "unexpected error");
        res = -1;
    }
    free(continuation);
    return res;
}


int not_connected_error(evaluation_continuation_t *self, shell_state_t *state)
{
    fprintf(state->err, "Not connected (try `:connect <URL>`, or `:help`)\n");
    return -1;
}


int run_failure(evaluation_continuation_t *self, shell_state_t *state)
{
    neo4j_perror(state->err, self->err, "failed to run statement");
    return -1;
}


int render_result(evaluation_continuation_t *self, shell_state_t *state)
{
    int result = -1;
    if (state->render(state, self->results))
    {
        if (errno == NEO4J_SESSION_RESET)
        {
            fprintf(state->err, "interrupted\n");
            goto cleanup;
        }
        else if (errno != NEO4J_STATEMENT_EVALUATION_FAILED)
        {
            neo4j_perror(state->err, errno, "unexpected error");
            goto cleanup;
        }

        const struct neo4j_failure_details *details =
                neo4j_failure_details(self->results);

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

    if (render_update_counts(state, self->results))
    {
        goto cleanup;
    }

    struct neo4j_statement_plan *plan = neo4j_statement_plan(self->results);
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
    if (neo4j_close_results(self->results) && result == 0)
    {
        neo4j_perror(state->err, errno, "failed to close results");
        return -1;
    }
    return result;
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
