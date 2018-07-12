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
#include "../src/deserialization.h"
#include "../src/iostream.h"
#include "../src/ring_buffer.h"
#include "../src/values.h"
#include "memiostream.h"
#include <check.h>
#include <errno.h>
#include <unistd.h>


static ring_buffer_t *rb;
static neo4j_iostream_t *ios;
static neo4j_mpool_t mpool;


static void setup(void)
{
    rb = rb_alloc(1024);
    ios = neo4j_loopback_iostream(rb);
    mpool = neo4j_mpool(&neo4j_std_memory_allocator, 128);
}


static void teardown(void)
{
    neo4j_mpool_drain(&mpool);
    neo4j_ios_close(ios);
    rb_free(rb);
}


START_TEST (deserialize_positive_tiny_int)
{
    uint8_t byte = 0x7F;
    rb_append(rb, &byte, 1);
    byte = 0x00;
    rb_append(rb, &byte, 1);

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), 127);

    n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), 0);

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_tiny_string)
{
    uint8_t bytes[] = { 0x86, 0x62, 0x65, 0x72, 0x6e, 0x69, 0x65 };
    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_STRING);
    ck_assert_int_eq(neo4j_string_length(value), 6);
    char buf[16];
    ck_assert_str_eq(neo4j_string_value(value, buf, sizeof(buf)), "bernie");

    ck_assert_int_eq(neo4j_string_length(value), 6);
    ck_assert(memcmp(neo4j_ustring_value(value), "bernie", 6) == 0);

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_tiny_list)
{
    uint8_t bytes[] = { 0x95, 0x05, 0x04, 0x03, 0x02, 0x01 };
    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_LIST);
    ck_assert_int_eq(neo4j_list_length(value), 5);

    for (int i = 0; i < 5; ++i)
    {
        const neo4j_value_t item = neo4j_list_get(value, i);
        ck_assert_int_eq(neo4j_type(item), NEO4J_INT);
        ck_assert_int_eq(neo4j_int_value(item), 5-i);
    }

    const neo4j_value_t item = neo4j_list_get(value, 6);
    ck_assert(neo4j_is_null(item));

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_tiny_map)
{
    uint8_t bytes[] =
            { 0xA3, 0x81, 0x62, 0x01, 0x81, 0x65, 0x02, 0x81, 0x72, 0x03 };
    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_MAP);
    ck_assert_int_eq(neo4j_map_size(value), 3);

    char buf[16];
    char *expected[] = { "b", "e", "r" };

    for (int i = 0; i < 3; ++i)
    {
        const neo4j_map_entry_t *entry = neo4j_map_getentry(value, i);
        ck_assert_int_eq(neo4j_type(entry->key), NEO4J_STRING);
        ck_assert_str_eq(neo4j_string_value(entry->key, buf, sizeof(buf)),
                expected[i]);

        ck_assert_int_eq(neo4j_type(entry->value), NEO4J_INT);
        ck_assert_int_eq(neo4j_int_value(entry->value), i+1);
    }

    const neo4j_map_entry_t *entry = neo4j_map_getentry(value, 6);
    ck_assert_ptr_eq(entry, NULL);

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_tiny_struct)
{
    uint8_t bytes[] = { 0xB2, 0x78, 0x01, 0xCA, 0x00, 0x7F, 0x57, 0x77 };
    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_STRUCT);
    ck_assert_int_eq(neo4j_struct_signature(value), 0x78);
    ck_assert_int_eq(neo4j_struct_size(value), 2);

    const neo4j_value_t field0 = neo4j_struct_getfield(value, 0);
    ck_assert_int_eq(neo4j_type(field0), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(field0), 1);

    const neo4j_value_t field1 = neo4j_struct_getfield(value, 1);
    ck_assert_int_eq(neo4j_type(field1), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(field1), 8345463);

    const neo4j_value_t field2 = neo4j_struct_getfield(value, 2);
    ck_assert(neo4j_is_null(field2));

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_null)
{
    uint8_t byte = 0xC0;
    rb_append(rb, &byte, 1);

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_NULL);

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_float)
{
    uint8_t bytes[] =
            { 0xC1, 0x3F, 0xF1, 0x99, 0x99, 0x99, 0x99, 0x99, 0x9A,
              0xC1, 0xBF, 0xF1, 0x99, 0x99, 0x99, 0x99, 0x99, 0x9A };
    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_FLOAT);
    ck_assert(neo4j_float_value(value) == 1.1);

    n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_FLOAT);
    ck_assert(neo4j_float_value(value) == -1.1);

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_boolean_false)
{
    uint8_t byte = 0xC2;
    rb_append(rb, &byte, 1);

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_BOOL);
    ck_assert_int_eq(neo4j_bool_value(value), false);

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_boolean_true)
{
    uint8_t byte = 0xC3;
    rb_append(rb, &byte, 1);

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_BOOL);
    ck_assert_int_eq(neo4j_bool_value(value), true);

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_int8)
{
    uint8_t bytes[] = { 0xC8, 0xD6, 0xC8, 0x80, 0xC8, 0xEF, 0xC8, 0x7F };
    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), -42);

    n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), -128);

    n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), -17);

    n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), 127);

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_int16)
{
    uint8_t bytes[] =
            { 0xC9, 0xD8, 0xF1, 0xC9, 0x80, 0x00,
              0xC9, 0x7F, 0xFF, 0xC9, 0x00, 0x7F };
    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), -9999);

    n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), -32768);

    n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), 32767);

    n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), 127);

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_int32)
{
    uint8_t bytes[] =
            { 0xCA, 0x3B, 0x9A, 0xCA, 0x00,
              0xCA, 0x80, 0x00, 0x00, 0x00,
              0xCA, 0x7F, 0xFF, 0xFF, 0xFF,
              0xCA, 0x00, 0x00, 0x00, 0x7F };
    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), 1000000000L);

    n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), -2147483648L);

    n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), 2147483647L);

    n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), 127);

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST

