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
#include "values.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>


static bool null_eq(const neo4j_value_t *value, const neo4j_value_t *other);
static bool bool_eq(const neo4j_value_t *value, const neo4j_value_t *other);
static bool int_eq(const neo4j_value_t *value, const neo4j_value_t *other);
static bool float_eq(const neo4j_value_t *value, const neo4j_value_t *other);
static bool string_eq(const neo4j_value_t *value, const neo4j_value_t *other);
static bool list_eq(const neo4j_value_t *value, const neo4j_value_t *other);
static bool map_eq(const neo4j_value_t *value, const neo4j_value_t *other);
static bool struct_eq(const neo4j_value_t *value, const neo4j_value_t *other);


/* types */

struct neo4j_type
{
    const char *name;
};

static const struct neo4j_type null_type = { .name = "Null" };
static const struct neo4j_type bool_type = { .name = "Boolean" };
static const struct neo4j_type int_type = { .name = "Integer" };
static const struct neo4j_type float_type = { .name = "Float" };
static const struct neo4j_type string_type = { .name = "String" };
static const struct neo4j_type list_type = { .name = "List" };
static const struct neo4j_type map_type = { .name = "Map" };
static const struct neo4j_type node_type = { .name = "Node" };
static const struct neo4j_type relationship_type = { .name = "Relationship" };
static const struct neo4j_type struct_type = { .name = "Struct" };

static const struct neo4j_type *neo4j_types[] =
    { &null_type,
      &bool_type,
      &int_type,
      &float_type,
      &string_type,
      &list_type,
      &map_type,
      &node_type,
      &relationship_type,
      &struct_type };

#define NULL_TYPE_OFF 0
const uint8_t NEO4J_NULL = NULL_TYPE_OFF;
#define BOOL_TYPE_OFF 1
const uint8_t NEO4J_BOOL = BOOL_TYPE_OFF;
#define INT_TYPE_OFF 2
const uint8_t NEO4J_INT = INT_TYPE_OFF;
#define FLOAT_TYPE_OFF 3
const uint8_t NEO4J_FLOAT = FLOAT_TYPE_OFF;
#define STRING_TYPE_OFF 4
const uint8_t NEO4J_STRING = STRING_TYPE_OFF;
#define LIST_TYPE_OFF 5
const uint8_t NEO4J_LIST = LIST_TYPE_OFF;
#define MAP_TYPE_OFF 6
const uint8_t NEO4J_MAP = MAP_TYPE_OFF;
#define NODE_TYPE_OFF 7
const uint8_t NEO4J_NODE = NODE_TYPE_OFF;
#define RELATIONSHIP_TYPE_OFF 8
const uint8_t NEO4J_RELATIONSHIP = RELATIONSHIP_TYPE_OFF;
#define STRUCT_TYPE_OFF 9
const uint8_t NEO4J_STRUCT = STRUCT_TYPE_OFF;
static const uint8_t _MAX_TYPE =
    (sizeof(neo4j_types) / sizeof(struct neo4j_type *));

static_assert(
    (sizeof(neo4j_types) / sizeof(struct neo4j_type *)) <= UINT8_MAX,
    "value vt table cannot hold more than 2^8 entries");


const char *neo4j_type_str(const neo4j_type_t type)
{
    assert(type < _MAX_TYPE);
    return neo4j_types[type]->name;
}


/* vector tables */

struct neo4j_value_vt
{
    ssize_t (*str)(const neo4j_value_t *self, char *strbuf, size_t n);
    int (*serialize)(const neo4j_value_t *self, neo4j_iostream_t *stream);
    bool (*eq)(const neo4j_value_t *self, const neo4j_value_t *other);
};

static struct neo4j_value_vt null_vt =
    { .str = neo4j_null_str,
      .serialize = neo4j_null_serialize,
      .eq = null_eq };
static struct neo4j_value_vt bool_vt =
    { .str = neo4j_bool_str,
      .serialize = neo4j_bool_serialize,
      .eq = bool_eq };
static struct neo4j_value_vt int_vt =
    { .str = neo4j_int_str,
      .serialize = neo4j_int_serialize,
      .eq = int_eq };
static struct neo4j_value_vt float_vt =
    { .str = neo4j_float_str,
      .serialize = neo4j_float_serialize,
      .eq = float_eq };
static struct neo4j_value_vt string_vt =
    { .str = neo4j_string_str,
      .serialize = neo4j_string_serialize,
      .eq = string_eq };
static struct neo4j_value_vt list_vt =
    { .str = neo4j_list_str,
      .serialize = neo4j_list_serialize,
      .eq = list_eq };
static struct neo4j_value_vt map_vt =
    { .str = neo4j_map_str,
      .serialize = neo4j_map_serialize,
      .eq = map_eq };
