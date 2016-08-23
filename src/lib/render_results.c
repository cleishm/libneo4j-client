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
static int write_field(FILE *stream, const char *s, unsigned int width);
static int write_field_value(FILE *stream, neo4j_value_t value, char **buf,
        size_t *bufcap, unsigned int width, uint_fast32_t flags);
static size_t value_tostring(neo4j_value_t *value, char *buf, size_t n,
        uint_fast32_t flags);
static int write_csv_quoted_string(FILE *stream, const char *s, size_t n);
static int write_value(FILE *stream, const neo4j_value_t *value,
        char **buf, size_t *bufcap, uint_fast32_t flags);
static int write_unprintable(FILE *stream, int codepoint, int width);


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

    // calculate size of columns, and set undersize if there's less columns
    // than fields
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

    // allocate a buffer for staging values before output
    char *buffer = NULL;
    size_t bufcap = column_width;
    if (column_width > 0 && (buffer = malloc(bufcap)) == NULL)
    {
        return -1;
    }

    if (render_hr(stream, nfields, column_width, undersize))
    {
        goto failure;
    }

    // render header
    unsigned int field_width = column_width - 2;
    for (unsigned int i = 0; i < nfields; ++i)
    {
        if (fputs("| ", stream) == EOF)
        {
            goto failure;
        }

        const char *fieldname = neo4j_fieldname(results, i);
        int overflow = write_field(stream, fieldname, field_width);
        if (overflow < 0)
        {
            goto failure;
        }
        if (fputc(overflow? '=' : ' ', stream) == EOF)
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
            int overflow = write_field_value(stream, value, &buffer, &bufcap,
                    field_width, flags);
            if (overflow < 0)
            {
                goto failure;
            }
            if (fputc(overflow? '=' : ' ', stream) == EOF)
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


int write_field(FILE *stream, const char *s, unsigned int width)
{
    while (width > 0 && *s != '\0')
    {
        size_t b = SIZE_MAX;
        int cp = neo4j_u8codepoint(s, &b);
        if (cp < 0)
        {
            return -1;
        }
        assert(b > 0);
        int w = neo4j_u8cpwidth(cp);
        if (w < 0)
        {
            s += b;
            w = write_unprintable(stream, cp, width);
            if (w < 0)
            {
                return -1;
            }
            if ((unsigned int)w > width)
            {
                width = 0;
                break;
            }
            width -= w;
            continue;
        }
        if ((unsigned int)w > width)
        {
            break;
        }
        if (fwrite(s, sizeof(char), b, stream) < b)
        {
            return -1;
        }
        s += b;
        width -= w;
    }
    if (width > 0 && fwrite(NEO4J_RENDER_CELL_LINE+1, sizeof(char),
            width, stream) < width)
    {
        return -1;
    }
    return (*s != '\0')? 1 : 0;
}


int write_field_value(FILE *stream, neo4j_value_t value, char **buf,
        size_t *bufcap, unsigned int width, uint_fast32_t flags)
{
    assert(*bufcap > 0);
    do
    {
        size_t length;
        if (neo4j_type(value) == NEO4J_STRING &&
                !(flags & NEO4J_RENDER_QUOTE_STRINGS))
        {
            neo4j_string_value(value, *buf, *bufcap);
            length = neo4j_string_length(value);
        }
        else
        {
            length = value_tostring(&value, *buf, *bufcap, flags);
        }

        if (length < *bufcap)
        {
            break;
        }
        int w = neo4j_u8cswidth(*buf, *bufcap);
        if (w > 0 && (unsigned int)w > width)
        {
            break;
        }

        char *newbuf = realloc(*buf, length + 1);
        if (newbuf == NULL)
        {
            return -1;
        }
        *bufcap = length + 1;
        *buf = newbuf;
    } while (true);

    return write_field(stream, *buf, width);
}


size_t value_tostring(neo4j_value_t *value, char *buf, size_t n,
        uint_fast32_t flags)
{
    assert(n > 0);
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
        if (write_csv_quoted_string(stream, fieldname, strlen(fieldname)))
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


int write_csv_quoted_string(FILE *stream, const char *s, size_t n)
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
        if (fputs("\"\"", stream) == EOF)
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
        char **buf, size_t *bufcap, uint_fast32_t flags)
{
    neo4j_type_t type = neo4j_type(*value);

    if (type == NEO4J_STRING)
    {
        return write_csv_quoted_string(stream, neo4j_ustring_value(*value),
                neo4j_string_length(*value));
    }

    if (!(flags & NEO4J_RENDER_SHOW_NULLS) && type == NEO4J_NULL)
    {
        return 0;
    }

    assert(*bufcap >= 2);
    do
    {
        size_t required = neo4j_ntostring(*value, *buf, *bufcap);
        if (required < *bufcap)
        {
            break;
        }

        char *newbuf = realloc(*buf, required);
        if (newbuf == NULL)
        {
            return -1;
        }
        *bufcap = required;
        *buf = newbuf;
    } while (true);

    if (type == NEO4J_NULL || type == NEO4J_BOOL || type == NEO4J_INT ||
            type == NEO4J_FLOAT)
    {
        if (fputs(*buf, stream) == EOF)
        {
            return -1;
        }
        return 0;
    }
    return write_csv_quoted_string(stream, *buf, strlen(*buf));
}


int write_unprintable(FILE *stream, int codepoint, int width)
{
    assert(codepoint >= 0);
    char buf[10];
    char *replacement;
    unsigned int n = 2;
    switch (codepoint)
    {
    case '\a':
        replacement = "\\a";
        break;
    case '\b':
        replacement = "\\b";
        break;
    case '\f':
        replacement = "\\f";
        break;
    case '\n':
        replacement = "\\n";
        break;
    case '\r':
        replacement = "\\r";
        break;
    case '\t':
        replacement = "\\t";
        break;
    case '\v':
        replacement = "\\v";
        break;
    default:
        replacement = buf;
        if (codepoint <= 0xFFFF)
        {
            if (snprintf(buf, sizeof(buf), "\\u%04X", codepoint) < 0)
            {
                return -1;
            }
            n = 6;
        }
        else
        {
            if (snprintf(buf, sizeof(buf), "\\U%08X", codepoint) < 0)
            {
                return -1;
            }
            n = 10;
        }
        break;
    }
    unsigned int towrite = min(n, width);
    if (fwrite(replacement, sizeof(char), towrite, stream) < towrite)
    {
        return -1;
    }
    return n;
}
