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
#include "../src/neo4j-client.h"
#include "../src/memory.h"
#include "canned_result_stream.h"
#include "memstream.h"
#include <check.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>


static neo4j_result_stream_t *build_stream(const char * const *fieldnames,
        unsigned int nfields, const char *table[][nfields], unsigned int nrows);
static const char *gstrsub(char from, char to, const char *s);

static neo4j_mpool_t mpool;
static char *memstream_buffer;
static size_t memstream_size;
static FILE *memstream;


static void setup(void)
{
    mpool = neo4j_mpool(&neo4j_std_memory_allocator, 1024);
    memstream = open_memstream(&memstream_buffer, &memstream_size);
}


static void teardown(void)
{
    fclose(memstream);
    free(memstream_buffer);
    neo4j_mpool_drain(&mpool);
}


neo4j_result_stream_t *build_stream(const char * const *fieldnames,
        unsigned int nfields, const char *table[][nfields], unsigned int nrows)
{
    neo4j_value_t *records = (nrows > 0)?
        neo4j_mpool_calloc(&mpool, nrows, sizeof(neo4j_value_t)) : NULL;
    ck_assert(nrows == 0 || records != NULL);

    for (unsigned int i = 0; i < nrows; ++i)
    {
        neo4j_value_t *values = neo4j_mpool_calloc(&mpool,
                nfields, sizeof(neo4j_value_t));
        ck_assert(values != NULL);
        for (unsigned int j = 0; j < nfields; ++j)
        {
            values[j] = neo4j_string(table[i][j]);
        }

        records[i] = neo4j_list(values, nfields);
    }

    return neo4j_canned_result_stream(fieldnames, nfields, records, nrows);
}


const char *gstrsub(char from, char to, const char *s)
{
    size_t n = strlen(s);
    char *buf = neo4j_mpool_alloc(&mpool, n+1);
    ck_assert(buf != NULL);
    for (size_t i = 0; i < n; ++i)
    {
        buf[i] = (s[i] == from)? to : s[i];
    }
    buf[n] = '\0';
    return buf;
}


