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


typedef int (*deserializer_t)(uint8_t marker, neo4j_iostream_t *stream,
            neo4j_mpool_t *pool, neo4j_value_t *value);

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
DESERIALIZER_FUNC_DEF(bytes8_deserialize);
DESERIALIZER_FUNC_DEF(bytes16_deserialize);
DESERIALIZER_FUNC_DEF(bytes32_deserialize);
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
static int bytes_deserialize(uint32_t length, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value);
static int list_deserialize(uint32_t nitems, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value);
static int map_deserialize(uint32_t nentries, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value);
static int struct_deserialize(uint16_t nfields, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value);

static const deserializer_t deserializers[UINT8_MAX+1] =
    { tiny_int_deserialize,              // 0x00
      tiny_int_deserialize,              // 0x01
      tiny_int_deserialize,              // 0x02
      tiny_int_deserialize,              // 0x03
      tiny_int_deserialize,              // 0x04
      tiny_int_deserialize,              // 0x05
      tiny_int_deserialize,              // 0x06
      tiny_int_deserialize,              // 0x07
      tiny_int_deserialize,              // 0x08
      tiny_int_deserialize,              // 0x09
      tiny_int_deserialize,              // 0x0a
      tiny_int_deserialize,              // 0x0b
      tiny_int_deserialize,              // 0x0c
      tiny_int_deserialize,              // 0x0d
      tiny_int_deserialize,              // 0x0e
      tiny_int_deserialize,              // 0x0f
      tiny_int_deserialize,              // 0x10
      tiny_int_deserialize,              // 0x11
      tiny_int_deserialize,              // 0x12
      tiny_int_deserialize,              // 0x13
      tiny_int_deserialize,              // 0x14
      tiny_int_deserialize,              // 0x15
      tiny_int_deserialize,              // 0x16
      tiny_int_deserialize,              // 0x17
      tiny_int_deserialize,              // 0x18
      tiny_int_deserialize,              // 0x19
      tiny_int_deserialize,              // 0x1a
      tiny_int_deserialize,              // 0x1b
      tiny_int_deserialize,              // 0x1c
      tiny_int_deserialize,              // 0x1d
      tiny_int_deserialize,              // 0x1e
      tiny_int_deserialize,              // 0x1f
      tiny_int_deserialize,              // 0x20
      tiny_int_deserialize,              // 0x21
      tiny_int_deserialize,              // 0x22
      tiny_int_deserialize,              // 0x23
      tiny_int_deserialize,              // 0x24
      tiny_int_deserialize,              // 0x25
      tiny_int_deserialize,              // 0x26
      tiny_int_deserialize,              // 0x27
      tiny_int_deserialize,              // 0x28
      tiny_int_deserialize,              // 0x29
      tiny_int_deserialize,              // 0x2a
      tiny_int_deserialize,              // 0x2b
      tiny_int_deserialize,              // 0x2c
      tiny_int_deserialize,              // 0x2d
      tiny_int_deserialize,              // 0x2e
      tiny_int_deserialize,              // 0x2f
      tiny_int_deserialize,              // 0x30
      tiny_int_deserialize,              // 0x31
      tiny_int_deserialize,              // 0x32
      tiny_int_deserialize,              // 0x33
      tiny_int_deserialize,              // 0x34
      tiny_int_deserialize,              // 0x35
      tiny_int_deserialize,              // 0x36
      tiny_int_deserialize,              // 0x37
      tiny_int_deserialize,              // 0x38
      tiny_int_deserialize,              // 0x39
      tiny_int_deserialize,              // 0x3a
      tiny_int_deserialize,              // 0x3b
      tiny_int_deserialize,              // 0x3c
      tiny_int_deserialize,              // 0x3d
      tiny_int_deserialize,              // 0x3e
      tiny_int_deserialize,              // 0x3f
      tiny_int_deserialize,              // 0x40
      tiny_int_deserialize,              // 0x41
      tiny_int_deserialize,              // 0x42
      tiny_int_deserialize,              // 0x43
      tiny_int_deserialize,              // 0x44
      tiny_int_deserialize,              // 0x45
      tiny_int_deserialize,              // 0x46
      tiny_int_deserialize,              // 0x47
      tiny_int_deserialize,              // 0x48
      tiny_int_deserialize,              // 0x49
      tiny_int_deserialize,              // 0x4a
      tiny_int_deserialize,              // 0x4b
      tiny_int_deserialize,              // 0x4c
      tiny_int_deserialize,              // 0x4d
      tiny_int_deserialize,              // 0x4e
      tiny_int_deserialize,              // 0x4f
      tiny_int_deserialize,              // 0x50
      tiny_int_deserialize,              // 0x51
      tiny_int_deserialize,              // 0x52
      tiny_int_deserialize,              // 0x53
      tiny_int_deserialize,              // 0x54
      tiny_int_deserialize,              // 0x55
      tiny_int_deserialize,              // 0x56
      tiny_int_deserialize,              // 0x57
      tiny_int_deserialize,              // 0x58
      tiny_int_deserialize,              // 0x59
      tiny_int_deserialize,              // 0x5a
      tiny_int_deserialize,              // 0x5b
      tiny_int_deserialize,              // 0x5c
      tiny_int_deserialize,              // 0x5d
      tiny_int_deserialize,              // 0x5e
      tiny_int_deserialize,              // 0x5f
      tiny_int_deserialize,              // 0x60
      tiny_int_deserialize,              // 0x61
      tiny_int_deserialize,              // 0x62
      tiny_int_deserialize,              // 0x63
      tiny_int_deserialize,              // 0x64
      tiny_int_deserialize,              // 0x65
      tiny_int_deserialize,              // 0x66
      tiny_int_deserialize,              // 0x67
      tiny_int_deserialize,              // 0x68
      tiny_int_deserialize,              // 0x69
      tiny_int_deserialize,              // 0x6a
      tiny_int_deserialize,              // 0x6b
      tiny_int_deserialize,              // 0x6c
      tiny_int_deserialize,              // 0x6d
      tiny_int_deserialize,              // 0x6e
      tiny_int_deserialize,              // 0x6f
      tiny_int_deserialize,              // 0x70
      tiny_int_deserialize,              // 0x71
      tiny_int_deserialize,              // 0x72
      tiny_int_deserialize,              // 0x73
      tiny_int_deserialize,              // 0x74
      tiny_int_deserialize,              // 0x75
      tiny_int_deserialize,              // 0x76
      tiny_int_deserialize,              // 0x77
      tiny_int_deserialize,              // 0x78
      tiny_int_deserialize,              // 0x79
      tiny_int_deserialize,              // 0x7a
      tiny_int_deserialize,              // 0x7b
      tiny_int_deserialize,              // 0x7c
      tiny_int_deserialize,              // 0x7d
      tiny_int_deserialize,              // 0x7e
      tiny_int_deserialize,              // 0x7f
      tiny_string_deserialize,           // 0x80
      tiny_string_deserialize,           // 0x81
      tiny_string_deserialize,           // 0x82
      tiny_string_deserialize,           // 0x83
      tiny_string_deserialize,           // 0x84
      tiny_string_deserialize,           // 0x85
      tiny_string_deserialize,           // 0x86
      tiny_string_deserialize,           // 0x87
      tiny_string_deserialize,           // 0x88
      tiny_string_deserialize,           // 0x89
      tiny_string_deserialize,           // 0x8a
      tiny_string_deserialize,           // 0x8b
      tiny_string_deserialize,           // 0x8c
      tiny_string_deserialize,           // 0x8d
      tiny_string_deserialize,           // 0x8e
      tiny_string_deserialize,           // 0x8f
      tiny_list_deserialize,             // 0x90
      tiny_list_deserialize,             // 0x91
      tiny_list_deserialize,             // 0x92
      tiny_list_deserialize,             // 0x93
      tiny_list_deserialize,             // 0x94
      tiny_list_deserialize,             // 0x95
      tiny_list_deserialize,             // 0x96
      tiny_list_deserialize,             // 0x97
      tiny_list_deserialize,             // 0x98
      tiny_list_deserialize,             // 0x99
      tiny_list_deserialize,             // 0x9a
      tiny_list_deserialize,             // 0x9b
      tiny_list_deserialize,             // 0x9c
      tiny_list_deserialize,             // 0x9d
      tiny_list_deserialize,             // 0x9e
      tiny_list_deserialize,             // 0x9f
      tiny_map_deserialize,              // 0xa0
      tiny_map_deserialize,              // 0xa1
      tiny_map_deserialize,              // 0xa2
      tiny_map_deserialize,              // 0xa3
      tiny_map_deserialize,              // 0xa4
      tiny_map_deserialize,              // 0xa5
      tiny_map_deserialize,              // 0xa6
      tiny_map_deserialize,              // 0xa7
      tiny_map_deserialize,              // 0xa8
      tiny_map_deserialize,              // 0xa9
      tiny_map_deserialize,              // 0xaa
      tiny_map_deserialize,              // 0xab
      tiny_map_deserialize,              // 0xac
      tiny_map_deserialize,              // 0xad
      tiny_map_deserialize,              // 0xae
      tiny_map_deserialize,              // 0xaf
      tiny_struct_deserialize,           // 0xb0
      tiny_struct_deserialize,           // 0xb1
      tiny_struct_deserialize,           // 0xb2
      tiny_struct_deserialize,           // 0xb3
      tiny_struct_deserialize,           // 0xb4
      tiny_struct_deserialize,           // 0xb5
      tiny_struct_deserialize,           // 0xb6
      tiny_struct_deserialize,           // 0xb7
      tiny_struct_deserialize,           // 0xb8
      tiny_struct_deserialize,           // 0xb9
      tiny_struct_deserialize,           // 0xba
      tiny_struct_deserialize,           // 0xbb
      tiny_struct_deserialize,           // 0xbc
      tiny_struct_deserialize,           // 0xbd
      tiny_struct_deserialize,           // 0xbe
      tiny_struct_deserialize,           // 0xbf
      null_deserialize,                  // 0xc0
      float_deserialize,                 // 0xc1
      boolean_false_deserialize,         // 0xc2
      boolean_true_deserialize,          // 0xc3
      NULL,                              // 0xc4
      NULL,                              // 0xc5
      NULL,                              // 0xc6
      NULL,                              // 0xc7
      int8_deserialize,                  // 0xc8
      int16_deserialize,                 // 0xc9
      int32_deserialize,                 // 0xca
      int64_deserialize,                 // 0xcb
      bytes8_deserialize,                // 0xcc
      bytes16_deserialize,               // 0xcd
      bytes32_deserialize,               // 0xce
      NULL,                              // 0xcf
      string8_deserialize,               // 0xd0
      string16_deserialize,              // 0xd1
      string32_deserialize,              // 0xd2
      NULL,                              // 0xd3
      list8_deserialize,                 // 0xd4
      list16_deserialize,                // 0xd5
      list32_deserialize,                // 0xd6
      NULL,                              // 0xd7
      map8_deserialize,                  // 0xd8
      map16_deserialize,                 // 0xd9
      map32_deserialize,                 // 0xda
      NULL,                              // 0xdb
      struct8_deserialize,               // 0xdc
      struct16_deserialize,              // 0xdd
      NULL,                              // 0xde
      NULL,                              // 0xdf
      NULL,                              // 0xe0
      NULL,                              // 0xe1
      NULL,                              // 0xe2
      NULL,                              // 0xe3
      NULL,                              // 0xe4
      NULL,                              // 0xe5
      NULL,                              // 0xe6
      NULL,                              // 0xe7
      NULL,                              // 0xe8
      NULL,                              // 0xe9
      NULL,                              // 0xea
      NULL,                              // 0xeb
      NULL,                              // 0xec
      NULL,                              // 0xed
      NULL,                              // 0xee
      NULL,                              // 0xef
      tiny_int_deserialize,              // 0xf0
      tiny_int_deserialize,              // 0xf1
      tiny_int_deserialize,              // 0xf2
      tiny_int_deserialize,              // 0xf3
      tiny_int_deserialize,              // 0xf4
      tiny_int_deserialize,              // 0xf5
      tiny_int_deserialize,              // 0xf6
      tiny_int_deserialize,              // 0xf7
      tiny_int_deserialize,              // 0xf8
      tiny_int_deserialize,              // 0xf9
      tiny_int_deserialize,              // 0xfa
      tiny_int_deserialize,              // 0xfb
      tiny_int_deserialize,              // 0xfc
      tiny_int_deserialize,              // 0xfd
      tiny_int_deserialize,              // 0xfe
      tiny_int_deserialize };            // 0xff


