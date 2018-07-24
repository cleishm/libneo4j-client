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
#include "print.h"
#include "util.h"
#include "timegm.h"
#include "values.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>


static size_t identifier_str(char *buf, size_t n, const neo4j_value_t *value);
static ssize_t identifier_fprint(const neo4j_value_t *value, FILE *stream);
static size_t string_str(char *buf, size_t n, char quot, const char *s,
        size_t len);
static ssize_t string_fprint(FILE *stream, char quot, const char *s,
        size_t len);
static size_t list_str(char *buf, size_t n, const neo4j_value_t *values,
        unsigned int nvalues);
static ssize_t list_fprint(const neo4j_value_t *values, unsigned int nvalues,
        FILE *stream);
static size_t format_double(char *buf, size_t n, double dbl);
static size_t format_nanoseconds(char *buf, size_t n, int nanoseconds);
static ssize_t format_offset(char *buf, size_t n, int offset);


/* null */

size_t neo4j_null_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_NULL);
    if (n > 0)
    {
        if (n > 5)
        {
            n = 5;
        }
        memcpy(buf, "null", n-1);
        buf[n-1] = '\0';
    }
    return 4;
}


ssize_t neo4j_null_fprint(const neo4j_value_t *value, FILE *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_NULL);
    return (fputs("null", stream) == EOF)? -1 : 4;
}


/* boolean */

size_t neo4j_bool_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_BOOL);
    const struct neo4j_bool *v = (const struct neo4j_bool *)value;
    if (v->value > 0)
    {
        if (n > 0)
        {
            if (n > 5)
            {
                n = 5;
            }
            memcpy(buf, "true", n-1);
            buf[n-1] = '\0';
        }
        return 4;
    }
    else
    {
        if (n > 0)
        {
            if (n > 6)
            {
                n = 6;
            }
            memcpy(buf, "false", n-1);
            buf[n-1] = '\0';
        }
        return 5;
    }
}


ssize_t neo4j_bool_fprint(const neo4j_value_t *value, FILE *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_BOOL);
    const struct neo4j_bool *v = (const struct neo4j_bool *)value;
    return (fputs((v->value > 0)? "true" : "false", stream) == EOF)?
        -1 : (v->value > 0)? 4 : 5;
}


/* integer */

size_t neo4j_int_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_INT ||
            neo4j_type(*value) == NEO4J_IDENTITY);
    const struct neo4j_int *v = (const struct neo4j_int *)value;
    int r = snprintf(buf, n, "%" PRId64, v->value);
    assert(r > 0);
    return (size_t)r;
}


ssize_t neo4j_int_fprint(const neo4j_value_t *value, FILE *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_INT ||
            neo4j_type(*value) == NEO4J_IDENTITY);
    const struct neo4j_int *v = (const struct neo4j_int *)value;
    return fprintf(stream, "%" PRId64, v->value);
}


/* float */

size_t neo4j_float_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_FLOAT);
    const struct neo4j_float *v = (const struct neo4j_float *)value;
    int r = snprintf(buf, n, "%f", v->value);
    assert(r > 0);
    return (size_t)r;
}


ssize_t neo4j_float_fprint(const neo4j_value_t *value, FILE *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_FLOAT);
    const struct neo4j_float *v = (const struct neo4j_float *)value;
    return fprintf(stream, "%f", v->value);
}


/* string */

size_t neo4j_string_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_STRING);
    const struct neo4j_string *v = (const struct neo4j_string *)value;
    return string_str(buf, n, '"', (const char *)v->ustring, v->length);
}


ssize_t neo4j_string_fprint(const neo4j_value_t *value, FILE *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_STRING);
    const struct neo4j_string *v = (const struct neo4j_string *)value;
    return string_fprint(stream, '"', (const char *)v->ustring, v->length);
}


size_t identifier_str(char *buf, size_t n, const neo4j_value_t *value)
{
    assert(neo4j_type(*value) == NEO4J_STRING);
    const struct neo4j_string *v = (const struct neo4j_string *)value;
    const char *s = (const char *)v->ustring;

    if (memspn_ident(s, v->length) < v->length)
    {
        return string_str(buf, n, '`', s, v->length);
    }

    if (n > 0)
    {
        size_t l = minzu(n-1, v->length);
        memcpy(buf, s, l);
        buf[l] = '\0';
    }
    return v->length;
}


ssize_t identifier_fprint(const neo4j_value_t *value, FILE *stream)
{
    assert(neo4j_type(*value) == NEO4J_STRING);
    const struct neo4j_string *v = (const struct neo4j_string *)value;
    const char *s = (const char *)v->ustring;

    if (memspn_ident(s, v->length) < v->length)
    {
        return string_fprint(stream, '`', s, v->length);
    }

    if (fwrite(s, sizeof(char), v->length, stream) < v->length)
    {
        return -1;
    }
    return v->length;
}