START_TEST (deserialize_int64)
{
    uint8_t bytes[] =
            { 0xCB, 0x9B, 0xC1, 0x86, 0x65, 0x88, 0xF6, 0x80, 0x94,
              0xCB, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0xCB, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
              0xCB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F };
    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), -7223344556677889900LL);

    n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), -9223372036854775807LL-1);

    n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), 9223372036854775807LL);

    n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), 127);

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_string8)
{
    uint8_t bytes[] =
            { 0xD0, 0x92, 0x54, 0x68, 0x69, 0x73, 0x20, 0x69,
              0x73, 0x20, 0x67, 0x6F, 0x69, 0x6E, 0x67, 0x20,
              0x74, 0x6F, 0x20, 0x62, 0x65, 0x20, 0x61, 0x20,
              0x76, 0x65, 0x72, 0x79, 0x20, 0x65, 0x78, 0x70,
              0x65, 0x6E, 0x73, 0x69, 0x76, 0x65, 0x20, 0x77,
              0x61, 0x72, 0x2C, 0x20, 0x61, 0x6E, 0x64, 0x20,
              0x56, 0x69, 0x63, 0x74, 0x6F, 0x72, 0x79, 0x20,
              0x69, 0x73, 0x20, 0x6E, 0x6F, 0x74, 0x20, 0x67,
              0x75, 0x61, 0x72, 0x61, 0x6E, 0x74, 0x65, 0x65,
              0x64, 0x20, 0xE2, 0x80, 0x94, 0x20, 0x66, 0x6F,
              0x72, 0x20, 0x61, 0x6E, 0x79, 0x6F, 0x6E, 0x65,
              0x2C, 0x20, 0x61, 0x6E, 0x64, 0x20, 0x63, 0x65,
              0x72, 0x74, 0x61, 0x69, 0x6E, 0x6C, 0x79, 0x20,
              0x6E, 0x6F, 0x74, 0x20, 0x66, 0x6F, 0x72, 0x20,
              0x61, 0x6E, 0x79, 0x6F, 0x6E, 0x65, 0x20, 0x61,
              0x73, 0x20, 0x62, 0x61, 0x66, 0x66, 0x6C, 0x65,
              0x64, 0x20, 0x61, 0x73, 0x20, 0x47, 0x65, 0x6F,
              0x72, 0x67, 0x65, 0x20, 0x57, 0x2E, 0x20, 0x42,
              0x75, 0x73, 0x68, 0x2E };
    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_STRING);
    ck_assert_int_eq(neo4j_string_length(value), 146);
    char buf[256];
    ck_assert_str_eq(neo4j_string_value(value, buf, sizeof(buf)),
            "This is going to be a very expensive war, and Victory is not "
            "guaranteed — for anyone, and certainly not for anyone as "
            "baffled as George W. Bush.");

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_string16)
{
    uint8_t bytes[] =
            { 0xD1, 0x01, 0xBA, 0x4D, 0x6F, 0x73, 0x74, 0x20,
              0x70, 0x65, 0x6F, 0x70, 0x6C, 0x65, 0x20, 0x77,
              0x68, 0x6F, 0x20, 0x64, 0x65, 0x61, 0x6C, 0x20,
              0x69, 0x6E, 0x20, 0x77, 0x6F, 0x72, 0x64, 0x73,
              0x20, 0x64, 0x6F, 0x6E, 0x27, 0x74, 0x20, 0x68,
              0x61, 0x76, 0x65, 0x20, 0x6D, 0x75, 0x63, 0x68,
              0x20, 0x66, 0x61, 0x69, 0x74, 0x68, 0x20, 0x69,
              0x6E, 0x20, 0x74, 0x68, 0x65, 0x6D, 0x20, 0x61,
              0x6E, 0x64, 0x20, 0x49, 0x20, 0x61, 0x6D, 0x20,
              0x6E, 0x6F, 0x20, 0x65, 0x78, 0x63, 0x65, 0x70,
              0x74, 0x69, 0x6F, 0x6E, 0x20, 0xE2, 0x80, 0x94,
              0x20, 0x65, 0x73, 0x70, 0x65, 0x63, 0x69, 0x61,
              0x6C, 0x6C, 0x79, 0x20, 0x74, 0x68, 0x65, 0x20,
              0x62, 0x69, 0x67, 0x20, 0x6F, 0x6E, 0x65, 0x73,
              0x20, 0x6C, 0x69, 0x6B, 0x65, 0x20, 0x48, 0x61,
              0x70, 0x70, 0x79, 0x20, 0x61, 0x6E, 0x64, 0x20,
              0x4C, 0x6F, 0x76, 0x65, 0x20, 0x61, 0x6E, 0x64,
              0x20, 0x48, 0x6F, 0x6E, 0x65, 0x73, 0x74, 0x20,
              0x61, 0x6E, 0x64, 0x20, 0x53, 0x74, 0x72, 0x6F,
              0x6E, 0x67, 0x2E, 0x20, 0x54, 0x68, 0x65, 0x79,
              0x20, 0x61, 0x72, 0x65, 0x20, 0x74, 0x6F, 0x6F,
              0x20, 0x65, 0x6C, 0x75, 0x73, 0x69, 0x76, 0x65,
              0x20, 0x61, 0x6E, 0x64, 0x20, 0x66, 0x61, 0x72,
              0x20, 0x74, 0x6F, 0x6F, 0x20, 0x72, 0x65, 0x6C,
              0x61, 0x74, 0x69, 0x76, 0x65, 0x20, 0x77, 0x68,
              0x65, 0x6E, 0x20, 0x79, 0x6F, 0x75, 0x20, 0x63,
              0x6F, 0x6D, 0x70, 0x61, 0x72, 0x65, 0x20, 0x74,
              0x68, 0x65, 0x6D, 0x20, 0x74, 0x6F, 0x20, 0x73,
              0x68, 0x61, 0x72, 0x70, 0x2C, 0x20, 0x6D, 0x65,
              0x61, 0x6E, 0x20, 0x6C, 0x69, 0x74, 0x74, 0x6C,
              0x65, 0x20, 0x77, 0x6F, 0x72, 0x64, 0x73, 0x20,
              0x6C, 0x69, 0x6B, 0x65, 0x20, 0x50, 0x75, 0x6E,
              0x6B, 0x20, 0x61, 0x6E, 0x64, 0x20, 0x43, 0x68,
              0x65, 0x61, 0x70, 0x20, 0x61, 0x6E, 0x64, 0x20,
              0x50, 0x68, 0x6F, 0x6E, 0x79, 0x2E, 0x20, 0x49,
              0x20, 0x66, 0x65, 0x65, 0x6C, 0x20, 0x61, 0x74,
              0x20, 0x68, 0x6F, 0x6D, 0x65, 0x20, 0x77, 0x69,
              0x74, 0x68, 0x20, 0x74, 0x68, 0x65, 0x73, 0x65,
              0x2C, 0x20, 0x62, 0x65, 0x63, 0x61, 0x75, 0x73,
              0x65, 0x20, 0x74, 0x68, 0x65, 0x79, 0x20, 0x61,
              0x72, 0x65, 0x20, 0x73, 0x63, 0x72, 0x61, 0x77,
              0x6E, 0x79, 0x20, 0x61, 0x6E, 0x64, 0x20, 0x65,
              0x61, 0x73, 0x79, 0x20, 0x74, 0x6F, 0x20, 0x70,
              0x69, 0x6E, 0x2C, 0x20, 0x62, 0x75, 0x74, 0x20,
              0x74, 0x68, 0x65, 0x20, 0x62, 0x69, 0x67, 0x20,
              0x6F, 0x6E, 0x65, 0x73, 0x20, 0x61, 0x72, 0x65,
              0x20, 0x74, 0x6F, 0x75, 0x67, 0x68, 0x20, 0x61,
              0x6E, 0x64, 0x20, 0x69, 0x74, 0x20, 0x74, 0x61,
              0x6B, 0x65, 0x73, 0x20, 0x65, 0x69, 0x74, 0x68,
              0x65, 0x72, 0x20, 0x61, 0x20, 0x70, 0x72, 0x69,
              0x65, 0x73, 0x74, 0x20, 0x6F, 0x72, 0x20, 0x61,
              0x20, 0x66, 0x6F, 0x6F, 0x6C, 0x20, 0x74, 0x6F,
              0x20, 0x75, 0x73, 0x65, 0x20, 0x74, 0x68, 0x65,
              0x6D, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x61,
              0x6E, 0x79, 0x20, 0x63, 0x6F, 0x6E, 0x66, 0x69,
              0x64, 0x65, 0x6E, 0x63, 0x65 };
    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_STRING);
    ck_assert_int_eq(neo4j_string_length(value), 442);
    char buf[1024];
    ck_assert_str_eq(neo4j_string_value(value, buf, sizeof(buf)),
            "Most people who deal in words don't have much faith in them and I "
            "am no exception — especially the big ones like Happy and Love and "
            "Honest and Strong. They are too elusive and far too relative when "
            "you compare them to sharp, mean little words like Punk and Cheap "
            "and Phony. I feel at home with these, because they are scrawny "
            "and easy to pin, but the big ones are tough and it takes either a "
            "priest or a fool to use them with any confidence");

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_list8)
{
    uint8_t bytes[] =
            { 0xD4, 0x10, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
              0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10 };
    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_LIST);
    ck_assert_int_eq(neo4j_list_length(value), 16);

    for (int i = 0; i < 16; ++i)
    {
        const neo4j_value_t item = neo4j_list_get(value, i);
        ck_assert_int_eq(neo4j_type(item), NEO4J_INT);
        ck_assert_int_eq(neo4j_int_value(item), i+1);
    }

    const neo4j_value_t item = neo4j_list_get(value, 16);
    ck_assert(neo4j_is_null(item));

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_list16)
{
    uint8_t bytes[] =
            { 0xD5, 0x01, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04,
              0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
              0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04,
              0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
              0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04,
              0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
              0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04,
              0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
              0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04,
              0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
              0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04,
              0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
              0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04,
              0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
              0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04,
              0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
              0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04,
              0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
              0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04,
              0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
              0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04,
              0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
              0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04,
              0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
              0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04,
              0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
              0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04,
              0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
              0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04,
              0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
              0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04,
              0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
              0x0D, 0x0E, 0x0F };
    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_LIST);
    ck_assert_int_eq(neo4j_list_length(value), 256);

    for (int i = 0; i < 256; ++i)
    {
        const neo4j_value_t item = neo4j_list_get(value, i);
        ck_assert_int_eq(neo4j_type(item), NEO4J_INT);
        ck_assert_int_eq(neo4j_int_value(item), i%16);
    }

    const neo4j_value_t item = neo4j_list_get(value, 256);
    ck_assert(neo4j_is_null(item));

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_map8)
{
    uint8_t bytes[] =
            { 0xD8, 0x10, 0x81, 0x30, 0x01, 0x81, 0x31, 0x02,
              0x81, 0x32, 0x03, 0x81, 0x33, 0x04, 0x81, 0x34,
              0x05, 0x81, 0x35, 0x06, 0x81, 0x36, 0x07, 0x81,
              0x37, 0x08, 0x81, 0x38, 0x09, 0x81, 0x39, 0x0A,
              0x81, 0x61, 0x0B, 0x81, 0x62, 0x0C, 0x81, 0x63,
              0x0D, 0x81, 0x64, 0x0E, 0x81, 0x65, 0x0F, 0x81,
              0x66, 0x10 };
    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_MAP);
    ck_assert_int_eq(neo4j_map_size(value), 16);

    char buf[16];
    char *expected[] =
            { "0", "1", "2", "3", "4", "5", "6", "7",
              "8", "9", "a", "b", "c", "d", "e", "f" };

    for (int i = 0; i < 16; ++i)
    {
        const neo4j_map_entry_t *entry = neo4j_map_getentry(value, i);
        ck_assert_int_eq(neo4j_type(entry->key), NEO4J_STRING);
        ck_assert_str_eq(neo4j_string_value(entry->key, buf, sizeof(buf)),
                expected[i]);

        ck_assert_int_eq(neo4j_type(entry->value), NEO4J_INT);
        ck_assert_int_eq(neo4j_int_value(entry->value), i+1);
    }

    const neo4j_map_entry_t *entry = neo4j_map_getentry(value, 16);
    ck_assert_ptr_eq(entry, NULL);

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_map8_with_invalid_key_type)
{
    uint8_t bytes[] =
            { 0xD8, 0x10, 0x81, 0x30, 0x01, 0x81, 0x31, 0x02,
              0x81, 0x32, 0x03, 0x81, 0x33, 0x04, 0x81, 0x34,
              0x05, 0x81, 0x35, 0x06, 0x81, 0x36, 0x07, 0x81,
              0x37, 0x08, 0x81, 0x38, 0x09, 0x81, 0x39, 0x0A,
              0x81, 0x61, 0x0B, 0x81, 0x62, 0x0C, 0x81, 0x63,
              0x0D, 0xC3, 0x0E, 0x81, 0x65, 0x0F, 0x81,
              0x66, 0x10 };
    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, -1);
    ck_assert_int_eq(errno, EPROTO);
}
END_TEST