typedef int (*struct_deserializer_t)(uint16_t nfields, neo4j_value_t *fields,
            neo4j_mpool_t *pool, neo4j_value_t *value);

#define STRUCT_DESERIALIZER_FUNC_DEF(funcname) \
    static int funcname(uint16_t nfields, neo4j_value_t *fields, \
            neo4j_mpool_t *pool, neo4j_value_t *value)

STRUCT_DESERIALIZER_FUNC_DEF(node_deserialize);
STRUCT_DESERIALIZER_FUNC_DEF(rel_deserialize);
STRUCT_DESERIALIZER_FUNC_DEF(path_deserialize);
STRUCT_DESERIALIZER_FUNC_DEF(unbound_rel_deserialize);
STRUCT_DESERIALIZER_FUNC_DEF(point2d_deserialize);
STRUCT_DESERIALIZER_FUNC_DEF(point3d_deserialize);
STRUCT_DESERIALIZER_FUNC_DEF(local_datetime_deserialize);
STRUCT_DESERIALIZER_FUNC_DEF(offset_datetime_deserialize);
STRUCT_DESERIALIZER_FUNC_DEF(zoned_datetime_deserialize);
STRUCT_DESERIALIZER_FUNC_DEF(local_date_deserialize);
STRUCT_DESERIALIZER_FUNC_DEF(local_time_deserialize);

