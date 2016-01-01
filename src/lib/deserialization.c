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
#include "deserialization.h"
#include "util.h"
#include "values.h"
#include <assert.h>
#include <errno.h>


struct deserializer
{
    uint8_t marker;
    uint8_t mask;
    int (*deserialize)(uint8_t marker, neo4j_iostream_t *stream,
            neo4j_mpool_t *pool, neo4j_value_t *value);
};

#define DESERIALIZER_FUNC_DEF(funcname) \
    static int funcname(uint8_t marker, neo4j_iostream_t *stream, \
            neo4j_mpool_t *pool, neo4j_value_t *value)

DESERIALIZER_FUNC_DEF(tiny_int_deserialize);
DESERIALIZER_FUNC_DEF(tiny_string_deserialize);
DESERIALIZER_FUNC_DEF(tiny_list_deserialize);
DESERIALIZER_FUNC_DEF(tiny_map_deserialize);
DESERIALIZER_FUNC_DEF(tiny_struct_deserialize);
DESERIALIZER_FUNC_DEF(null_deserialize);
DESERIALIZER_FUNC_DEF(float_deserialize);
DESERIALIZER_FUNC_DEF(boolean_false_deserialize);
DESERIALIZER_FUNC_DEF(boolean_true_deserialize);
DESERIALIZER_FUNC_DEF(int8_deserialize);
DESERIALIZER_FUNC_DEF(int16_deserialize);
DESERIALIZER_FUNC_DEF(int32_deserialize);
DESERIALIZER_FUNC_DEF(int64_deserialize);
DESERIALIZER_FUNC_DEF(string8_deserialize);
DESERIALIZER_FUNC_DEF(string16_deserialize);
DESERIALIZER_FUNC_DEF(string32_deserialize);
DESERIALIZER_FUNC_DEF(list8_deserialize);
DESERIALIZER_FUNC_DEF(list16_deserialize);
DESERIALIZER_FUNC_DEF(list32_deserialize);
DESERIALIZER_FUNC_DEF(map8_deserialize);
DESERIALIZER_FUNC_DEF(map16_deserialize);
DESERIALIZER_FUNC_DEF(map32_deserialize);
DESERIALIZER_FUNC_DEF(struct8_deserialize);
DESERIALIZER_FUNC_DEF(struct16_deserialize);

static int string_deserialize(uint32_t length, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value);
static int list_deserialize(uint32_t nitems, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value);
static int map_deserialize(uint32_t nentries, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value);
static int struct_deserialize(uint16_t nfields, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value);


static const struct deserializer deserializers[] =
    { { .marker = 0x00, .mask=0x80, .deserialize = tiny_int_deserialize },
      { .marker = 0x80, .mask=0xF0, .deserialize = tiny_string_deserialize },
      { .marker = 0x90, .mask=0xF0, .deserialize = tiny_list_deserialize },
      { .marker = 0xA0, .mask=0xF0, .deserialize = tiny_map_deserialize },
      { .marker = 0xB0, .mask=0xF0, .deserialize = tiny_struct_deserialize },
      { .marker = 0xC0, .mask=0xFF, .deserialize = null_deserialize },
      { .marker = 0xC1, .mask=0xFF, .deserialize = float_deserialize },
      { .marker = 0xC2, .mask=0xFF, .deserialize = boolean_false_deserialize },
      { .marker = 0xC3, .mask=0xFF, .deserialize = boolean_true_deserialize },
      { .marker = 0xC8, .mask=0xFF, .deserialize = int8_deserialize },
      { .marker = 0xC9, .mask=0xFF, .deserialize = int16_deserialize },
      { .marker = 0xCA, .mask=0xFF, .deserialize = int32_deserialize },
      { .marker = 0xCB, .mask=0xFF, .deserialize = int64_deserialize },
      { .marker = 0xD0, .mask=0xFF, .deserialize = string8_deserialize },
      { .marker = 0xD1, .mask=0xFF, .deserialize = string16_deserialize },
      { .marker = 0xD2, .mask=0xFF, .deserialize = string32_deserialize },
      { .marker = 0xD4, .mask=0xFF, .deserialize = list8_deserialize },
      { .marker = 0xD5, .mask=0xFF, .deserialize = list16_deserialize },
      { .marker = 0xD6, .mask=0xFF, .deserialize = list32_deserialize },
      { .marker = 0xD8, .mask=0xFF, .deserialize = map8_deserialize },
      { .marker = 0xD9, .mask=0xFF, .deserialize = map16_deserialize },
      { .marker = 0xDA, .mask=0xFF, .deserialize = map32_deserialize },
      { .marker = 0xDC, .mask=0xFF, .deserialize = struct8_deserialize },
      { .marker = 0xDD, .mask=0xFF, .deserialize = struct16_deserialize },
      { .marker = 0xF0, .mask=0xF0, .deserialize = tiny_int_deserialize } };