size_t string_str(char *buf, size_t n, char quot, const char *s, size_t len)
{
    const unsigned char esc[2] = { quot, '\\' };

    if (n > 0)
    {
        buf[0] = quot;
    }

    size_t l = 1;
    const char *end = s + len;
    while (s < end)
    {
        size_t i = memcspn(s, end - s, esc, 2);
        if ((l+1) < n)
        {
            memcpy(buf+l, s, minzu(n-l-1, i));
        }
        s += i;
        l += i;

        if (s >= end)
        {
            assert(s == end);
            break;
        }

        if ((l+2) < n)
        {
            buf[l] = '\\';
            buf[l+1] = *s;
        }
        else if ((l+1) < n)
        {
            buf[l] = '\0';
        }
        l += 2;
        ++s;
    }

    if ((l+1) < n)
    {
        buf[l] = quot;
    }
    l++;
    if (n > 0)
    {
        buf[minzu(n - 1, l)] = '\0';
    }
    return l;
}


ssize_t string_fprint(FILE *stream, char quot, const char *s, size_t len)
{
    const unsigned char esc[2] = { quot, '\\' };

    if (fputc(quot, stream) == EOF)
    {
        return -1;
    }

    size_t l = 1;
    const char *end = s + len;
    while (s < end)
    {
        size_t i = memcspn(s, end - s, esc, 2);
        if (fwrite(s, sizeof(unsigned char), i, stream) < i)
        {
            return -1;
        }
        s += i;
        l += i;

        if (s >= end)
        {
            assert(s == end);
            break;
        }

        if (fputc('\\', stream) == EOF || fputc(*s, stream) == EOF)
        {
            return -1;
        }
        l += 2;
        ++s;
    }

    if (fputc(quot, stream) == EOF)
    {
        return -1;
    }
    return ++l;
}


/* bytes */

size_t neo4j_bytes_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_BYTES);
    const struct neo4j_bytes *v = (const struct neo4j_bytes *)value;

    if (n > 0)
    {
        buf[0] = '#';
    }
    size_t l = 1;

    for (unsigned int i = 0; i < v->length; ++i)
    {
        int r = snprintf(buf + l, (l < n)? n-l : 0, "%02x", v->bytes[i]);
        if (r < 0)
        {
            return -1;
        }
        l += r;
    }

    if (n > 0)
    {
        buf[minzu(n - 1, l)] = '\0';
    }
    return l;
}


ssize_t neo4j_bytes_fprint(const neo4j_value_t *value, FILE *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_BYTES);
    const struct neo4j_bytes *v = (const struct neo4j_bytes *)value;

    if (fputc('#', stream) == EOF)
    {
        return -1;
    }

    ssize_t l = 1;
    for (unsigned int i = 0; i < v->length; ++i)
    {
        int r = fprintf(stream, "%02x", v->bytes[i]);
        if (r < 0)
        {
            return -1;
        }
        l += r;
    }

    return l;
}


/* list */

size_t neo4j_list_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_LIST);
    const struct neo4j_list *v = (const struct neo4j_list *)value;

    if (n > 0)
    {
        buf[0] = '[';
    }
    size_t l = 1;

    l += list_str(buf+l, (l < n)? n-l : 0, v->items, v->length);

    if ((l+1) < n)
    {
        buf[l] = ']';
    }
    l++;
    if (n > 0)
    {
        buf[minzu(n - 1, l)] = '\0';
    }
    return l;
}


ssize_t neo4j_list_fprint(const neo4j_value_t *value, FILE *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_LIST);
    const struct neo4j_list *v = (const struct neo4j_list *)value;

    if (fputc('[', stream) == EOF)
    {
        return -1;
    }

    ssize_t l = list_fprint(v->items, v->length, stream);
    if (l < 0)
    {
        return -1;
    }
    l++;

    if (fputc(']', stream) == EOF)
    {
        return -1;
    }
    return ++l;
}


size_t list_str(char *buf, size_t n, const neo4j_value_t *values,
        unsigned int nvalues)
{
    size_t l = 0;
    for (unsigned int i = 0; i < nvalues; ++i)
    {
        l += neo4j_ntostring(values[i], buf+l, (l < n)? n-l : 0);

        if ((i+1) < nvalues)
        {
            if ((l+1) < n)
            {
                buf[l] = ',';
            }
            l++;
        }
    }
    return l;
}