static const struct_deserializer_t struct_deserializers[UINT8_MAX+1] =
    { NULL,                              // 0x00
      NULL,                              // 0x01
      NULL,                              // 0x02
      NULL,                              // 0x03
      NULL,                              // 0x04
      NULL,                              // 0x05
      NULL,                              // 0x06
      NULL,                              // 0x07
      NULL,                              // 0x08
      NULL,                              // 0x09
      NULL,                              // 0x0a
      NULL,                              // 0x0b
      NULL,                              // 0x0c
      NULL,                              // 0x0d
      NULL,                              // 0x0e
      NULL,                              // 0x0f
      NULL,                              // 0x10
      NULL,                              // 0x11
      NULL,                              // 0x12
      NULL,                              // 0x13
      NULL,                              // 0x14
      NULL,                              // 0x15
      NULL,                              // 0x16
      NULL,                              // 0x17
      NULL,                              // 0x18
      NULL,                              // 0x19
      NULL,                              // 0x1a
      NULL,                              // 0x1b
      NULL,                              // 0x1c
      NULL,                              // 0x1d
      NULL,                              // 0x1e
      NULL,                              // 0x1f
      NULL,                              // 0x20
      NULL,                              // 0x21
      NULL,                              // 0x22
      NULL,                              // 0x23
      NULL,                              // 0x24
      NULL,                              // 0x25
      NULL,                              // 0x26
      NULL,                              // 0x27
      NULL,                              // 0x28
      NULL,                              // 0x29
      NULL,                              // 0x2a
      NULL,                              // 0x2b
      NULL,                              // 0x2c
      NULL,                              // 0x2d
      NULL,                              // 0x2e
      NULL,                              // 0x2f
      NULL,                              // 0x30
      NULL,                              // 0x31
      NULL,                              // 0x32
      NULL,                              // 0x33
      NULL,                              // 0x34
      NULL,                              // 0x35
      NULL,                              // 0x36
      NULL,                              // 0x37
      NULL,                              // 0x38
      NULL,                              // 0x39
      NULL,                              // 0x3a
      NULL,                              // 0x3b
      NULL,                              // 0x3c
      NULL,                              // 0x3d
      NULL,                              // 0x3e
      NULL,                              // 0x3f
      NULL,                              // 0x40
      NULL,                              // 0x41
      NULL,                              // 0x42
      NULL,                              // 0x43
      local_date_deserialize,            // 0x44
      NULL,                              // 0x45
      offset_datetime_deserialize,       // 0x46
      NULL,                              // 0x47
      NULL,                              // 0x48
      NULL,                              // 0x49
      NULL,                              // 0x4a
      NULL,                              // 0x4b
      NULL,                              // 0x4c
      NULL,                              // 0x4d
      node_deserialize,                  // 0x4e
      NULL,                              // 0x4f
      path_deserialize,                  // 0x50
      NULL,                              // 0x51
      rel_deserialize,                   // 0x52
      NULL,                              // 0x53
      NULL,                              // 0x54
      NULL,                              // 0x55
      NULL,                              // 0x56
      NULL,                              // 0x57
      point2d_deserialize,               // 0x58
      point3d_deserialize,               // 0x59
      NULL,                              // 0x5a
      NULL,                              // 0x5b
      NULL,                              // 0x5c
      NULL,                              // 0x5d
      NULL,                              // 0x5e
      NULL,                              // 0x5f
      NULL,                              // 0x60
      NULL,                              // 0x61
      NULL,                              // 0x62
      NULL,                              // 0x63
      local_datetime_deserialize,        // 0x64
      NULL,                              // 0x65
      zoned_datetime_deserialize,        // 0x66
      NULL,                              // 0x67
      NULL,                              // 0x68
      NULL,                              // 0x69
      NULL,                              // 0x6a
      NULL,                              // 0x6b
      NULL,                              // 0x6c
      NULL,                              // 0x6d
      NULL,                              // 0x6e
      NULL,                              // 0x6f
      NULL,                              // 0x70
      NULL,                              // 0x71
      unbound_rel_deserialize,           // 0x72
      NULL,                              // 0x73
      local_time_deserialize,            // 0x74
      NULL,                              // 0x75
      NULL,                              // 0x76
      NULL,                              // 0x77
      NULL,                              // 0x78
      NULL,                              // 0x79
      NULL,                              // 0x7a
      NULL,                              // 0x7b
      NULL,                              // 0x7c
      NULL,                              // 0x7d
      NULL,                              // 0x7e
      NULL,                              // 0x7f
      NULL,                              // 0x80
      NULL,                              // 0x81
      NULL,                              // 0x82
      NULL,                              // 0x83
      NULL,                              // 0x84
      NULL,                              // 0x85
      NULL,                              // 0x86
      NULL,                              // 0x87
      NULL,                              // 0x88
      NULL,                              // 0x89
      NULL,                              // 0x8a
      NULL,                              // 0x8b
      NULL,                              // 0x8c
      NULL,                              // 0x8d
      NULL,                              // 0x8e
      NULL,                              // 0x8f
      NULL,                              // 0x90
      NULL,                              // 0x91
      NULL,                              // 0x92
      NULL,                              // 0x93
      NULL,                              // 0x94
      NULL,                              // 0x95
      NULL,                              // 0x96
      NULL,                              // 0x97
      NULL,                              // 0x98
      NULL,                              // 0x99
      NULL,                              // 0x9a
      NULL,                              // 0x9b
      NULL,                              // 0x9c
      NULL,                              // 0x9d
      NULL,                              // 0x9e
      NULL,                              // 0x9f
      NULL,                              // 0xa0
      NULL,                              // 0xa1
      NULL,                              // 0xa2
      NULL,                              // 0xa3
      NULL,                              // 0xa4
      NULL,                              // 0xa5
      NULL,                              // 0xa6
      NULL,                              // 0xa7
      NULL,                              // 0xa8
      NULL,                              // 0xa9
      NULL,                              // 0xaa
      NULL,                              // 0xab
      NULL,                              // 0xac
      NULL,                              // 0xad
      NULL,                              // 0xae
      NULL,                              // 0xaf
      NULL,                              // 0xb0
      NULL,                              // 0xb1
      NULL,                              // 0xb2
      NULL,                              // 0xb3
      NULL,                              // 0xb4
      NULL,                              // 0xb5
      NULL,                              // 0xb6
      NULL,                              // 0xb7
      NULL,                              // 0xb8
      NULL,                              // 0xb9
      NULL,                              // 0xba
      NULL,                              // 0xbb
      NULL,                              // 0xbc
      NULL,                              // 0xbd
      NULL,                              // 0xbe
      NULL,                              // 0xbf
      NULL,                              // 0xc0
      NULL,                              // 0xc1
      NULL,                              // 0xc2
      NULL,                              // 0xc3
      NULL,                              // 0xc4
      NULL,                              // 0xc5
      NULL,                              // 0xc6
      NULL,                              // 0xc7
      NULL,                              // 0xc8
      NULL,                              // 0xc9
      NULL,                              // 0xca
      NULL,                              // 0xcb
      NULL,                              // 0xcc
      NULL,                              // 0xcd
      NULL,                              // 0xce
      NULL,                              // 0xcf
      NULL,                              // 0xd0
      NULL,                              // 0xd1
      NULL,                              // 0xd2
      NULL,                              // 0xd3
      NULL,                              // 0xd4
      NULL,                              // 0xd5
      NULL,                              // 0xd6
      NULL,                              // 0xd7
      NULL,                              // 0xd8
      NULL,                              // 0xd9
      NULL,                              // 0xda
      NULL,                              // 0xdb
      NULL,                              // 0xdc
      NULL,                              // 0xdd
      NULL,                              // 0xde
      NULL,                              // 0xdf
      NULL,                              // 0xe0
      NULL,                              // 0xe1
      NULL,                              // 0xe2
      NULL,                              // 0xe3
      NULL,                              // 0xe4
      NULL,                              // 0xe5
      NULL,                              // 0xe6
      NULL,                              // 0xe7
      NULL,                              // 0xe8
      NULL,                              // 0xe9
      NULL,                              // 0xea
      NULL,                              // 0xeb
      NULL,                              // 0xec
      NULL,                              // 0xed
      NULL,                              // 0xee
      NULL,                              // 0xef
      NULL,                              // 0xf0
      NULL,                              // 0xf1
      NULL,                              // 0xf2
      NULL,                              // 0xf3
      NULL,                              // 0xf4
      NULL,                              // 0xf5
      NULL,                              // 0xf6
      NULL,                              // 0xf7
      NULL,                              // 0xf8
      NULL,                              // 0xf9
      NULL,                              // 0xfa
      NULL,                              // 0xfb
      NULL,                              // 0xfc
      NULL,                              // 0xfd
      NULL,                              // 0xfe
      NULL };                            // 0xff


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

    deserializer_t deserialize = deserializers[marker];
    if (deserialize == NULL)
    {
        errno = EPROTO;
        goto failure;
    }

    if (deserialize(marker, stream, pool, value))
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

    double_data.data = be64toh(double_data.data);
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
    data = be64toh(data);
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