static struct neo4j_value_vt node_vt =
    { .str = neo4j_node_str,
      .serialize = neo4j_struct_serialize,
      .eq = struct_eq };
static struct neo4j_value_vt relationship_vt =
    { .str = neo4j_rel_str,
      .serialize = neo4j_struct_serialize,
      .eq = struct_eq };
static struct neo4j_value_vt struct_vt =
    { .str = neo4j_struct_str,
      .serialize = neo4j_struct_serialize,
      .eq = struct_eq };

static const struct neo4j_value_vt *neo4j_value_vts[] =
    { &null_vt,
      &bool_vt,
      &int_vt,
      &float_vt,
      &string_vt,
      &list_vt,
      &map_vt,
      &node_vt,
      &relationship_vt,
      &struct_vt };

#define NULL_VT_OFF 0
#define BOOL_VT_OFF 1
#define INT_VT_OFF 2
#define FLOAT_VT_OFF 3
#define STRING_VT_OFF 4
#define LIST_VT_OFF 5
#define MAP_VT_OFF 6
#define NODE_VT_OFF 7
#define RELATIONSHIP_VT_OFF 8
#define STRUCT_VT_OFF 9
#define _MAX_VT_OFF (sizeof(neo4j_value_vts) / sizeof(struct neo4j_value_vt *))

static_assert(
    (sizeof(neo4j_value_vts) / sizeof(struct neo4j_value_vt *)) <= UINT8_MAX,
    "value vt table cannot hold more than 2^8 entries");


/* method dispatch */

char *neo4j_tostring(neo4j_value_t value, char *strbuf, size_t n)
{
    ssize_t result = neo4j_ntostring(value, strbuf, n);
    if (result < 0)
    {
        return NULL;
    }
    return strbuf;
}


ssize_t neo4j_ntostring(neo4j_value_t value, char *strbuf, size_t n)
{
    REQUIRE(value._vt_off < _MAX_VT_OFF, -1);
    REQUIRE(value._type < _MAX_TYPE, -1);
    const struct neo4j_value_vt *vt = neo4j_value_vts[value._vt_off];
    return vt->str(&value, strbuf, n);
}


int neo4j_serialize(neo4j_value_t value, neo4j_iostream_t *stream)
{
    REQUIRE(value._vt_off < _MAX_VT_OFF, -1);
    REQUIRE(value._type < _MAX_TYPE, -1);
    const struct neo4j_value_vt *vt = neo4j_value_vts[value._vt_off];
    return vt->serialize(&value, stream);
}


bool neo4j_eq(neo4j_value_t value1, neo4j_value_t value2)
{
    REQUIRE(value1._vt_off < _MAX_VT_OFF, false);
    REQUIRE(value1._type < _MAX_TYPE, false);
    errno = 0;
    REQUIRE(neo4j_type(value1) == neo4j_type(value2), false);
    const struct neo4j_value_vt *vt = neo4j_value_vts[value1._vt_off];
    return vt->eq(&value1, &value2);
}


/* constructors and accessors */

// null

const neo4j_value_t neo4j_null =
    { ._type = NULL_TYPE_OFF, ._vt_off = NULL_VT_OFF };


static bool null_eq(const neo4j_value_t *value, const neo4j_value_t *other)
{
    return true;
}

// bool

neo4j_value_t neo4j_bool(bool value)
{
    struct neo4j_bool v =
        { ._type = NEO4J_BOOL, ._vt_off = BOOL_VT_OFF,
          .value = value? 1 : 0 };
    return *((neo4j_value_t *)(&v));
}


bool bool_eq(const neo4j_value_t *value, const neo4j_value_t *other)
{
    const struct neo4j_bool *v = (const struct neo4j_bool *)value;
    const struct neo4j_bool *o = (const struct neo4j_bool *)other;
    return v->value == o->value;
}


bool neo4j_bool_value(neo4j_value_t value)
{
    REQUIRE(neo4j_type(value) == NEO4J_BOOL, false);
    return ((const struct neo4j_bool *)&value)->value? true : false;
}


// int

neo4j_value_t neo4j_int(int64_t value)
{
    struct neo4j_int v =
        { ._type = NEO4J_INT, ._vt_off = INT_VT_OFF, .value = value };
    return *((neo4j_value_t *)(&v));
}


bool int_eq(const neo4j_value_t *value, const neo4j_value_t *other)
{
    const struct neo4j_int *v = (const struct neo4j_int *)value;
    const struct neo4j_int *o = (const struct neo4j_int *)other;
    return v->value == o->value;
}


int64_t neo4j_int_value(neo4j_value_t value)
{
    REQUIRE(neo4j_type(value) == NEO4J_INT, 0);
    return ((const struct neo4j_int *)&value)->value;
}


