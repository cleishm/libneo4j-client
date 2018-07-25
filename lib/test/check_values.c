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
#include "../src/values.h"
#include "memstream.h"
#include <check.h>
#include <errno.h>
#include <unistd.h>


static char buf[1024];
static char *memstream_buffer;
static size_t memstream_size;
static FILE *memstream;


static void setup(void)
{
    memset(buf, 0x7a, 1023);
    buf[1023] = '\0';
    memstream = open_memstream(&memstream_buffer, &memstream_size);
}


static void teardown(void)
{
    fclose(memstream);
    free(memstream_buffer);
}


START_TEST (null_value)
{
    neo4j_value_t value = neo4j_null;
    ck_assert(neo4j_type(value) == NEO4J_NULL);

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(buf, "null");

    ck_assert_int_eq(neo4j_ntostring(value, buf, 2), 4);
    ck_assert_str_eq(buf, "n");
    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 4);

    ck_assert_int_eq(neo4j_fprint(value, memstream), 4);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "null");
}
END_TEST


START_TEST (null_eq)
{
    ck_assert(neo4j_eq(neo4j_null, neo4j_null));
    ck_assert(!neo4j_eq(neo4j_null, neo4j_bool(true)));
}
END_TEST


START_TEST (bool_value)
{
    neo4j_value_t value = neo4j_bool(true);
    ck_assert(neo4j_type(value) == NEO4J_BOOL);

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(buf, "true");

    ck_assert_int_eq(neo4j_ntostring(value, buf, 2), 4);
    ck_assert_str_eq(buf, "t");

    value = neo4j_bool(false);
    ck_assert_str_eq(neo4j_tostring(value, buf, sizeof(buf)), "false");

    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 5);

    ck_assert_int_eq(neo4j_fprint(value, memstream), 5);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "false");
}
END_TEST


START_TEST (bool_eq)
{
    ck_assert(neo4j_eq(neo4j_bool(true), neo4j_bool(true)));
    ck_assert(neo4j_eq(neo4j_bool(false), neo4j_bool(false)));
    ck_assert(!neo4j_eq(neo4j_bool(true), neo4j_bool(false)));
    ck_assert(!neo4j_eq(neo4j_bool(false), neo4j_bool(true)));
    ck_assert(!neo4j_eq(neo4j_bool(true), neo4j_int(1)));
}
END_TEST


START_TEST (int_value)
{
    neo4j_value_t value = neo4j_int(42);
    ck_assert(neo4j_type(value) == NEO4J_INT);

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(buf, "42");

    value = neo4j_int(-53);
    ck_assert_str_eq(neo4j_tostring(value, buf, sizeof(buf)), "-53");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 2), 3);
    ck_assert_str_eq(buf, "-");
    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 3);

    ck_assert_int_eq(neo4j_fprint(value, memstream), 3);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "-53");
}
END_TEST


START_TEST (int_eq)
{
    ck_assert(neo4j_eq(neo4j_int(0), neo4j_int(0)));
    ck_assert(neo4j_eq(neo4j_int(42), neo4j_int(42)));
    ck_assert(neo4j_eq(neo4j_int(-127), neo4j_int(-127)));
    ck_assert(!neo4j_eq(neo4j_int(-127), neo4j_int(0)));
    ck_assert(!neo4j_eq(neo4j_int(0), neo4j_int(42)));
    ck_assert(!neo4j_eq(neo4j_int(127), neo4j_int(0)));
    ck_assert(!neo4j_eq(neo4j_int(42), neo4j_int(0)));
    ck_assert(!neo4j_eq(neo4j_int(1), neo4j_float(1.0)));
}
END_TEST


START_TEST (float_value)
{
    neo4j_value_t value = neo4j_float(4.2);
    ck_assert(neo4j_type(value) == NEO4J_FLOAT);

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "4.200000");

    value = neo4j_float(-89.83423);
    neo4j_tostring(value, buf, sizeof(buf));
    ck_assert_str_eq(buf, "-89.834230");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 4), 10);
    ck_assert_str_eq(buf, "-89");
    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 10);

    ck_assert_int_eq(neo4j_fprint(value, memstream), 10);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "-89.834230");
}
END_TEST


START_TEST (float_eq)
{
    ck_assert(neo4j_eq(neo4j_float(0), neo4j_float(0)));
    ck_assert(neo4j_eq(neo4j_float(42), neo4j_float(42)));
    ck_assert(neo4j_eq(neo4j_float(-1.27), neo4j_float(-1.27)));
    ck_assert(!neo4j_eq(neo4j_float(-127), neo4j_float(0)));
    ck_assert(!neo4j_eq(neo4j_float(0), neo4j_float(42)));
    ck_assert(!neo4j_eq(neo4j_float(127), neo4j_float(0)));
    ck_assert(!neo4j_eq(neo4j_float(42), neo4j_float(0)));
    ck_assert(!neo4j_eq(neo4j_float(1), neo4j_string("bernie")));
}
END_TEST


START_TEST (string_value)
{
    neo4j_value_t value = neo4j_string("the \"rum diary\"");
    ck_assert(neo4j_type(value) == NEO4J_STRING);

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "\"the \\\"rum diary\\\"\"");

    ck_assert_int_eq(neo4j_ntostring(value, buf, sizeof(buf)), 19);
    ck_assert_str_eq(buf, "\"the \\\"rum diary\\\"\"");

    value = neo4j_ustring("the \"rum diary\"", 8);
    ck_assert_int_eq(neo4j_ntostring(value, buf, sizeof(buf)), 11);
    ck_assert_str_eq(buf, "\"the \\\"rum\"");

    value = neo4j_string("the \"rum\"");
    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 13);
    ck_assert_int_eq(neo4j_ntostring(value, buf, 1), 13);
    ck_assert_str_eq(buf, "");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 2), 13);
    ck_assert_str_eq(buf, "\"");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 3), 13);
    ck_assert_str_eq(buf, "\"t");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 4), 13);
    ck_assert_str_eq(buf, "\"th");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 5), 13);
    ck_assert_str_eq(buf, "\"the");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 6), 13);
    ck_assert_str_eq(buf, "\"the ");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 7), 13);
    ck_assert_str_eq(buf, "\"the ");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 8), 13);
    ck_assert_str_eq(buf, "\"the \\\"");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 9), 13);
    ck_assert_str_eq(buf, "\"the \\\"r");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 10), 13);
    ck_assert_str_eq(buf, "\"the \\\"ru");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 11), 13);
    ck_assert_str_eq(buf, "\"the \\\"rum");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 12), 13);
    ck_assert_str_eq(buf, "\"the \\\"rum");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 13), 13);
    ck_assert_str_eq(buf, "\"the \\\"rum\\\"");

    value = neo4j_string("black\\white");
    ck_assert_int_eq(neo4j_ntostring(value, buf, sizeof(buf)), 14);
    ck_assert_str_eq(buf, "\"black\\\\white\"");

    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 14);
    ck_assert_int_eq(neo4j_ntostring(value, buf, 7), 14);
    ck_assert_str_eq(buf, "\"black");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 8), 14);
    ck_assert_str_eq(buf, "\"black");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 9), 14);
    ck_assert_str_eq(buf, "\"black\\\\");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 10), 14);
    ck_assert_str_eq(buf, "\"black\\\\w");

    value = neo4j_string("the \"rum diary\"");
    ck_assert_int_eq(neo4j_fprint(value, memstream), 19);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "\"the \\\"rum diary\\\"\"");
}
END_TEST