int bytes8_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint8_t length;
    if (neo4j_ios_read_all(stream, &length, sizeof(length), NULL) < 0)
    {
        return -1;
    }
    return bytes_deserialize(length, stream, pool, value);
}

int bytes16_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint16_t length;
    if (neo4j_ios_read_all(stream, &length, sizeof(length), NULL) < 0)
    {
        return -1;
    }
    length = ntohs(length);
    return bytes_deserialize(length, stream, pool, value);
}

int bytes32_deserialize(uint8_t marker, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    uint32_t length;
    if (neo4j_ios_read_all(stream, &length, sizeof(length), NULL) < 0)
    {
        return -1;
    }
    length = ntohl(length);
    return bytes_deserialize(length, stream, pool, value);
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


int bytes_deserialize(uint32_t length, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    char *bytes = NULL;
    if (length > 0)
    {
        bytes = neo4j_mpool_alloc(pool, length);
        if (bytes == NULL)
        {
            return -1;
        }

        if (neo4j_ios_read_all(stream, bytes, length, NULL) < 0)
        {
            return -1;
        }
    }

    *value = neo4j_bytes(bytes, length);
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
            if (neo4j_deserialize(stream, pool, &(entries[i].value)))
            {
                return -1;
            }
        }
    }

    neo4j_value_t v = neo4j_map(entries, nentries);
    if (neo4j_is_null(v))
    {
        errno = EPROTO;
        return -1;
    }
    *value = v;
    return 0;
}


