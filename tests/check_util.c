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
#include "../config.h"
#include "../src/lib/neo4j-client.h"
#include <check.h>
#include <errno.h>
#include <limits.h>


START_TEST (test_neo4j_dirname)
{
    char buf[PATH_MAX];
    ck_assert_int_eq(neo4j_dirname("/foo/bar.baz/", NULL, 0), 4);
    ck_assert_int_eq(neo4j_dirname("/foo/bar.baz/", buf, sizeof(buf)), 4);
    ck_assert_str_eq(buf, "/foo");

    ck_assert_int_eq(neo4j_dirname("foo/bar.baz/", NULL, 0), 3);
    ck_assert_int_eq(neo4j_dirname("foo/bar.baz/", buf, sizeof(buf)), 3);
    ck_assert_str_eq(buf, "foo");

    ck_assert_int_eq(neo4j_dirname("foo", NULL, 0), 1);
    ck_assert_int_eq(neo4j_dirname("foo", buf, sizeof(buf)), 1);
    ck_assert_str_eq(buf, ".");

    ck_assert_int_eq(neo4j_dirname("////", NULL, 0), 1);
    ck_assert_int_eq(neo4j_dirname("////", buf, sizeof(buf)), 1);
    ck_assert_str_eq(buf, "/");

    ck_assert_int_eq(neo4j_dirname("", NULL, 0), 1);
    ck_assert_int_eq(neo4j_dirname("", buf, sizeof(buf)), 1);
    ck_assert_str_eq(buf, ".");

    ck_assert_int_eq(neo4j_dirname(NULL, NULL, 0), 1);
    ck_assert_int_eq(neo4j_dirname(NULL, buf, sizeof(buf)), 1);
    ck_assert_str_eq(buf, ".");
}
END_TEST


START_TEST (test_neo4j_basename)
{
    char buf[PATH_MAX];
    ck_assert_int_eq(neo4j_basename("/foo/bar.baz/", NULL, 0), 7);
    ck_assert_int_eq(neo4j_basename("/foo/bar.baz/", buf, sizeof(buf)), 7);
    ck_assert_str_eq(buf, "bar.baz");

    ck_assert_int_eq(neo4j_basename("bar.baz/", NULL, 0), 7);
    ck_assert_int_eq(neo4j_basename("bar.baz/", buf, sizeof(buf)), 7);
    ck_assert_str_eq(buf, "bar.baz");

    ck_assert_int_eq(neo4j_basename("bar.baz", NULL, 0), 7);
    ck_assert_int_eq(neo4j_basename("bar.baz", buf, sizeof(buf)), 7);
    ck_assert_str_eq(buf, "bar.baz");

    ck_assert_int_eq(neo4j_basename("////", NULL, 0), 1);
    ck_assert_int_eq(neo4j_basename("////", buf, sizeof(buf)), 1);
    ck_assert_str_eq(buf, "/");

    ck_assert_int_eq(neo4j_basename("", NULL, 0), 1);
    ck_assert_int_eq(neo4j_basename("", buf, sizeof(buf)), 1);
    ck_assert_str_eq(buf, ".");

    ck_assert_int_eq(neo4j_basename(NULL, NULL, 0), 1);
    ck_assert_int_eq(neo4j_basename(NULL, buf, sizeof(buf)), 1);
    ck_assert_str_eq(buf, ".");
}
END_TEST


TCase* util_tcase(void)
{
    TCase *tc = tcase_create("util");
    tcase_add_test(tc, test_neo4j_dirname);
    tcase_add_test(tc, test_neo4j_basename);
    return tc;
}