START_TEST (deserialize_struct8)
{
    uint8_t bytes[] =
            { 0xDC, 0x10, 0x78, 0x01, 0x02, 0x03, 0x04, 0x05,
              0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
              0x0E, 0x0F, 0x10 };
    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_STRUCT);
    ck_assert_int_eq(neo4j_struct_signature(value), 0x78);
    ck_assert_int_eq(neo4j_struct_size(value), 16);

    for (int i = 0; i < 16; ++i)
    {
        const neo4j_value_t field = neo4j_struct_getfield(value, i);
        ck_assert_int_eq(neo4j_type(field), NEO4J_INT);
        ck_assert_int_eq(neo4j_int_value(field), i+1);
    }

    const neo4j_value_t field = neo4j_struct_getfield(value, 16);
    ck_assert(neo4j_is_null(field));

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_struct16)
{
    uint8_t bytes[] =
            { 0xDD, 0x01, 0x00, 0x78, 0x00, 0x01, 0x02, 0x03,
              0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
              0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03,
              0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
              0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03,
              0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
              0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03,
              0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
              0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03,
              0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
              0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03,
              0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
              0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03,
              0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
              0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03,
              0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
              0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03,
              0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
              0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03,
              0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
              0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03,
              0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
              0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03,
              0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
              0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03,
              0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
              0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03,
              0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
              0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03,
              0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
              0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03,
              0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
              0x0C, 0x0D, 0x0E, 0x0F };

    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_STRUCT);
    ck_assert_int_eq(neo4j_struct_signature(value), 0x78);
    ck_assert_int_eq(neo4j_struct_size(value), 256);

    const neo4j_value_t *fields = neo4j_struct_fields(value);
    ck_assert_ptr_ne(fields, NULL);

    for (int i = 0; i < 256; ++i)
    {
        const neo4j_value_t field = neo4j_struct_getfield(value, i);
        ck_assert_int_eq(neo4j_type(field), NEO4J_INT);
        ck_assert_int_eq(neo4j_int_value(field), i%16);
    }

    const neo4j_value_t field = neo4j_struct_getfield(value, 256);
    ck_assert(neo4j_is_null(field));

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_negative_tiny_int)
{
    uint8_t byte = 0xFF;
    rb_append(rb, &byte, 1);
    byte = 0xF0;
    rb_append(rb, &byte, 1);

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), -1);

    n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_INT);
    ck_assert_int_eq(neo4j_int_value(value), -16);

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_node)
{
    uint8_t bytes[] =
            { 0xDC, 0x03, 0x4E, 0x01, 0x91, 0x8A, 0x4A, 0x6f,
              0x75, 0x72, 0x6E, 0x61, 0x6C, 0x69, 0x73, 0x74,
              0xA1, 0x84, 0x74, 0x79, 0x70, 0x65, 0x85, 0x47,
              0x6F, 0x6E, 0x7A, 0x6F };

    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_NODE);

    neo4j_value_t labels = neo4j_node_labels(value);
    ck_assert_int_eq(neo4j_type(labels), NEO4J_LIST);
    ck_assert_int_eq(neo4j_list_length(labels), 1);
    char buf[16];
    neo4j_value_t label = neo4j_list_get(labels, 0);
    ck_assert_int_eq(neo4j_type(label), NEO4J_STRING);
    ck_assert_str_eq(neo4j_string_value(label, buf, sizeof(buf)),
            "Journalist");

    neo4j_value_t props = neo4j_node_properties(value);
    ck_assert_int_eq(neo4j_type(props), NEO4J_MAP);
    ck_assert_int_eq(neo4j_map_size(props), 1);
    const neo4j_map_entry_t *entry = neo4j_map_getentry(props, 0);
    ck_assert_ptr_ne(entry, NULL);
    ck_assert_int_eq(neo4j_type(entry->key), NEO4J_STRING);
    ck_assert_int_eq(neo4j_type(entry->value), NEO4J_STRING);
    ck_assert_str_eq(neo4j_string_value(entry->key, buf, sizeof(buf)),
            "type");
    ck_assert_str_eq(neo4j_string_value(entry->value, buf, sizeof(buf)),
            "Gonzo");

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_node_with_incorrect_field_count)
{
    uint8_t bytes[] =
            { 0xDC, 0x02, 0x4E, 0x01, 0x91, 0x8A, 0x4A, 0x6f,
              0x75, 0x72, 0x6E, 0x61, 0x6C, 0x69, 0x73, 0x74 };

    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, -1);
    ck_assert_int_eq(errno, EPROTO);
}
END_TEST