static const int deserializers_max =
    sizeof(deserializers) / sizeof(struct deserializer);


int neo4j_deserialize(neo4j_iostream_t *stream, neo4j_mpool_t *pool,
        neo4j_value_t *value)
{
    REQUIRE(stream != NULL, -1);
    REQUIRE(pool != NULL, -1);
    REQUIRE(value != NULL, -1);
    size_t pdepth = neo4j_mpool_depth(*pool);

    uint8_t marker;
    if (neo4j_ios_read_all(stream, &marker, sizeof(marker), NULL) < 0)
    {
        goto failure;
    }

    int i = 0;
    while (i < deserializers_max &&
            (marker & deserializers[i].mask) != deserializers[i].marker)
    {
        ++i;
    }

    if (i >= deserializers_max)
    {
        errno = EPROTO;
        goto failure;
    }

    if (deserializers[i].deserialize(marker, stream, pool, value))
    {
        goto failure;
    }

    return 0;

    int errsv;
failure:
    errsv = errno;
    neo4j_mpool_drainto(pool, pdepth);
    errno = errsv;
    return -1;
}


int tiny_int_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    *value = neo4j_int((int8_t)marker);
    return 0;
}


int tiny_string_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint32_t length = marker & 0x0F;
    return string_deserialize(length, stream, pool, value);
}


int tiny_list_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint16_t nitems = marker & 0x0F;
    return list_deserialize(nitems, stream, pool, value);
}

int tiny_map_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint16_t nentries = marker & 0x0F;
    return map_deserialize(nentries, stream, pool, value);
}

int tiny_struct_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint16_t nfields = marker & 0x0F;
    return struct_deserialize(nfields, stream, pool, value);
}

int null_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    *value = neo4j_null;
    return 0;
}

int float_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    union
    {
        uint64_t data;
        double value;
    } double_data;

    if (neo4j_ios_read_all(stream, &(double_data.data),
                sizeof(double_data.data), NULL) < 0)
    {
        return -1;
    }

    double_data.data = ntohll(double_data.data);
    *value = neo4j_float(double_data.value);
    return 0;
}

int boolean_false_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    *value = neo4j_bool(false);
    return 0;
}

int boolean_true_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    *value = neo4j_bool(true);
    return 0;
}

int int8_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    int8_t data;
    if (neo4j_ios_read_all(stream, &data, sizeof(data), NULL) < 0)
    {
        return -1;
    }
    *value = neo4j_int(data);
    return 0;
}

int int16_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    int16_t data;
    if (neo4j_ios_read_all(stream, &data, sizeof(data), NULL) < 0)
    {
        return -1;
    }
    data = ntohs(data);
    *value = neo4j_int(data);
    return 0;
}

int int32_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    int32_t data;
    if (neo4j_ios_read_all(stream, &data, sizeof(data), NULL) < 0)
    {
        return -1;
    }
    data = ntohl(data);
    *value = neo4j_int(data);
    return 0;
}

int int64_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    int64_t data;
    if (neo4j_ios_read_all(stream, &data, sizeof(data), NULL) < 0)
    {
        return -1;
    }
    data = ntohll(data);
    *value = neo4j_int(data);
    return 0;
}

int string8_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint8_t length;
    if (neo4j_ios_read_all(stream, &length, sizeof(length), NULL) < 0)
    {
        return -1;
    }
    return string_deserialize(length, stream, pool, value);
}

int string16_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint16_t length;
    if (neo4j_ios_read_all(stream, &length, sizeof(length), NULL) < 0)
    {
        return -1;
    }
    length = ntohs(length);
    return string_deserialize(length, stream, pool, value);
}

int string32_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint32_t length;
    if (neo4j_ios_read_all(stream, &length, sizeof(length), NULL) < 0)
    {
        return -1;
    }
    length = ntohl(length);
    return string_deserialize(length, stream, pool, value);
}

int list8_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint8_t nitems;
    if (neo4j_ios_read_all(stream, &nitems, sizeof(nitems), NULL) < 0)
    {
        return -1;
    }
    return list_deserialize(nitems, stream, pool, value);
}

int list16_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint16_t nitems;
    if (neo4j_ios_read_all(stream, &nitems, sizeof(nitems), NULL) < 0)
    {
        return -1;
    }
    nitems = ntohs(nitems);
    return list_deserialize(nitems, stream, pool, value);
}

int list32_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint16_t nitems;
    if (neo4j_ios_read_all(stream, &nitems, sizeof(nitems), NULL) < 0)
    {
        return -1;
    }
    nitems = ntohl(nitems);
    return list_deserialize(nitems, stream, pool, value);
}