START_TEST (bytes_value)
{
    neo4j_value_t value = neo4j_bytes("UVWXYZ", 6);
    ck_assert(neo4j_type(value) == NEO4J_BYTES);

    ck_assert_int_eq(neo4j_bytes_length(value), 6);

    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 13);
    ck_assert_int_eq(neo4j_ntostring(value, buf, 4), 13);
    ck_assert_str_eq(buf, "#55");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 14), 13);
    ck_assert_str_eq(buf, "#55565758595a");
    ck_assert_int_eq(neo4j_ntostring(value, buf, sizeof(buf)), 13);
    ck_assert_str_eq(buf, "#55565758595a");

    ck_assert_int_eq(neo4j_fprint(value, memstream), 13);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "#55565758595a");
}
END_TEST


START_TEST (string_eq)
{
    neo4j_value_t value = neo4j_string("the rum diary");

    ck_assert(neo4j_eq(value, neo4j_string("the rum diary")));
    ck_assert(!neo4j_eq(value, neo4j_string("the rum")));
    ck_assert(!neo4j_eq(value, neo4j_string("the rum journal")));
    ck_assert(!neo4j_eq(value, neo4j_string("the rum diary 2")));
}
END_TEST


START_TEST (list_value)
{
    neo4j_value_t list_values[] = { neo4j_int(1), neo4j_string("the \"rum\"") };
    neo4j_value_t value = neo4j_list(list_values, 2);
    ck_assert(neo4j_type(value) == NEO4J_LIST);

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "[1,\"the \\\"rum\\\"\"]");

    ck_assert_int_eq(neo4j_ntostring(value, buf, sizeof(buf)), 17);
    ck_assert_str_eq(str, "[1,\"the \\\"rum\\\"\"]");

    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 17);
    ck_assert_int_eq(neo4j_ntostring(value, buf, 1), 17);
    ck_assert_str_eq(buf, "");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 2), 17);
    ck_assert_str_eq(buf, "[");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 3), 17);
    ck_assert_str_eq(buf, "[1");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 4), 17);
    ck_assert_str_eq(buf, "[1,");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 5), 17);
    ck_assert_str_eq(buf, "[1,\"");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 6), 17);
    ck_assert_str_eq(buf, "[1,\"t");

    ck_assert_int_eq(neo4j_ntostring(value, buf, 9), 17);
    ck_assert_str_eq(buf, "[1,\"the ");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 10), 17);
    ck_assert_str_eq(buf, "[1,\"the ");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 11), 17);
    ck_assert_str_eq(buf, "[1,\"the \\\"");

    ck_assert_int_eq(neo4j_ntostring(value, buf, 14), 17);
    ck_assert_str_eq(buf, "[1,\"the \\\"rum");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 15), 17);
    ck_assert_str_eq(buf, "[1,\"the \\\"rum");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 16), 17);
    ck_assert_str_eq(buf, "[1,\"the \\\"rum\\\"");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 17), 17);
    ck_assert_str_eq(buf, "[1,\"the \\\"rum\\\"\"");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 18), 17);
    ck_assert_str_eq(buf, "[1,\"the \\\"rum\\\"\"]");

    value = neo4j_list(list_values, 0);
    str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert_str_eq(str, "[]");

    value = neo4j_list(list_values, 2);
    ck_assert_int_eq(neo4j_fprint(value, memstream), 17);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "[1,\"the \\\"rum\\\"\"]");
}
END_TEST


START_TEST (list_eq)
{
    neo4j_value_t list_values1[] = { neo4j_int(1), neo4j_int(2) };
    neo4j_value_t value1 = neo4j_list(list_values1, 2);
    neo4j_value_t list_values2[] = { neo4j_int(1), neo4j_int(2) };
    neo4j_value_t value2 = neo4j_list(list_values2, 2);
    neo4j_value_t list_values3[] = { neo4j_int(1), neo4j_int(3) };
    neo4j_value_t value3 = neo4j_list(list_values3, 2);
    neo4j_value_t list_values4[] = { neo4j_int(1) };
    neo4j_value_t value4 = neo4j_list(list_values4, 1);
    neo4j_value_t list_values5[] = { neo4j_int(1), neo4j_int(2), neo4j_int(3) };
    neo4j_value_t value5 = neo4j_list(list_values5, 3);

    ck_assert(neo4j_eq(value1, value2));
    ck_assert(!neo4j_eq(value1, value3));
    ck_assert(!neo4j_eq(value3, value1));
    ck_assert(!neo4j_eq(value1, value4));
    ck_assert(!neo4j_eq(value4, value1));
    ck_assert(!neo4j_eq(value1, value5));
    ck_assert(!neo4j_eq(value5, value1));
}
END_TEST