START_TEST (deserialize_node_with_incorrect_identifier_type)
{
    uint8_t bytes[] =
            { 0xDC, 0x03, 0x4E, 0xC3, 0x91, 0x8A, 0x4A, 0x6f,
              0x75, 0x72, 0x6E, 0x61, 0x6C, 0x69, 0x73, 0x74,
              0xA1, 0x84, 0x74, 0x79, 0x70, 0x65, 0x85, 0x47,
              0x6F, 0x6E, 0x7A, 0x6F };

    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, -1);
    ck_assert_int_eq(errno, EPROTO);
}
END_TEST


START_TEST (deserialize_node_with_incorrect_labels_type)
{
    uint8_t bytes[] =
            { 0xDC, 0x03, 0x4E, 0x01, 0xC3,
              0xA1, 0x84, 0x74, 0x79, 0x70, 0x65, 0x85, 0x47,
              0x6F, 0x6E, 0x7A, 0x6F };

    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, -1);
    ck_assert_int_eq(errno, EPROTO);
}
END_TEST


START_TEST (deserialize_node_with_bad_label_type)
{
    uint8_t bytes[] =
            { 0xDC, 0x03, 0x4E, 0xC3, 0x91, 0x8A, 0x4A, 0x6f,
              0x75, 0x72, 0x6E, 0x61, 0x6C, 0x69, 0x73, 0x74,
              0xA1, 0x84, 0x74, 0x79, 0x70, 0x65, 0x85, 0x47,
              0x6F, 0x6E, 0x7A, 0x6F };

    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, -1);
    ck_assert_int_eq(errno, EPROTO);
}
END_TEST


