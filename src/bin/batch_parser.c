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
#include "batch_parser.h"
#include <assert.h>
#include <errno.h>
#include <setjmp.h>

typedef struct _yycontext yycontext;

#define YY_CTX_LOCAL
#define YY_PARSE(T) static T
#define YY_CTX_MEMBERS \
    FILE *stream; \
    sigjmp_buf abort_env; \
    size_t begin; \
    size_t end; \
    bool complete;

#define YY_MALLOC abort_malloc
#define YY_REALLOC abort_realloc

#define YY_INPUT(yy, buf, result, max_size) \
    source(yy, buf, &result, max_size)

static void *abort_malloc(yycontext *yy, size_t size);
static void *abort_realloc(yycontext *yy, void *ptr, size_t size);
static inline void source(yycontext *yy, char *buf, int *result, int max_size);
static inline void capture(yycontext *yy);

#pragma GCC diagnostic ignored "-Wunused-function"
#include "batch_parser_leg.c"


#define abort_parse(yy) \
    do { assert(errno != 0); siglongjmp(yy->abort_env, errno); } while (0)
static int safe_yyparse(yycontext *yy);


void source(yycontext *yy, char *buf, int *result, int max_size)
{
    int c = getc(yy->stream);
    if (c == EOF)
    {
        *result = 0;
        return;
    }
    *buf = c;
    *result = 1;
}


int batch_parse(FILE *stream,
        int (*callback)(void *d, const char *s, size_t n), void *data)
{
    yycontext yy;
    memset(&yy, 0, sizeof(yycontext));

    yy.stream = stream;
    int err = -1;

    for (;;)
    {
        int result = yyparse(&yy);
        if (result <= 0)
        {
            goto cleanup;
        }

        if (yy.end == 0)
        {
            break;
        }

        err = callback(data, yy.__buf + yy.begin, yy.end - yy.begin);
        if (err)
        {
            goto cleanup;
        }
    }

    err = 0;

cleanup:
    yyrelease(&yy);
    return err;
}


int safe_yyparse(yycontext *yy)
{
    int err;
    if ((err = sigsetjmp(yy->abort_env, 0)) != 0)
    {
        errno = err;
        return -1;
    }

    int result = yyparse(yy);
    memset(yy->abort_env, 0, sizeof(sigjmp_buf));
    return result;
}


void *abort_malloc(yycontext *yy, size_t size)
{
    void *m = malloc(size);
    if (m == NULL)
    {
        abort_parse(yy);
    }
    return m;
}


void *abort_realloc(yycontext *yy, void *ptr, size_t size)
{
    void *m = realloc(ptr, size);
    if (m == NULL)
    {
        abort_parse(yy);
    }
    return m;
}


void capture(yycontext *yy)
{
    yy->begin = yy->__begin;
    yy->end = yy->__end;
}