START_TEST (map_value)
{
    neo4j_map_entry_t map_entries[] =
        { { .key = neo4j_string("bernie"), .value = neo4j_string("sanders") },
          { .key = neo4j_string("b. sanders"), .value = neo4j_int(2) } };
    neo4j_value_t value = neo4j_map(map_entries, 2);
    ck_assert(neo4j_type(value) == NEO4J_MAP);

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "{bernie:\"sanders\",`b. sanders`:2}");

    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 33);
    ck_assert_int_eq(neo4j_ntostring(value, buf, sizeof(buf)), 33);
    ck_assert_str_eq(buf, "{bernie:\"sanders\",`b. sanders`:2}");

    ck_assert_int_eq(neo4j_ntostring(value, buf, 1), 33);
    ck_assert_str_eq(buf, "");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 2), 33);
    ck_assert_str_eq(buf, "{");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 3), 33);
    ck_assert_str_eq(buf, "{b");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 9), 33);
    ck_assert_str_eq(buf, "{bernie:");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 10), 33);
    ck_assert_str_eq(buf, "{bernie:\"");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 11), 33);
    ck_assert_str_eq(buf, "{bernie:\"s");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 19), 33);
    ck_assert_str_eq(buf, "{bernie:\"sanders\",");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 20), 33);
    ck_assert_str_eq(buf, "{bernie:\"sanders\",`");

    value = neo4j_map(map_entries, 0);
    str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert_str_eq(buf, "{}");

    value = neo4j_map(map_entries, 2);
    ck_assert_int_eq(neo4j_fprint(value, memstream), 33);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "{bernie:\"sanders\",`b. sanders`:2}");
}
END_TEST


START_TEST (invalid_map_value)
{
    neo4j_map_entry_t map_entries[] =
        { { .key = neo4j_string("bernie"), .value = neo4j_int(1) },
          { .key = neo4j_int(1), .value = neo4j_int(2) } };
    neo4j_value_t value = neo4j_map(map_entries, 2);
    ck_assert(neo4j_is_null(value));
    ck_assert_int_eq(errno, NEO4J_INVALID_MAP_KEY_TYPE);
}
END_TEST


START_TEST (map_eq)
{
    neo4j_map_entry_t map_entries1[] =
        { { .key = neo4j_string("bernie"), .value = neo4j_int(1) },
          { .key = neo4j_string("sanders"), .value = neo4j_int(2) } };
    neo4j_value_t value1 = neo4j_map(map_entries1, 2);
    neo4j_map_entry_t map_entries2[] =
        { { .key = neo4j_string("sanders"), .value = neo4j_int(2) },
          { .key = neo4j_string("bernie"), .value = neo4j_int(1) } };
    neo4j_value_t value2 = neo4j_map(map_entries2, 2);
    neo4j_map_entry_t map_entries3[] =
        { { .key = neo4j_string("sanders"), .value = neo4j_int(2) } };
    neo4j_value_t value3 = neo4j_map(map_entries3, 1);
    neo4j_map_entry_t map_entries4[] =
        { { .key = neo4j_string("bernie"), .value = neo4j_int(1) },
          { .key = neo4j_string("sanders"), .value = neo4j_int(2) },
          { .key = neo4j_string("president"), .value = neo4j_int(3) } };
    neo4j_value_t value4 = neo4j_map(map_entries4, 1);
    neo4j_map_entry_t map_entries5[] =
        { { .key = neo4j_string("bernie"), .value = neo4j_int(1) },
          { .key = neo4j_string("sanders"), .value = neo4j_int(3) } };
    neo4j_value_t value5 = neo4j_map(map_entries5, 2);

    ck_assert(neo4j_eq(value1, value2));
    ck_assert(!neo4j_eq(value1, value3));
    ck_assert(!neo4j_eq(value3, value1));
    ck_assert(!neo4j_eq(value1, value4));
    ck_assert(!neo4j_eq(value4, value1));
    ck_assert(!neo4j_eq(value1, value5));
    ck_assert(!neo4j_eq(value5, value1));
}
END_TEST


START_TEST (map_get)
{
    neo4j_map_entry_t map_entries[] =
        { { .key = neo4j_string("bernie"), .value = neo4j_int(1) },
          { .key = neo4j_string("sanders"), .value = neo4j_int(2) } };
    neo4j_value_t value = neo4j_map(map_entries, 2);

    const neo4j_value_t v = neo4j_map_get(value, "bernie");
    ck_assert(neo4j_type(v) == NEO4J_INT);
    ck_assert(neo4j_eq(v, neo4j_int(1)));
}
END_TEST


START_TEST (node_value)
{
    neo4j_value_t labels[] =
        { neo4j_string("Person"), neo4j_string("Democrat Senator") };
    neo4j_map_entry_t props[] =
        { { .key = neo4j_string("bernie"), .value = neo4j_int(1) },
          { .key = neo4j_string("sanders"), .value = neo4j_int(2) } };

    neo4j_value_t field_values[] =
        { neo4j_identity(1), neo4j_list(labels, 2), neo4j_map(props, 2) };
    neo4j_value_t value = neo4j_node(field_values);
    ck_assert(neo4j_type(value) == NEO4J_NODE);

    ck_assert(neo4j_eq(neo4j_node_identity(value), neo4j_identity(1)));

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "(:Person:`Democrat Senator`{bernie:1,sanders:2})");

    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 48);
    ck_assert_int_eq(neo4j_ntostring(value, buf, sizeof(buf)), 48);
    ck_assert_str_eq(buf, "(:Person:`Democrat Senator`{bernie:1,sanders:2})");

    ck_assert_int_eq(neo4j_fprint(value, memstream), 48);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer,
            "(:Person:`Democrat Senator`{bernie:1,sanders:2})");
}
END_TEST


START_TEST (invalid_node_label_value)
{
    neo4j_value_t labels[] =
        { neo4j_string("Person"), neo4j_int(1) };
    neo4j_map_entry_t props[] =
        { { .key = neo4j_string("bernie"), .value = neo4j_int(1) },
          { .key = neo4j_string("sanders"), .value = neo4j_int(2) } };

    neo4j_value_t field_values[] =
        { neo4j_identity(1), neo4j_list(labels, 2), neo4j_map(props, 2) };
    neo4j_value_t value = neo4j_node(field_values);
    ck_assert(neo4j_is_null(value));
    ck_assert_int_eq(errno, NEO4J_INVALID_LABEL_TYPE);
}
END_TEST


START_TEST (relationship_value)
{
    neo4j_value_t type = neo4j_string("Candidate");
    neo4j_map_entry_t props[] =
        { { .key = neo4j_string("year"), .value = neo4j_int(2016) } };

    neo4j_value_t field_values[] =
        { neo4j_identity(1), neo4j_identity(8), neo4j_identity(9),
          type, neo4j_map(props, 1) };
    neo4j_value_t value = neo4j_relationship(field_values);
    ck_assert(neo4j_type(value) == NEO4J_RELATIONSHIP);

    ck_assert(neo4j_eq(neo4j_relationship_identity(value),
                neo4j_identity(1)));
    ck_assert(neo4j_eq(neo4j_relationship_start_node_identity(value),
                neo4j_identity(8)));
    ck_assert(neo4j_eq(neo4j_relationship_end_node_identity(value),
                neo4j_identity(9)));

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "-[:Candidate{year:2016}]-");

    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 25);
    ck_assert_int_eq(neo4j_ntostring(value, buf, sizeof(buf)), 25);
    ck_assert_str_eq(buf, "-[:Candidate{year:2016}]-");

    ck_assert_int_eq(neo4j_fprint(value, memstream), 25);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "-[:Candidate{year:2016}]-");
}
END_TEST