ssize_t list_fprint(const neo4j_value_t *values, unsigned int nvalues,
        FILE *stream)
{
    size_t l = 0;
    for (unsigned int i = 0; i < nvalues; ++i)
    {
        ssize_t ll = neo4j_fprint(values[i], stream);
        if (ll < 0)
        {
            return -1;
        }
        l += (size_t)ll;

        if ((i+1) < nvalues)
        {
            if (fputc(',', stream) == EOF)
            {
                return -1;
            }
            l++;
        }
    }
    return l;
}


/* map */

size_t neo4j_map_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_MAP);
    const struct neo4j_map *v = (const struct neo4j_map *)value;

    if (n > 0)
    {
        buf[0] = '{';
    }
    size_t l = 1;

    for (unsigned int i = 0; i < v->nentries; ++i)
    {
        const neo4j_map_entry_t *entry = v->entries + i;
        assert(neo4j_type(entry->key) == NEO4J_STRING);
        l += identifier_str(buf+l, (l < n)? n-l : 0, &(entry->key));

        if ((l+1) < n)
        {
            buf[l] = ':';
        }
        l++;

        l += neo4j_ntostring(entry->value, buf+l, (l < n)? n-l : 0);

        if ((i+1) < v->nentries)
        {
            if ((l+1) < n)
            {
                buf[l] = ',';
            }
            l++;
        }
    }

    if ((l+1) < n)
    {
        buf[l] = '}';
    }
    l++;
    if (n > 0)
    {
        buf[minzu(n - 1, l)] = '\0';
    }
    return l;
}


ssize_t neo4j_map_fprint(const neo4j_value_t *value, FILE *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_MAP);
    const struct neo4j_map *v = (const struct neo4j_map *)value;

    if (fputc('{', stream) == EOF)
    {
        return -1;
    }
    size_t l = 1;

    for (unsigned int i = 0; i < v->nentries; ++i)
    {
        const neo4j_map_entry_t *entry = v->entries + i;
        assert(neo4j_type(entry->key) == NEO4J_STRING);
        ssize_t ll = identifier_fprint(&(entry->key), stream);
        if (ll < 0)
        {
            return -1;
        }
        l += (size_t)ll;

        if (fputc(':', stream) == EOF)
        {
            return -1;
        }
        l++;

        ll = neo4j_fprint(entry->value, stream);
        if (ll < 0)
        {
            return -1;
        }
        l += (size_t)ll;

        if ((i+1) < v->nentries)
        {
            if (fputc(',', stream) == EOF)
            {
                return -1;
            }
            l++;
        }
    }

    if (fputc('}', stream) == EOF)
    {
        return -1;
    }
    return ++l;
}


/* node */

size_t neo4j_node_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_NODE);
    const struct neo4j_struct *v = (const struct neo4j_struct *)value;
    assert(v->nfields == 3);

    if (n > 0)
    {
        buf[0] = '(';
    }
    size_t l = 1;

    assert(neo4j_type(v->fields[1]) == NEO4J_LIST);
    const struct neo4j_list *labels =
        (const struct neo4j_list *)&(v->fields[1]);

    for (unsigned int i = 0; i < labels->length; ++i)
    {
        const neo4j_value_t *label = labels->items + i;
        assert(neo4j_type(*label) == NEO4J_STRING);
        if ((l+1) < n)
        {
            buf[l] = ':';
        }
        l++;
        l += identifier_str(buf+l, (l < n)? n-l : 0, label);
    }

    assert(neo4j_type(v->fields[2]) == NEO4J_MAP);
    if (neo4j_map_size(v->fields[2]) > 0)
    {
        l += neo4j_map_str(&(v->fields[2]), buf+l, (l < n)? n-l : 0);
    }

    if ((l+1) < n)
    {
        buf[l] = ')';
    }
    l++;
    if (n > 0)
    {
        buf[minzu(n - 1, l)] = '\0';
    }
    return l;
}


ssize_t neo4j_node_fprint(const neo4j_value_t *value, FILE *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_NODE);
    const struct neo4j_struct *v = (const struct neo4j_struct *)value;
    assert(v->nfields == 3);

    if (fputc('(', stream) == EOF)
    {
        return -1;
    }
    size_t l = 1;

    assert(neo4j_type(v->fields[1]) == NEO4J_LIST);
    const struct neo4j_list *labels =
        (const struct neo4j_list *)&(v->fields[1]);

    for (unsigned int i = 0; i < labels->length; ++i)
    {
        const neo4j_value_t *label = labels->items + i;
        assert(neo4j_type(*label) == NEO4J_STRING);
        if (fputc(':', stream) == EOF)
        {
            return -1;
        }
        l++;
        ssize_t ll = identifier_fprint(label, stream);
        if (ll < 0)
        {
            return -1;
        }
        l += (size_t)ll;
    }

    assert(neo4j_type(v->fields[2]) == NEO4J_MAP);
    if (neo4j_map_size(v->fields[2]) > 0)
    {
        ssize_t ll = neo4j_map_fprint(&(v->fields[2]), stream);
        if (ll < 0)
        {
            return -1;
        }
        l += (size_t)ll;
    }

    if (fputc(')', stream) == EOF)
    {
        return -1;
    }
    return ++l;
}


