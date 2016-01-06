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
static ssize_t list_str(char *buf, size_t n, const neo4j_value_t *values,
        unsigned int nvalues);
static int build_header(struct iovec *iov, struct length_header *header,
        size_t length, struct markers *markers);


/* null */

ssize_t neo4j_null_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_NULL);
    return snprintf(buf, n, "null");
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

ssize_t neo4j_bool_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_BOOL);
    const struct neo4j_bool *v = (const struct neo4j_bool *)value;
    return snprintf(buf, n, (v->value > 0) ? "true" : "false");
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

ssize_t neo4j_int_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_INT);
    const struct neo4j_int *v = (const struct neo4j_int *)value;
    return snprintf(buf, n, "%" PRId64, v->value);
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

ssize_t neo4j_float_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_FLOAT);
    const struct neo4j_float *v = (const struct neo4j_float *)value;
    return snprintf(buf, n, "%f", v->value);
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

ssize_t neo4j_string_str(const neo4j_value_t *value, char *buf, size_t n)
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

    bool simple = true;
    for (unsigned int i = 0; i < v->length; ++i)
    {
        char c = s[i];
        if (!isalnum(c) && c != '_')
        {
            simple = false;
            break;
        }
    }

    if (!simple)
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
    size_t l = 0;
    if ((l+1) < n)
    {
        buf[l] = quot;
    }
    l++;
    for (size_t i = 0; i < len; ++i)
    {
        if (s[i] == quot || s[i] == '\\')
        {
            if ((l+2) < n)
            {
                buf[l] = '\\';
            }
            else if ((l+1) < n)
            {
                buf[l] = '\0';
            }
            l++;
        }
        if ((l+1) < n)
        {
            buf[l] = s[i];
        }
        l++;
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

ssize_t neo4j_list_str(const neo4j_value_t *value, char *buf, size_t n)
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

    ssize_t slen = list_str(buf+l, (l < n)? n-l : 0, v->items, v->length);
    if (slen < 0)
    {
        return -1;
    }
    l += slen;

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


ssize_t list_str(char *buf, size_t n, const neo4j_value_t *values,
        unsigned int nvalues)
{
    size_t l = 0;
    for (unsigned int i = 0; i < nvalues; ++i)
    {
        ssize_t vlen = neo4j_ntostring(values[i], buf+l, (l < n)? n-l : 0);
        if (vlen < 0)
        {
            return -1;
        }
        l += vlen;

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

ssize_t neo4j_map_str(const neo4j_value_t *value, char *buf, size_t n)
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
        if (neo4j_type(entry->key) != NEO4J_STRING)
        {
            errno = NEO4J_INVALID_MAP_KEY_TYPE;
            return -1;
        }
        l += identifier_str(buf+l, (l < n)? n-l : 0, &(entry->key));

        if ((l+1) < n)
        {
            buf[l] = ':';
        }
        l++;

        ssize_t vlen = neo4j_ntostring(entry->value, buf+l, (l < n)? n-l : 0);
        if (vlen < 0)
        {
            return -1;
        }
        l += vlen;

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

ssize_t neo4j_node_str(const neo4j_value_t *value, char *buf, size_t n)
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
        if (neo4j_type(*label) != NEO4J_STRING)
        {
            errno = NEO4J_INVALID_LABEL_TYPE;
            return -1;
        }
        if ((l+1) < n)
        {
            buf[l] = ':';
        }
        l++;
        l += identifier_str(buf+l, (l < n)? n-l : 0, label);
    }

    assert(neo4j_type(v->fields[2]) == NEO4J_MAP);
    ssize_t mlen = neo4j_map_str(&(v->fields[2]), buf+l, (l < n)? n-l : 0);
    if (mlen < 0)
    {
        return -1;
    }
    l += mlen;

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

ssize_t neo4j_rel_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_RELATIONSHIP);
    const struct neo4j_struct *v = (const struct neo4j_struct *)value;
    assert(v->nfields == 5);

    size_t l = 0;
    if ((l+1) < n)
    {
        buf[l] = '[';
    }
    l++;

    assert(neo4j_type(v->fields[3]) == NEO4J_STRING);

    if ((l+1) < n)
    {
        buf[l] = ':';
    }
    l++;
    l += identifier_str(buf+l, (l < n)? n-l : 0, &(v->fields[3]));

    assert(neo4j_type(v->fields[4]) == NEO4J_MAP);
    ssize_t mlen = neo4j_map_str(&(v->fields[4]), buf+l, (l < n)? n-l : 0);
    if (mlen < 0)
    {
        return -1;
    }
    l += mlen;

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


/* structure */

ssize_t neo4j_struct_str(const neo4j_value_t *value, char *buf, size_t n)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(n == 0 || buf != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_STRUCT);
    const struct neo4j_struct *v = (const struct neo4j_struct *)value;

    ssize_t hlen = snprintf(buf, n, "struct<0x%X>", v->signature);
    if (hlen < 0)
    {
        return -1;
    }

    size_t l = hlen;
    if ((l+1) < n)
    {
        buf[l] = '(';
    }
    l++;

    ssize_t slen = list_str(buf+l, (l < n)? n-l : 0, v->fields, v->nfields);
    if (slen < 0)
    {
        return -1;
    }
    l += slen;

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
