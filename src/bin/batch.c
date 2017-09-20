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


typedef struct evaluation_queue
{
    unsigned int next;
    unsigned int depth;
    unsigned int capacity;
    evaluation_continuation_t *continuations[];
} evaluation_queue_t;


struct parse_callback_data
{
    shell_state_t *state;
    evaluation_queue_t *queue;
};


static int parse_string(void *data, struct parse_callback_data *cdata);
static int parse_stream(void *data, struct parse_callback_data *cdata);
static int process(shell_state_t *state, struct cypher_input_position pos,
        int (*parse)(void *data, struct parse_callback_data *cdata),
        void *parse_data);

static int parse_callback(void *data,
        const cypher_quick_parse_segment_t *segment);
static int evaluate(shell_state_t *state, evaluation_queue_t *queue,
        const cypher_quick_parse_segment_t *segment);
static int finalize(shell_state_t *state, evaluation_queue_t *queue,
        unsigned int n);
static int abort_outstanding(shell_state_t *state, evaluation_queue_t *queue);


int source(shell_state_t *state, struct cypher_input_position pos,
        const char *filename)
{
    if (state->source_depth >= state->source_max_depth)
    {
        print_error(state, pos, "Too many nested calls to `:source`");
        return -1;
    }

    FILE *stream;
    if (strcmp(filename, "-") == 0)
    {
        stream = stdin;
        filename = "<stdin>";
    }
    else
    {
        stream = fopen(filename, "r");
        if (stream == NULL)
        {
            print_error(state, pos, "Unable to read file '%s': %s",
                    filename, strerror(errno));
            return -1;
        }
    }

    bool interactive = state->interactive;
    state->interactive = false;
    const char *prev_infile = state->infile;
    state->infile = filename;
    ++(state->source_depth);
    int result = batch(state, pos, stream);
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


int eval(shell_state_t *state, struct cypher_input_position pos,
        const char *script)
{
    return process(state, pos, parse_string, (void *)(uintptr_t)script);
}


int batch(shell_state_t *state, struct cypher_input_position pos, FILE *stream)
{
    return process(state, pos, parse_stream, (void *)stream);
}


int parse_string(void *data, struct parse_callback_data *cdata)
{
    return cypher_quick_parse((const char *)data, parse_callback, cdata, 0);
}


int parse_stream(void *data, struct parse_callback_data *cdata)
{
    return cypher_quick_fparse((FILE *)data, parse_callback, cdata, 0);
}


int process(shell_state_t *state, struct cypher_input_position pos,
        int (*parse)(void *data, struct parse_callback_data *cdata),
        void *parse_data)
{
    evaluation_queue_t *queue = calloc(1, sizeof(evaluation_queue_t) +
            (state->pipeline_max * sizeof(evaluation_continuation_t *)));
    if (queue == NULL)
    {
        print_error_errno(state, pos, errno, "calloc");
        return -1;
    }
    queue->capacity = state->pipeline_max;

    int result = -1;
    struct parse_callback_data cdata = { .state = state, .queue = queue };
    int err = parse(parse_data, &cdata);
    if (err)
    {
        if (err != -2)
        {
            print_errno(state, pos, errno);
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
    if (abort_outstanding(state, queue) && result == 0)
    {
        free(queue);
        return -1;
    }
    free(queue);
    errno = errsv;
    return result;
}


int parse_callback(void *data, const cypher_quick_parse_segment_t *segment)
{
    struct parse_callback_data *cbdata = (struct parse_callback_data *)data;
    if (evaluate(cbdata->state, cbdata->queue, segment))
    {
        return -2;
    }
    return 0;
}


int evaluate(shell_state_t *state, evaluation_queue_t *queue,
        const cypher_quick_parse_segment_t *segment)
{
    size_t n;
    const char *s = cypher_quick_parse_segment_get_text(segment, &n);
    if (n == 0)
    {
        return 0;
    }

    struct cypher_input_range range =
            cypher_quick_parse_segment_get_range(segment);

    if (cypher_quick_parse_segment_is_command(segment))
    {
        // drain queue before running commands
        int err = finalize(state, queue, queue->depth);
        if (err)
        {
            return err;
        }
        return evaluate_command(state, s, n, range.start);
    }

    assert(queue->depth <= queue->capacity);
    if ((queue->depth >= queue->capacity) && finalize(state, queue, 1))
    {
        return -1;
    }
    assert(queue->depth < queue->capacity);

    evaluation_continuation_t *continuation =
            prepare_statement(state, s, n, range.start);
    if (continuation == NULL)
    {
        return -1;
    }

    unsigned int i = queue->next + queue->depth;
    if (i >= queue->capacity)
    {
        i -= queue->capacity;
    }
    queue->continuations[i] = continuation;
    ++(queue->depth);
    return 0;
}


int finalize(shell_state_t *state, evaluation_queue_t *queue, unsigned int n)
{
    assert(n <= queue->depth);
    while (n-- > 0)
    {
        assert(queue->next < queue->capacity);
        evaluation_continuation_t *continuation =
                queue->continuations[queue->next];
        if (++(queue->next) >= queue->capacity)
        {
            queue->next = 0;
        }
        --(queue->depth);
        if (complete_evaluation(continuation, state))
        {
            return -1;
        }
    }
    return 0;
}


int abort_outstanding(shell_state_t *state, evaluation_queue_t *queue)
{
    int result = 0;
    int err;
    while (queue->depth > 0)
    {
        assert(queue->next < queue->capacity);
        evaluation_continuation_t *continuation =
                queue->continuations[queue->next];
        if (++(queue->next) >= queue->capacity)
        {
            queue->next = 0;
        }
        --(queue->depth);
        if (abort_evaluation(continuation, state) && result == 0)
        {
            err = errno;
            result = -1;
        }
    }
    if (result != 0)
    {
        errno = err;
    }
    return result;
}