int map8_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint8_t nentries;
    if (neo4j_ios_read_all(stream, &nentries, sizeof(nentries), NULL) < 0)
    {
        return -1;
    }
    return map_deserialize(nentries, stream, pool, value);
}

int map16_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint16_t nentries;
    if (neo4j_ios_read_all(stream, &nentries, sizeof(nentries), NULL) < 0)
    {
        return -1;
    }
    nentries = ntohs(nentries);
    return map_deserialize(nentries, stream, pool, value);
}

int map32_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint32_t nentries;
    if (neo4j_ios_read_all(stream, &nentries, sizeof(nentries), NULL) < 0)
    {
        return -1;
    }
    nentries = ntohl(nentries);
    return map_deserialize(nentries, stream, pool, value);
}

int struct8_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint8_t nfields;
    if (neo4j_ios_read_all(stream, &nfields, sizeof(nfields), NULL) < 0)
    {
        return -1;
    }
    return struct_deserialize(nfields, stream, pool, value);
}

int struct16_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint16_t nfields;
    if (neo4j_ios_read_all(stream, &nfields, sizeof(nfields), NULL) < 0)
    {
        return -1;
    }
    nfields = ntohs(nfields);
    return struct_deserialize(nfields, stream, pool, value);
}

int string_deserialize(uint32_t length, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    char *ustring = NULL;
    if (length > 0)
    {
        ustring = neo4j_mpool_alloc(pool, length);
        if (ustring == NULL)
        {
            return -1;
        }

        if (neo4j_ios_read_all(stream, ustring, length, NULL) < 0)
        {
            return -1;
        }
    }

    *value = neo4j_ustring(ustring, length);
    return 0;
}


int list_deserialize(uint32_t nitems, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    neo4j_value_t *items = NULL;
    if (nitems > 0)
    {
        items = neo4j_mpool_calloc(pool, nitems, sizeof(neo4j_value_t));
        if (items == NULL)
        {
            return -1;
        }

        for (unsigned i = 0; i < nitems; ++i)
        {
            if (neo4j_deserialize(stream, pool, &(items[i])))
            {
                return -1;
            }
        }
    }

    *value = neo4j_list(items, nitems);
    return 0;
}


int map_deserialize(uint32_t nentries, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    neo4j_map_entry_t *entries = NULL;
    if (nentries > 0)
    {
        entries = neo4j_mpool_calloc(pool, nentries, sizeof(neo4j_map_entry_t));
        if (entries == NULL)
        {
            return -1;
        }

        for (unsigned i = 0; i < nentries; ++i)
        {
            if (neo4j_deserialize(stream, pool, &(entries[i].key)))
            {
                return -1;
            }
            if (neo4j_type(entries[i].key) != NEO4J_STRING)
            {
                errno = NEO4J_INVALID_MAP_KEY_TYPE;
                return -1;
            }
            if (neo4j_deserialize(stream, pool, &(entries[i].value)))
            {
                return -1;
            }
        }
    }

    *value = neo4j_map(entries, nentries);
    return 0;
}


int struct_deserialize(uint16_t nfields, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint8_t signature;
    if (neo4j_ios_read_all(stream, &signature, sizeof(signature), NULL) < 0)
    {
        return -1;
    }

    neo4j_value_t *fields = NULL;
    if (nfields > 0)
    {
        fields = neo4j_mpool_calloc(pool, nfields, sizeof(neo4j_value_t));
        if (fields == NULL)
        {
            return -1;
        }

        for (unsigned i = 0; i < nfields; ++i)
        {
            if (neo4j_deserialize(stream, pool, &(fields[i])))
            {
                return -1;
            }
        }
    }

    switch (signature)
    {
    case NEO4J_NODE_SIGNATURE:
        if (nfields != 3)
        {
            errno = EPROTO;
            return -1;
        }
        if (neo4j_type(fields[0]) != NEO4J_INT ||
            neo4j_type(fields[1]) != NEO4J_LIST ||
            neo4j_type(fields[2]) != NEO4J_MAP)
        {
            errno = EPROTO;
            return -1;
        }
        // TODO: check all labels are strings
        *value = neo4j_node(fields);
        break;
    case NEO4J_REL_SIGNATURE:
        if (nfields != 5)
        {
            errno = EPROTO;
            return -1;
        }
        if (neo4j_type(fields[0]) != NEO4J_INT ||
            neo4j_type(fields[1]) != NEO4J_INT ||
            neo4j_type(fields[2]) != NEO4J_INT ||
            neo4j_type(fields[3]) != NEO4J_STRING ||
            neo4j_type(fields[4]) != NEO4J_MAP)
        {
            errno = EPROTO;
            return -1;
        }
        *value = neo4j_relationship(fields);
        break;
    default:
        *value = neo4j_struct(signature, fields, nfields);
        break;
    }
    return 0;
}
