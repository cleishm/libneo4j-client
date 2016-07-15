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
#include "interactive.h"
#include "evaluate.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <histedit.h>
#include <limits.h>
#include <neo4j-client.h>
#include <cypher-parser.h>


static int editline_setup(shell_state_t *state,
        EditLine **el, History **el_history);
static int setup_history(shell_state_t *state, History *el_history);
static char *prompt(EditLine *el);
static unsigned char literal_newline(EditLine *el, int ch);
static unsigned char check_line(EditLine *el, int ch);
static int check_processable(void *data, const char *segment, size_t n,
        struct cypher_input_range range, bool eof);
static int process_input(shell_state_t *state, const char *input, size_t length,
        const char **end);
static int process_segment(void *data, const char *directive, size_t n,
        struct cypher_input_range range, bool eof);


int interact(shell_state_t *state)
{
    EditLine *el = NULL;
    History *el_history = NULL;

    if (editline_setup(state, &el, &el_history))
    {
        goto cleanup;
    }

    fprintf(state->out,
            "neo4j-client " PACKAGE_VERSION ".\n"
            "Enter `:help` for usage hints.\n");
    display_status(state->out, state);

    const char *input;
    int length;
    while ((input = el_gets(el, &length)) != NULL)
    {
        fputc('\n', state->out);
        const char *end;
        int r = process_input(state, input, length, &end);
        if (r < 0)
        {
            goto cleanup;
        }
        if (r > 0)
        {
            break;
        }

        const char *c;
        for (c = input; c < end && isspace(*c); ++c)
            ;
        if (c != end)
        {
            size_t n = end - input;
            for (; isspace(input[n-1]); --n)
                    ;
            const char *entry = temp_copy(state, input, n);
            if (entry == NULL)
            {
                neo4j_perror(state->err, errno, "unexpected error");
                goto cleanup;
            }
            HistEvent ev;
            if (history(el_history, &ev, H_ENTER, entry) < 0)
            {
                neo4j_perror(state->err, errno, "unexpected error");
                goto cleanup;
            }
            if (state->histfile != NULL &&
                    history(el_history, &ev, H_SAVE, state->histfile) < 0)
            {
                neo4j_perror(state->err, errno, "unexpected error");
                goto cleanup;
            }
        }

        if (end < (input + length))
        {
            char *buffer = temp_copy(state, end, (input + length) - end);
            if (buffer == NULL)
            {
                neo4j_perror(state->err, errno, "unexpected error");
                goto cleanup;
            }
            el_push(el, buffer);
        }
    }

    if (input == NULL)
    {
        fputc('\n', state->out);
    }

cleanup:
    if (el_history != NULL)
    {
        history_end(el_history);
    }
    if (el != NULL)
    {
        el_end(el);
    }
    return -1;
}


int editline_setup(shell_state_t *state, EditLine **el, History **el_history)
{
    assert(el != NULL);
    assert(el_history != NULL);

    *el = el_init(state->prog_name, state->in, state->out, state->err);
    if (*el == NULL)
    {
        neo4j_perror(state->err, errno, "failed to initialize editline");
        return -1;
    }
    el_set(*el, EL_CLIENTDATA, state);
    el_set(*el, EL_PROMPT, &prompt);
    el_set(*el, EL_EDITOR, "emacs");

    *el_history = history_init();
    if (*el_history == NULL)
    {
        neo4j_perror(state->err, errno, "failed to initialize history");
        return -1;
    }
    HistEvent ev;
    history(*el_history, &ev, H_SETSIZE, 500);
    history(*el_history, &ev, H_SETUNIQUE, 1);
    el_set(*el, EL_HIST, history, *el_history);

    if (state->histfile != NULL && setup_history(state, *el_history))
    {
        return -1;
    }

    el_set(*el, EL_ADDFN, "ed-literal-newline",
            "Add a literal newline", literal_newline);
    el_set(*el, EL_ADDFN, "ed-check-line",
            "Check a line for a complete directive "
            "or insert a newline", check_line);
    el_set(*el, EL_BIND, "\r", "ed-literal-newline", NULL);
    el_set(*el, EL_BIND, "\n", "ed-check-line", NULL);
    el_set(*el, EL_BIND, "-a", "\r", "ed-literal-newline", NULL);
    el_set(*el, EL_BIND, "-a", "\n", "ed-check-line", NULL);
    el_set(*el, EL_BIND, "-a", "k", "ed-prev-line", NULL);
    el_set(*el, EL_BIND, "-a", "j", "ed-next-line", NULL);

    el_source(*el, NULL);

    el_set(*el, EL_SIGNAL, 1);
    return 0;
}


