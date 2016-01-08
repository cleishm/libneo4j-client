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
#include "client_config.h"
#include "util.h"
#include <assert.h>


#define FIELD_BUFFER_INITIAL_CAPACITY 1024


static int render_hr(FILE *stream, unsigned int nfields,
        unsigned int column_width, bool undersize);
static int write_field(FILE *stream, neo4j_value_t value, unsigned int width,
        uint_fast32_t flags);
static const char *value_tostring(neo4j_value_t *value, char *buf, size_t n,
        uint_fast32_t flags);
static int write_quoted_string(FILE *stream, const char *s, size_t n,
        char quot);
static int write_value(FILE *stream, const neo4j_value_t *value,
        char **buffer, size_t *bufcap, uint_fast32_t flags);


int neo4j_render_table(FILE *stream, neo4j_result_stream_t *results,
        unsigned int width, uint_fast32_t flags)
{
    REQUIRE(width > 1, -1);
    REQUIRE(width < NEO4J_RENDER_MAX_WIDTH, -1);

    int err = neo4j_check_failure(results);
    if (err != 0)
    {
        errno = err;
        return -1;
    }

    unsigned int nfields = neo4j_nfields(results);
    if (nfields == 0)
    {
        return 0;
    }

    unsigned int column_width = (nfields == 0 || width <= (nfields+1))? 0 :
        (width - nfields - 1) / nfields;
    bool undersize = false;
    while (column_width < 2 && nfields > 0)
    {
        undersize = true;
        nfields--;
        column_width = (nfields == 0 || width <= (nfields+1))? 0 :
            (width - nfields - 1) / nfields;
    }

    unsigned int field_width = (column_width <= 2)? 0 : column_width - 2;

    if (render_hr(stream, nfields, column_width, undersize))
    {
        goto failure;
    }

    for (unsigned int i = 0; i < nfields; ++i)
    {
        const char *fieldname = neo4j_fieldname(results, i);
        if (fprintf(stream, "| %-*.*s%c", field_width, field_width, fieldname,
                (strlen(fieldname) > field_width)? '=' : ' ') < 0)
        {
            goto failure;
        }
    }
    if (fputs(undersize? "|=\n" : "|\n", stream) == EOF)
    {
        goto failure;
    }

    if (render_hr(stream, nfields, column_width, undersize))
    {
        goto failure;
    }

    neo4j_result_t *result;
    while ((result = neo4j_fetch_next(results)) != NULL)
    {
        for (unsigned int i = 0; i < nfields; ++i)
        {
            if (fputs("| ", stream) == EOF)
            {
                goto failure;
            }
            neo4j_value_t value = neo4j_result_field(result, i);
            if (write_field(stream, value, field_width, flags))
            {
                goto failure;
            }
        }
        if (fputs(undersize? "|=\n" : "|\n", stream) == EOF)
        {
            goto failure;
        }
    }

    err = neo4j_check_failure(results);
    if (err != 0)
    {
        errno = err;
        goto failure;
    }

    if (render_hr(stream, nfields, column_width, undersize))
    {
        goto failure;
    }

    if (fflush(stream) == EOF)
    {
        return -1;
    }

    return 0;

    int errsv;
failure:
    errsv = errno;
    fflush(stream);
    errno = errsv;
    return -1;
}


int render_hr(FILE *stream, unsigned int nfields, unsigned int column_width,
        bool undersize)
{
    char buffer[column_width+2];
    buffer[0] = '+';
    memset(buffer+1, '-', column_width);
    buffer[column_width+1] = 0;
    for (unsigned int i = 0; i < nfields; ++i)
    {
        if (fputs(buffer, stream) == EOF)
        {
            return -1;
        }
    }
    if (fputs(undersize? "+=\n" : "+\n", stream) == EOF)
    {
        return -1;
    }
    return 0;
}


int write_field(FILE *stream, neo4j_value_t value, unsigned int width,
        uint_fast32_t flags)
{
    char buffer[width+2];
    size_t length;

    if (neo4j_type(value) == NEO4J_STRING &&
            !(flags & NEO4J_RENDER_QUOTE_STRINGS))
    {
        neo4j_string_value(value, buffer, width+2);
        length = min(width+1, neo4j_string_length(value));
    }
    else
    {
        value_tostring(&value, buffer, width+2, flags);
        length = strlen(buffer);
    }
    assert(length < width + 2);
    bool oversize = (length > width);

    if (length < width)
    {
        memset(buffer+length, ' ', width - length);
    }
    buffer[width] = '\0';

    if (fputs(buffer, stream) == EOF)
    {
        return -1;
    }
    if (fputc((oversize)? '=' : ' ', stream) == EOF)
    {
        return -1;
    }

    return 0;
}