/* relationship */

size_t neo4j_rel_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_RELATIONSHIP);
    const struct neo4j_struct *v = (const struct neo4j_struct *)value;
    assert(v->nfields == 5 || v->nfields == 3);

    if (n > 0)
    {
        buf[0] = '-';
        if (n > 1)
        {
            buf[1] = '[';
        }
    }
    size_t l = 2;

    int idx = (v->nfields == 5)? 3 : 1;
    assert(neo4j_type(v->fields[idx]) == NEO4J_STRING);

    if ((l+1) < n)
    {
        buf[l] = ':';
    }
    l++;
    l += identifier_str(buf+l, (l < n)? n-l : 0, &(v->fields[idx]));

    assert(neo4j_type(v->fields[idx+1]) == NEO4J_MAP);
    if (neo4j_map_size(v->fields[idx+1]) > 0)
    {
        l += neo4j_map_str(&(v->fields[idx+1]), buf+l, (l < n)? n-l : 0);
    }

    if ((l+1) < n)
    {
        buf[l] = ']';
        if ((l+2) < n)
        {
            buf[l+1] = '-';
        }
    }
    l+=2;
    if (n > 0)
    {
        buf[minzu(n - 1, l)] = '\0';
    }
    return l;
}


ssize_t neo4j_rel_fprint(const neo4j_value_t *value, FILE *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_RELATIONSHIP);
    const struct neo4j_struct *v = (const struct neo4j_struct *)value;
    assert(v->nfields == 5 || v->nfields == 3);

    if (fputs("-[:", stream) == EOF)
    {
        return -1;
    }
    size_t l = 3;

    int idx = (v->nfields == 5)? 3 : 1;
    assert(neo4j_type(v->fields[idx]) == NEO4J_STRING);

    ssize_t ll = identifier_fprint(&(v->fields[idx]), stream);
    if (ll < 0)
    {
        return -1;
    }
    l += (size_t)ll;

    assert(neo4j_type(v->fields[idx+1]) == NEO4J_MAP);
    if (neo4j_map_size(v->fields[idx+1]) > 0)
    {
        ll = neo4j_map_fprint(&(v->fields[idx+1]), stream);
        if (ll < 0)
        {
            return -1;
        }
        l += (size_t)ll;
    }

    if (fputs("]-", stream) == EOF)
    {
        return -1;
    }
    return l+2;
}


/* path */

size_t neo4j_path_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_PATH);
    const struct neo4j_struct *v = (const struct neo4j_struct *)value;
    assert(v->nfields == 3);

    assert(neo4j_type(v->fields[0]) == NEO4J_LIST);
    const struct neo4j_list *nodes = (const struct neo4j_list *)&(v->fields[0]);
    assert(neo4j_type(v->fields[1]) == NEO4J_LIST);
    const struct neo4j_list *rels = (const struct neo4j_list *)&(v->fields[1]);
    assert(neo4j_type(v->fields[2]) == NEO4J_LIST);
    const struct neo4j_list *seq = (const struct neo4j_list *)&(v->fields[2]);

    assert(nodes->length > 0);
    assert(neo4j_type(nodes->items[0]) == NEO4J_NODE);

    size_t l = neo4j_node_str(&(nodes->items[0]), buf, n);

    assert(seq->length % 2 == 0);
    for (unsigned int i = 0; i < seq->length; i += 2)
    {
        assert(neo4j_type(seq->items[i]) == NEO4J_INT);
        const struct neo4j_int *ridx_val =
            (const struct neo4j_int *)&(seq->items[i]);
        assert(neo4j_type(seq->items[i+1]) == NEO4J_INT);
        const struct neo4j_int *nidx_val =
            (const struct neo4j_int *)&(seq->items[i+1]);

        assert((ridx_val->value > 0 && ridx_val->value <= rels->length) ||
               (ridx_val->value < 0 && -(ridx_val->value) <= rels->length));
        unsigned int ridx = (unsigned int)(llabs(ridx_val->value) - 1);
        assert(neo4j_type(rels->items[ridx]) == NEO4J_RELATIONSHIP);

        assert(nidx_val->value >= 0 && nidx_val->value < nodes->length);
        unsigned int nidx = (unsigned int)nidx_val->value;
        assert(neo4j_type(nodes->items[nidx]) == NEO4J_NODE);

        if (ridx_val->value < 0)
        {
            if ((l+1) < n)
            {
                buf[l] = '<';
            }
            l++;
        }

        l += neo4j_rel_str(&(rels->items[ridx]), buf+l, (l < n)? n-l : 0);

        if (ridx_val->value > 0)
        {
            if ((l+1) < n)
            {
                buf[l] = '>';
            }
            l++;
        }

        l += neo4j_node_str(&(nodes->items[nidx]), buf+l, (l < n)? n-l : 0);
    }

    if (n > 0)
    {
        buf[minzu(n - 1, l)] = '\0';
    }
    return l;
}