int struct_deserialize(uint16_t nfields, neo4j_iostream_t *stream,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    size_t pdepth = neo4j_mpool_depth(*pool);

    uint8_t signature;
    if (neo4j_ios_read_all(stream, &signature, sizeof(signature), NULL) < 0)
    {
        goto failure;
    }

    neo4j_value_t *fields = NULL;
    if (nfields > 0)
    {
        fields = neo4j_mpool_calloc(pool, nfields, sizeof(neo4j_value_t));
        if (fields == NULL)
        {
            goto failure;
        }

        for (unsigned i = 0; i < nfields; ++i)
        {
            if (neo4j_deserialize(stream, pool, &(fields[i])))
            {
                goto failure;
            }
        }
    }

    neo4j_value_t v;
    const struct_deserializer_t deserializer = struct_deserializers[signature];
    if (deserializer != NULL)
    {
        if (deserializer(nfields, fields, pool, &v))
        {
            goto failure;
        }
    }
    else
    {
        v = neo4j_struct(signature, fields, nfields);
    }

    if (neo4j_is_null(v))
    {
        errno = EPROTO;
        goto failure;
    }
    *value = v;
    return 0;

    int errsv;
failure:
    errsv = errno;
    neo4j_mpool_drainto(pool, pdepth);
    errno = errsv;
    return -1;
}