const char *value_tostring(neo4j_value_t *value, char *buf, size_t n,
        uint_fast32_t flags)
{
    if (!(flags & NEO4J_RENDER_SHOW_NULLS) && neo4j_is_null(*value))
    {
        buf[0] = '\0';
        return buf;
    }
    return neo4j_tostring(*value, buf, n);
}


int neo4j_render_csv(FILE *stream, neo4j_result_stream_t *results,
        uint_fast32_t flags)
{
    size_t bufcap = FIELD_BUFFER_INITIAL_CAPACITY;
    char *buffer = malloc(bufcap);
    if (buffer == NULL)
    {
        return -1;
    }

    int err = neo4j_check_failure(results);
    if (err != 0)
    {
        errno = err;
        goto failure;
    }

    unsigned int nfields = neo4j_nfields(results);
    if (nfields == 0)
    {
        free(buffer);
        return 0;
    }

    for (unsigned int i = 0; i < nfields; ++i)
    {
        const char *fieldname = neo4j_fieldname(results, i);
        if (write_quoted_string(stream, fieldname, strlen(fieldname), '"'))
        {
            goto failure;
        }
        if ((i + 1) < nfields && fputc(',', stream) == EOF)
        {
            goto failure;
        }
    }
    if (fputc('\n', stream) == EOF)
    {
        goto failure;
    }

    neo4j_result_t *result;
    while ((result = neo4j_fetch_next(results)) != NULL)
    {
        for (unsigned int i = 0; i < nfields; ++i)
        {
            neo4j_value_t value = neo4j_result_field(result, i);
            if (write_value(stream, &value, &buffer, &bufcap, flags))
            {
                goto failure;
            }
            if ((i + 1) < nfields && fputc(',', stream) == EOF)
            {
                goto failure;
            }
        }
        if (fputc('\n', stream) == EOF)
        {
            goto failure;
        }
    }

    err = neo4j_check_failure(results);
    if (err != 0)
    {
        errno = err;
        goto failure;
    }

    if (fflush(stream) == EOF)
    {
        goto failure;
    }

    free(buffer);
    return 0;

    int errsv;
failure:
    errsv = errno;
    if (buffer != NULL)
    {
        free(buffer);
    }
    fflush(stream);
    errno = errsv;
    return -1;
}


int write_quoted_string(FILE *stream, const char *s, size_t n, char quot)
{
    if (fputc('"', stream) == EOF)
    {
        return -1;
    }

    const char *end = s + n;
    while (s < end)
    {
        const char *c = (const char *)memchr((void *)(intptr_t)s, '"', n);
        if (c == NULL)
        {
            if (fwrite(s, sizeof(char), n, stream) < n)
            {
                return -1;
            }
            break;
        }

        assert(c >= s && c < end);
        assert(*c == '"');
        size_t l = c - s;
        if (fwrite(s, sizeof(char), l, stream) < l)
        {
            return -1;
        }
        if (fputc(quot, stream) == EOF)
        {
            return -1;
        }
        if (fputc('"', stream) == EOF)
        {
            return -1;
        }
        n -= l+1;
        s = c+1;
    }
    if (fputc('"', stream) == EOF)
    {
        return -1;
    }
    return 0;
}


int write_value(FILE *stream, const neo4j_value_t *value,
        char **buffer, size_t *bufcap, uint_fast32_t flags)
{
    neo4j_type_t type = neo4j_type(*value);

    if (type == NEO4J_STRING)
    {
        return write_quoted_string(stream, neo4j_ustring_value(*value),
                neo4j_string_length(*value), '"');
    }

    if (!(flags & NEO4J_RENDER_SHOW_NULLS) && type == NEO4J_NULL)
    {
        return 0;
    }

    assert(*bufcap >= 2);
    for (;;)
    {
        size_t required = neo4j_ntostring(*value, *buffer, *bufcap);
        if (required < *bufcap)
        {
            break;
        }

        char *newbuf = realloc(*buffer, required);
        if (newbuf == NULL)
        {
            return -1;
        }
        *bufcap = required;
        *buffer = newbuf;
    }

    if (type == NEO4J_NULL || type == NEO4J_BOOL || type == NEO4J_INT ||
            type == NEO4J_FLOAT)
    {
        if (fputs(*buffer, stream) == EOF)
        {
            return -1;
        }
        return 0;
    }
    return write_quoted_string(stream, *buffer, strlen(*buffer), '"');
}