ssize_t neo4j_path_fprint(const neo4j_value_t *value, FILE *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_PATH);
    const struct neo4j_struct *v = (const struct neo4j_struct *)value;
    assert(v->nfields == 3);

    assert(neo4j_type(v->fields[0]) == NEO4J_LIST);
    const struct neo4j_list *nodes = (const struct neo4j_list *)&(v->fields[0]);
    assert(neo4j_type(v->fields[1]) == NEO4J_LIST);
    const struct neo4j_list *rels = (const struct neo4j_list *)&(v->fields[1]);
    assert(neo4j_type(v->fields[2]) == NEO4J_LIST);
    const struct neo4j_list *seq = (const struct neo4j_list *)&(v->fields[2]);

    assert(nodes->length > 0);
    assert(neo4j_type(nodes->items[0]) == NEO4J_NODE);

    ssize_t ll = neo4j_node_fprint(&(nodes->items[0]), stream);
    if (ll < 0)
    {
        return -1;
    }
    size_t l = (size_t)ll;

    assert(seq->length % 2 == 0);
    for (unsigned int i = 0; i < seq->length; i += 2)
    {
        assert(neo4j_type(seq->items[i]) == NEO4J_INT);
        const struct neo4j_int *ridx_val =
            (const struct neo4j_int *)&(seq->items[i]);
        assert(neo4j_type(seq->items[i+1]) == NEO4J_INT);
        const struct neo4j_int *nidx_val =
            (const struct neo4j_int *)&(seq->items[i+1]);

        assert((ridx_val->value > 0 && ridx_val->value <= rels->length) ||
               (ridx_val->value < 0 && -(ridx_val->value) <= rels->length));
        unsigned int ridx = (unsigned int)(llabs(ridx_val->value) - 1);
        assert(neo4j_type(rels->items[ridx]) == NEO4J_RELATIONSHIP);

        assert(nidx_val->value >= 0 && nidx_val->value < nodes->length);
        unsigned int nidx = (unsigned int)nidx_val->value;
        assert(neo4j_type(nodes->items[nidx]) == NEO4J_NODE);

        if (ridx_val->value < 0)
        {
            if (fputc('<', stream) == EOF)
            {
                return -1;
            }
            l++;
        }

        ll = neo4j_rel_fprint(&(rels->items[ridx]), stream);
        if (ll < 0)
        {
            return -1;
        }
        l += (size_t)ll;

        if (ridx_val->value > 0)
        {
            if (fputc('>', stream) == EOF)
            {
                return -1;
            }
            l++;
        }

        ll = neo4j_node_fprint(&(nodes->items[nidx]), stream);
        if (ll < 0)
        {
            return -1;
        }
        l += (size_t)ll;
    }
    return l;
}


/* structure */

size_t neo4j_struct_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_STRUCT);
    const struct neo4j_struct *v = (const struct neo4j_struct *)value;

    int hlen = snprintf(buf, n, "struct<0x%X>", v->signature);
    assert(hlen > 10);

    size_t l = (size_t)hlen;
    if ((l+1) < n)
    {
        buf[l] = '(';
    }
    l++;

    l += list_str(buf+l, (l < n)? n-l : 0, v->fields, v->nfields);

    if ((l+1) < n)
    {
        buf[l] = ')';
    }
    l++;
    if (n > 0)
    {
        buf[minzu(n - 1, l)] = '\0';
    }
    return l;
}


ssize_t neo4j_struct_fprint(const neo4j_value_t *value, FILE *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_STRUCT);
    const struct neo4j_struct *v = (const struct neo4j_struct *)value;

    int hlen = fprintf(stream, "struct<0x%X>", v->signature);
    if (hlen < 0)
    {
        return -1;
    }
    assert(hlen > 10);

    if (fputc('(', stream) == EOF)
    {
        return -1;
    }
    size_t l = (size_t)hlen + 1;

    ssize_t ll =  list_fprint(v->fields, v->nfields, stream);
    if (ll < 0)
    {
        return -1;
    }
    l += (size_t)ll;

    if (fputc(')', stream) == EOF)
    {
        return -1;
    }
    return ++l;
}


/* point */

