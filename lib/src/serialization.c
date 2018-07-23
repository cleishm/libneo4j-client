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
static struct markers bytes_markers = {0x00, 0xCC, 0xCD, 0xCE, 0x00};
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
        int32_t l32;
    } length;
};


static int write_struct(uint8_t signature, uint16_t nfields,
        const neo4j_value_t *fields, neo4j_iostream_t *stream);
static int build_header(struct iovec *iov, struct length_header *header,
        size_t length, struct markers *markers);


/* null */

int neo4j_null_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(stream != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_NULL);
    uint8_t marker = 0xC0;
    return neo4j_ios_write_all(stream, &marker, 1, NULL);
}


/* boolean */

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

int neo4j_int_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(stream != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_INT ||
            neo4j_type(*value) == NEO4J_IDENTITY);
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


/* structure */

int neo4j_struct_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream)
{
    REQUIRE(value != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_STRUCT ||
            neo4j_type(*value) == NEO4J_NODE ||
            neo4j_type(*value) == NEO4J_RELATIONSHIP ||
            neo4j_type(*value) == NEO4J_PATH);
    const struct neo4j_struct *v = (const struct neo4j_struct *)value;
    return write_struct(v->signature, v->nfields, v->fields, stream);
}


/* bytes */

int neo4j_bytes_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream)
{
    REQUIRE(value, -1);
    REQUIRE(stream, -1);
    assert(neo4j_type(*value) == NEO4J_BYTES);
    const struct neo4j_bytes *v = (const struct neo4j_bytes *)value;

    struct iovec iov[3];
    struct length_header header;
    int iovcnt = build_header(iov, &header, v->length, &bytes_markers);
    iov[iovcnt].iov_base = (void *)(uintptr_t)(v->bytes);
    iov[iovcnt].iov_len = v->length;
    iovcnt++;

    return neo4j_ios_writev_all(stream, iov, iovcnt, NULL);
}


/* point */

int neo4j_point_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(stream != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_POINT);
    const struct neo4j_point *v = (const struct neo4j_point *)value;
    REQUIRE(v->data != NULL, -1);

    uint8_t signature;
    switch (v->dimensions)
    {
    case 2:
        signature = NEO4J_2DPOINT_SIGNATURE;
        break;
    case 3:
        signature = NEO4J_3DPOINT_SIGNATURE;
        break;
    default:
        return -1;
    }

    neo4j_value_t fields[4] = {
        neo4j_int(v->srid),
        neo4j_float(v->data->x),
        neo4j_float(v->data->y),
        neo4j_float(v->data->z)
    };

    return write_struct(signature, v->dimensions + 1, fields, stream);
}


/* local datetime */

int neo4j_local_datetime_serialize(const neo4j_value_t *value,
        neo4j_iostream_t *stream)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(stream != NULL, -1);
    assert(neo4j_type(*value) == NEO4J_LOCAL_DATETIME);
    const struct neo4j_local_datetime *v =
            (const struct neo4j_local_datetime *)value;

    neo4j_value_t fields[2] = {
        neo4j_int(v->epoch_seconds),
        neo4j_int(v->nanoseconds)
    };
    return write_struct(NEO4J_LOCAL_DATETIME_SIGNATURE, 2, fields, stream);
}


/* offset datetime */

int neo4j_offset_datetime_serialize(const neo4j_value_t *value,
        neo4j_iostream_t *stream)
{
    REQUIRE(value != NULL, -1);
    REQUIRE(stream != NULL, -1);
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

    neo4j_value_t fields[3] = {
        neo4j_int(v->epoch_seconds),
        neo4j_int(nanoseconds),
        neo4j_int(offset)
    };
    return write_struct(NEO4J_OFFSET_DATETIME_SIGNATURE, 3, fields, stream);
}


int write_struct(uint8_t signature, uint16_t nfields,
        const neo4j_value_t *fields, neo4j_iostream_t *stream)
{
    REQUIRE(nfields == 0 || fields != NULL, -1);
    REQUIRE(stream != NULL, -1);

    struct iovec iov[3];
    struct length_header header;
    int iovcnt = build_header(iov, &header, nfields, &structure_markers);
    iov[iovcnt].iov_base = (void *)(uintptr_t)(&signature);
    iov[iovcnt].iov_len = 1;
    iovcnt++;

    if (neo4j_ios_writev_all(stream, iov, iovcnt, NULL))
    {
        return -1;
    }

    for (int i = 0; i < nfields; ++i)
    {
        if (neo4j_serialize(fields[i], stream))
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

    // 0x00 is reserved for tiny ints
    if (markers->m4 != 0x00 && (length >> 4) == 0)
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
        assert(((uint64_t)length >> 32) == 0);
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
