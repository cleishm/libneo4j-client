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
#include "serialization.h"
#include "util.h"
#include "values.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>


struct markers
{
    uint8_t m4;
    uint8_t m8;
    uint8_t m16;
    uint8_t m32;
    uint8_t m64;
};

static struct markers int_markers = {0x00, 0xC8, 0xC9, 0xCA, 0xCB};
static struct markers string_markers = {0x80, 0xD0, 0xD1, 0xD2, 0x00};
static struct markers list_markers = {0x90, 0xD4, 0xD5, 0xD6, 0x00};
static struct markers map_markers = {0xA0, 0xD8, 0xD9, 0xDA, 0x00};
static struct markers structure_markers = {0xB0, 0xDC, 0xDD, 0x00, 0x00};


/* NOTE: do not serialize entire header, as it is (unfortunately) not
 * aligned correctly - instead write marker and length out separately
 */
struct length_header
{
    uint8_t marker;
    union
    {
        int8_t l8;
        int16_t l16;
        int16_t l32;
    } length;
};


static size_t identifier_str(char *buf, size_t n, const neo4j_value_t *value);
static size_t string_str(char *buf, size_t n, char quot, const char *s,
        size_t len);
static size_t list_str(char *buf, size_t n, const neo4j_value_t *values,
        unsigned int nvalues);
static int build_header(struct iovec *iov, struct length_header *header,
        size_t length, struct markers *markers);


/* null */

size_t neo4j_null_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_NULL);
    int r = snprintf(buf, n, "null");
    assert(r == 4);
    return (size_t)r;
}


int neo4j_null_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(stream != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_NULL);
    uint8_t marker = 0xC0;
    return neo4j_ios_write_all(stream, &marker, 1, NULL);
}


/* boolean */

size_t neo4j_bool_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_BOOL);
    const struct neo4j_bool *v = (const struct neo4j_bool *)value;
    int r = snprintf(buf, n, (v->value > 0) ? "true" : "false");
    assert(r == 4 || r == 5);
    return (size_t)r;
}


int neo4j_bool_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(stream != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_BOOL);
    const struct neo4j_bool *v = (const struct neo4j_bool *)value;

    uint8_t marker = (v->value > 0) ? 0xC3 : 0xC2;
    return neo4j_ios_write_all(stream, &marker, 1, NULL);
}


/* integer */

size_t neo4j_int_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_INT);
    const struct neo4j_int *v = (const struct neo4j_int *)value;
    int r = snprintf(buf, n, "%" PRId64, v->value);
    assert(r > 0);
    return (size_t)r;
}


int neo4j_int_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(stream != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_INT);
    const struct neo4j_int *v = (const struct neo4j_int *)value;

    uint8_t marker;
    union
    {
        int8_t v8;
        int16_t v16;
        int32_t v32;
        int64_t v64;
    } data;
    int datalen;

    if (v->value >= -(1<<4) && v->value < (1<<7))
    {
        marker = v->value;
        datalen = 0;
    }
    else if (v->value >= INT8_MIN && v->value <= INT8_MAX)
    {
        marker = int_markers.m8;
        data.v8 = v->value;
        datalen = 1;
    }
    else if (v->value >= INT16_MIN && v->value <= INT16_MAX)
    {
        marker = int_markers.m16;
        data.v16 = htons(v->value);
        datalen = 2;
    }
    else if (v->value >= INT32_MIN && v->value <= INT32_MAX)
    {
        marker = int_markers.m32;
        data.v32 = htonl(v->value);
        datalen = 4;
    }
    else
    {
        marker = int_markers.m64;
        data.v64 = htobe64(v->value);
        datalen = 8;
    }

    struct iovec iov[2];
    iov[0].iov_base = &marker;
    iov[0].iov_len = 1;
    iov[1].iov_base = &data;
    iov[1].iov_len = datalen;

    return neo4j_ios_writev_all(stream, iov, 2, NULL);
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


int neo4j_float_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(stream != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_FLOAT);
    const struct neo4j_float *v = (const struct neo4j_float *)value;

    uint8_t marker = 0xC1;
    struct iovec iov[2];
    iov[0].iov_base = &marker;
    iov[0].iov_len = 1;

    union
    {
        uint64_t data;
        double value;
    } double_data;

    double_data.value = v->value;
    double_data.data = htobe64(double_data.data);
    iov[1].iov_base = &(double_data.data);
    iov[1].iov_len = 8;
    return neo4j_ios_writev_all(stream, iov, 2, NULL);
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


size_t identifier_str(char *buf, size_t n, const neo4j_value_t *value)
{
    assert(neo4j_type(*value) == NEO4J_STRING);
    const struct neo4j_string *v = (const struct neo4j_string *)value;
    const char *s = (const char *)v->ustring;

    if (memcspn_ident(s, v->length) < v->length)
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


int neo4j_string_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream)
{
    REQUIRE(value, -1);
    REQUIRE(stream, -1);
    assert(neo4j_type(*value) == NEO4J_STRING);
    const struct neo4j_string *v = (const struct neo4j_string *)value;

    struct iovec iov[3];
    struct length_header header;
    int iovcnt = build_header(iov, &header, v->length, &string_markers);
    iov[iovcnt].iov_base = (void *)(uintptr_t)(v->ustring);
    iov[iovcnt].iov_len = v->length;
    iovcnt++;

    return neo4j_ios_writev_all(stream, iov, iovcnt, NULL);
}


