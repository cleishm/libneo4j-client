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
#include "../src/serialization.h"
#include "../src/iostream.h"
#include "../src/ring_buffer.h"
#include "../src/values.h"
#include "memiostream.h"
#include <check.h>
#include <errno.h>
#include <unistd.h>


static ring_buffer_t *rb;
static neo4j_iostream_t *ios;


static void setup(void)
{
    rb = rb_alloc(1024);
    ios = neo4j_loopback_iostream(rb);
}


static void teardown(void)
{
    neo4j_ios_close(ios);
    rb_free(rb);
}


START_TEST (serialize_null)
{
    int r;
    uint8_t byte;

    r = neo4j_serialize(neo4j_null, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 1);

    rb_extract(rb, &byte, 1);
    ck_assert(byte == 0xC0);
}
END_TEST


START_TEST (serialize_bool)
{
    int r;
    uint8_t byte;

    neo4j_value_t true_value = neo4j_bool(true);
    neo4j_value_t false_value = neo4j_bool(false);

    r = neo4j_serialize(true_value, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 1);

    rb_extract(rb, &byte, 1);
    ck_assert(byte == 0xC3);

    r = neo4j_serialize(false_value, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 1);

    rb_extract(rb, &byte, 1);
    ck_assert(byte == 0xC2);
}
END_TEST


START_TEST (serialize_tiny_int)
{
    int r;
    uint8_t byte;

    neo4j_value_t tiny_int = neo4j_int(42);
    neo4j_value_t min_tiny_int = neo4j_int(-16);
    neo4j_value_t max_tiny_int = neo4j_int(127);

    r = neo4j_serialize(tiny_int, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 1);

    rb_extract(rb, &byte, 1);
    ck_assert(byte == 0x2A);

    r = neo4j_serialize(min_tiny_int, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 1);

    rb_extract(rb, &byte, 1);
    ck_assert(byte == 0xF0);

    r = neo4j_serialize(max_tiny_int, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 1);

    rb_extract(rb, &byte, 1);
    ck_assert(byte == 0x7F);
}
END_TEST


START_TEST (serialize_int8)
{
    int r;
    uint8_t buf[64];

    neo4j_value_t int8 = neo4j_int(-42);
    neo4j_value_t min_int8 = neo4j_int(-128);
    neo4j_value_t max_int8 = neo4j_int(-17);
    uint8_t expected_int8[] = { 0xC8, 0xD6 };
    uint8_t expected_min[] = { 0xC8, 0x80 };
    uint8_t expected_max[] = { 0xC8, 0xEF };

    r = neo4j_serialize(int8, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 2);

    rb_extract(rb, &buf, 2);
    ck_assert(memcmp(buf, expected_int8, 2) == 0);

    r = neo4j_serialize(min_int8, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 2);

    rb_extract(rb, &buf, 2);
    ck_assert(memcmp(buf, expected_min, 2) == 0);

    r = neo4j_serialize(max_int8, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 2);

    rb_extract(rb, &buf, 2);
    ck_assert(memcmp(buf, expected_max, 2) == 0);
}
END_TEST


START_TEST (serialize_int16)
{
    int r;
    uint8_t buf[64];

    neo4j_value_t int16 = neo4j_int(-9999);
    neo4j_value_t min_int16 = neo4j_int(-32768);
    neo4j_value_t max_int16 = neo4j_int(32767);
    uint8_t expected_int16[] = { 0xC9, 0xD8, 0xF1 };
    uint8_t expected_min[] = { 0xC9, 0x80, 0x00 };
    uint8_t expected_max[] = { 0xC9, 0x7F, 0xFF };

    r = neo4j_serialize(int16, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 3);

    rb_extract(rb, &buf, 3);
    ck_assert(memcmp(buf, expected_int16, 3) == 0);

    r = neo4j_serialize(min_int16, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 3);

    rb_extract(rb, &buf, 3);
    ck_assert(memcmp(buf, expected_min, 3) == 0);

    r = neo4j_serialize(max_int16, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 3);

    rb_extract(rb, &buf, 3);
    ck_assert(memcmp(buf, expected_max, 3) == 0);
}
END_TEST


