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


static neo4j_mpool_t mpool;
static const char *identifiers[] = { "n", "m", "l", "k", "j", "i", "h", "g" };
static char *memstream_buffer;
static size_t memstream_size;
static FILE *memstream;
static struct neo4j_statement_plan *plan;


static struct neo4j_statement_execution_step *produceStep(void)
{
    struct neo4j_statement_execution_step *step = neo4j_mpool_calloc(&mpool, 1,
            sizeof(struct neo4j_statement_execution_step));
    ck_assert(step != NULL);
    step->operator_type = "ProduceResults";
    step->identifiers = identifiers;
    step->nidentifiers = 8;
    step->estimated_rows = 5.4;
    step->rows = 8;
    step->db_hits = 935;
    step->sources = neo4j_mpool_calloc(&mpool, 5,
            sizeof(struct neo4j_statement_execution_step *));
    ck_assert(step->sources != NULL);
    step->arguments = neo4j_list(NULL, 0);
    return step;
}


static struct neo4j_statement_execution_step *labelScanStep(void)
{
    struct neo4j_statement_execution_step *step = neo4j_mpool_calloc(&mpool, 1,
            sizeof(struct neo4j_statement_execution_step));
    ck_assert(step != NULL);
    step->operator_type = "NodeByLabelScan";
    step->identifiers = identifiers;
    step->nidentifiers = 8;
    step->estimated_rows = 10;
    step->rows = 5;
    step->db_hits = 42;
    step->sources = neo4j_mpool_calloc(&mpool, 5,
            sizeof(struct neo4j_statement_execution_step *));
    neo4j_map_entry_t *arguments = neo4j_mpool_calloc(&mpool, 1,
            sizeof(neo4j_map_entry_t));
    ck_assert(arguments != NULL);
    arguments[0] = neo4j_map_entry("LabelName", neo4j_string(":Person"));
    step->arguments = neo4j_map(arguments, 1);
    return step;
}


static void setup(void)
{
    mpool = neo4j_mpool(&neo4j_std_memory_allocator, 1024);
    memstream = open_memstream(&memstream_buffer, &memstream_size);
    ck_assert(memstream != NULL);
    plan = neo4j_mpool_calloc(&mpool, 1, sizeof(struct neo4j_statement_plan));
    ck_assert(plan != NULL);
    plan->version = "CYPHER 3.0";
    plan->planner = "COST";
    plan->runtime = "INTERPRETTED";
}


static void teardown(void)
{
    fclose(memstream);
    free(memstream_buffer);
    neo4j_mpool_drain(&mpool);
}