size_t neo4j_point_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_POINT);
    const struct neo4j_point *v = (const struct neo4j_point *)value;

    char x[DBL_MAX_10_EXP];
    char y[DBL_MAX_10_EXP];
    format_double(x, sizeof(x), v->data->x);
    format_double(y, sizeof(y), v->data->y);

    if (v->dimensions == 3)
    {
        char z[DBL_MAX_10_EXP];
        format_double(z, sizeof(z), v->data->z);

        if (v->srid == NEO4J_WGS84_3D)
        {
            return snprintf(buf, n,
                    "point({latitude:%s,longitude:%s,height:%s})", y, x, z);
        }
        else if (v->srid == NEO4J_CARTESIAN_3D)
        {
            return snprintf(buf, n, "point({x:%s,y:%s,z:%s})", x, y, z);
        }
        else
        {
            return snprintf(buf, n, "point({x:%s,y:%s,z:%s,srid:%d})", x, y, z,
                    v->srid);
        }
    }
    else
    {
        assert(v->dimensions == 2);
        if (v->srid == NEO4J_WGS84)
        {
            return snprintf(buf, n, "point({latitude:%s,longitude:%s})", y, x);
        }
        else if (v->srid == NEO4J_CARTESIAN)
        {
            return snprintf(buf, n, "point({x:%s,y:%s})", x, y);
        }
        else
        {
            return snprintf(buf, n, "point({x:%s,y:%s,srid:%d})", x, y,
                    v->srid);
        }
    }
}


ssize_t neo4j_point_fprint(const neo4j_value_t *value, FILE *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_POINT);
    const struct neo4j_point *v = (const struct neo4j_point *)value;

    char x[DBL_MAX_10_EXP];
    char y[DBL_MAX_10_EXP];
    format_double(x, sizeof(x), v->data->x);
    format_double(y, sizeof(y), v->data->y);

    if (v->dimensions == 3)
    {
        char z[DBL_MAX_10_EXP];
        format_double(z, sizeof(z), v->data->z);

        if (v->srid == NEO4J_WGS84_3D)
        {
            return fprintf(stream,
                    "point({latitude:%s,longitude:%s,height:%s})", y, x, z);
        }
        else if (v->srid == NEO4J_CARTESIAN_3D)
        {
            return fprintf(stream, "point({x:%s,y:%s,z:%s})", y, x, z);
        }
        else
        {
            return fprintf(stream, "point({x:%s,y:%s,z:%s,srid:%d})",
                    y, x, z, v->srid);
        }
    }
    else
    {
        assert(v->dimensions == 2);
        if (v->srid == NEO4J_WGS84)
        {
            return fprintf(stream, "point({latitude:%s,longitude:%s})", y, x);
        }
        else if (v->srid == NEO4J_CARTESIAN)
        {
            return fprintf(stream, "point({x:%s,y:%s})", y, x);
        }
        else
        {
            return fprintf(stream, "point({x:%s,y:%s,srid:%d})", y, x, v->srid);
        }
    }
}


size_t format_double(char *buf, size_t n, double dbl)
{
    size_t l = snprintf(buf, n, "%f", dbl);
    if (l >= n)
    {
        l = strlen(buf);
    }
    while (l > 0 && buf[l-1] == '0')
    {
        buf[l-1] = '\0';
        --l;
    }
    if (l > 0 && !isdigit(buf[l-1]))
    {
        buf[l-1] = '\0';
        --l;
    }
    return l;
}


/* local datetime */

size_t neo4j_local_datetime_str(const neo4j_value_t * restrict value,
        char * restrict buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_LOCAL_DATETIME);
    const struct neo4j_local_datetime *v =
            (const struct neo4j_local_datetime *)value;

    if (v->nanoseconds > 999999999)
    {
        return snprintf(buf, n, "<invalid date nsec %d>", v->nanoseconds);
    }

    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    if (neo4j_epoch_secs_to_tm(v->epoch_seconds, &tm))
    {
        return snprintf(buf, n, "<invalid date sec %lld>",
                (const long long) v->epoch_seconds);
    }

    static const char format[] = "%m-%dT%H:%M:%S";
    char mdHMS_part[sizeof(format) + 1];
    if (strftime(mdHMS_part, sizeof(mdHMS_part), format, &tm) == 0)
    {
        return snprintf(buf, n, "<invalid date sec %lld>",
                (const long long) v->epoch_seconds);
    }

    char nano_part[11];
    format_nanoseconds(nano_part, sizeof(nano_part), v->nanoseconds);

    return snprintf(buf, n, "%d-%s%s", tm.tm_year+1900, mdHMS_part, nano_part);
}