/* list */

size_t neo4j_list_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_LIST);
    const struct neo4j_list *v = (const struct neo4j_list *)value;

    size_t l = 0;
    if ((l+1) < n)
    {
        buf[l] = '[';
    }
    l++;

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


int neo4j_list_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(stream != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_LIST);
    const struct neo4j_list *v = (const struct neo4j_list *)value;
    REQUIRE(v->length == 0 || v->items != NULL, -1);

    struct iovec iov[2];
    struct length_header header;
    int iovcnt = build_header(iov, &header, v->length, &list_markers);

    if (neo4j_ios_writev_all(stream, iov, iovcnt, NULL))
    {
        return -1;
    }

    for (unsigned i = 0; i < v->length; ++i)
    {
        if (neo4j_serialize(v->items[i], stream))
        {
            return -1;
        }
    }
    return 0;
}


/* map */

size_t neo4j_map_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_MAP);
    const struct neo4j_map *v = (const struct neo4j_map *)value;

    size_t l = 0;
    if ((l+1) < n)
    {
        buf[l] = '{';
    }
    l++;

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


int neo4j_map_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(stream != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_MAP);
    const struct neo4j_map *v = (const struct neo4j_map *)value;
    REQUIRE(v->nentries == 0 || v->entries != NULL, -1);

    struct iovec iov[2];
    struct length_header header;
    int iovcnt = build_header(iov, &header, v->nentries, &map_markers);

    if (neo4j_ios_writev_all(stream, iov, iovcnt, NULL))
    {
        return -1;
    }

    for (unsigned i = 0; i < v->nentries; ++i)
    {
        const neo4j_map_entry_t *entry = v->entries + i;
        if (neo4j_type(entry->key) != NEO4J_STRING)
        {
            errno = NEO4J_INVALID_MAP_KEY_TYPE;
            return -1;
        }
        if (neo4j_serialize(entry->key, stream))
        {
            return -1;
        }
        if (neo4j_serialize(entry->value, stream))
        {
            return -1;
        }
    }
    return 0;
}


/* node */

size_t neo4j_node_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_NODE);
    const struct neo4j_struct *v = (const struct neo4j_struct *)value;
    assert(v->nfields == 3);

    size_t l = 0;
    if ((l+1) < n)
    {
        buf[l] = '(';
    }
    l++;

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
    l += neo4j_map_str(&(v->fields[2]), buf+l, (l < n)? n-l : 0);

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


/* relationship */

size_t neo4j_rel_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_RELATIONSHIP);
    const struct neo4j_struct *v = (const struct neo4j_struct *)value;
    assert(v->nfields == 5 || v->nfields == 3);

    size_t l = 0;
    if ((l+1) < n)
    {
        buf[l] = '[';
    }
    l++;

    int idx = (v->nfields == 5)? 3 : 1;
    assert(neo4j_type(v->fields[idx]) == NEO4J_STRING);

    if ((l+1) < n)
    {
        buf[l] = ':';
    }
    l++;
    l += identifier_str(buf+l, (l < n)? n-l : 0, &(v->fields[idx]));

    assert(neo4j_type(v->fields[idx+1]) == NEO4J_MAP);
    l += neo4j_map_str(&(v->fields[idx+1]), buf+l, (l < n)? n-l : 0);

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
        if ((l+1) < n)
        {
            buf[l] = '-';
        }
        l++;

        l += neo4j_rel_str(&(rels->items[ridx]), buf+l, (l < n)? n-l : 0);

        if ((l+1) < n)
        {
            buf[l] = '-';
        }
        l++;
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


int neo4j_struct_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(stream != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_STRUCT ||
            neo4j_type(*value) == NEO4J_NODE);
    const struct neo4j_struct *v = (const struct neo4j_struct *)value;
    REQUIRE(v->nfields == 0 || v->fields != NULL, -1);

    struct iovec iov[3];
    struct length_header header;
    int iovcnt = build_header(iov, &header, v->nfields, &structure_markers);
    iov[iovcnt].iov_base = (void *)(uintptr_t)(&(v->signature));
    iov[iovcnt].iov_len = 1;
    iovcnt++;

    if (neo4j_ios_writev_all(stream, iov, iovcnt, NULL))
    {
        return -1;
    }

    for (int i = 0; i < v->nfields; ++i)
    {
        if (neo4j_serialize(v->fields[i], stream))
        {
            return -1;
        }
    }
    return 0;
}


int build_header(struct iovec *iov, struct length_header *header,
        size_t length, struct markers *markers)
{
    int lengthBytes;

    if ((length >> 4) == 0)
    {
        header->marker = markers->m4 + length;
        lengthBytes = 0;
    }
    else if ((length >> 8) == 0)
    {
        header->marker = markers->m8;
        header->length.l8 = length;
        lengthBytes = 1;
    }
    else if ((length >> 16) == 0)
    {
        header->marker = markers->m16;
        header->length.l16 = htons(length);
        lengthBytes = 2;
    }
    else
    {
        header->marker = markers->m32;
        header->length.l32 = htonl(length);
        lengthBytes = 4;
    }

    iov[0].iov_base = &(header->marker);
    iov[0].iov_len = 1;
    int iovcnt = 1;
    if (lengthBytes > 0)
    {
        iov[iovcnt].iov_base = &(header->length);
        iov[iovcnt].iov_len = lengthBytes;
        iovcnt++;
    }
    return iovcnt;
}