START_TEST (unbound_relationship_value)
{
    neo4j_value_t type = neo4j_string("Candidate");
    neo4j_map_entry_t props[] =
        { { .key = neo4j_string("year"), .value = neo4j_int(2016) } };

    neo4j_value_t field_values[] =
        { neo4j_identity(1), type, neo4j_map(props, 1) };
    neo4j_value_t value = neo4j_unbound_relationship(field_values);
    ck_assert(neo4j_type(value) == NEO4J_RELATIONSHIP);

    ck_assert(neo4j_eq(neo4j_relationship_identity(value),
                neo4j_identity(1)));
    ck_assert(neo4j_is_null(neo4j_relationship_start_node_identity(value)));
    ck_assert(neo4j_is_null(neo4j_relationship_end_node_identity(value)));

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "-[:Candidate{year:2016}]-");

    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 25);
    ck_assert_int_eq(neo4j_ntostring(value, buf, sizeof(buf)), 25);
    ck_assert_str_eq(buf, "-[:Candidate{year:2016}]-");
}
END_TEST


START_TEST (path_value)
{
    neo4j_value_t node1_labels[] = { neo4j_string("State") };
    neo4j_value_t rel1_type = neo4j_string("Senator");
    neo4j_value_t node2_labels[] = { neo4j_string("Person") };
    neo4j_value_t rel2_type = neo4j_string("Candidate");
    neo4j_value_t node3_labels[] = { neo4j_string("Campaign") };

    neo4j_value_t node1_fields[] =
        { neo4j_identity(1), neo4j_list(node1_labels, 1), neo4j_map(NULL, 0) };
    neo4j_value_t node1 = neo4j_node(node1_fields);

    neo4j_value_t rel1_fields[] =
        { neo4j_identity(8), neo4j_identity(2), neo4j_identity(1),
          rel1_type, neo4j_map(NULL, 0) };
    neo4j_value_t rel1 = neo4j_relationship(rel1_fields);

    neo4j_value_t node2_fields[] =
        { neo4j_identity(2), neo4j_list(node2_labels, 1), neo4j_map(NULL, 0) };
    neo4j_value_t node2 = neo4j_node(node2_fields);

    neo4j_value_t rel2_fields[] =
        { neo4j_identity(9), neo4j_identity(2), neo4j_identity(3),
          rel2_type, neo4j_map(NULL, 0) };
    neo4j_value_t rel2 = neo4j_relationship(rel2_fields);

    neo4j_value_t node3_fields[] =
        { neo4j_identity(3), neo4j_list(node3_labels, 1), neo4j_map(NULL, 0) };
    neo4j_value_t node3 = neo4j_node(node3_fields);

    neo4j_value_t path_nodes[] = { node1, node2, node3 };
    neo4j_value_t path_rels[] = { rel1, rel2 };
    neo4j_value_t path_seq[] =
        { /*neo4j_int(0),*/ neo4j_int(-1),
          neo4j_int(1), neo4j_int(2),
          neo4j_int(2) };

    neo4j_value_t path_fields[] =
        { neo4j_list(path_nodes, 3), neo4j_list(path_rels, 2),
          neo4j_list(path_seq, 4) };
    neo4j_value_t value = neo4j_path(path_fields);
    ck_assert(neo4j_type(value) == NEO4J_PATH);

    ck_assert_int_eq(neo4j_path_length(value), 2);

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str,
            "(:State)<-[:Senator]-(:Person)-[:Candidate]->(:Campaign)"
            );

    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 56);
    ck_assert_int_eq(neo4j_ntostring(value, buf, sizeof(buf)), 56);
    ck_assert_str_eq(buf,
            "(:State)<-[:Senator]-(:Person)-[:Candidate]->(:Campaign)"
            );

    ck_assert_int_eq(neo4j_fprint(value, memstream), 56);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer,
            "(:State)<-[:Senator]-(:Person)-[:Candidate]->(:Campaign)"
            );
}
END_TEST


