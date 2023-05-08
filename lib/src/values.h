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
#define NEO4J_DATE_SIGNATURE 0x44
#define NEO4J_TIME_SIGNATURE 0x54
#define NEO4J_LOCALTIME_SIGNATURE 0x74
#define NEO4J_DATETIME_SIGNATURE 0x49
#define NEO4J_LEGACY_DATETIME_SIGNATURE 0x46
#define NEO4J_LOCALDATETIME_SIGNATURE 0x64
#define NEO4J_DURATION_SIGNATURE 0x45
#define NEO4J_POINT2D_SIGNATURE 0x58
#define NEO4J_POINT3D_SIGNATURE 0x59

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
 *         node, a List of Strings for label, a Map of properties, and
 *         a String ElementID (v5.0+)
 * @return The neo4j value encoding the node.
 */
__neo4j_pure
neo4j_value_t neo4j_node(const neo4j_value_t fields[4]);

/**
 * Construct a neo4j value encoding a relationship.
 *
 * @internal
 *
 * @param [fields] The fields for the relationship, which must be the
 *         Identity of the relationship, the Identity of the start node, the
 *         Identity of the end node, a String reltype, a Map of properties,
 *         and (v5.0+) Strings ElementID, start node ElementID, and end node
 *         ElementID.
 * @return The neo4j value encoding the relationship.
 */
__neo4j_pure
neo4j_value_t neo4j_relationship(const neo4j_value_t fields[8]);

/**
 * Construct a neo4j value encoding an unbound relationship.
 *
 * @internal
 *
 * @param [fields] The fields for the relationship, which must be an Int
 *         identity, a String reltype, a Map of properties, and a String
 *         ElementID (v5.0+)
 * @return The neo4j value encoding the relationship.
 */
__neo4j_pure
neo4j_value_t neo4j_unbound_relationship(const neo4j_value_t fields[4]);

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

/* NEW TYPES */

/**
 * Construct a neo4j value encoding a date.
 *
 * @internal
 *
 * @param [fields] The fields for the date, which must be an 
 *         integer, days since the Unix epoch
 * @return The neo4j value encoding the date.
 */
__neo4j_pure
neo4j_value_t neo4j_date(const neo4j_value_t fields[1]);

/**
 * Construct a neo4j value encoding a time.
 *
 * @internal
 *
 * @param [fields] The fields for the time, which must be an
 *         integer number of nanoseconds since midnight, and
 *         an offset in seconds from UTC
 * @return The neo4j value encoding the time.
 */
__neo4j_pure
neo4j_value_t neo4j_time(const neo4j_value_t fields[1]);

/**
 * Construct a neo4j value encoding a localtime.
 *
 * @internal
 *
 * @param [fields] The fields for the local time, which must be an
 *         integer number of nanoseconds since midnight
 * @return The neo4j value encoding the time.
 */
__neo4j_pure
neo4j_value_t neo4j_localtime(const neo4j_value_t fields[1]);

/**
 * Construct a neo4j value encoding a datetime.
 *
 * @internal
 *
 * @param [fields] The fields for the date and time, which must be an
 *         integer number of seconds elapsed since the Unix epoch, 
 *         an integer remainder in nanoseconds, and an integer
 *         offset in seconds from UTC
 * @return The neo4j value encoding the date and time.
 */
__neo4j_pure
neo4j_value_t neo4j_datetime(const neo4j_value_t fields[3]);

/**
 * Construct a neo4j value encoding a local datetime.
 *
 * @internal
 *
 * @param [fields] The fields for the local date and time, which must be an
 *         integer number of seconds elapsed since the Unix epoch, and
 *         an integer remainder in nanoseconds
 * @return The neo4j value encoding the local date and time.
 */
__neo4j_pure
neo4j_value_t neo4j_localdatetime(const neo4j_value_t fields[2]);

/**
 * Construct a neo4j value encoding a duration.
 *
 * @internal
 *
 * @param [fields] The fields for the duration, which must be an
 *         integer number of months, an integer number of days,
 *         an integer number of seconds, and an integer number
 *         of nanoseconds
 * @return The neo4j value encoding the duration.
 */
__neo4j_pure
neo4j_value_t neo4j_duration(const neo4j_value_t fields[4]);

/**
 * Construct a neo4j value encoding a point in 2D space.
 *
 * @internal
 *
 * @param [fields] The fields for the 2D point, which must be an
 *         integer srid (Spatial Reference System Identifier),
 *         an x coordinate (float) and a y coordinate (float)
 * @return The neo4j value encoding the 2D point.
 */
__neo4j_pure
neo4j_value_t neo4j_point2d(const neo4j_value_t fields[3]);

/**
 * Construct a neo4j value encoding a point in 3D space.
 *
 * @internal
 *
 * @param [fields] The fields for the 3D point, which must be an
 *         integer srid (Spatial Reference System Identifier),
 *         an x coordinate (float), a y coordinate (float), and
 *         a z coordinate (float)
 * @return The neo4j value encoding the 3D point.
 */
__neo4j_pure
neo4j_value_t neo4j_point3d(const neo4j_value_t fields[4]);

/**
 * Construct a neo4j identity.
 *
 * @internal
 *
 * @param [id] The numeric identity value.
 * @return The neo4j value encoding the identity.
 */
__neo4j_pure
neo4j_value_t neo4j_identity(long long identity);

/**
 * Construct a neo4j element ID.
 *
 * @internal
 *
 * @param [id] The String identity value.
 * @return The neo4j value encoding the identity.
 */
__neo4j_pure
neo4j_value_t neo4j_elementid(const char *eid);

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