START_TEST (serialize_int32)
{
    int r;
    uint8_t buf[64];

    neo4j_value_t int32 = neo4j_int(1000000000L);
    neo4j_value_t min_int32 = neo4j_int(-2147483648L);
    neo4j_value_t max_int32 = neo4j_int(2147483647L);
    uint8_t expected_int32[] = { 0xCA, 0x3B, 0x9A, 0xCA, 0x00 };
    uint8_t expected_min[] = { 0xCA, 0x80, 0x00, 0x00, 0x00 };
    uint8_t expected_max[] = { 0xCA, 0x7F, 0xFF, 0xFF, 0xFF };

    r = neo4j_serialize(int32, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 5);

    rb_extract(rb, &buf, 5);
    ck_assert(memcmp(buf, expected_int32, 5) == 0);

    r = neo4j_serialize(min_int32, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 5);

    rb_extract(rb, &buf, 5);
    ck_assert(memcmp(buf, expected_min, 5) == 0);

    r = neo4j_serialize(max_int32, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 5);

    rb_extract(rb, &buf, 5);
    ck_assert(memcmp(buf, expected_max, 5) == 0);
}
END_TEST


START_TEST (serialize_int64)
{
    int r;
    uint8_t buf[64];

    neo4j_value_t int64 = neo4j_int(-7223344556677889900LL);
    neo4j_value_t min_int64 = neo4j_int(-9223372036854775807LL-1);
    neo4j_value_t max_int64 = neo4j_int(9223372036854775807LL);
    uint8_t expected_int64[] =
            { 0xCB, 0x9B, 0xC1, 0x86, 0x65, 0x88, 0xF6, 0x80, 0x94 };
    uint8_t expected_min[] =
            { 0xCB, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t expected_max[] =
            { 0xCB, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

    r = neo4j_serialize(int64, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 9);

    rb_extract(rb, &buf, 9);
    ck_assert(memcmp(buf, expected_int64, 9) == 0);

    r = neo4j_serialize(min_int64, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 9);

    rb_extract(rb, &buf, 9);
    ck_assert(memcmp(buf, expected_min, 9) == 0);

    r = neo4j_serialize(max_int64, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 9);

    rb_extract(rb, &buf, 9);
    ck_assert(memcmp(buf, expected_max, 9) == 0);
}
END_TEST


START_TEST (serialize_float)
{
    int r;
    uint8_t buf[64];

    neo4j_value_t positive_float = neo4j_float(1.1);
    neo4j_value_t negative_float = neo4j_float(-1.1);
    uint8_t expected_positive[] =
            { 0xC1, 0x3F, 0xF1, 0x99, 0x99, 0x99, 0x99, 0x99, 0x9A };
    uint8_t expected_negative[] =
            { 0xC1, 0xBF, 0xF1, 0x99, 0x99, 0x99, 0x99, 0x99, 0x9A };

    r = neo4j_serialize(positive_float, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 9);

    rb_extract(rb, buf, 9);
    ck_assert(memcmp(buf, expected_positive, 9) == 0);

    r = neo4j_serialize(negative_float, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), 9);

    rb_extract(rb, buf, 9);
    ck_assert(memcmp(buf, expected_negative, 9) == 0);
}
END_TEST


START_TEST (serialize_tiny_string)
{
    int r;
    uint8_t buf[64];

    neo4j_value_t empty_tiny_string = neo4j_string("");
    neo4j_value_t len6_tiny_string = neo4j_ustring("hunter", 6);
    neo4j_value_t len15_tiny_string = neo4j_string("hunter thompson");
    uint8_t expected_empty[] =
            { 0x80 };
    uint8_t expected_len6[] =
            { 0x86, 0x68, 0x75, 0x6E, 0x74, 0x65, 0x72 };
    uint8_t expected_len15[] =
            { 0x8F, 0x68, 0x75, 0x6E, 0x74, 0x65, 0x72, 0x20,
              0x74, 0x68, 0x6F, 0x6D, 0x70, 0x73, 0x6F, 0x6E };

    r = neo4j_serialize(empty_tiny_string, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), sizeof(expected_empty));

    rb_extract(rb, &buf, sizeof(expected_empty));
    ck_assert(memcmp(buf, expected_empty, sizeof(expected_empty)) == 0);

    r = neo4j_serialize(len6_tiny_string, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), sizeof(expected_len6));

    rb_extract(rb, &buf, sizeof(expected_len6));
    ck_assert(memcmp(buf, expected_len6, sizeof(expected_len6)) == 0);

    r = neo4j_serialize(len15_tiny_string, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), sizeof(expected_len15));

    rb_extract(rb, &buf, sizeof(expected_len15));
    ck_assert(memcmp(buf, expected_len15, sizeof(expected_len15)) == 0);
}
END_TEST


