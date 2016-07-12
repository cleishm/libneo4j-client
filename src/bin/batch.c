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


int batch(shell_state_t *state)
{
    evaluation_queue_t *queue = calloc(1, sizeof(evaluation_queue_t) +
            (state->pipeline_max * sizeof(struct evaluation)));
    if (queue == NULL)
    {
        return -1;
    }
    queue->capacity = state->pipeline_max;

    int result = -1;
    struct parse_callback_data cbdata = { .state = state, .queue = queue };
    if (cypher_quick_fparse(state->in, parse_callback, &cbdata, 0))
    {
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
    return evaluate(cbdata->state, cbdata->queue, s, n, range.start);
}


int evaluate(shell_state_t *state, evaluation_queue_t *queue,
        const char *directive, size_t n, struct cypher_input_position pos)
{
    if (is_command(directive))
    {
        // drain queue before running commands
        if (finalize(state, queue, queue->depth))
        {
            neo4j_perror(state->err, errno, "unexpected error");
            return -1;
        }
        const char *command = temp_copy(state, directive, n);
        if (command == NULL)
        {
            neo4j_perror(state->err, errno, "unexpected error");
            return -1;
        }
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
    const char *statement = strncpy_alloc(&(e->buffer), &(e->buffer_capacity),
            directive, n);
    if (statement == NULL)
    {
        neo4j_perror(state->err, errno, "unexpected error");
        return -1;
    }
    e->continuation = evaluate_statement(state, statement, pos);
    return 0;
}


int finalize(shell_state_t *state, evaluation_queue_t *queue, unsigned int n)
{
    assert(n <= queue->depth);
    while (n-- > 0)
    {
        assert(queue->next < queue->capacity);
        evaluation_continuation_t *continuation =
                &(queue->directives[queue->next].continuation);
        if (++(queue->next) >= queue->capacity)
        {
            queue->next = 0;
        }
        --(queue->depth);
        if (continuation->complete(continuation, state))
        {
            return -1;
        }
    }
    return 0;
}
