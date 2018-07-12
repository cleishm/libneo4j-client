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
#include "../src/render.h"
#include <check.h>
#include <errno.h>


START_TEST (single_column_reduction)
{
    unsigned int widths[1] = { 40 };
    ck_assert_int_eq(fit_column_widths(1, widths, 1, 14), 0);
    ck_assert_uint_eq(widths[0], 14);
}
END_TEST


START_TEST (first_column_reduction)
{
    unsigned int widths[2] = { 40, 1 };
    ck_assert_int_eq(fit_column_widths(2, widths, 1, 20), 0);
    ck_assert_uint_eq(widths[0], 19);
    ck_assert_uint_eq(widths[1], 1);
}
END_TEST


START_TEST (equal_columns_reduction)
{
    unsigned int widths[2] = { 40, 40 };
    ck_assert_int_eq(fit_column_widths(2, widths, 1, 20), 0);
    ck_assert_uint_eq(widths[0], 10);
    ck_assert_uint_eq(widths[1], 10);
}
END_TEST


START_TEST (some_equal_columns_reduction)
{
    unsigned int widths[3] = { 20, 2, 20 };
    ck_assert_int_eq(fit_column_widths(3, widths, 1, 22), 0);
    ck_assert_uint_eq(widths[0], 10);
    ck_assert_uint_eq(widths[1], 2);
    ck_assert_uint_eq(widths[2], 10);
}
END_TEST


START_TEST (multi_step_reduction)
{
    unsigned int widths[3] = { 20, 8, 15 };
    ck_assert_int_eq(fit_column_widths(3, widths, 1, 15), 0);
    ck_assert_uint_eq(widths[0], 5);
    ck_assert_uint_eq(widths[1], 5);
    ck_assert_uint_eq(widths[2], 5);
}
END_TEST


START_TEST (uneven_reduction)
{
    unsigned int widths[4] = { 20, 8, 15, 9 };
    ck_assert_int_eq(fit_column_widths(4, widths, 1, 18), 0);
    ck_assert(widths[0] >= 4 && widths[0] <= 5);
    ck_assert(widths[1] >= 4 && widths[1] <= 5);
    ck_assert(widths[2] >= 4 && widths[2] <= 5);
    ck_assert(widths[2] >= 4 && widths[2] <= 5);
    ck_assert_uint_eq(widths[0] + widths[1] + widths[2] + widths[3], 18);
}
END_TEST


START_TEST (single_column_reduction_with_min)
{
    unsigned int widths[1] = { 40 };
    ck_assert_int_eq(fit_column_widths(1, widths, 15, 14), 0);
    ck_assert_uint_eq(widths[0], 0);
}
END_TEST


START_TEST (multi_columns_reduction_with_min)
{
    unsigned int widths[4] = { 40, 39, 3, 99 };
    ck_assert_int_eq(fit_column_widths(4, widths, 15, 31), 0);
    ck_assert_uint_eq(widths[0], 16);
    ck_assert_uint_eq(widths[1], 15);
    ck_assert_uint_eq(widths[2], 0);
    ck_assert_uint_eq(widths[3], 0);
}
END_TEST


START_TEST (single_column_expand)
{
    unsigned int widths[1] = { 20 };
    ck_assert_int_eq(fit_column_widths(1, widths, 1, 24), 0);
    ck_assert_uint_eq(widths[0], 24);
}
END_TEST


START_TEST (multi_column_expand)
{
    unsigned int widths[4] = { 20, 3, 15, 20 };
    ck_assert_int_eq(fit_column_widths(4, widths, 1, 80), 0);
    ck_assert_uint_eq(widths[0], 26);
    ck_assert_uint_eq(widths[1], 9);
    ck_assert_uint_eq(widths[2], 20);
    ck_assert_uint_eq(widths[3], 25);
}
END_TEST


TCase* fit_column_widths_tcase(void)
{
    TCase *tc = tcase_create("fit_column_widths");
    tcase_add_test(tc, single_column_reduction);
    tcase_add_test(tc, first_column_reduction);
    tcase_add_test(tc, equal_columns_reduction);
    tcase_add_test(tc, some_equal_columns_reduction);
    tcase_add_test(tc, multi_step_reduction);
    tcase_add_test(tc, uneven_reduction);
    tcase_add_test(tc, single_column_reduction_with_min);
    tcase_add_test(tc, multi_columns_reduction_with_min);
    tcase_add_test(tc, single_column_expand);
    tcase_add_test(tc, multi_column_expand);
    return tc;
}