int node_deserialize(uint16_t nfields, neo4j_value_t *fields,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    if (nfields != 3 || neo4j_type(fields[0]) != NEO4J_INT)
    {
        errno = EPROTO;
        return -1;
    }
    fields[0] = neo4j_identity(neo4j_int_value(fields[0]));
    if (neo4j_is_null(fields[0]))
    {
        errno = EPROTO;
        return -1;
    }
    *value = neo4j_node(fields);
    return 0;
}


int rel_deserialize(uint16_t nfields, neo4j_value_t *fields,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    if (nfields != 5 ||
            neo4j_type(fields[0]) != NEO4J_INT ||
            neo4j_type(fields[1]) != NEO4J_INT ||
            neo4j_type(fields[2]) != NEO4J_INT)
    {
        errno = EPROTO;
        return -1;
    }
    fields[0] = neo4j_identity(neo4j_int_value(fields[0]));
    fields[1] = neo4j_identity(neo4j_int_value(fields[1]));
    fields[2] = neo4j_identity(neo4j_int_value(fields[2]));
    if (neo4j_is_null(fields[0]) ||
            neo4j_is_null(fields[1]) ||
            neo4j_is_null(fields[2]))
    {
        errno = EPROTO;
        return -1;
    }
    *value = neo4j_relationship(fields);
    return 0;
}