START_TEST (render_simple_plan)
{
    plan->output_step = produceStep();
    plan->output_step->nidentifiers = 2;
    plan->output_step->nsources = 1;
    plan->output_step->sources[0] = labelScanStep();
    plan->output_step->sources[0]->nidentifiers = 1;

    fputc('\n', memstream);

    int result = neo4j_render_plan_table(memstream, plan, 73, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+------------------+----------------+-------------+---------------------+\n"
 "| Operator         | Estimated Rows | Identifiers | Other               |\n"
 "+------------------+----------------+-------------+---------------------+\n"
 "| *NodeByLabelScan |             10 | n           | :Person             |\n"
 "| |                +----------------+-------------+---------------------+\n"
 "| *ProduceResults  |              5 | n, m        |                     |\n"
 "+------------------+----------------+-------------+---------------------+\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_branched_plan)
{
    plan->output_step = produceStep();
    plan->output_step->nidentifiers = 3;
    plan->output_step->nsources = 2;
    plan->output_step->sources[0] = labelScanStep();
    plan->output_step->sources[0]->nidentifiers = 1;
    plan->output_step->sources[1] = labelScanStep();
    plan->output_step->sources[1]->nidentifiers = 8;
    neo4j_map_entry_t *arguments = neo4j_mpool_calloc(&mpool, 2,
            sizeof(neo4j_map_entry_t));
    ck_assert(arguments != NULL);
    arguments[0] = neo4j_map_entry("LabelName", neo4j_string(":City"));
    arguments[1] = neo4j_map_entry("LegacyExpression",
            neo4j_string("n.age > { AUTOINT0 }"));
    plan->output_step->sources[1]->arguments = neo4j_map(arguments, 2);

    fputc('\n', memstream);

    int result = neo4j_render_plan_table(memstream, plan, 73, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+--------------------+----------------+----------------+----------------+\n"
 "| Operator           | Estimated Rows | Identifiers    | Other          |\n"
 "+--------------------+----------------+----------------+----------------+\n"
 "| *NodeByLabelScan   |             10 | n              | :Person        |\n"
 "| |                  +----------------+----------------+----------------+\n"
 "| | *NodeByLabelScan |             10 | n, m, l, k, j, | :City; n.age > |\n"
 "| | |                |                |  i, h, g       |  { AUTOINT0 }  |\n"
 "| |/                 +----------------+----------------+----------------+\n"
 "| *ProduceResults    |              5 | n, m, l        |                |\n"
 "+--------------------+----------------+----------------+----------------+\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_multi_branched_plan)
{
    plan->output_step = produceStep();
    plan->output_step->nidentifiers = 2;
    plan->output_step->nsources = 3;
    plan->output_step->sources[0] = labelScanStep();
    plan->output_step->sources[0]->nidentifiers = 8;
    plan->output_step->sources[1] = labelScanStep();
    plan->output_step->sources[1]->identifiers = (identifiers + 1);
    plan->output_step->sources[1]->nidentifiers = 1;
    neo4j_map_entry_t *arguments = neo4j_mpool_calloc(&mpool, 2,
            sizeof(neo4j_map_entry_t));
    ck_assert(arguments != NULL);
    arguments[0] = neo4j_map_entry("LabelName", neo4j_string(":City"));
    arguments[1] = neo4j_map_entry("LegacyExpression",
            neo4j_string("n.age > { AUTOINT0 }"));
    plan->output_step->sources[1]->arguments = neo4j_map(arguments, 2);
    plan->output_step->sources[2] = labelScanStep();
    plan->output_step->sources[2]->identifiers = (identifiers + 2);
    plan->output_step->sources[2]->nidentifiers = 1;

    fputc('\n', memstream);

    int result = neo4j_render_plan_table(memstream, plan, 73, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+--------------------+----------------+----------------+----------------+\n"
 "| Operator           | Estimated Rows | Identifiers    | Other          |\n"
 "+--------------------+----------------+----------------+----------------+\n"
 "| *NodeByLabelScan   |             10 | n, m, l, k, j, | :Person        |\n"
 "| |                  |                |  i, h, g       |                |\n"
 "| |                  +----------------+----------------+----------------+\n"
 "| | *NodeByLabelScan |             10 | m              | :City; n.age > |\n"
 "| | |                |                |                |  { AUTOINT0 }  |\n"
 "| |/                 +----------------+----------------+----------------+\n"
 "| | *NodeByLabelScan |             10 | l              | :Person        |\n"
 "| |/                 +----------------+----------------+----------------+\n"
 "| *ProduceResults    |              5 | n, m           |                |\n"
 "+--------------------+----------------+----------------+----------------+\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_deep_branched_plan)
{
    plan->output_step = produceStep();
    plan->output_step->nidentifiers = 2;
    plan->output_step->nsources = 2;
    plan->output_step->sources[0] = labelScanStep();
    plan->output_step->sources[0]->nidentifiers = 1;
    plan->output_step->sources[1] = labelScanStep();
    plan->output_step->sources[1]->identifiers = (identifiers + 1);
    plan->output_step->sources[1]->nidentifiers = 1;

    plan->output_step->sources[1]->nsources = 2;
    plan->output_step->sources[1]->sources[0] = labelScanStep();
    plan->output_step->sources[1]->sources[0]->identifiers = (identifiers + 2);
    plan->output_step->sources[1]->sources[0]->nidentifiers = 1;
    plan->output_step->sources[1]->sources[1] = labelScanStep();
    plan->output_step->sources[1]->sources[1]->identifiers = (identifiers + 3);
    plan->output_step->sources[1]->sources[1]->nidentifiers = 1;

    fputc('\n', memstream);

    int result = neo4j_render_plan_table(memstream, plan, 73, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+----------------------+----------------+-------------+-----------------+\n"
 "| Operator             | Estimated Rows | Identifiers | Other           |\n"
 "+----------------------+----------------+-------------+-----------------+\n"
 "| *NodeByLabelScan     |             10 | n           | :Person         |\n"
 "| |                    +----------------+-------------+-----------------+\n"
 "| | *NodeByLabelScan   |             10 | l           | :Person         |\n"
 "| | |                  +----------------+-------------+-----------------+\n"
 "| | | *NodeByLabelScan |             10 | k           | :Person         |\n"
 "| | |/                 +----------------+-------------+-----------------+\n"
 "| | *NodeByLabelScan   |             10 | m           | :Person         |\n"
 "| |/                   +----------------+-------------+-----------------+\n"
 "| *ProduceResults      |              5 | n, m        |                 |\n"
 "+----------------------+----------------+-------------+-----------------+\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_narrow_plan)
{
    plan->output_step = produceStep();
    plan->output_step->nidentifiers = 3;
    plan->output_step->nsources = 2;
    plan->output_step->sources[0] = labelScanStep();
    plan->output_step->sources[0]->nidentifiers = 8;
    plan->output_step->sources[1] = labelScanStep();
    plan->output_step->sources[1]->identifiers = (identifiers + 1);
    plan->output_step->sources[1]->nidentifiers = 1;
    neo4j_map_entry_t *arguments = neo4j_mpool_calloc(&mpool, 2,
            sizeof(neo4j_map_entry_t));
    ck_assert(arguments != NULL);
    arguments[0] = neo4j_map_entry("LabelName", neo4j_string(":City"));
    arguments[1] = neo4j_map_entry("LegacyExpression",
            neo4j_string("n.age > { AUTOINT0 }"));
    plan->output_step->sources[1]->arguments = neo4j_map(arguments, 2);

    fputc('\n', memstream);

    int result = neo4j_render_plan_table(memstream, plan, 61, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+--------------------+----------------+-------------+-------+\n"
 "| Operator           | Estimated Rows | Identifiers | Other |\n"
 "+--------------------+----------------+-------------+-------+\n"
 "| *NodeByLabelScan   |             10 | n, m, l, k, | :Pers |\n"
 "| |                  |                |  j, i, h, g | on    |\n"
 "| |                  +----------------+-------------+-------+\n"
 "| | *NodeByLabelScan |             10 | m           | :City |\n"
 "| | |                |                |             | ; n.a |\n"
 "| | |                |                |             | ge >  |\n"
 "| | |                |                |             | { AUT |\n"
 "| | |                |                |             | OINT0 |\n"
 "| | |                |                |             |  }    |\n"
 "| |/                 +----------------+-------------+-------+\n"
 "| *ProduceResults    |              5 | n, m, l     |       |\n"
 "+--------------------+----------------+-------------+-------+\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_undersized_plan)
{
    plan->output_step = produceStep();
    plan->output_step->nidentifiers = 8;
    plan->output_step->nsources = 2;
    plan->output_step->sources[0] = labelScanStep();
    plan->output_step->sources[0]->nidentifiers = 1;
    plan->output_step->sources[1] = labelScanStep();
    plan->output_step->sources[1]->identifiers = (identifiers + 1);
    plan->output_step->sources[1]->nidentifiers = 1;

    fputc('\n', memstream);

    int result = neo4j_render_plan_table(memstream, plan, 60, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+--------------------+----------------+-------------+-\n"
 "| Operator           | Estimated Rows | Identifiers |=\n"
 "+--------------------+----------------+-------------+-\n"
 "| *NodeByLabelScan   |             10 | n           |=\n"
 "| |                  +----------------+-------------+-\n"
 "| | *NodeByLabelScan |             10 | m           |=\n"
 "| |/                 +----------------+-------------+-\n"
 "| *ProduceResults    |              5 | n, m, l, k, |=\n"
 "|                    |                |  j, i, h, g |=\n"
 "+--------------------+----------------+-------------+-\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_very_undersized_plan)
{
    plan->output_step = produceStep();
    plan->output_step->nidentifiers = 3;
    plan->output_step->nsources = 2;
    plan->output_step->sources[0] = labelScanStep();
    plan->output_step->sources[0]->nidentifiers = 1;
    plan->output_step->sources[1] = labelScanStep();
    plan->output_step->sources[1]->identifiers = (identifiers + 1);
    plan->output_step->sources[1]->nidentifiers = 1;

    fputc('\n', memstream);

    int result = neo4j_render_plan_table(memstream, plan, 50, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+--------------------+----------------+-\n"
 "| Operator           | Estimated Rows |=\n"
 "+--------------------+----------------+-\n"
 "| *NodeByLabelScan   |             10 |=\n"
 "| |                  +----------------+-\n"
 "| | *NodeByLabelScan |             10 |=\n"
 "| |/                 +----------------+-\n"
 "| *ProduceResults    |              5 |=\n"
 "+--------------------+----------------+-\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_vvery_undersized_plan)
{
    plan->output_step = produceStep();
    plan->output_step->nidentifiers = 3;
    plan->output_step->nsources = 2;
    plan->output_step->sources[0] = labelScanStep();
    plan->output_step->sources[0]->nidentifiers = 1;
    plan->output_step->sources[1] = labelScanStep();
    plan->output_step->sources[1]->identifiers = (identifiers + 1);
    plan->output_step->sources[1]->nidentifiers = 1;

    fputc('\n', memstream);

    int result = neo4j_render_plan_table(memstream, plan, 38, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+--------------------+-\n"
 "| Operator           |=\n"
 "+--------------------+-\n"
 "| *NodeByLabelScan   |=\n"
 "| |                  +-\n"
 "| | *NodeByLabelScan |=\n"
 "| |/                 +-\n"
 "| *ProduceResults    |=\n"
 "+--------------------+-\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_vvvery_undersized_plan)
{
    plan->output_step = produceStep();
    plan->output_step->nidentifiers = 3;
    plan->output_step->nsources = 2;
    plan->output_step->sources[0] = labelScanStep();
    plan->output_step->sources[0]->nidentifiers = 1;
    plan->output_step->sources[1] = labelScanStep();
    plan->output_step->sources[1]->identifiers = (identifiers + 1);
    plan->output_step->sources[1]->nidentifiers = 1;

    fputc('\n', memstream);

    int result = neo4j_render_plan_table(memstream, plan, 21, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+-\n"
 "|=\n"
 "+-\n"
 "|=\n"
 "|=\n"
 "|=\n"
 "|=\n"
 "|=\n"
 "+-\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_simple_profile)
{
    plan->is_profile = true;
    plan->output_step = produceStep();
    plan->output_step->nidentifiers = 2;
    plan->output_step->nsources = 1;
    plan->output_step->sources[0] = labelScanStep();
    plan->output_step->sources[0]->nidentifiers = 1;

    fputc('\n', memstream);

    int result = neo4j_render_plan_table(memstream, plan, 78, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+------------------+----------------+------+---------+-------------+---------+\n"
 "| Operator         | Estimated Rows | Rows | DB Hits | Identifiers | Other   |\n"
 "+------------------+----------------+------+---------+-------------+---------+\n"
 "| *NodeByLabelScan |             10 |    5 |      42 | n           | :Person |\n"
 "| |                +----------------+------+---------+-------------+---------+\n"
 "| *ProduceResults  |              5 |    8 |     935 | n, m        |         |\n"
 "+------------------+----------------+------+---------+-------------+---------+\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


START_TEST (render_narrow_profile)
{
    plan->is_profile = true;
    plan->output_step = produceStep();
    plan->output_step->nidentifiers = 2;
    plan->output_step->nsources = 1;
    plan->output_step->sources[0] = labelScanStep();
    plan->output_step->sources[0]->nidentifiers = 1;

    fputc('\n', memstream);

    int result = neo4j_render_plan_table(memstream, plan, 60, NEO4J_RENDER_ASCII);
    ck_assert(result == 0);
    fflush(memstream);

    const char *expect = "\n"
//1       10        20        30        40        50        60        70
 "+------------------+----------------+------+---------+-\n"
 "| Operator         | Estimated Rows | Rows | DB Hits |=\n"
 "+------------------+----------------+------+---------+-\n"
 "| *NodeByLabelScan |             10 |    5 |      42 |=\n"
 "| |                +----------------+------+---------+-\n"
 "| *ProduceResults  |              5 |    8 |     935 |=\n"
 "+------------------+----------------+------+---------+-\n";
    ck_assert_str_eq(memstream_buffer, expect);
}
END_TEST


TCase* render_plan_tcase(void)
{
    TCase *tc = tcase_create("render_plan");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, render_simple_plan);
    tcase_add_test(tc, render_branched_plan);
    tcase_add_test(tc, render_multi_branched_plan);
    tcase_add_test(tc, render_deep_branched_plan);
    tcase_add_test(tc, render_narrow_plan);
    tcase_add_test(tc, render_undersized_plan);
    tcase_add_test(tc, render_very_undersized_plan);
    tcase_add_test(tc, render_vvery_undersized_plan);
    tcase_add_test(tc, render_vvvery_undersized_plan);
    tcase_add_test(tc, render_simple_profile);
    tcase_add_test(tc, render_narrow_profile);
    return tc;
}