// float

neo4j_value_t neo4j_float(double value)
{
    struct neo4j_float v =
        { ._type = NEO4J_FLOAT, ._vt_off = FLOAT_VT_OFF, .value = value };
    return *((neo4j_value_t *)(&v));
}


bool float_eq(const neo4j_value_t *value, const neo4j_value_t *other)
{
    const struct neo4j_float *v = (const struct neo4j_float *)value;
    const struct neo4j_float *o = (const struct neo4j_float *)other;
    return v->value == o->value;
}


double neo4j_float_value(neo4j_value_t value)
{
    REQUIRE(neo4j_type(value) == NEO4J_FLOAT, 0.0);
    return ((const struct neo4j_float *)&value)->value;
}


// string

neo4j_value_t neo4j_ustring(const char *u, uint32_t n)
{
    struct neo4j_string v =
        { ._type = NEO4J_STRING, ._vt_off = STRING_VT_OFF,
          .ustring = u, .length = n };
    return *((neo4j_value_t *)(&v));
}


bool string_eq(const neo4j_value_t *value, const neo4j_value_t *other)
{
    const struct neo4j_string *v = (const struct neo4j_string *)value;
    const struct neo4j_string *o = (const struct neo4j_string *)other;
    if (v->length != o->length)
    {
        return false;
    }
    return strncmp(v->ustring, o->ustring, v->length) == 0;
}


uint32_t neo4j_string_length(neo4j_value_t value)
{
    REQUIRE(neo4j_type(value) == NEO4J_STRING, 0);
    return ((const struct neo4j_string *)&value)->length;
}


const char *neo4j_ustring_value(neo4j_value_t value)
{
    REQUIRE(neo4j_type(value) == NEO4J_STRING, NULL);
    return ((const struct neo4j_string *)&value)->ustring;
}


char *neo4j_string_value(neo4j_value_t value, char *buffer, size_t length)
{
    REQUIRE(neo4j_type(value) == NEO4J_STRING, NULL);
    const struct neo4j_string *v = (const struct neo4j_string *)&value;
    size_t tocopy = min(v->length, length-1);
    memcpy(buffer, v->ustring, tocopy);
    buffer[tocopy] = '\0';
    return buffer;
}


// list

neo4j_value_t neo4j_list(const neo4j_value_t *items, uint32_t n)
{
    struct neo4j_list v =
        { ._type = NEO4J_LIST, ._vt_off = LIST_VT_OFF,
          .items = items, .length = n };
    return *((neo4j_value_t *)(&v));
}


bool list_eq(const neo4j_value_t *value, const neo4j_value_t *other)
{
    const struct neo4j_list *v = (const struct neo4j_list *)value;
    const struct neo4j_list *o = (const struct neo4j_list *)other;

    if (v->length != o->length)
    {
        return false;
    }

    for (unsigned int i = 0; i < v->length; ++i)
    {
        if (!neo4j_eq(v->items[i], o->items[i]))
        {
            return false;
        }
    }

    return true;
}


uint32_t neo4j_list_length(neo4j_value_t value)
{
    REQUIRE(neo4j_type(value) == NEO4J_LIST, 0);
    return ((const struct neo4j_list *)&value)->length;
}


neo4j_value_t neo4j_list_get(neo4j_value_t value, unsigned int index)
{
    REQUIRE(neo4j_type(value) == NEO4J_LIST, neo4j_null);
    const struct neo4j_list *list = (const struct neo4j_list *)&value;
    if (list->length <= index)
    {
        return neo4j_null;
    }
    return list->items[index];
}


// map

neo4j_value_t neo4j_map(const neo4j_map_entry_t *entries, uint32_t n)
{
    struct neo4j_map v =
        { ._type = NEO4J_MAP, ._vt_off = MAP_VT_OFF,
          .entries = entries, .nentries = n };
    return *((neo4j_value_t *)(&v));
}


bool map_eq(const neo4j_value_t *value, const neo4j_value_t *other)
{
    const struct neo4j_map *v = (const struct neo4j_map *)value;
    const struct neo4j_map *o = (const struct neo4j_map *)other;

    if (v->nentries != o->nentries)
    {
        return false;
    }

    for (unsigned int i = 0; i < v->nentries; ++i)
    {
        const neo4j_map_entry_t *ventry = &(v->entries[i]);
        const neo4j_map_entry_t *oentry = NULL;
        for (unsigned int j = 0; j < o->nentries; ++j)
        {
            if (neo4j_eq(ventry->key, o->entries[j].key))
            {
                oentry = &(o->entries[j]);
                break;
            }
        }
        if (oentry == NULL || !neo4j_eq(ventry->value, oentry->value))
        {
            return false;
        }
    }
    return true;
}