int path_deserialize(uint16_t nfields, neo4j_value_t *fields,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    if (nfields != 3)
    {
        errno = EPROTO;
        return -1;
    }
    *value = neo4j_path(fields);
    return 0;
}


int unbound_rel_deserialize(uint16_t nfields, neo4j_value_t *fields,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    if (nfields != 3 || neo4j_type(fields[0]) != NEO4J_INT)
    {
        errno = EPROTO;
        return -1;
    }
    fields[0] = neo4j_identity(neo4j_int_value(fields[0]));
    if (neo4j_is_null(fields[0]))
    {
        errno = EPROTO;
        return -1;
    }
    *value = neo4j_unbound_relationship(fields);
    return 0;
}


// rather than allocate point data, re-use the fields buffer after
// deserialization
static_assert(2*sizeof(neo4j_value_t) > sizeof(neo4j_point_data_t),
        "point data should be smaller than 2 values");
int point2d_deserialize(uint16_t nfields, neo4j_value_t *fields,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    if (nfields != 3 || neo4j_type(fields[0]) != NEO4J_INT ||
            neo4j_type(fields[1]) != NEO4J_FLOAT ||
            neo4j_type(fields[2]) != NEO4J_FLOAT)
    {
        errno = EPROTO;
        return -1;
    }
    long long srid = neo4j_int_value(fields[0]);
    if (srid < 0 || srid > UINT_MAX)
    {
        errno = EPROTO;
        return -1;
    }
    double x = neo4j_float_value(fields[1]);
    double y = neo4j_float_value(fields[2]);
    *value = neo4j_2d_point((neo4j_point_data_t *)fields, srid, x, y);
    return 0;
}


int point3d_deserialize(uint16_t nfields, neo4j_value_t *fields,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    if (nfields != 4 || neo4j_type(fields[0]) != NEO4J_INT ||
            neo4j_type(fields[1]) != NEO4J_FLOAT ||
            neo4j_type(fields[2]) != NEO4J_FLOAT ||
            neo4j_type(fields[3]) != NEO4J_FLOAT)
    {
        errno = EPROTO;
        return -1;
    }
    long long srid = neo4j_int_value(fields[0]);
    if (srid < 0 || srid > UINT_MAX)
    {
        errno = EPROTO;
        return -1;
    }
    double x = neo4j_float_value(fields[1]);
    double y = neo4j_float_value(fields[2]);
    double z = neo4j_float_value(fields[3]);
    *value = neo4j_3d_point((neo4j_point_data_t *)fields, srid, x, y, z);
    return 0;
}