START_TEST (deserialize_node_with_incorrect_map_type)
{
    uint8_t bytes[] =
            { 0xDC, 0x03, 0x4E, 0x01, 0x91, 0x8A, 0x4A, 0x6f,
              0x75, 0x72, 0x6E, 0x61, 0x6C, 0x69, 0x73, 0x74,
              0xC3 };

    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, -1);
    ck_assert_int_eq(errno, EPROTO);
}
END_TEST


START_TEST (deserialize_relationship)
{
    uint8_t bytes[] =
            { 0xDC, 0x05, 0x52, 0x01, 0x01, 0x02, 0x8A, 0x4A,
              0x6f, 0x75, 0x72, 0x6E, 0x61, 0x6C, 0x69, 0x73,
              0x74, 0xA1, 0x84, 0x74, 0x79, 0x70, 0x65, 0x85,
              0x47, 0x6F, 0x6E, 0x7A, 0x6F };

    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_RELATIONSHIP);

    neo4j_value_t reltype = neo4j_relationship_type(value);
    ck_assert_int_eq(neo4j_type(reltype), NEO4J_STRING);
    char buf[16];
    ck_assert_str_eq(neo4j_string_value(reltype, buf, sizeof(buf)),
            "Journalist");

    neo4j_value_t props = neo4j_relationship_properties(value);
    ck_assert_int_eq(neo4j_type(props), NEO4J_MAP);
    ck_assert_int_eq(neo4j_map_size(props), 1);
    const neo4j_map_entry_t *entry = neo4j_map_getentry(props, 0);
    ck_assert_ptr_ne(entry, NULL);
    ck_assert_int_eq(neo4j_type(entry->key), NEO4J_STRING);
    ck_assert_int_eq(neo4j_type(entry->value), NEO4J_STRING);
    ck_assert_str_eq(neo4j_string_value(entry->key, buf, sizeof(buf)),
            "type");
    ck_assert_str_eq(neo4j_string_value(entry->value, buf, sizeof(buf)),
            "Gonzo");

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_path)
{
    uint8_t bytes[] =
            { 0xDC, 0x03, 0x50, 0x92, 0xDC, 0x03, 0x4E, 0x01,
              0x91, 0x81, 0x41, 0xA0, 0xDC, 0x03, 0x4E, 0x02,
              0x91, 0x81, 0x42, 0xA0, 0x92, 0xDC, 0x03, 0x72,
              0x08, 0x81, 0x59, 0xA0, 0xDC, 0x03, 0x72, 0x09,
              0x81, 0x5A, 0xA0, 0x94, 0x01, 0x01, 0xFE, 0x00 };

    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_PATH);

    ck_assert_int_eq(neo4j_path_length(value), 2);

    neo4j_value_t node1 = neo4j_path_get_node(value, 0);
    ck_assert_int_eq(neo4j_type(node1), NEO4J_NODE);
    ck_assert_int_eq(neo4j_type(neo4j_node_labels(node1)), NEO4J_LIST);
    neo4j_value_t node1_label = neo4j_list_get(neo4j_node_labels(node1), 0);
    char buf[16];
    ck_assert_str_eq(neo4j_string_value(node1_label, buf, sizeof(buf)), "A");

    bool forward;
    neo4j_value_t rel1 = neo4j_path_get_relationship(value, 0, &forward);
    ck_assert_int_eq(neo4j_type(rel1), NEO4J_RELATIONSHIP);
    neo4j_value_t rel1_type = neo4j_relationship_type(rel1);
    ck_assert_int_eq(neo4j_type(rel1_type), NEO4J_STRING);
    ck_assert_str_eq(neo4j_string_value(rel1_type, buf, sizeof(buf)), "Y");
    ck_assert(forward == true);

    neo4j_value_t node2 = neo4j_path_get_node(value, 1);
    ck_assert_int_eq(neo4j_type(node2), NEO4J_NODE);
    ck_assert_int_eq(neo4j_type(neo4j_node_labels(node2)), NEO4J_LIST);
    neo4j_value_t node2_label = neo4j_list_get(neo4j_node_labels(node2), 0);
    ck_assert_str_eq(neo4j_string_value(node2_label, buf, sizeof(buf)), "B");

    neo4j_value_t rel2 = neo4j_path_get_relationship(value, 1, &forward);
    ck_assert_int_eq(neo4j_type(rel2), NEO4J_RELATIONSHIP);
    neo4j_value_t rel2_type = neo4j_relationship_type(rel2);
    ck_assert_int_eq(neo4j_type(rel2_type), NEO4J_STRING);
    ck_assert_str_eq(neo4j_string_value(rel2_type, buf, sizeof(buf)), "Z");
    ck_assert(forward == false);

    neo4j_value_t node3 = neo4j_path_get_node(value, 2);
    ck_assert_int_eq(neo4j_type(node3), NEO4J_NODE);
    ck_assert_int_eq(neo4j_type(neo4j_node_labels(node3)), NEO4J_LIST);
    neo4j_value_t node3_label = neo4j_list_get(neo4j_node_labels(node3), 0);
    ck_assert_str_eq(neo4j_string_value(node3_label, buf, sizeof(buf)), "A");

    ck_assert(neo4j_is_null(neo4j_path_get_node(value, 3)));
    ck_assert(neo4j_is_null(neo4j_path_get_relationship(value, 3, NULL)));

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