START_TEST (invalid_path_node_value)
{
    neo4j_value_t rel1_type = neo4j_string("Senator");
    neo4j_value_t rel2_type = neo4j_string("Candidate");

    neo4j_value_t node1_fields[] =
        { neo4j_identity(1), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node1 = neo4j_node(node1_fields);

    neo4j_value_t rel1_fields[] =
        { neo4j_identity(8), neo4j_identity(2), neo4j_identity(1),
          rel1_type, neo4j_map(NULL, 0) };
    neo4j_value_t rel1 = neo4j_relationship(rel1_fields);

    neo4j_value_t rel2_fields[] =
        { neo4j_identity(9), neo4j_identity(2), neo4j_identity(3),
          rel2_type, neo4j_map(NULL, 0) };
    neo4j_value_t rel2 = neo4j_relationship(rel2_fields);

    neo4j_value_t node3_fields[] =
        { neo4j_identity(3), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node3 = neo4j_node(node3_fields);

    neo4j_value_t path_nodes[] = { node1, neo4j_bool(true), node3 };
    neo4j_value_t path_rels[] = { rel1, rel2 };
    neo4j_value_t path_seq[] =
        { /*neo4j_int(0),*/ neo4j_int(-1),
          neo4j_int(1), neo4j_int(2),
          neo4j_int(2) };

    neo4j_value_t path_fields[] =
        { neo4j_list(path_nodes, 3), neo4j_list(path_rels, 2),
          neo4j_list(path_seq, 4) };
    neo4j_value_t value = neo4j_path(path_fields);
    ck_assert(neo4j_is_null(value));
    ck_assert_int_eq(errno, NEO4J_INVALID_PATH_NODE_TYPE);
}
END_TEST


START_TEST (invalid_path_relationship_value)
{
    neo4j_value_t rel1_type = neo4j_string("Senator");

    neo4j_value_t node1_fields[] =
        { neo4j_identity(1), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node1 = neo4j_node(node1_fields);

    neo4j_value_t rel1_fields[] =
        { neo4j_identity(8), neo4j_identity(2), neo4j_identity(1),
          rel1_type, neo4j_map(NULL, 0) };
    neo4j_value_t rel1 = neo4j_relationship(rel1_fields);

    neo4j_value_t node2_fields[] =
        { neo4j_identity(2), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node2 = neo4j_node(node2_fields);

    neo4j_value_t node3_fields[] =
        { neo4j_identity(3), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node3 = neo4j_node(node3_fields);

    neo4j_value_t path_nodes[] = { node1, node2, node3 };
    neo4j_value_t path_rels[] = { rel1, neo4j_bool(true) };
    neo4j_value_t path_seq[] =
        { /*neo4j_int(0),*/ neo4j_int(-1),
          neo4j_int(1), neo4j_int(2),
          neo4j_int(2) };

    neo4j_value_t path_fields[] =
        { neo4j_list(path_nodes, 3), neo4j_list(path_rels, 2),
          neo4j_list(path_seq, 4) };
    neo4j_value_t value = neo4j_path(path_fields);
    ck_assert(neo4j_is_null(value));
    ck_assert_int_eq(errno, NEO4J_INVALID_PATH_RELATIONSHIP_TYPE);
}
END_TEST


START_TEST (invalid_path_seq_length)
{
    neo4j_value_t rel1_type = neo4j_string("Senator");
    neo4j_value_t rel2_type = neo4j_string("Candidate");

    neo4j_value_t node1_fields[] =
        { neo4j_identity(1), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node1 = neo4j_node(node1_fields);

    neo4j_value_t rel1_fields[] =
        { neo4j_identity(8), neo4j_identity(2), neo4j_identity(1),
          rel1_type, neo4j_map(NULL, 0) };
    neo4j_value_t rel1 = neo4j_relationship(rel1_fields);

    neo4j_value_t node2_fields[] =
        { neo4j_identity(2), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node2 = neo4j_node(node2_fields);

    neo4j_value_t rel2_fields[] =
        { neo4j_identity(9), neo4j_identity(2), neo4j_identity(3),
          rel2_type, neo4j_map(NULL, 0) };
    neo4j_value_t rel2 = neo4j_relationship(rel2_fields);

    neo4j_value_t node3_fields[] =
        { neo4j_identity(3), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node3 = neo4j_node(node3_fields);

    neo4j_value_t path_nodes[] = { node1, node2, node3 };
    neo4j_value_t path_rels[] = { rel1, rel2 };
    neo4j_value_t path_seq[] =
        { /*neo4j_int(0),*/ neo4j_int(-1),
          neo4j_int(1), neo4j_int(2),
          neo4j_int(2) };

    neo4j_value_t path_fields[] =
        { neo4j_list(path_nodes, 3), neo4j_list(path_rels, 2),
          neo4j_list(path_seq, 3) };
    neo4j_value_t value = neo4j_path(path_fields);
    ck_assert(neo4j_is_null(value));
    ck_assert_int_eq(errno, NEO4J_INVALID_PATH_SEQUENCE_LENGTH);
}
END_TEST


START_TEST (invalid_path_seq_rel_index_type)
{
    neo4j_value_t rel1_type = neo4j_string("Senator");
    neo4j_value_t rel2_type = neo4j_string("Candidate");

    neo4j_value_t node1_fields[] =
        { neo4j_identity(1), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node1 = neo4j_node(node1_fields);

    neo4j_value_t rel1_fields[] =
        { neo4j_identity(8), neo4j_identity(2), neo4j_identity(1),
          rel1_type, neo4j_map(NULL, 0) };
    neo4j_value_t rel1 = neo4j_relationship(rel1_fields);

    neo4j_value_t node2_fields[] =
        { neo4j_identity(2), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node2 = neo4j_node(node2_fields);

    neo4j_value_t rel2_fields[] =
        { neo4j_identity(9), neo4j_identity(2), neo4j_identity(3),
          rel2_type, neo4j_map(NULL, 0) };
    neo4j_value_t rel2 = neo4j_relationship(rel2_fields);

    neo4j_value_t node3_fields[] =
        { neo4j_identity(3), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node3 = neo4j_node(node3_fields);

    neo4j_value_t path_nodes[] = { node1, node2, node3 };
    neo4j_value_t path_rels[] = { rel1, rel2 };
    neo4j_value_t path_seq[] =
        { /*neo4j_int(0),*/ neo4j_int(-1),
          neo4j_int(1), neo4j_bool(true),
          neo4j_int(2) };

    neo4j_value_t path_fields[] =
        { neo4j_list(path_nodes, 3), neo4j_list(path_rels, 2),
          neo4j_list(path_seq, 4) };
    neo4j_value_t value = neo4j_path(path_fields);
    ck_assert(neo4j_is_null(value));
    ck_assert_int_eq(errno, NEO4J_INVALID_PATH_SEQUENCE_IDX_TYPE);
}
END_TEST


START_TEST (invalid_path_seq_node_index_type)
{
    neo4j_value_t rel1_type = neo4j_string("Senator");
    neo4j_value_t rel2_type = neo4j_string("Candidate");

    neo4j_value_t node1_fields[] =
        { neo4j_identity(1), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node1 = neo4j_node(node1_fields);

    neo4j_value_t rel1_fields[] =
        { neo4j_identity(8), neo4j_identity(2), neo4j_identity(1),
          rel1_type, neo4j_map(NULL, 0) };
    neo4j_value_t rel1 = neo4j_relationship(rel1_fields);

    neo4j_value_t node2_fields[] =
        { neo4j_identity(2), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node2 = neo4j_node(node2_fields);

    neo4j_value_t rel2_fields[] =
        { neo4j_identity(9), neo4j_identity(2), neo4j_identity(3),
          rel2_type, neo4j_map(NULL, 0) };
    neo4j_value_t rel2 = neo4j_relationship(rel2_fields);

    neo4j_value_t node3_fields[] =
        { neo4j_identity(3), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node3 = neo4j_node(node3_fields);

    neo4j_value_t path_nodes[] = { node1, node2, node3 };
    neo4j_value_t path_rels[] = { rel1, rel2 };
    neo4j_value_t path_seq[] =
        { /*neo4j_int(0),*/ neo4j_int(-1),
          neo4j_bool(true), neo4j_int(2),
          neo4j_int(2) };

    neo4j_value_t path_fields[] =
        { neo4j_list(path_nodes, 3), neo4j_list(path_rels, 2),
          neo4j_list(path_seq, 4) };
    neo4j_value_t value = neo4j_path(path_fields);
    ck_assert(neo4j_is_null(value));
    ck_assert_int_eq(errno, NEO4J_INVALID_PATH_SEQUENCE_IDX_TYPE);
}
END_TEST


START_TEST (invalid_path_seq_rel_index_range)
{
    neo4j_value_t rel1_type = neo4j_string("Senator");
    neo4j_value_t rel2_type = neo4j_string("Candidate");

    neo4j_value_t node1_fields[] =
        { neo4j_identity(1), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node1 = neo4j_node(node1_fields);

    neo4j_value_t rel1_fields[] =
        { neo4j_identity(8), neo4j_identity(2), neo4j_identity(1),
          rel1_type, neo4j_map(NULL, 0) };
    neo4j_value_t rel1 = neo4j_relationship(rel1_fields);

    neo4j_value_t node2_fields[] =
        { neo4j_identity(2), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node2 = neo4j_node(node2_fields);

    neo4j_value_t rel2_fields[] =
        { neo4j_identity(9), neo4j_identity(2), neo4j_identity(3),
          rel2_type, neo4j_map(NULL, 0) };
    neo4j_value_t rel2 = neo4j_relationship(rel2_fields);

    neo4j_value_t node3_fields[] =
        { neo4j_identity(3), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node3 = neo4j_node(node3_fields);

    neo4j_value_t path_nodes[] = { node1, node2, node3 };
    neo4j_value_t path_rels[] = { rel1, rel2 };
    neo4j_value_t path_seq[] =
        { /*neo4j_int(0),*/ neo4j_int(-1),
          neo4j_int(1), neo4j_int(3),
          neo4j_int(2) };

    neo4j_value_t path_fields[] =
        { neo4j_list(path_nodes, 3), neo4j_list(path_rels, 2),
          neo4j_list(path_seq, 4) };
    neo4j_value_t value = neo4j_path(path_fields);
    ck_assert(neo4j_is_null(value));
    ck_assert_int_eq(errno, NEO4J_INVALID_PATH_SEQUENCE_IDX_RANGE);
}
END_TEST


START_TEST (invalid_path_seq_rel_zero_index_range)
{
    neo4j_value_t rel1_type = neo4j_string("Senator");
    neo4j_value_t rel2_type = neo4j_string("Candidate");

    neo4j_value_t node1_fields[] =
        { neo4j_identity(1), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node1 = neo4j_node(node1_fields);

    neo4j_value_t rel1_fields[] =
        { neo4j_identity(8), neo4j_identity(2), neo4j_identity(1),
          rel1_type, neo4j_map(NULL, 0) };
    neo4j_value_t rel1 = neo4j_relationship(rel1_fields);

    neo4j_value_t node2_fields[] =
        { neo4j_identity(2), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node2 = neo4j_node(node2_fields);

    neo4j_value_t rel2_fields[] =
        { neo4j_identity(9), neo4j_identity(2), neo4j_identity(3),
          rel2_type, neo4j_map(NULL, 0) };
    neo4j_value_t rel2 = neo4j_relationship(rel2_fields);

    neo4j_value_t node3_fields[] =
        { neo4j_identity(3), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node3 = neo4j_node(node3_fields);

    neo4j_value_t path_nodes[] = { node1, node2, node3 };
    neo4j_value_t path_rels[] = { rel1, rel2 };
    neo4j_value_t path_seq[] =
        { /*neo4j_int(0),*/ neo4j_int(-1),
          neo4j_int(1), neo4j_int(0),
          neo4j_int(2) };

    neo4j_value_t path_fields[] =
        { neo4j_list(path_nodes, 3), neo4j_list(path_rels, 2),
          neo4j_list(path_seq, 4) };
    neo4j_value_t value = neo4j_path(path_fields);
    ck_assert(neo4j_is_null(value));
    ck_assert_int_eq(errno, NEO4J_INVALID_PATH_SEQUENCE_IDX_RANGE);
}
END_TEST


START_TEST (invalid_path_seq_rel_neg_index_range)
{
    neo4j_value_t rel1_type = neo4j_string("Senator");
    neo4j_value_t rel2_type = neo4j_string("Candidate");

    neo4j_value_t node1_fields[] =
        { neo4j_identity(1), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node1 = neo4j_node(node1_fields);

    neo4j_value_t rel1_fields[] =
        { neo4j_identity(8), neo4j_identity(2), neo4j_identity(1),
          rel1_type, neo4j_map(NULL, 0) };
    neo4j_value_t rel1 = neo4j_relationship(rel1_fields);

    neo4j_value_t node2_fields[] =
        { neo4j_identity(2), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node2 = neo4j_node(node2_fields);

    neo4j_value_t rel2_fields[] =
        { neo4j_identity(9), neo4j_identity(2), neo4j_identity(3),
          rel2_type, neo4j_map(NULL, 0) };
    neo4j_value_t rel2 = neo4j_relationship(rel2_fields);

    neo4j_value_t node3_fields[] =
        { neo4j_identity(3), neo4j_list(NULL, 0), neo4j_map(NULL, 0) };
    neo4j_value_t node3 = neo4j_node(node3_fields);

    neo4j_value_t path_nodes[] = { node1, node2, node3 };
    neo4j_value_t path_rels[] = { rel1, rel2 };
    neo4j_value_t path_seq[] =
        { /*neo4j_int(0),*/ neo4j_int(-1),
          neo4j_int(1), neo4j_int(-3),
          neo4j_int(2) };

    neo4j_value_t path_fields[] =
        { neo4j_list(path_nodes, 3), neo4j_list(path_rels, 2),
          neo4j_list(path_seq, 4) };
    neo4j_value_t value = neo4j_path(path_fields);
    ck_assert(neo4j_is_null(value));
    ck_assert_int_eq(errno, NEO4J_INVALID_PATH_SEQUENCE_IDX_RANGE);
}
END_TEST


START_TEST (struct_value)
{
    neo4j_value_t field_values[] = { neo4j_int(1), neo4j_string("bernie") };
    neo4j_value_t value = neo4j_struct(0x78, field_values, 2);
    ck_assert(neo4j_type(value) == NEO4J_STRUCT);

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "struct<0x78>(1,\"bernie\")");

    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 24);
    ck_assert_int_eq(neo4j_ntostring(value, buf, sizeof(buf)), 24);
    ck_assert_str_eq(buf, "struct<0x78>(1,\"bernie\")");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 24), 24);
    ck_assert_str_eq(buf, "struct<0x78>(1,\"bernie\"");
    ck_assert_int_eq(neo4j_ntostring(value, buf, 23), 24);
    ck_assert_str_eq(buf, "struct<0x78>(1,\"bernie");

    ck_assert_int_eq(neo4j_fprint(value, memstream), 24);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "struct<0x78>(1,\"bernie\")");
}
END_TEST


START_TEST (struct_eq)
{
    neo4j_value_t field_values1[] = { neo4j_int(1), neo4j_int(2) };
    neo4j_value_t value1 = neo4j_struct(0x78, field_values1, 2);
    neo4j_value_t field_values2[] = { neo4j_int(1), neo4j_int(2) };
    neo4j_value_t value2 = neo4j_struct(0x78, field_values2, 2);
    neo4j_value_t field_values3[] = { neo4j_int(1), neo4j_int(2) };
    neo4j_value_t value3 = neo4j_struct(0x79, field_values3, 2);
    neo4j_value_t field_values4[] = { neo4j_int(1), neo4j_bool(false) };
    neo4j_value_t value4 = neo4j_struct(0x78, field_values4, 2);
    neo4j_value_t field_values5[] = { neo4j_int(1) };
    neo4j_value_t value5 = neo4j_struct(0x78, field_values5, 1);

    ck_assert(neo4j_eq(value1, value2));
    ck_assert(neo4j_eq(value2, value1));
    ck_assert(!neo4j_eq(value1, value3));
    ck_assert(!neo4j_eq(value3, value1));
    ck_assert(!neo4j_eq(value1, value4));
    ck_assert(!neo4j_eq(value4, value1));
    ck_assert(!neo4j_eq(value1, value5));
    ck_assert(!neo4j_eq(value5, value1));
}
END_TEST


START_TEST (identity_value)
{
    neo4j_value_t value = neo4j_identity(42);
    ck_assert(neo4j_type(value) == NEO4J_IDENTITY);

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(buf, "42");

    ck_assert_int_eq(neo4j_fprint(value, memstream), 2);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "42");
}
END_TEST


START_TEST (point_value)
{
    neo4j_point_data_t data;
    neo4j_value_t gps_point = neo4j_point(&data, -37.034825, 144.3167543);
    ck_assert(neo4j_type(gps_point) == NEO4J_POINT);

    char *str = neo4j_tostring(gps_point, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "point({latitude:-37.034825,longitude:144.316754})");

    ck_assert_int_eq(neo4j_ntostring(gps_point, NULL, 0), 49);
    ck_assert_int_eq(neo4j_ntostring(gps_point, buf, sizeof(buf)), 49);
    ck_assert_str_eq(buf, "point({latitude:-37.034825,longitude:144.316754})");

    neo4j_value_t c3_point = neo4j_3d_point(&data, NEO4J_CARTESIAN_3D,
            10, 15, -5);
    str = neo4j_tostring(c3_point, buf, sizeof(buf));
    ck_assert_str_eq(buf, "point({x:10,y:15,z:-5})");

    neo4j_value_t point_2d = neo4j_2d_point(&data, 1234, -5, 6.34);
    ck_assert_int_eq(neo4j_fprint(point_2d, memstream), 30);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "point({x:6.34,y:-5,srid:1234})");
}
END_TEST


START_TEST (local_datetime_value)
{
    neo4j_value_t value = neo4j_local_datetime(1929, 1, 15, 10, 15, 45,
            -2000009870);
    ck_assert(neo4j_type(value) == NEO4J_LOCAL_DATETIME);

    ck_assert_int_eq(neo4j_local_datetime_get_epoch_seconds(value),
            -1292593458);
    ck_assert_int_eq(neo4j_local_datetime_get_nanoseconds(value), 999990130);

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "1929-01-15T10:15:42.99999013");

    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 28);
    ck_assert_int_eq(neo4j_ntostring(value, buf, sizeof(buf)), 28);
    ck_assert_str_eq(buf, "1929-01-15T10:15:42.99999013");

    ck_assert_int_eq(neo4j_fprint(value, memstream), 28);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "1929-01-15T10:15:42.99999013");

    value = neo4j_local_datetime_from_epoch(10, 567);
    ck_assert_int_eq(neo4j_local_datetime_get_epoch_seconds(value), 10);
    ck_assert_int_eq(neo4j_local_datetime_get_nanoseconds(value), 567);

    str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "1970-01-01T00:00:10.000000567");
}
END_TEST


START_TEST (offset_datetime_value)
{
    neo4j_value_t value = neo4j_offset_datetime(1929, 1, 15, 10, 15, 45,
            -2000009870, 36000);
    ck_assert(neo4j_type(value) == NEO4J_OFFSET_DATETIME);

    ck_assert_int_eq(neo4j_offset_datetime_get_epoch_seconds(value),
            -1292593458);
    ck_assert_int_eq(neo4j_offset_datetime_get_nanoseconds(value), 999990130);
    ck_assert_int_eq(neo4j_offset_datetime_get_offset_seconds(value), 36000);

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "1929-01-15T10:15:42.99999013+10:00");

    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 34);
    ck_assert_int_eq(neo4j_ntostring(value, buf, sizeof(buf)), 34);
    ck_assert_str_eq(buf, "1929-01-15T10:15:42.99999013+10:00");

    ck_assert_int_eq(neo4j_fprint(value, memstream), 34);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "1929-01-15T10:15:42.99999013+10:00");

    value = neo4j_offset_datetime_from_epoch(10, 567, -14490);
    ck_assert_int_eq(neo4j_offset_datetime_get_epoch_seconds(value), 10);
    ck_assert_int_eq(neo4j_offset_datetime_get_nanoseconds(value), 567);
    ck_assert_int_eq(neo4j_offset_datetime_get_offset_seconds(value), -14490);

    str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "1970-01-01T00:00:10.000000567-04:01:30");
}
END_TEST


START_TEST (zoned_datetime_value)
{
    neo4j_zone_data_t data;
    neo4j_value_t value = neo4j_zoned_datetime(&data, 1929, 1, 15, 10, 15, 45,
            -2000009870, "Australia/Melbourne");
    ck_assert(neo4j_type(value) == NEO4J_ZONED_DATETIME);

    ck_assert_int_eq(neo4j_zoned_datetime_get_epoch_seconds(value),
            -1292593458);
    ck_assert_int_eq(neo4j_zoned_datetime_get_nanoseconds(value), 999990130);
    ck_assert_str_eq(neo4j_zoned_datetime_get_zoneid(value),
            "Australia/Melbourne");

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "1929-01-15T10:15:42.99999013[Australia/Melbourne]");

    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 49);
    ck_assert_int_eq(neo4j_ntostring(value, buf, sizeof(buf)), 49);
    ck_assert_str_eq(buf, "1929-01-15T10:15:42.99999013[Australia/Melbourne]");

    ck_assert_int_eq(neo4j_fprint(value, memstream), 49);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "1929-01-15T10:15:42.99999013[Australia/Melbourne]");

    value = neo4j_zoned_datetime_from_epoch(&data, 10, 567, "Europe/Berlin");
    ck_assert_int_eq(neo4j_zoned_datetime_get_epoch_seconds(value), 10);
    ck_assert_int_eq(neo4j_zoned_datetime_get_nanoseconds(value), 567);
    ck_assert_str_eq(neo4j_zoned_datetime_get_zoneid(value), "Europe/Berlin");

    str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "1970-01-01T00:00:10.000000567[Europe/Berlin]");
}
END_TEST


START_TEST (local_date_value)
{
    neo4j_value_t value = neo4j_local_date(1929, 1, 15);
    ck_assert(neo4j_type(value) == NEO4J_LOCAL_DATE);

    ck_assert_int_eq(neo4j_local_date_get_epoch_days(value), -14961);

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "1929-01-15");

    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 10);
    ck_assert_int_eq(neo4j_ntostring(value, buf, sizeof(buf)), 10);
    ck_assert_str_eq(buf, "1929-01-15");

    ck_assert_int_eq(neo4j_fprint(value, memstream), 10);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "1929-01-15");

    value = neo4j_local_date_from_epoch(576);
    ck_assert_int_eq(neo4j_local_date_get_epoch_days(value), 576);

    str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "1971-07-31");
}
END_TEST