START_TEST (render_empty_table)
{
    const char *fieldnames[4] =
        { "firstname", "lastname", "role", "title" };
    neo4j_result_stream_t *results = build_stream(fieldnames, 4, NULL, 0);

    ck_assert(fputc('\n', memstream) != EOF);

    int result = neo4j_render_table(memstream, results, 49, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);
    neo4j_close_results(results);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+--------------+-------------+--------+---------+\n"
 "| firstname    | lastname    | role   | title   |\n"
 "+--------------+-------------+--------+---------+\n"
 "+--------------+-------------+--------+---------+\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_simple_table)
{
    const char *fieldnames[4] =
        { "firstname", "lastname", "role", "title" };
    const char *table[3][4] =
        { { "Keanu", "Reeves", "Neo", "The Matrix" },
          { "Hugo", "Weaving", "V", "V for Vendetta" },
          { "Halle", "Berry", "Luisa Rey", "Cloud Atlas" } };
    neo4j_result_stream_t *results = build_stream(fieldnames, 4, table, 3);

    ck_assert(fputc('\n', memstream) != EOF);

    int result = neo4j_render_table(memstream, results, 73, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);
    neo4j_close_results(results);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+----------------+---------------+----------------+---------------------+\n"
 "| firstname      | lastname      | role           | title               |\n"
 "+----------------+---------------+----------------+---------------------+\n"
 "| Keanu          | Reeves        | Neo            | The Matrix          |\n"
 "| Hugo           | Weaving       | V              | V for Vendetta      |\n"
 "| Halle          | Berry         | Luisa Rey      | Cloud Atlas         |\n"
 "+----------------+---------------+----------------+---------------------+\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_simple_table_with_quoted_strings)
{
    const char *fieldnames[4] =
        { "firstname", "lastname", "role", "title" };
    const char *table[3][4] =
        { { "Keanu", "Reeves", "Neo", "The Matrix" },
          { "Hugo", "Weaving", "V", "V for Vendetta" },
          { "Halle", "Berry", "Luisa Rey", "Cloud Atlas" } };
    neo4j_result_stream_t *results = build_stream(fieldnames, 4, table, 3);

    ck_assert(fputc('\n', memstream) != EOF);

    int result = neo4j_render_table(memstream, results, 56,
            NEO4J_RENDER_QUOTE_STRINGS | NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);
    neo4j_close_results(results);

    const char *expect = gstrsub('\'', '"', "\n"
//1       10        20        30        40        50        60        70
 "+-----------+-----------+-------------+----------------+\n"
 "| firstname | lastname  | role        | title          |\n"
 "+-----------+-----------+-------------+----------------+\n"
 "| 'Keanu'   | 'Reeves'  | 'Neo'       | 'The Matrix'   |\n"
 "| 'Hugo'    | 'Weaving' | 'V'         | 'V for Vendett=|\n"
 "| 'Halle'   | 'Berry'   | 'Luisa Rey' | 'Cloud Atlas'  |\n"
 "+-----------+-----------+-------------+----------------+\n");
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_narrow_table)
{
    const char *fieldnames[4] =
        { "the first name", "lastname", "role", "title" };
    const char *table[3][4] =
        { { "Keanu", "Reeves", "Neo", "The Matrix" },
          { "Hugo", "Weaving", "V", "V for Vendetta" },
          { "Halle", "Berry", "Luisa Rey", "Cloud Atlas" } };
    neo4j_result_stream_t *results = build_stream(fieldnames, 4, table, 3);

    ck_assert(fputc('\n', memstream) != EOF);

    int result = neo4j_render_table(memstream, results, 53, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);
    neo4j_close_results(results);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+--------------+----------+-----------+-------------+\n"
 "| the first na=| lastname | role      | title       |\n"
 "+--------------+----------+-----------+-------------+\n"
 "| Keanu        | Reeves   | Neo       | The Matrix  |\n"
 "| Hugo         | Weaving  | V         | V for Vende=|\n"
 "| Halle        | Berry    | Luisa Rey | Cloud Atlas |\n"
 "+--------------+----------+-----------+-------------+\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_very_narrow_table)
{
    const char *fieldnames[4] =
        { "the first name", "lastname", "role", "title" };
    const char *table[3][4] =
        { { "Keanu", "Reeves", "Neo", "The Matrix" },
          { "Hugo", "Weaving", "V", "V for Vendetta" },
          { "", "Berry", "Luisa Rey", "Cloud Atlas" } };
    neo4j_result_stream_t *results = build_stream(fieldnames, 4, table, 3);

    ck_assert(fputc('\n', memstream) != EOF);

    int result = neo4j_render_table(memstream, results, 13, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);
    neo4j_close_results(results);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+--+--+--+--+\n"
 "| =| =| =| =|\n"
 "+--+--+--+--+\n"
 "| =| =| =| =|\n"
 "| =| =| =| =|\n"
 "|  | =| =| =|\n"
 "+--+--+--+--+\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_undersized_table)
{
    const char *fieldnames[4] =
        { "the first name", "lastname", "role", "title" };
    const char *table[3][4] =
        { { "Keanu", "Reeves", "Neo", "The Matrix" },
          { "Hugo", "Weaving", "V", "V for Vendetta" },
          { "", "Berry", "Luisa Rey", "Cloud Atlas" } };
    neo4j_result_stream_t *results = build_stream(fieldnames, 4, table, 3);

    ck_assert(fputc('\n', memstream) != EOF);

    int result = neo4j_render_table(memstream, results, 8, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);
    neo4j_close_results(results);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+--+--+-\n"
 "| =| =|=\n"
 "+--+--+-\n"
 "| =| =|=\n"
 "| =| =|=\n"
 "|  | =|=\n"
 "+--+--+-\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_min_width_table)
{
    const char *fieldnames[4] =
        { "the first name", "lastname", "role", "title" };
    const char *table[3][4] =
        { { "Keanu", "Reeves", "Neo", "The Matrix" },
          { "Hugo", "Weaving", "V", "V for Vendetta" },
          { "", "Berry", "Luisa Rey", "Cloud Atlas" } };
    neo4j_result_stream_t *results = build_stream(fieldnames, 4, table, 3);

    ck_assert(fputc('\n', memstream) != EOF);

    int result = neo4j_render_table(memstream, results, 2, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);
    neo4j_close_results(results);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+-\n"
 "|=\n"
 "+-\n"
 "|=\n"
 "|=\n"
 "|=\n"
 "+-\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_zero_col_table)
{
    neo4j_result_stream_t *results = build_stream(NULL, 0, NULL, 0);
    int result = neo4j_render_table(memstream, results, 2, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);
    neo4j_close_results(results);

    const char *expect =
//1       10        20        30        40        50        60        70
 "";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_table_with_wrapped_values)
{
    const char *fieldnames[4] =
        { "firstname", "lastname", "role", "title" };
    const char *table[3][4] =
        { { "Keanu", "Reeves", "Neo", "The Matrix" },
          { "Hugo With A Long Middle Name", "Weaving", "V", "V for Vendetta" },
          { "Halle", "Berry", "Luisa Rey", "The Cloud Atlas" } };
    neo4j_result_stream_t *results = build_stream(fieldnames, 4, table, 3);

    ck_assert(fputc('\n', memstream) != EOF);

    int result = neo4j_render_table(memstream, results, 61,
            NEO4J_RENDER_QUOTE_STRINGS | NEO4J_RENDER_ASCII |
            NEO4J_RENDER_WRAP_VALUES);
    ck_assert(result == 0);
    fflush(memstream);
    neo4j_close_results(results);

    const char *expect = gstrsub('\'', '"', "\n"
//1       10        20        30        40        50        60        70
 "+----------------+-----------+-------------+----------------+\n"
 "| firstname      | lastname  | role        | title          |\n"
 "+----------------+-----------+-------------+----------------+\n"
 "| 'Keanu'        | 'Reeves'  | 'Neo'       | 'The Matrix'   |\n"
 "| 'Hugo With A L=| 'Weaving' | 'V'         | 'V for Vendett=|\n"
 "|=ong Middle Nam=|           |             |=a'             |\n"
 "|=e'             |           |             |                |\n"
 "| 'Halle'        | 'Berry'   | 'Luisa Rey' | 'The Cloud Atl=|\n"
 "|                |           |             |=as'            |\n"
 "+----------------+-----------+-------------+----------------+\n");
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_undersized_table_with_wrapped_values)
{
    const char *fieldnames[4] =
        { "first", "last", "role", "title" };
    const char *table[3][4] =
        { { "Keanu", "Reeves", "Neo", "The Matrix" },
          { "Hugo", "Weaving", "V", "V for Vendetta" },
          { "", "Berry", "Luisa Rey", "Cloud Atlas" } };
    neo4j_result_stream_t *results = build_stream(fieldnames, 4, table, 3);

    ck_assert(fputc('\n', memstream) != EOF);

    int result = neo4j_render_table(memstream, results, 7,
            NEO4J_RENDER_ASCII | NEO4J_RENDER_WRAP_VALUES |
            NEO4J_RENDER_ROWLINES);
    ck_assert(result == 0);
    fflush(memstream);
    neo4j_close_results(results);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+----+-\n"
 "| fi=|=\n"
 "|=rs=|=\n"
 "|=t  |=\n"
 "+----+-\n"
 "| Ke=|=\n"
 "|=an=|=\n"
 "|=u  |=\n"
 "+----+-\n"
 "| Hu=|=\n"
 "|=go |=\n"
 "+----+-\n"
 "|    |=\n"
 "+----+-\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_table_with_nulls)
{
    const char *fieldnames[3] =
        { "firstname", "lastname", "born" };
    neo4j_value_t row[3] =
        { neo4j_string("Keanu"), neo4j_null, neo4j_int(1964) };
    neo4j_value_t records[1] = { neo4j_list(row, 3) };

    neo4j_result_stream_t *results =
        neo4j_canned_result_stream(fieldnames, 3, records, 1);

    ck_assert(fputc('\n', memstream) != EOF);

    int result = neo4j_render_table(memstream, results, 52, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);
    neo4j_close_results(results);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+------------------+-----------------+-------------+\n"
 "| firstname        | lastname        | born        |\n"
 "+------------------+-----------------+-------------+\n"
 "| Keanu            |                 | 1964        |\n"
 "+------------------+-----------------+-------------+\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_table_with_visible_nulls)
{
    const char *fieldnames[3] =
        { "firstname", "lastname", "born" };
    neo4j_value_t row[3] =
        { neo4j_string("Keanu"), neo4j_null, neo4j_int(1964) };
    neo4j_value_t records[1] = { neo4j_list(row, 3) };

    neo4j_result_stream_t *results =
        neo4j_canned_result_stream(fieldnames, 3, records, 1);

    ck_assert(fputc('\n', memstream) != EOF);

    int result = neo4j_render_table(memstream, results, 52,
            NEO4J_RENDER_SHOW_NULLS | NEO4J_RENDER_QUOTE_STRINGS | NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);
    neo4j_close_results(results);

    const char *expect = gstrsub('\'', '"', "\n"
//1       10        20        30        40        50        60        70
 "+------------------+-----------------+-------------+\n"
 "| firstname        | lastname        | born        |\n"
 "+------------------+-----------------+-------------+\n"
 "| 'Keanu'          | null            | 1964        |\n"
 "+------------------+-----------------+-------------+\n");
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_no_table_if_stream_has_error)
{
    neo4j_result_stream_t *results = build_stream(NULL, 0, NULL, 0);
    neo4j_crs_set_error(results, "Failed");

    int result = neo4j_render_table(memstream, results, 2, NEO4J_RENDER_ASCII);
    ck_assert(result != 0);
    fflush(memstream);
    neo4j_close_results(results);

    ck_assert_str_eq(memstream_buffer, "");
}
END_TEST


START_TEST (render_empty_csv)
{
    const char *fieldnames[4] =
        { "firstname", "lastname", "role", "title" };

    neo4j_result_stream_t *results = build_stream(fieldnames, 4, NULL, 0);
    int result = neo4j_render_csv(memstream, results, 0);
    ck_assert(result == 0);
    fflush(memstream);
    neo4j_close_results(results);

    const char *expect =
 "\"firstname\",\"lastname\",\"role\",\"title\"\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_simple_csv)
{
    const char *fieldnames[4] =
        { "firstname", "lastname", "role", "title" };
    const char *table[3][4] =
        { { "Keanu", "Reeves", "Neo", "The Matrix" },
          { "Hugo", "Weaving", "V", "V for Vendetta" },
          { "Halle", "Berry", "Luisa Rey", "Cloud Atlas" } };

    neo4j_result_stream_t *results = build_stream(fieldnames, 4, table, 3);
    int result = neo4j_render_csv(memstream, results, 0);
    ck_assert(result == 0);
    fflush(memstream);
    neo4j_close_results(results);

    const char *expect =
 "\"firstname\",\"lastname\",\"role\",\"title\"\n"
 "\"Keanu\",\"Reeves\",\"Neo\",\"The Matrix\"\n"
 "\"Hugo\",\"Weaving\",\"V\",\"V for Vendetta\"\n"
 "\"Halle\",\"Berry\",\"Luisa Rey\",\"Cloud Atlas\"\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_quotes_in_csv_values)
{
    const char *fieldnames[4] =
        { "firstname", "lastname", "\"role\"", "title" };
    const char *table[3][4] =
        { { "Keanu", "Reeves", "Neo", "The Matrix" },
          { "Hugo", "Weaving", "\"V\"", "V for Vendetta" },
          { "Halle", "Berry", "Luisa Rey", "Cloud Atlas" } };

    neo4j_result_stream_t *results = build_stream(fieldnames, 4, table, 3);
    int result = neo4j_render_csv(memstream, results, 0);
    ck_assert(result == 0);
    fflush(memstream);
    neo4j_close_results(results);

    const char *expect =
 "\"firstname\",\"lastname\",\"\"\"role\"\"\",\"title\"\n"
 "\"Keanu\",\"Reeves\",\"Neo\",\"The Matrix\"\n"
 "\"Hugo\",\"Weaving\",\"\"\"V\"\"\",\"V for Vendetta\"\n"
 "\"Halle\",\"Berry\",\"Luisa Rey\",\"Cloud Atlas\"\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_zero_col_csv)
{
    neo4j_result_stream_t *results = build_stream(NULL, 0, NULL, 0);
    int result = neo4j_render_csv(memstream, results, 0);
    ck_assert(result == 0);
    fflush(memstream);
    neo4j_close_results(results);

    const char *expect = "";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


TCase* render_results_tcase(void)
{
    TCase *tc = tcase_create("render_results");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, render_empty_table);
    tcase_add_test(tc, render_simple_table);
    tcase_add_test(tc, render_simple_table_with_quoted_strings);
    tcase_add_test(tc, render_narrow_table);
    tcase_add_test(tc, render_very_narrow_table);
    tcase_add_test(tc, render_undersized_table);
    tcase_add_test(tc, render_min_width_table);
    tcase_add_test(tc, render_zero_col_table);
    tcase_add_test(tc, render_table_with_wrapped_values);
    tcase_add_test(tc, render_undersized_table_with_wrapped_values);
    tcase_add_test(tc, render_table_with_nulls);
    tcase_add_test(tc, render_table_with_visible_nulls);
    tcase_add_test(tc, render_no_table_if_stream_has_error);
    tcase_add_test(tc, render_empty_csv);
    tcase_add_test(tc, render_simple_csv);
    tcase_add_test(tc, render_quotes_in_csv_values);
    tcase_add_test(tc, render_zero_col_csv);
    return tc;
}
