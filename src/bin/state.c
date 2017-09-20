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
#include "state.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>


#define NEO4J_DEFAULT_MAX_SOURCE_DEPTH 10


int shell_state_init(shell_state_t *state, const char *prog_name,
        FILE *in, FILE *out, FILE *err, FILE *tty)
{
    memset(state, 0, sizeof(shell_state_t));
    state->prog_name = prog_name;
    state->in = in;
    state->out = out;
    state->err = err;
    state->tty = tty;
    state->output = out;
    state->config = neo4j_new_config();
    if (state->config == NULL)
    {
        return -1;
    }
    state->pipeline_max =
            neo4j_config_get_max_pipelined_requests(state->config) / 2;
    state->source_max_depth = NEO4J_DEFAULT_MAX_SOURCE_DEPTH;
    state->colorize = no_shell_colorization;
    neo4j_config_set_render_wrapped_values(state->config, true);
    return 0;
}


void shell_state_destroy(shell_state_t *state)
{
    assert(state != NULL);
    if (state->connection != NULL)
    {
        neo4j_close(state->connection);
    }
    if (state->outfile != NULL)
    {
        free(state->outfile);
        fclose(state->output);
    }
    neo4j_config_free(state->config);
    free(state->temp_buffer);

    for (unsigned int i = 0; i < state->nexports; ++i)
    {
        free(state->exports_storage[i]);
    }
    free(state->exports);
    free(state->exports_storage);

    memset(state, 0, sizeof(shell_state_t));
}


static int valert(shell_state_t *state, struct cypher_input_position pos,
        const char *type, const char *fmt, va_list ap)
{
    int written = 0;
    int r;

    struct error_colorization *colors = state->colorize->error;

    if (state->infile != NULL)
    {
        r = fprintf(state->err, "%s%s:%u:%u:%s ",
            colors->pos[0], state->infile, pos.line,
            pos.column, state->colorize->error->pos[1]);
        if (r < 0)
        {
            return -1;
        }
        written += r;
    }

    r = fprintf(state->err, "%s%s:%s %s",
            colors->typ[0], type, colors->typ[1],
            colors->msg[0]);
    if (r < 0)
    {
        return -1;
    }
    written += r;

    r = vfprintf(state->err, fmt, ap);
    if (r < 0)
    {
        return -1;
    }
    written += r;
    r = fprintf(state->err, "%s\n", colors->msg[1]);
    if (r < 0)
    {
        return -1;
    }
    written += r;
    return written;
}


static int alert(shell_state_t *state, struct cypher_input_position pos,
        const char *type, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = valert(state, pos, type, fmt, ap);
    va_end(ap);
    return r;
}


int print_error(shell_state_t *state, struct cypher_input_position pos,
        const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = valert(state, pos, "error", fmt, ap);
    va_end(ap);
    return r;
}


int print_errno(shell_state_t *state, struct cypher_input_position pos, int err)
{
    char buf[256];
    return alert(state, pos, "error", "%s",
            neo4j_strerror(err, buf, sizeof(buf)));
}


int print_error_errno(shell_state_t *state, struct cypher_input_position pos,
        int err, const char *msg)
{
    char buf[256];
    return alert(state, pos, "error", "%s: %s", msg,
            neo4j_strerror(err, buf, sizeof(buf)));
}


int print_warning(shell_state_t *state, struct cypher_input_position pos,
        const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = valert(state, pos, "warning", fmt, ap);
    va_end(ap);
    return r;
}


int redirect_output(shell_state_t *state, struct cypher_input_position pos,
        const char *filename)
{
    char *outfile = NULL;
    FILE *output = state->out;

    if (filename != NULL && *filename != '\0' && strcmp(filename, "-") != 0)
    {
        outfile = strdup(filename);
        if (outfile == NULL)
        {
            print_error_errno(state, pos, errno, "strdup");
            return -1;
        }

        output = fopen(filename, "w");
        if (output == NULL)
        {
            print_error(state, pos, "Unable to open output file '%s': %s",
                    filename, strerror(errno));
            return -1;
        }
    }

    if (state->outfile != NULL)
    {
        free(state->outfile);
        fclose(state->output);
    }

    state->outfile = outfile;
    state->output = output;
    return 0;
}


