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
#include "../src/util.h"
#include <check.h>
#include <errno.h>
#include <limits.h>


START_TEST (test_neo4j_dirname)
{
    char buf[256];
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
    char buf[256];
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


START_TEST (test_strcasecmp_indep)
{
    ck_assert_int_eq(strcasecmp_indep(
                "Fear and loathing",
                "Fear and loathing"), 0);
    ck_assert_int_eq(strcasecmp_indep(
                "the rum diary",
                "THE rum DIARY"), 0);
    ck_assert_int_lt(strcasecmp_indep(
                "She rum diary",
                "the rum DIARY"), 0);
    ck_assert_int_gt(strcasecmp_indep(
                "the sum diary",
                "THe rum DIARY"), 0);
    ck_assert_int_lt(strcasecmp_indep(
                "Fear and loathing",
                "Fear and loathing2"), 0);
    ck_assert_int_gt(strcasecmp_indep(
                "Fear and loathing ",
                "Fear and loathing"), 0);
}
END_TEST


START_TEST (test_strncasecmp_indep)
{
    ck_assert_int_eq(strncasecmp_indep(
                "Fear and loathing",
                "Fear and loathing", 99), 0);
    ck_assert_int_eq(strncasecmp_indep(
                "the rum diary",
                "THE rum DIARY", 99), 0);
    ck_assert_int_lt(strncasecmp_indep(
                "She rum diary",
                "the rum DIARY", 99), 0);
    ck_assert_int_gt(strncasecmp_indep(
                "the sum diary",
                "THe rum DIARY", 99), 0);
    ck_assert_int_lt(strncasecmp_indep(
                "Fear and loathing",
                "Fear and loathing2", 99), 0);
    ck_assert_int_gt(strncasecmp_indep(
                "Fear and loathing ",
                "Fear and loathing", 99), 0);
    ck_assert_int_eq(strncasecmp_indep(
                "Fear and loathing",
                "The RUM diary", 0), 0);
    ck_assert_int_eq(strncasecmp_indep(
                "Fear and loathing",
                "Fear and loathing on the campaign trail", 17), 0);
    ck_assert_int_eq(strncasecmp_indep(
                "Fear and loathing in las vegas",
                "Fear and loathing on the campaign trail", 5), 0);
}
END_TEST


START_TEST (test_hostname_matching)
{
    ck_assert(hostname_matches("neo4j.com", "neo4j.com"));
    ck_assert(hostname_matches("test.neo4j.com", "*.neo4j.com"));
    ck_assert(hostname_matches("test.neo4j.com", "*st.neo4j.com"));
    ck_assert(hostname_matches("test.neo4j.com", "te*.neo4j.com"));
    ck_assert(hostname_matches("test.neo4j.com", "t*t.neo4j.com"));
    ck_assert(!hostname_matches("neo4j.com", "google.com"));
    ck_assert(!hostname_matches("test.neo4j.com", "*.google.com"));
    ck_assert(!hostname_matches("neo4j.com", "neo4j.net"));
    ck_assert(!hostname_matches("status.neo4j.com", "*st.neo4j.com"));
    ck_assert(!hostname_matches("status.neo4j.com", "te*.neo4j.com"));
    ck_assert(!hostname_matches("test.neo4j.com", "tes*t.neo4j.com"));
}
END_TEST


TCase* util_tcase(void)
{
    TCase *tc = tcase_create("util");
    tcase_add_test(tc, test_neo4j_dirname);
    tcase_add_test(tc, test_neo4j_basename);
    tcase_add_test(tc, test_strcasecmp_indep);
    tcase_add_test(tc, test_strncasecmp_indep);
    tcase_add_test(tc, test_hostname_matching);
    return tc;
}