int setup_history(shell_state_t *state, History *el_history)
{
    assert(state->histfile != NULL);

    char dir[PATH_MAX];
    if (neo4j_dirname(state->histfile, dir, sizeof(dir)) < 0)
    {
        fprintf(state->err, "invalid history file\n");
        return -1;
    }
    if (neo4j_mkdir_p(dir))
    {
        neo4j_perror(state->err, errno, "failed to create history file");
        return -1;
    }

    HistEvent ev;
    if (history(el_history, &ev, H_LOAD, state->histfile) < 0)
    {
        if (errno != ENOENT)
        {
            neo4j_perror(state->err, errno, "failed to load history");
            return -1;
        }

        if (history(el_history, &ev, H_SAVE, state->histfile) < 0)
        {
            neo4j_perror(state->err, errno, "failed to create history file");
            return -1;
        }
    }
    return 0;
}


char *prompt(EditLine *el)
{
    shell_state_t *state;
    if (el_get(el, EL_CLIENTDATA, &state) == 0 && state->session != NULL)
    {
        return "neo4j> ";
    }
    else
    {
        return "neo4j# ";
    }
}


unsigned char literal_newline(EditLine *el, int ch)
{
    char s[2] = { '\n', 0 };
    if (el_insertstr(el, s))
    {
        return CC_ERROR;
    }
    return CC_REFRESH;
}


unsigned char check_line(EditLine *el, int ch)
{
    shell_state_t *state;
    if (el_get(el, EL_CLIENTDATA, &state))
    {
        neo4j_perror(state->err, errno, "unexpected error");
        return CC_FATAL;
    }
    const LineInfo *li = el_line(el);

    size_t length = li->lastchar - li->buffer;

    // if there's no other characters in the line, then we insert a newline
    // at the cursor (the end) and let it process - which will be a noop
    if (length == 0)
    {
        if (literal_newline(el, ch) == CC_ERROR)
        {
            return CC_ERROR;
        }
        return CC_NEWLINE;
    }

    char *line = temp_copy(state, li->buffer, length);
    if (line == NULL)
    {
        neo4j_perror(state->err, errno, "unexpected error");
        return CC_FATAL;
    }

    // add a newline to the end (replacing the '\0')
    // NOTE: we don't use literal_newline, as the cursor may not be
    // at the end of the line
    line[length] = '\n';

    bool process = false;
    if (cypher_quick_uparse(line, length + 1, check_processable, &process,
                CYPHER_PARSE_SINGLE))
    {
        neo4j_perror(state->err, errno, "unexpected error");
        return CC_FATAL;
    }

    if (process)
    {
        return CC_NEWLINE;
    }

    // if we're not processing, then we insert a newline at the cursor position
    return literal_newline(el, ch);
}


int check_processable(void *data, const char *segment, size_t n,
        struct cypher_input_range range, bool eof)
{
    bool *processable = (bool *)data;
    *processable = !eof;
    return 1;
}


struct process_data
{
    shell_state_t *state;
    const char *input;
    size_t last_offset;
    int result;
};


int process_input(shell_state_t *state, const char *input, size_t length,
        const char **end)
{
    // TODO: use previous parse rather than copy&re-parse
    char *line = temp_copy(state, input, length);
    line[length] = '\n';

    struct process_data cbdata =
        { .state = state, .input = input, .last_offset = 0, .result = 0 };

    if (cypher_quick_uparse(line, length + 1, process_segment, &cbdata, 0))
    {
        neo4j_perror(state->err, errno, "unexpected error");
        return -1;
    }

    size_t last_offset = cbdata.last_offset;
    for (; last_offset < length && isspace(input[last_offset]); ++last_offset)
        ;
    *end = input + last_offset;

    return cbdata.result;
}


int process_segment(void *data, const char *directive, size_t n,
        struct cypher_input_range range, bool eof)
{
    struct process_data *cbdata = (struct process_data *)data;

    if (eof)
    {
        assert(cbdata->result == 0);
        cbdata->last_offset = range.start.offset;
        return 1;
    }

    if (n == 0)
    {
        return 0;
    }

    // ensure null terminated
    const char *s = temp_copy(cbdata->state, directive, n);
    if (s == NULL)
    {
        neo4j_perror(cbdata->state->err, errno, "unexpected error");
        return -1;
    }

    int r = 0;
    if (is_command(s))
    {
        r = evaluate_command_string(cbdata->state, s);
    }
    else
    {
        evaluation_continuation_t continuation =
                evaluate_statement(cbdata->state, s, range.start);
        r = continuation.complete(&continuation, cbdata->state);
    }

    cbdata->last_offset = range.end.offset;
    cbdata->result = (r > 0)? r : 0;
    return cbdata->result;
}
