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
#include "render.h"
#include "util.h"
#include <assert.h>


static int render_hr(FILE *stream, unsigned int nfields,
        unsigned int column_width, bool undersize);
static int write_field(FILE *stream, neo4j_value_t value, char *buffer,
        unsigned int column_width, uint_fast32_t flags);
static size_t value_tostring(neo4j_value_t *value, char *buf, size_t n,
        uint_fast32_t flags);
static int write_quoted_string(FILE *stream, const char *s, size_t n,
        char quot);
static int write_value(FILE *stream, const neo4j_value_t *value,
        char **buffer, size_t *bufcap, uint_fast32_t flags);


int neo4j_render_table(FILE *stream, neo4j_result_stream_t *results,
        unsigned int width, uint_fast32_t flags)
{
    REQUIRE(stream != NULL, -1);
    REQUIRE(results != NULL, -1);
    REQUIRE(width > 1 && width < NEO4J_RENDER_MAX_WIDTH, -1);

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
    assert(column_width >= 2 || nfields == 0);

    char *buffer = NULL;
    if (column_width > 0 && (buffer = malloc(column_width)) == NULL)
    {
        return -1;
    }

    if (render_hr(stream, nfields, column_width, undersize))
    {
        goto failure;
    }

    for (unsigned int i = 0; i < nfields; ++i)
    {
        if (column_width == 2)
        {
            if (fputs("| =", stream) == EOF)
            {
                return -1;
            }
            continue;
        }
        const char *fieldname = neo4j_fieldname(results, i);
        unsigned int field_width = column_width - 2;
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
            if (fputc('|', stream) == EOF)
            {
                goto failure;
            }
            neo4j_value_t value = neo4j_result_field(result, i);
            if (write_field(stream, value, buffer, column_width, flags))
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

    free(buffer);
    return 0;

    int errsv;
failure:
    errsv = errno;
    fflush(stream);
    if (buffer != NULL)
    {
        free(buffer);
    }
    errno = errsv;
    return -1;
}


int render_hr(FILE *stream, unsigned int nfields, unsigned int column_width,
        bool undersize)
{
    assert(column_width < NEO4J_RENDER_MAX_WIDTH-1);
    column_width++;
    for (unsigned int i = 0; i < nfields; ++i)
    {
        if (fwrite(NEO4J_RENDER_TABLE_LINE, sizeof(char),
                    column_width, stream) < column_width)
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


int write_field(FILE *stream, neo4j_value_t value, char *buffer,
        unsigned int column_width, uint_fast32_t flags)
{
    assert(column_width >= 2 && column_width < NEO4J_RENDER_MAX_WIDTH);
    buffer[0] = ' ';

    size_t length;
    if (neo4j_type(value) == NEO4J_STRING &&
            !(flags & NEO4J_RENDER_QUOTE_STRINGS))
    {
        neo4j_string_value(value, buffer+1, column_width-1);
        length = neo4j_string_length(value)+1;
    }
    else
    {
        length = value_tostring(&value, buffer+1, column_width-1, flags) + 1;
    }

    if (length < column_width)
    {
        memset(buffer+length, ' ', column_width - length);
    }
    else
    {
        buffer[column_width-1] = '=';
    }

    if (fwrite(buffer, sizeof(char), column_width, stream) < column_width)
    {
        return -1;
    }

    return 0;
}


size_t value_tostring(neo4j_value_t *value, char *buf, size_t n,
        uint_fast32_t flags)
{
    if (!(flags & NEO4J_RENDER_SHOW_NULLS) && neo4j_is_null(*value))
    {
        buf[0] = '\0';
        return 0;
    }
    return neo4j_ntostring(*value, buf, n);
}


int neo4j_render_csv(FILE *stream, neo4j_result_stream_t *results,
        uint_fast32_t flags)
{
    size_t bufcap = NEO4J_FIELD_BUFFER_INITIAL_CAPACITY;
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