ssize_t neo4j_local_datetime_fprint(const neo4j_value_t *value, FILE *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_LOCAL_DATETIME);
    const struct neo4j_local_datetime *v =
            (const struct neo4j_local_datetime *)value;

    if (v->nanoseconds > 999999999)
    {
        return fprintf(stream, "<invalid date nsec %d>", v->nanoseconds);
    }

    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    if (neo4j_epoch_secs_to_tm(v->epoch_seconds, &tm))
    {
        return fprintf(stream, "<invalid date sec %lld>",
                (const long long) v->epoch_seconds);
    }

    static const char format[] = "%m-%dT%H:%M:%S";
    char mdHMS_part[sizeof(format) + 1];
    if (strftime(mdHMS_part, sizeof(mdHMS_part), format, &tm) == 0)
    {
        return fprintf(stream, "<invalid date sec %lld>",
                (const long long) v->epoch_seconds);
    }

    char nano_part[11];
    format_nanoseconds(nano_part, sizeof(nano_part), v->nanoseconds);

    return fprintf(stream, "%d-%s%s", tm.tm_year+1900, mdHMS_part, nano_part);
}


/* offset datetime */

size_t neo4j_offset_datetime_str(const neo4j_value_t * restrict value,
        char * restrict buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_OFFSET_DATETIME);
    const struct neo4j_offset_datetime *v =
            (const struct neo4j_offset_datetime *)value;

    int nanoseconds = v->nanoseconds;
    int offset = v->offset;

    if (nanoseconds & (1<<31))
    {
        nanoseconds &= ~(1<<31);
        offset = -offset;
    }

    if (nanoseconds > 999999999)
    {
        return snprintf(buf, n, "<invalid date nsec %d>", nanoseconds);
    }

    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    if (neo4j_epoch_secs_to_tm(v->epoch_seconds, &tm))
    {
        return snprintf(buf, n, "<invalid date sec %lld>",
                (const long long) v->epoch_seconds);
    }

    static const char format[] = "%m-%dT%H:%M:%S";
    char mdHMS_part[sizeof(format) + 1];
    if (strftime(mdHMS_part, sizeof(mdHMS_part), format, &tm) == 0)
    {
        return snprintf(buf, n, "<invalid date sec %lld>",
                (const long long) v->epoch_seconds);
    }

    char nano_part[11];
    format_nanoseconds(nano_part, sizeof(nano_part), nanoseconds);

    char offset_part[10];
    if (format_offset(offset_part, sizeof(offset_part), offset) < 0)
    {
        return snprintf(buf, n, "<invalid date offset %d>", offset);
    }

    return snprintf(buf, n, "%d-%s%s%s", tm.tm_year+1900, mdHMS_part,
            nano_part, offset_part);
}


ssize_t neo4j_offset_datetime_fprint(const neo4j_value_t *value, FILE *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_OFFSET_DATETIME);
    const struct neo4j_offset_datetime *v =
            (const struct neo4j_offset_datetime *)value;

    int nanoseconds = v->nanoseconds;
    int offset = v->offset;

    if (nanoseconds & (1<<31))
    {
        nanoseconds &= ~(1<<31);
        offset = -offset;
    }

    if (nanoseconds > 999999999)
    {
        return fprintf(stream, "<invalid date nsec %d>", nanoseconds);
    }

    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    if (neo4j_epoch_secs_to_tm(v->epoch_seconds, &tm))
    {
        return fprintf(stream, "<invalid date sec %lld>",
                (const long long) v->epoch_seconds);
    }

    static const char format[] = "%m-%dT%H:%M:%S";
    char mdHMS_part[sizeof(format) + 1];
    if (strftime(mdHMS_part, sizeof(mdHMS_part), format, &tm) == 0)
    {
        return fprintf(stream, "<invalid date sec %lld>",
                (const long long) v->epoch_seconds);
    }

    char nano_part[11];
    format_nanoseconds(nano_part, sizeof(nano_part), nanoseconds);

    char offset_part[10];
    if (format_offset(offset_part, sizeof(offset_part), offset) < 0)
    {
        return fprintf(stream, "<invalid date offset %d>", offset);
    }

    return fprintf(stream, "%d-%s%s%s", tm.tm_year+1900, mdHMS_part, nano_part,
            offset_part);
}


/* zoned datetime */