uint32_t neo4j_map_size(neo4j_value_t value)
{
    REQUIRE(neo4j_type(value) == NEO4J_MAP, 0);
    return ((const struct neo4j_map *)&value)->nentries;
}


const neo4j_map_entry_t *neo4j_map_getentry(neo4j_value_t value,
        unsigned int index)
{
    REQUIRE(neo4j_type(value) == NEO4J_MAP, 0);
    const struct neo4j_map *map = (const struct neo4j_map *)&value;

    if (map->nentries <= index)
    {
        return NULL;
    }
    return &(map->entries[index]);
}


neo4j_value_t neo4j_map_get(neo4j_value_t value, neo4j_value_t key)
{
    REQUIRE(neo4j_type(value) == NEO4J_MAP, neo4j_null);
    const struct neo4j_map *map = (const struct neo4j_map *)&value;

    for (unsigned int i = 0; i < map->nentries; ++i)
    {
        const neo4j_map_entry_t *entry = &(map->entries[i]);
        if (neo4j_eq(entry->key, key))
        {
            return entry->value;
        }
    }
    return neo4j_null;
}


neo4j_map_entry_t neo4j_map_entry(neo4j_value_t key, neo4j_value_t value)
{
    struct neo4j_map_entry entry = { .key = key, .value = value };
    return entry;
}


// node

neo4j_value_t neo4j_node(const neo4j_value_t fields[3])
{
    struct neo4j_struct v =
            { ._type = NEO4J_NODE, ._vt_off = NODE_VT_OFF,
              .signature = NEO4J_NODE_SIGNATURE,
              .fields = fields, .nfields = 3 };
    return *((neo4j_value_t *)(&v));
}


neo4j_value_t neo4j_node_labels(neo4j_value_t value)
{
    REQUIRE(neo4j_type(value) == NEO4J_NODE, neo4j_null);
    const struct neo4j_struct *v = (const struct neo4j_struct *)&value;
    assert(v->nfields == 3);
    assert(neo4j_type(v->fields[1]) == NEO4J_LIST);
    return v->fields[1];
}


neo4j_value_t neo4j_node_properties(neo4j_value_t value)
{
    REQUIRE(neo4j_type(value) == NEO4J_NODE, neo4j_null);
    const struct neo4j_struct *v = (const struct neo4j_struct *)&value;
    assert(v->nfields == 3);
    assert(neo4j_type(v->fields[2]) == NEO4J_MAP);
    return v->fields[2];
}


// relationship

neo4j_value_t neo4j_relationship(const neo4j_value_t fields[5])
{
    struct neo4j_struct v =
            { ._type = NEO4J_RELATIONSHIP, ._vt_off = RELATIONSHIP_VT_OFF,
              .signature = NEO4J_REL_SIGNATURE,
              .fields = fields, .nfields = 5 };
    return *((neo4j_value_t *)(&v));
}


neo4j_value_t neo4j_relationship_type(neo4j_value_t value)
{
    REQUIRE(neo4j_type(value) == NEO4J_RELATIONSHIP, neo4j_null);
    const struct neo4j_struct *v = (const struct neo4j_struct *)&value;
    assert(v->nfields == 5);
    assert(neo4j_type(v->fields[3]) == NEO4J_STRING);
    return v->fields[3];
}


neo4j_value_t neo4j_relationship_properties(neo4j_value_t value)
{
    REQUIRE(neo4j_type(value) == NEO4J_RELATIONSHIP, neo4j_null);
    const struct neo4j_struct *v = (const struct neo4j_struct *)&value;
    assert(v->nfields == 5);
    assert(neo4j_type(v->fields[4]) == NEO4J_MAP);
    return v->fields[4];
}


// struct

neo4j_value_t neo4j_struct(uint8_t signature,
        const neo4j_value_t *fields, uint16_t n)
{
    struct neo4j_struct v =
            { ._type = NEO4J_STRUCT, ._vt_off = STRUCT_VT_OFF,
              .signature = signature, .fields = fields, .nfields = n };
    return *((neo4j_value_t *)(&v));
}


bool struct_eq(const neo4j_value_t *value, const neo4j_value_t *other)
{
    const struct neo4j_struct *v = (const struct neo4j_struct *)value;
    const struct neo4j_struct *o = (const struct neo4j_struct *)other;

    if (v->signature != o->signature)
    {
        return false;
    }
    if (v->nfields != o->nfields)
    {
        return false;
    }

    for (unsigned int i = 0; i < v->nfields; ++i)
    {
        if (!neo4j_eq(v->fields[i], o->fields[i]))
        {
            return false;
        }
    }
    return true;
}