START_TEST (deserialize_unbound_relationship)
{
    uint8_t bytes[] =
            { 0xDC, 0x03, 0x72, 0x01, 0x8A, 0x4A, 0x6f, 0x75,
              0x72, 0x6E, 0x61, 0x6C, 0x69, 0x73, 0x74, 0xA1,
              0x84, 0x74, 0x79, 0x70, 0x65, 0x85, 0x47, 0x6F,
              0x6E, 0x7A, 0x6F };

    rb_append(rb, bytes, sizeof(bytes));

    neo4j_value_t value;
    int n = neo4j_deserialize(ios, &mpool, &value);
    ck_assert_int_eq(n, 0);
    ck_assert_int_eq(neo4j_type(value), NEO4J_RELATIONSHIP);

    neo4j_value_t reltype = neo4j_relationship_type(value);
    ck_assert_int_eq(neo4j_type(reltype), NEO4J_STRING);
    char buf[16];
    ck_assert_str_eq(neo4j_string_value(reltype, buf, sizeof(buf)),
            "Journalist");

    neo4j_value_t props = neo4j_relationship_properties(value);
    ck_assert_int_eq(neo4j_type(props), NEO4J_MAP);
    ck_assert_int_eq(neo4j_map_size(props), 1);
    const neo4j_map_entry_t *entry = neo4j_map_getentry(props, 0);
    ck_assert_ptr_ne(entry, NULL);
    ck_assert_int_eq(neo4j_type(entry->key), NEO4J_STRING);
    ck_assert_int_eq(neo4j_type(entry->value), NEO4J_STRING);
    ck_assert_str_eq(neo4j_string_value(entry->key, buf, sizeof(buf)),
            "type");
    ck_assert_str_eq(neo4j_string_value(entry->value, buf, sizeof(buf)),
            "Gonzo");

    ck_assert_int_eq(rb_used(rb), 0);
}
END_TEST