START_TEST (serialize_string8)
{
    int r;
    uint8_t buf[256];

    neo4j_value_t string8 = neo4j_string(
            "This is going to be a very expensive war, and Victory is not "
            "guaranteed — for anyone, and certainly not for anyone as "
            "baffled as George W. Bush.");
    uint8_t expected[] =
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

    r = neo4j_serialize(string8, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), sizeof(expected));

    rb_extract(rb, &buf, sizeof(expected));
    ck_assert(memcmp(buf, expected, sizeof(expected)) == 0);
}
END_TEST


START_TEST (serialize_string16)
{
    int r;
    uint8_t buf[1024];

    neo4j_value_t string16 = neo4j_string(
            "Most people who deal in words don't have much faith in them and I "
            "am no exception — especially the big ones like Happy and Love and "
            "Honest and Strong. They are too elusive and far too relative when "
            "you compare them to sharp, mean little words like Punk and Cheap "
            "and Phony. I feel at home with these, because they are scrawny "
            "and easy to pin, but the big ones are tough and it takes either a "
            "priest or a fool to use them with any confidence");
    uint8_t expected[] =
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

    r = neo4j_serialize(string16, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), sizeof(expected));

    rb_extract(rb, &buf, sizeof(expected));
    ck_assert(memcmp(buf, expected, sizeof(expected)) == 0);
}
END_TEST


START_TEST (serialize_string32)
{
    int r;
    char str[65536];
    uint8_t buf[65541];
    uint8_t expected[65541];
    ring_buffer_t *large_rb = rb_alloc(66000);
    neo4j_iostream_t *ios = neo4j_loopback_iostream(large_rb);

    memset(str, 'x', sizeof(str));
    memset(expected, 'x', sizeof(expected));
    expected[0] = 0xD2;
    expected[1] = 0x00;
    expected[2] = 0x01;
    expected[3] = 0x00;
    expected[4] = 0x00;

    neo4j_value_t string32 = neo4j_ustring(str, sizeof(str));

    r = neo4j_serialize(string32, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(large_rb), 5 + sizeof(str));
    rb_extract(large_rb, &buf, sizeof(expected));
    ck_assert(memcmp(buf, expected, sizeof(expected)) == 0);

    neo4j_ios_close(ios);
    rb_free(large_rb);
}
END_TEST


START_TEST (serialize_tiny_list)
{
    int r;
    uint8_t buf[64];

    neo4j_value_t items[] =
            { neo4j_int(1), neo4j_int(8345463) };
    neo4j_value_t tiny_list = neo4j_list(items, 2);
    uint8_t expected[] =
            { 0x92, 0x01, 0xCA, 0x00, 0x7F, 0x57, 0x77 };

    r = neo4j_serialize(tiny_list, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), sizeof(expected));

    rb_extract(rb, &buf, sizeof(expected));
    ck_assert(memcmp(buf, expected, sizeof(expected)) == 0);
}
END_TEST


