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
#ifndef NEO4J_VALUES_H
#define NEO4J_VALUES_H

#include "neo4j-client.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>


#define ASSERT_VALUE_ALIGNMENT(type) \
    static_assert( \
        sizeof(type) == sizeof(struct neo4j_value), \
        #type " must be the same size as struct neo4j_value"); \
    static_assert( \
        offsetof(type, _vt_off) == offsetof(struct neo4j_value, _vt_off), \
        #type "#_vt_off is at the wrong offset"); \
    static_assert( \
        offsetof(type, _type) == offsetof(struct neo4j_value, _type), \
        #type "#_type is at the wrong offset")


struct neo4j_bool
{
    uint8_t _vt_off;
    uint8_t _type;
    uint16_t _pad1;
    uint32_t _pad2;
    union {
        uint8_t value;
        union _neo4j_value_data _pad3;
    };
};
ASSERT_VALUE_ALIGNMENT(struct neo4j_bool);


struct neo4j_int
{
    uint8_t _vt_off;
    uint8_t _type;
    uint16_t _pad1;
    uint32_t _pad2;
    union {
        int64_t value;
        union _neo4j_value_data _pad3;
    };
};
ASSERT_VALUE_ALIGNMENT(struct neo4j_int);


struct neo4j_float
{
    uint8_t _vt_off;
    uint8_t _type;
    uint16_t _pad1;
    uint32_t _pad2;
    union {
        double value;
        union _neo4j_value_data _pad3;
    };
};
ASSERT_VALUE_ALIGNMENT(struct neo4j_float);


struct neo4j_string
{
    uint8_t _vt_off;
    uint8_t _type;
    uint16_t _pad1;
    uint32_t length;
    union {
        const void *ustring;
        union _neo4j_value_data _pad2;
    };
};
ASSERT_VALUE_ALIGNMENT(struct neo4j_string);


struct neo4j_bytes
{
    uint8_t _vt_off;
    uint8_t _type;
    uint16_t _pad1;
    uint32_t length;
    union {
        const char *bytes;
        union _neo4j_value_data _pad2;
    };
};
ASSERT_VALUE_ALIGNMENT(struct neo4j_bytes);


struct neo4j_list
{
    uint8_t _vt_off;
    uint8_t _type;
    uint16_t _pad1;
    uint32_t length;
    union {
        const neo4j_value_t *items;
        union _neo4j_value_data _pad2;
    };
};
ASSERT_VALUE_ALIGNMENT(struct neo4j_list);


struct neo4j_map
{
    uint8_t _vt_off;
    uint8_t _type;
    uint16_t _pad1;
    uint32_t nentries;
    union {
        const neo4j_map_entry_t *entries;
        union _neo4j_value_data _pad2;
    };
};
ASSERT_VALUE_ALIGNMENT(struct neo4j_map);


#define NEO4J_NODE_SIGNATURE 0x4E
#define NEO4J_REL_SIGNATURE 0x52
#define NEO4J_PATH_SIGNATURE 0x50
#define NEO4J_UNBOUND_REL_SIGNATURE 0x72

struct neo4j_struct
{
    uint8_t _vt_off;
    uint8_t _type;
    uint16_t _pad1;
    uint8_t signature;
    uint8_t _pad2;
    uint16_t nfields;
    union {
        const neo4j_value_t *fields;
        union _neo4j_value_data _pad3;
    };
};
ASSERT_VALUE_ALIGNMENT(struct neo4j_struct);


/**
 * @internal
 *
 * Construct a neo4j value encoding a struct.
 *
 *
 * @param [signature] The struct signature.
 * @param [fields] The fields for the structure.
 * @param [n] The number of fields.
 * @return The neo4j value encoding the struct.
 */
__neo4j_pure
neo4j_value_t neo4j_struct(uint8_t signature,
        const neo4j_value_t *fields, uint16_t n);

/**
 * Construct a neo4j value encoding a node.
 *
 * @internal
 *
 * @param [fields] The fields for the node, which must be the Identity of the
 *         node, a List of Strings for labels and a Map of properties.
 * @return The neo4j value encoding the node.
 */
__neo4j_pure
neo4j_value_t neo4j_node(const neo4j_value_t fields[3]);

/**
 * Construct a neo4j value encoding a relationship.
 *
 * @internal
 *
 * @param [fields] The fields for the relationship, which must be the
 *         Identity of the relationship, the Identity of the start node, the
 *         Identity of the end node, a String reltype and a Map of properties.
 * @return The neo4j value encoding the relationship.
 */
__neo4j_pure
neo4j_value_t neo4j_relationship(const neo4j_value_t fields[5]);

/**
 * Construct a neo4j value encoding an unbound relationship.
 *
 * @internal
 *
 * @param [fields] The fields for the relationship, which must be an Int
 *         identity, a String reltype and a Map of properties.
 * @return The neo4j value encoding the relationship.
 */
__neo4j_pure
neo4j_value_t neo4j_unbound_relationship(const neo4j_value_t fields[3]);

/**
 * Construct a neo4j value encoding a path.
 *
 * @internal
 *
 * @param [fields] The fields for the path, which must be an List of Nodes,
 *         a List of Relationships and a List of Int indexes describing the
 *         Path sequence.
 * @return The neo4j value encoding the path.
 */
__neo4j_pure
neo4j_value_t neo4j_path(const neo4j_value_t fields[3]);

/**
 * Construct a neo4j identity.
 *
 * @internal
 *
 * @param [id] The numberic identity value.
 * @return The neo4j value encoding the identity.
 */
__neo4j_pure
neo4j_value_t neo4j_identity(long long identity);

/**
 * Get the signature of a neo4j struct.
 *
 * Note that the result is undefined if the value is not of type NEO4J_STRUCT.
 *
 * @internal
 *
 * @param [value] The neo4j struct.
 * @return The signature.
 */
__neo4j_pure
static inline uint8_t neo4j_struct_signature(neo4j_value_t value)
{
    return ((const struct neo4j_struct *)&value)->signature;
}

/**
 * Get the size of a neo4j struct.
 *
 * Note that the result is undefined if the value is not of type NEO4J_STRUCT.
 *
 * @internal
 *
 * @param [value] The neo4j struct.
 * @return The size of the struct.
 */
__neo4j_pure
static inline uint16_t neo4j_struct_size(neo4j_value_t value)
{
    return ((const struct neo4j_struct *)&value)->nfields;
}

/**
 * Get a field from a neo4j struct.
 *
 * Note that the result is undefined if the value is not of type NEO4J_STRUCT.
 *
 * @internal
 *
 * @param [value] The neo4j struct.
 * @param [index] The index of the field.
 * @return The size of the struct.
 */
__neo4j_pure
static inline neo4j_value_t neo4j_struct_getfield(neo4j_value_t value,
        unsigned int index)
{
    const struct neo4j_struct *v = (const struct neo4j_struct *)&value;
    if (v->nfields <= index)
    {
        return neo4j_null;
    }
    return v->fields[index];
}

/**
 * Get the array of fields from a neo4j struct.
 *
 * Note that the result is undefined if the value is not of type NEO4J_STRUCT.
 *
 * @internal
 *
 * @param [value] The neo4j struct.
 * @return A pointer to the array of fields.
 */
__neo4j_pure
static inline const neo4j_value_t *neo4j_struct_fields(neo4j_value_t value)
{
    return ((const struct neo4j_struct *)&value)->fields;
}

#endif/*NEO4J_VALUES_H*/