size_t neo4j_zoned_datetime_str(const neo4j_value_t * restrict value,
        char * restrict buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_ZONED_DATETIME);
    const struct neo4j_zoned_datetime *v =
            (const struct neo4j_zoned_datetime *)value;
    assert(v->data != NULL);

    if (v->nanoseconds > 999999999)
    {
        return snprintf(buf, n, "<invalid date nsec %d>", v->nanoseconds);
    }

    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    if (neo4j_epoch_secs_to_tm(v->data->epoch_seconds, &tm))
    {
        return snprintf(buf, n, "<invalid date sec %lld>",
                (const long long) v->data->epoch_seconds);
    }

    static const char format[] = "%m-%dT%H:%M:%S";
    char mdHMS_part[sizeof(format) + 1];
    if (strftime(mdHMS_part, sizeof(mdHMS_part), format, &tm) == 0)
    {
        return snprintf(buf, n, "<invalid date sec %lld>",
                (const long long) v->data->epoch_seconds);
    }

    char nano_part[11];
    format_nanoseconds(nano_part, sizeof(nano_part), v->nanoseconds);

    return snprintf(buf, n, "%d-%s%s[%s]", tm.tm_year+1900, mdHMS_part,
            nano_part, v->data->zoneid);
}


ssize_t neo4j_zoned_datetime_fprint(const neo4j_value_t *value, FILE *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_ZONED_DATETIME);
    const struct neo4j_zoned_datetime *v =
            (const struct neo4j_zoned_datetime *)value;
    assert(v->data != NULL);

    if (v->nanoseconds > 999999999)
    {
        return fprintf(stream, "<invalid date nsec %d>", v->nanoseconds);
    }

    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    if (neo4j_epoch_secs_to_tm(v->data->epoch_seconds, &tm))
    {
        return fprintf(stream, "<invalid date sec %lld>",
                (const long long) v->data->epoch_seconds);
    }

    static const char format[] = "%m-%dT%H:%M:%S";
    char mdHMS_part[sizeof(format) + 1];
    if (strftime(mdHMS_part, sizeof(mdHMS_part), format, &tm) == 0)
    {
        return fprintf(stream, "<invalid date sec %lld>",
                (const long long) v->data->epoch_seconds);
    }

    char nano_part[11];
    format_nanoseconds(nano_part, sizeof(nano_part), v->nanoseconds);

    return fprintf(stream, "%d-%s%s[%s]", tm.tm_year+1900, mdHMS_part,
            nano_part, v->data->zoneid);
}


/* local date */

size_t neo4j_local_date_str(const neo4j_value_t * restrict value,
        char * restrict buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_LOCAL_DATE);
    const struct neo4j_local_date *v = (const struct neo4j_local_date *)value;

    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    if (neo4j_epoch_secs_to_tm(v->epoch_days * SEC_IN_DAY, &tm))
    {
        return snprintf(buf, n, "<invalid date days %lld>",
                (const long long) v->epoch_days);
    }

    static const char format[] = "%m-%d";
    char md_part[sizeof(format) + 1];
    if (strftime(md_part, sizeof(md_part), format, &tm) == 0)
    {
        return snprintf(buf, n, "<invalid date days %lld>",
                (const long long) v->epoch_days);
    }

    return snprintf(buf, n, "%d-%s", tm.tm_year+1900, md_part);
}


ssize_t neo4j_local_date_fprint(const neo4j_value_t *value, FILE *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_LOCAL_DATE);
    const struct neo4j_local_date *v = (const struct neo4j_local_date *)value;

    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    if (neo4j_epoch_secs_to_tm(v->epoch_days * SEC_IN_DAY, &tm))
    {
        return fprintf(stream, "<invalid date days %lld>",
                (const long long) v->epoch_days);
    }

    static const char format[] = "%m-%d";
    char md_part[sizeof(format) + 1];
    if (strftime(md_part, sizeof(md_part), format, &tm) == 0)
    {
        return fprintf(stream, "<invalid date days %lld>",
                (const long long) v->epoch_days);
    }

    return fprintf(stream, "%d-%s", tm.tm_year+1900, md_part);
}


size_t format_nanoseconds(char *buf, size_t n, int nanoseconds)
{
    assert(n >= 11);
    size_t l = snprintf(buf, n, ".%09d", nanoseconds);
    if (l >= n)
    {
        l = strlen(buf);
    }
    while (l > 0 && buf[l-1] == '0')
    {
        buf[l-1] = '\0';
        --l;
    }
    if (l == 1)
    {
        buf[0] = '\0';
        return 0;
    }
    return l;
}


ssize_t format_offset(char *buf, size_t n, int offset)
{
    int sec = (offset < 0)? -offset : offset;
    int min = sec / 60;
    sec = sec % 60;
    int hour = min / 60;
    min = min % 60;

    if (hour > 18)
    {
        return -1;
    }

    if (sec != 0)
    {
        return snprintf(buf, n, "%c%02d:%02d:%02d", (offset < 0)? '-' : '+',
                hour, min, sec);
    }
    else
    {
        return snprintf(buf, n, "%c%02d:%02d", (offset < 0)? '-' : '+',
                hour, min);
    }
}