START_TEST (local_time_value)
{
    neo4j_value_t value = neo4j_local_time(10, 15, 45, -2000009870);
    ck_assert(neo4j_type(value) == NEO4J_LOCAL_TIME);

    ck_assert_int_eq(neo4j_local_time_get_seconds_of_day(value), 36942);
    ck_assert_int_eq(neo4j_local_time_get_nanoseconds(value), 999990130);

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "10:15:42.99999013");

    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 17);
    ck_assert_int_eq(neo4j_ntostring(value, buf, sizeof(buf)), 17);
    ck_assert_str_eq(buf, "10:15:42.99999013");

    ck_assert_int_eq(neo4j_fprint(value, memstream), 17);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "10:15:42.99999013");

    value = neo4j_local_time_from_midnight(10, 567);
    ck_assert_int_eq(neo4j_local_time_get_seconds_of_day(value), 10);
    ck_assert_int_eq(neo4j_local_time_get_nanoseconds(value), 567);

    str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "00:00:10.000000567");
}
END_TEST


START_TEST (offset_time_value)
{
    neo4j_value_t value = neo4j_offset_time(10, 15, 45, -2000009870, 36000);
    ck_assert(neo4j_type(value) == NEO4J_OFFSET_TIME);

    ck_assert_int_eq(neo4j_offset_time_get_seconds_of_day(value), 36942);
    ck_assert_int_eq(neo4j_offset_time_get_nanoseconds(value), 999990130);
    ck_assert_int_eq(neo4j_offset_time_get_offset_seconds(value), 36000);

    char *str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "10:15:42.99999013+10:00");

    ck_assert_int_eq(neo4j_ntostring(value, NULL, 0), 23);
    ck_assert_int_eq(neo4j_ntostring(value, buf, sizeof(buf)), 23);
    ck_assert_str_eq(buf, "10:15:42.99999013+10:00");

    ck_assert_int_eq(neo4j_fprint(value, memstream), 23);
    fflush(memstream);
    ck_assert_str_eq(memstream_buffer, "10:15:42.99999013+10:00");

    value = neo4j_offset_time_from_midnight(10, 567, -14490);
    ck_assert_int_eq(neo4j_offset_time_get_seconds_of_day(value), 10);
    ck_assert_int_eq(neo4j_offset_time_get_nanoseconds(value), 567);

    str = neo4j_tostring(value, buf, sizeof(buf));
    ck_assert(str == buf);
    ck_assert_str_eq(str, "00:00:10.000000567-04:01:30");
}
END_TEST