TCase* deserialization_tcase(void)
{
    TCase *tc = tcase_create("deserialization");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, deserialize_positive_tiny_int);
    tcase_add_test(tc, deserialize_tiny_string);
    tcase_add_test(tc, deserialize_tiny_list);
    tcase_add_test(tc, deserialize_tiny_map);
    tcase_add_test(tc, deserialize_tiny_struct);
    tcase_add_test(tc, deserialize_null);
    tcase_add_test(tc, deserialize_float);
    tcase_add_test(tc, deserialize_boolean_false);
    tcase_add_test(tc, deserialize_boolean_true);
    tcase_add_test(tc, deserialize_int8);
    tcase_add_test(tc, deserialize_int16);
    tcase_add_test(tc, deserialize_int32);
    tcase_add_test(tc, deserialize_int64);
    tcase_add_test(tc, deserialize_string8);
    tcase_add_test(tc, deserialize_string16);
    tcase_add_test(tc, deserialize_list8);
    tcase_add_test(tc, deserialize_list16);
    tcase_add_test(tc, deserialize_map8);
    tcase_add_test(tc, deserialize_map8_with_invalid_key_type);
    tcase_add_test(tc, deserialize_struct8);
    tcase_add_test(tc, deserialize_struct16);
    tcase_add_test(tc, deserialize_negative_tiny_int);
    tcase_add_test(tc, deserialize_node);
    tcase_add_test(tc, deserialize_node_with_incorrect_field_count);
    tcase_add_test(tc, deserialize_node_with_incorrect_identifier_type);
    tcase_add_test(tc, deserialize_node_with_incorrect_labels_type);
    tcase_add_test(tc, deserialize_node_with_bad_label_type);
    tcase_add_test(tc, deserialize_node_with_incorrect_map_type);
    tcase_add_test(tc, deserialize_relationship);
    tcase_add_test(tc, deserialize_path);
    tcase_add_test(tc, deserialize_unbound_relationship);
    return tc;
}