START_TEST (serialize_list8)
{
    int r;
    uint8_t buf[64];

    neo4j_value_t items[] =
            {neo4j_int(1), neo4j_int(2), neo4j_int(3), neo4j_int(4),
             neo4j_int(5), neo4j_int(6), neo4j_int(7), neo4j_int(8),
             neo4j_int(9), neo4j_int(10), neo4j_int(11), neo4j_int(12),
             neo4j_int(13), neo4j_int(14), neo4j_int(15), neo4j_int(16)};
    neo4j_value_t list8 = neo4j_list(items, 16);
    uint8_t expected[] =
            { 0xD4, 0x10, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
              0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10 };

    r = neo4j_serialize(list8, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), sizeof(expected));

    rb_extract(rb, &buf, sizeof(expected));
    ck_assert(memcmp(buf, expected, sizeof(expected)) == 0);
}
END_TEST


START_TEST (serialize_list16)
{
    int r;
    uint8_t buf[1024];

    neo4j_value_t items[256];
    for (int i = 0; i < 256; ++i)
    {
        items[i] = neo4j_int(i%16);
    }
    neo4j_value_t list16 = neo4j_list(items, 256);
    uint8_t expected[] =
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

    r = neo4j_serialize(list16, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), sizeof(expected));

    rb_extract(rb, &buf, sizeof(expected));
    ck_assert(memcmp(buf, expected, sizeof(expected)) == 0);
}
END_TEST


START_TEST (serialize_tiny_struct)
{
    int r;
    uint8_t buf[64];

    neo4j_value_t items[] =
            { neo4j_int(1), neo4j_int(8345463) };
    neo4j_value_t tiny_struct = neo4j_struct(0x78, items, 2);
    uint8_t expected[] =
            { 0xB2, 0x78, 0x01, 0xCA, 0x00, 0x7F, 0x57, 0x77 };

    r = neo4j_serialize(tiny_struct, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), sizeof(expected));

    rb_extract(rb, &buf, sizeof(expected));
    ck_assert(memcmp(buf, expected, sizeof(expected)) == 0);
}
END_TEST


START_TEST (serialize_struct8)
{
    int r;
    uint8_t buf[64];

    neo4j_value_t items[] =
            { neo4j_int(1), neo4j_int(2), neo4j_int(3), neo4j_int(4),
              neo4j_int(5), neo4j_int(6), neo4j_int(7), neo4j_int(8),
              neo4j_int(9), neo4j_int(10), neo4j_int(11), neo4j_int(12),
              neo4j_int(13), neo4j_int(14), neo4j_int(15), neo4j_int(16) };
    neo4j_value_t struct8 = neo4j_struct(0x78, items, 16);
    uint8_t expected[] =
            { 0xDC, 0x10, 0x78, 0x01, 0x02, 0x03, 0x04, 0x05,
              0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
              0x0E, 0x0F, 0x10 };

    r = neo4j_serialize(struct8, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), sizeof(expected));

    rb_extract(rb, &buf, sizeof(expected));
    ck_assert(memcmp(buf, expected, sizeof(expected)) == 0);
}
END_TEST


START_TEST (serialize_struct16)
{
    int r;
    uint8_t buf[1024];

    neo4j_value_t items[256];
    for (int i = 0; i < 256; ++i)
    {
        items[i] = neo4j_int(i%16);
    }
    neo4j_value_t struct16 = neo4j_struct(0x78, items, 256);
    uint8_t expected[] =
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

    r = neo4j_serialize(struct16, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), sizeof(expected));

    rb_extract(rb, &buf, sizeof(expected));
    ck_assert(memcmp(buf, expected, sizeof(expected)) == 0);
}
END_TEST


