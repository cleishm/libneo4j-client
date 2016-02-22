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
#include <assert.h>
#include <errno.h>
#include <neo4j-client.h>


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


static int evaluate(shell_state_t *state, evaluation_queue_t *queue,
        const char *directive, size_t n);
static int finalize(shell_state_t *state, evaluation_queue_t *queue,
        unsigned int n);


int batch(shell_state_t *state)
{
    char *buffer = NULL;
    size_t bufcap = 0;

    evaluation_queue_t *queue = calloc(1, sizeof(evaluation_queue_t) +
            (state->pipeline_max * sizeof(struct evaluation)));
    if (queue == NULL)
    {
        return -1;
    }
    queue->capacity = state->pipeline_max;

    int result = -1;

    for (;;)
    {
        char *start;
        size_t length;
        bool complete;
        ssize_t n = neo4j_cli_fparse(state->in, &buffer, &bufcap,
                &start, &length, &complete);
        if (n < 0)
        {
            neo4j_perror(state->err, errno, "unexpected error");
            goto cleanup;
        }
        if (n == 0 || !complete)
        {
            break;
        }

        int r = evaluate(state, queue, start, length);
        if (r < 0)
        {
            goto cleanup;
        }
        if (r > 0)
        {
            break;
        }
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
    free(buffer);
    errno = errsv;
    return result;
}


int evaluate(shell_state_t *state, evaluation_queue_t *queue,
        const char *directive, size_t n)
{
    if (n > 0 && is_command(directive))
    {
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
        return evaluate_command(state, command);
    }

    assert (queue->depth <= queue->capacity);
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
    e->continuation = evaluate_statement(state, statement);
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