static int neo4j_string_cmp(neo4j_value_t s1, neo4j_value_t s2)
{
    size_t s1len = neo4j_string_length(s1);
    size_t s2len = neo4j_string_length(s2);
    size_t n = (s1len < s2len)? s1len : s2len;
    int cmp = strncmp(neo4j_ustring_value(s1), neo4j_ustring_value(s2), n);
    return cmp? cmp : (s1len < s2len)? -1 : (s1len == s2len)? 0 : 1;
}


int shell_state_add_export(shell_state_t *state, neo4j_value_t name,
        neo4j_value_t value, void *storage)
{
    unsigned int i;
    int cmp = -1;
    for (i = 0; i < state->nexports &&
            (cmp = neo4j_string_cmp(state->exports[i].key, name)) < 0; ++i)
        ;
    if (cmp == 0)
    {
        state->exports[i].key = name;
        state->exports[i].value = value;
        free(state->exports_storage[i]);
        state->exports_storage[i] = storage;
        return 0;
    }

    assert(state->nexports <= state->exports_cap);
    if (state->nexports >= state->exports_cap)
    {
        size_t cap = (state->exports_cap == 0)? 8 : state->exports_cap * 2;
        neo4j_map_entry_t *exports = realloc(state->exports,
                cap * sizeof(neo4j_map_entry_t));
        if (exports == NULL)
        {
            return -1;
        }

        void **storage = realloc(state->exports_storage, cap * sizeof(void *));
        if (storage == NULL)
        {
            free(exports);
            return -1;
        }

        state->exports = exports;
        state->exports_storage = storage;
        state->exports_cap = cap;
    }

    if (i < state->nexports)
    {
        memmove(state->exports + i + 1, state->exports + i,
                (state->nexports - i) * sizeof(neo4j_map_entry_t));
        memmove(state->exports_storage + i + 1, state->exports_storage + i,
                (state->nexports - i) * sizeof(void *));
    }
    state->exports[i].key = name;
    state->exports[i].value = value;
    state->exports_storage[i] = storage;
    ++(state->nexports);
    return 0;
}


void shell_state_unexport(shell_state_t *state, neo4j_value_t name)
{
    for (unsigned int i = 0; i < state->nexports; ++i)
    {
        if (neo4j_eq(state->exports[i].key, name))
        {
            free(state->exports_storage[i]);
            state->exports[i] = state->exports[state->nexports-1];
            state->exports_storage[i] =
                    state->exports_storage[state->nexports-1];
            --(state->nexports);
            return;
        }
    }
}


void display_status(FILE* stream, shell_state_t *state)
{
    struct status_colorization *colors = state->colorize->status;
    if (state->connection == NULL)
    {
        fprintf(stream, "%sNot connected%s\n", colors->url[0], colors->url[1]);
    }
    else
    {
        const char *username = neo4j_connection_username(state->connection);
        const char *hostname = neo4j_connection_hostname(state->connection);
        bool ipv6 = (strchr(hostname, ':') != NULL);
        unsigned int port = neo4j_connection_port(state->connection);
        bool secure = neo4j_connection_is_secure(state->connection);
        const char *server_id = neo4j_server_id(state->connection);

        fprintf(stream, "Connected to '%sneo4j://%s%s%s%s%s:%u%s'",
                colors->url[0],
                (username != NULL)? username : "",
                (username != NULL)? "@" : "",
                ipv6? "[" : "", hostname, ipv6? "]" : "",
                port,
                colors->url[1]);

        if (!secure)
        {
            fprintf(stream, " (%sinsecure%s)", colors->wrn[0], colors->wrn[1]);
        }

        if (server_id != NULL)
        {
            fprintf(stream, " [%s]", server_id);
        }
        fputc('\n', stream);
    }
}