START_TEST (serialize_tiny_map)
{
    int r;
    uint8_t buf[64];

    neo4j_map_entry_t entries[] =
            { { .key = neo4j_string("a"), .value = neo4j_int(1) },
              { .key = neo4j_string("b"), .value = neo4j_int(8345463) } };
    neo4j_value_t tiny_map = neo4j_map(entries, 2);
    uint8_t expected[] =
            { 0xA2, 0x81, 0x61, 0x01, 0x81, 0x62, 0xCA, 0x00,
              0x7F, 0x57, 0x77 };

    r = neo4j_serialize(tiny_map, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), sizeof(expected));

    rb_extract(rb, &buf, sizeof(expected));
    ck_assert(memcmp(buf, expected, sizeof(expected)) == 0);
}
END_TEST


START_TEST (serialize_map8)
{
    int r;
    uint8_t buf[64];

    neo4j_map_entry_t entries[] =
            { { .key = neo4j_string("0"), .value = neo4j_int(1) },
              { .key = neo4j_string("1"), .value = neo4j_int(2) },
              { .key = neo4j_string("2"), .value = neo4j_int(3) },
              { .key = neo4j_string("3"), .value = neo4j_int(4) },
              { .key = neo4j_string("4"), .value = neo4j_int(5) },
              { .key = neo4j_string("5"), .value = neo4j_int(6) },
              { .key = neo4j_string("6"), .value = neo4j_int(7) },
              { .key = neo4j_string("7"), .value = neo4j_int(8) },
              { .key = neo4j_string("8"), .value = neo4j_int(9) },
              { .key = neo4j_string("9"), .value = neo4j_int(10) },
              { .key = neo4j_string("a"), .value = neo4j_int(11) },
              { .key = neo4j_string("b"), .value = neo4j_int(12) },
              { .key = neo4j_string("c"), .value = neo4j_int(13) },
              { .key = neo4j_string("d"), .value = neo4j_int(14) },
              { .key = neo4j_string("e"), .value = neo4j_int(15) },
              { .key = neo4j_string("f"), .value = neo4j_int(16) } };
    neo4j_value_t map8 = neo4j_map(entries, 16);
    uint8_t expected[] =
            { 0xD8, 0x10, 0x81, 0x30, 0x01, 0x81, 0x31, 0x02,
              0x81, 0x32, 0x03, 0x81, 0x33, 0x04, 0x81, 0x34,
              0x05, 0x81, 0x35, 0x06, 0x81, 0x36, 0x07, 0x81,
              0x37, 0x08, 0x81, 0x38, 0x09, 0x81, 0x39, 0x0A,
              0x81, 0x61, 0x0B, 0x81, 0x62, 0x0C, 0x81, 0x63,
              0x0D, 0x81, 0x64, 0x0E, 0x81, 0x65, 0x0F, 0x81,
              0x66, 0x10 };

    r = neo4j_serialize(map8, ios);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(rb_used(rb), sizeof(expected));

    rb_extract(rb, &buf, sizeof(expected));
    ck_assert(memcmp(buf, expected, sizeof(expected)) == 0);
}
END_TEST


TCase* serialization_tcase(void)
{
    TCase *tc = tcase_create("serialization");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, serialize_null);
    tcase_add_test(tc, serialize_bool);
    tcase_add_test(tc, serialize_tiny_int);
    tcase_add_test(tc, serialize_int8);
    tcase_add_test(tc, serialize_int16);
    tcase_add_test(tc, serialize_int32);
    tcase_add_test(tc, serialize_int64);
    tcase_add_test(tc, serialize_float);
    tcase_add_test(tc, serialize_tiny_string);
    tcase_add_test(tc, serialize_string8);
    tcase_add_test(tc, serialize_string16);
    tcase_add_test(tc, serialize_string32);
    tcase_add_test(tc, serialize_tiny_list);
    tcase_add_test(tc, serialize_list8);
    tcase_add_test(tc, serialize_list16);
    tcase_add_test(tc, serialize_tiny_struct);
    tcase_add_test(tc, serialize_struct8);
    tcase_add_test(tc, serialize_struct16);
    tcase_add_test(tc, serialize_tiny_map);
    tcase_add_test(tc, serialize_map8);
    return tc;
}
