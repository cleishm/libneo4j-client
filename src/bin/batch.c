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
#include "batch.h"
#include "evaluate.h"
#include <neo4j-client.h>
#include <cypher-parser.h>
#include <assert.h>
#include <errno.h>


struct evaluation
{
    char *statement;
    char *buffer;
    size_t buffer_capacity;
    evaluation_continuation_t continuation;
};


typedef struct evaluation_queue
{
    unsigned int next;
    unsigned int depth;
    unsigned int capacity;
    struct evaluation directives[];
} evaluation_queue_t;


struct parse_callback_data
{
    shell_state_t *state;
    evaluation_queue_t *queue;
};


static int parse_callback(void *data, const char *s, size_t n,
        struct cypher_input_range range, bool eof);
static int evaluate(shell_state_t *state, evaluation_queue_t *queue,
        const char *directive, size_t n, struct cypher_input_position pos);
static int finalize(shell_state_t *state, evaluation_queue_t *queue,
        unsigned int n);
static void echo(shell_state_t *state, const char * restrict format, ...);


int source(shell_state_t *state, const char *filename)
{
    if (state->source_depth >= state->source_max_depth)
    {
        fprintf(state->err, "Too many nested calls to `:source`\n");
        return -1;
    }

    FILE *stream = fopen(filename, "r");
    if (stream == NULL)
    {
        fprintf(state->err, "Unable to read file '%s': %s\n",
                filename, strerror(errno));
        return -1;
    }
    bool interactive = state->interactive;
    state->interactive = false;
    const char *prev_infile = state->infile;
    state->infile = filename;
    ++(state->source_depth);
    int result = batch(state, stream);
    fclose(stream);
    --(state->source_depth);
    state->infile = prev_infile;
    state->interactive = interactive;
    if (result == 0 && interactive && state->outfile != NULL)
    {
        fprintf(state->out, "<Output redirected to '%s'>\n", state->outfile);
    }
    return result;
}


int batch(shell_state_t *state, FILE *stream)
{
    evaluation_queue_t *queue = calloc(1, sizeof(evaluation_queue_t) +
            (state->pipeline_max * sizeof(struct evaluation)));
    if (queue == NULL)
    {
        neo4j_perror(state->err, errno, "unexpected error");
        return -1;
    }
    queue->capacity = state->pipeline_max;

    int result = -1;
    struct parse_callback_data cbdata = { .state = state, .queue = queue };
    int err = cypher_quick_fparse(stream, parse_callback, &cbdata, 0);
    if (err)
    {
        if (err != -2)
        {
            neo4j_perror(state->err, errno, "unexpected error");
        }
        goto cleanup;
    }

    if (finalize(state, queue, queue->depth))
    {
        goto cleanup;
    }

    result = 0;

    int errsv;
cleanup:
    errsv = errno;
    for (unsigned int i = queue->capacity; i-- > 0; )
    {
        free(queue->directives[i].buffer);
    }
    free(queue);
    errno = errsv;
    return result;
}


int parse_callback(void *data, const char *s, size_t n,
        struct cypher_input_range range, bool eof)
{
    struct parse_callback_data *cbdata = (struct parse_callback_data *)data;
    if (evaluate(cbdata->state, cbdata->queue, s, n, range.start))
    {
        return -2;
    }
    return 0;
}


int evaluate(shell_state_t *state, evaluation_queue_t *queue,
        const char *directive, size_t n, struct cypher_input_position pos)
{
    if (is_command(directive))
    {
        // drain queue before running commands
        int err = finalize(state, queue, queue->depth);
        if (err)
        {
            return err;
        }
        const char *command = temp_copy(state, directive, n);
        if (command == NULL)
        {
            neo4j_perror(state->err, errno, "unexpected error");
            return -1;
        }
        echo(state, "%s", command);
        return evaluate_command_string(state, command);
    }

    trim_statement(&directive, &n, &pos);
    if (n == 0)
    {
        return 0;
    }

    assert(queue->depth <= queue->capacity);
    if ((queue->depth >= queue->capacity) && finalize(state, queue, 1))
    {
        neo4j_perror(state->err, errno, "unexpected error");
        return -1;
    }
    assert (queue->depth < queue->capacity);

    unsigned int i = queue->next + queue->depth;
    if (i >= queue->capacity)
    {
        i -= queue->capacity;
    }
    ++(queue->depth);

    struct evaluation *e = &(queue->directives[i]);
    e->statement = strncpy_alloc(&(e->buffer), &(e->buffer_capacity),
            directive, n);
    if (e->statement == NULL)
    {
        neo4j_perror(state->err, errno, "unexpected error");
        return -1;
    }
    e->continuation = evaluate_statement(state, e->statement, pos);
    return 0;
}


int finalize(shell_state_t *state, evaluation_queue_t *queue, unsigned int n)
{
    assert(n <= queue->depth);
    while (n-- > 0)
    {
        assert(queue->next < queue->capacity);
        struct evaluation *e = &(queue->directives[queue->next]);
        if (++(queue->next) >= queue->capacity)
        {
            queue->next = 0;
        }
        --(queue->depth);
        echo(state, "%s;\n", e->statement);
        evaluation_continuation_t *continuation = &(e->continuation);
        if (continuation->complete(continuation, state))
        {
            return -1;
        }
    }
    return 0;
}


void echo(shell_state_t *state, const char * restrict format, ...)
{
    if (!state->batch_echo)
    {
        return;
    }
    for (unsigned int i = state->source_depth + 1; i > 0; --i)
    {
        fputc('+', state->out);
    }
    va_list ap;
    va_start(ap, format);
    vfprintf(state->out, format, ap);
    va_end(ap);
}
