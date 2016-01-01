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
#include "neo4j-client.h"
#include "util.h"
#include <assert.h>

typedef struct _yycontext yycontext;

#define YY_CTX_LOCAL
#define YY_PARSE(T) static T
#define YY_CTX_MEMBERS \
    size_t consumed; \
    size_t begin; \
    size_t end; \
    bool complete; \
    int (*source)(void *data, char *buf, int n); \
    void *source_data;

static inline void source(yycontext *ctx, char *buf, int *result,
        int max_size);
static inline void capture(yycontext *ctx, size_t pos, bool complete);

#define YY_INPUT(ctx, buf, result, max_size) \
    source((ctx), (buf), &(result), (max_size))

#pragma GCC diagnostic ignored "-Wunused-function"
#include "cli_parser_leg.c"

struct source_from_buffer_data
{
    const char *buffer;
    size_t length;
};

static int source_from_buffer(void *data, char *buf, int n);
static int source_from_stream(void *data, char *buf, int n);
static ssize_t uparse(yyrule rule, const char *s, size_t n,
        const char **start, size_t *length, bool *complete);
static ssize_t fparse(yyrule rule, FILE *stream,
        char ** restrict buf, size_t * restrict bufcap,
        char ** restrict start, size_t * restrict length, bool *complete);


ssize_t neo4j_cli_uparse(const char *s, size_t n,
        const char **start, size_t *length, bool *complete)
{
    return uparse(yy_directive, s, n, start, length, complete);
}


ssize_t neo4j_cli_fparse(FILE *stream,
        char ** restrict buf, size_t * restrict bufcap,
        char ** restrict start, size_t * restrict length, bool *complete)
{
    return fparse(yy_directive, stream, buf, bufcap, start, length, complete);
}


ssize_t neo4j_cli_arg_uparse(const char *s, size_t n,
        const char **start, size_t *length, bool *complete)
{
    return uparse(yy_argument, s, n, start, length, complete);
}


ssize_t uparse(yyrule rule, const char *s, size_t n,
        const char **start, size_t *length, bool *complete)
{
    REQUIRE(s != NULL, -1);
    REQUIRE(complete != NULL, -1);

    yycontext ctx;
    memset(&ctx, 0, sizeof(yycontext));

    struct source_from_buffer_data data = { .buffer = s, .length = n };
    ctx.source = source_from_buffer;
    ctx.source_data = &data;

    int consumed = 0;

    int result = yyparsefrom(&ctx, rule);
    if (result <= 0)
    {
        // no match
        ctx.consumed = 0;
        ctx.begin = 0;
        ctx.end = 0;
        ctx.complete = false;
    }
    consumed = ctx.consumed;
    *complete = ctx.complete;
    if (start != NULL)
    {
        *start = s + ctx.begin;
    }
    if (length != NULL)
    {
        *length = ctx.end - ctx.begin;
    }

    yyrelease(&ctx);
    return consumed;
}


ssize_t fparse(yyrule rule, FILE *stream,
        char ** restrict buf, size_t * restrict bufcap,
        char ** restrict start, size_t * restrict length, bool *complete)
{
    REQUIRE(stream != NULL, -1);
    REQUIRE(buf != NULL, -1);
    REQUIRE(bufcap != NULL, -1);
    REQUIRE(complete != NULL, -1);

    yycontext ctx;
    memset(&ctx, 0, sizeof(yycontext));

    ctx.source = source_from_stream;
    ctx.source_data = stream;

    int consumed = 0;

    int result = yyparsefrom(&ctx, rule);
    if (result <= 0)
    {
        // no match
        ctx.consumed = 0;
        ctx.begin = 0;
        ctx.end = 0;
        ctx.complete = false;
    }
    else
    {
        // rather than realloc and copy, just steal the malloced buffer from
        // yycontext, and replace it with either the buffer passed in or with
        // a small malloced region.
        if (*buf != NULL)
        {
            free(*buf);
        }
        *buf = ctx.__buf;
        *bufcap = ctx.__buflen;
        ctx.__buf = NULL;
    }

    consumed = ctx.consumed;
    *complete = ctx.complete;
    if (start != NULL)
    {
        *start = *buf + ctx.begin;
    }
    if (length != NULL)
    {
        *length = ctx.end - ctx.begin;
    }

    yyrelease(&ctx);
    return consumed;
}


void capture(yycontext *ctx, size_t pos, bool complete)
{
    ctx->consumed = pos;
    ctx->begin = ctx->__begin;
    ctx->end = ctx->__end;
    ctx->complete = complete;
}


void source(yycontext *ctx, char *buf, int *result, int max_size)
{
    assert(ctx != NULL && ctx->source != NULL);
    *result = ctx->source(ctx->source_data, buf, max_size);
}


int source_from_buffer(void *data, char *buf, int n)
{
    struct source_from_buffer_data *input = data;
    int len = min(input->length, n);
    input->length -= len;
    if (len == 0)
    {
        return len;
    }
    memcpy(buf, input->buffer, len);
    return len;
}


int source_from_stream(void *data, char *buf, int n)
{
    FILE *stream = data;
    int c = getc(stream);
    if (c == EOF)
    {
        return 0;
    }
    *buf = c;
    return 1;
}
