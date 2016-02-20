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
#include <errno.h>
#include <neo4j-client.h>


int batch(shell_state_t *state)
{
    char *buffer = NULL;
    size_t capacity = 0;
    int result = -1;

    for (;;)
    {
        char *start;
        size_t length;
        bool complete;
        ssize_t n = neo4j_cli_fparse(state->in, &buffer, &capacity,
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

        const char *directive = temp_copy(state, start, length);
        if (directive == NULL)
        {
            neo4j_perror(state->err, errno, "unexpected error");
            goto cleanup;
        }

        int r;
        if (is_command(directive))
        {
            r = evaluate_command(state, directive);
        }
        else
        {
            evaluation_continuation_t continuation =
                evaluate_statement(state, directive);
            r = continuation.complete(&continuation, state);
        }

        if (r < 0)
        {
            goto cleanup;
        }
        if (r > 0)
        {
            break;
        }
    }

    result = 0;

cleanup:
    if (buffer != NULL)
    {
        free(buffer);
    }
    return result;
}