TCase* values_tcase(void)
{
    TCase *tc = tcase_create("values");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, null_value);
    tcase_add_test(tc, null_eq);
    tcase_add_test(tc, bool_value);
    tcase_add_test(tc, bool_eq);
    tcase_add_test(tc, int_value);
    tcase_add_test(tc, int_eq);
    tcase_add_test(tc, float_value);
    tcase_add_test(tc, float_eq);
    tcase_add_test(tc, string_value);
    tcase_add_test(tc, string_eq);
    tcase_add_test(tc, bytes_value);
    tcase_add_test(tc, list_value);
    tcase_add_test(tc, list_eq);
    tcase_add_test(tc, map_value);
    tcase_add_test(tc, invalid_map_value);
    tcase_add_test(tc, map_eq);
    tcase_add_test(tc, map_get);
    tcase_add_test(tc, node_value);
    tcase_add_test(tc, invalid_node_label_value);
    tcase_add_test(tc, relationship_value);
    tcase_add_test(tc, unbound_relationship_value);
    tcase_add_test(tc, path_value);
    tcase_add_test(tc, invalid_path_node_value);
    tcase_add_test(tc, invalid_path_relationship_value);
    tcase_add_test(tc, invalid_path_seq_length);
    tcase_add_test(tc, invalid_path_seq_rel_index_type);
    tcase_add_test(tc, invalid_path_seq_node_index_type);
    tcase_add_test(tc, invalid_path_seq_rel_index_range);
    tcase_add_test(tc, invalid_path_seq_rel_zero_index_range);
    tcase_add_test(tc, invalid_path_seq_rel_neg_index_range);
    tcase_add_test(tc, identity_value);
    tcase_add_test(tc, struct_value);
    tcase_add_test(tc, struct_eq);
    tcase_add_test(tc, point_value);
    tcase_add_test(tc, local_datetime_value);
    tcase_add_test(tc, offset_datetime_value);
    tcase_add_test(tc, zoned_datetime_value);
    tcase_add_test(tc, local_date_value);
    tcase_add_test(tc, local_time_value);
    tcase_add_test(tc, offset_time_value);
    return tc;
}