int local_datetime_deserialize(uint16_t nfields, neo4j_value_t *fields,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    if (nfields != 2 || neo4j_type(fields[0]) != NEO4J_INT ||
            neo4j_type(fields[1]) != NEO4J_INT)
    {
        errno = EPROTO;
        return -1;
    }
    long long epoch_seconds = neo4j_int_value(fields[0]);
    long long nanoseconds = neo4j_int_value(fields[1]);
    if (nanoseconds < INT_MIN || nanoseconds > INT_MAX)
    {
        errno = EPROTO;
        return -1;
    }
    *value = neo4j_local_datetime_from_epoch(epoch_seconds, nanoseconds);
    return 0;
}


int offset_datetime_deserialize(uint16_t nfields, neo4j_value_t *fields,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    if (nfields != 3 || neo4j_type(fields[0]) != NEO4J_INT ||
            neo4j_type(fields[1]) != NEO4J_INT ||
            neo4j_type(fields[2]) != NEO4J_INT)
    {
        errno = EPROTO;
        return -1;
    }
    long long epoch_seconds = neo4j_int_value(fields[0]);
    long long nanoseconds = neo4j_int_value(fields[1]);
    if (nanoseconds < INT_MIN || nanoseconds > INT_MAX)
    {
        errno = EPROTO;
        return -1;
    }
    long long offset_seconds = neo4j_int_value(fields[2]);
    if (offset_seconds < -64800 || offset_seconds > 64800)
    {
        errno = EPROTO;
        return -1;
    }
    *value = neo4j_offset_datetime_from_epoch(epoch_seconds, nanoseconds,
            offset_seconds);
    return 0;
}


// rather than allocate zone data, re-use the fields buffer after
// deserialization
static_assert(3*sizeof(neo4j_value_t) > sizeof(neo4j_point_data_t),
        "zone data should be smaller than 3 values");
int zoned_datetime_deserialize(uint16_t nfields, neo4j_value_t *fields,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    if (nfields != 3 || neo4j_type(fields[0]) != NEO4J_INT ||
            neo4j_type(fields[1]) != NEO4J_INT ||
            neo4j_type(fields[2]) != NEO4J_STRING)
    {
        errno = EPROTO;
        return -1;
    }
    long long epoch_seconds = neo4j_int_value(fields[0]);
    long long nanoseconds = neo4j_int_value(fields[1]);
    if (nanoseconds < INT_MIN || nanoseconds > INT_MAX)
    {
        errno = EPROTO;
        return -1;
    }

    size_t zoneid_length = neo4j_string_length(fields[2]);
    char *zoneid = neo4j_mpool_alloc(pool, zoneid_length + 1);
    if (zoneid == NULL)
    {
        return -1;
    }
    neo4j_string_value(fields[2], zoneid, zoneid_length + 1);
    *value = neo4j_zoned_datetime_from_epoch((neo4j_zone_data_t *)fields,
            epoch_seconds, nanoseconds, zoneid);
    return 0;
}


int local_date_deserialize(uint16_t nfields, neo4j_value_t *fields,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    if (nfields != 1 || neo4j_type(fields[0]) != NEO4J_INT)
    {
        errno = EPROTO;
        return -1;
    }
    long long epoch_days = neo4j_int_value(fields[0]);
    *value = neo4j_local_date_from_epoch(epoch_days);
    return 0;
}


int local_time_deserialize(uint16_t nfields, neo4j_value_t *fields,
        neo4j_mpool_t *pool, neo4j_value_t *value)
{
    if (nfields != 1 || neo4j_type(fields[0]) != NEO4J_INT)
    {
        errno = EPROTO;
        return -1;
    }
    long long nanos = neo4j_int_value(fields[0]);
    *value = neo4j_local_time_from_midnight(nanos / 1000000000,
            nanos % 1000000000);
    return 0;
}
