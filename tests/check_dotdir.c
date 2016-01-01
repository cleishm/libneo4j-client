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
#include <stdlib.h>


static const char *oldhome = NULL;


static void setup(void)
{
    oldhome = getenv("HOME");
}


static void teardown(void)
{
    if (oldhome != NULL)
    {
        setenv("HOME", oldhome, 1);
    }
    else
    {
        unsetenv("HOME");
    }
}


START_TEST (test_neo4j_dot_dir_returns_default_dir)
{
    setenv("HOME", "/path/to/home", 1);
    char buf[PATH_MAX];
    ck_assert_int_eq(neo4j_dot_dir(buf, sizeof(buf), NULL), 20);
    ck_assert_str_eq(buf, "/path/to/home/.neo4j");

    ck_assert_int_eq(neo4j_dot_dir(NULL, 0, NULL), 20);
}
END_TEST


START_TEST (test_neo4j_dot_dir_appends_dir)
{
    setenv("HOME", "/path/to/home", 1);
    char buf[PATH_MAX];
    ck_assert_int_eq(neo4j_dot_dir(buf, sizeof(buf), "foo.bar"), 28);
    ck_assert_str_eq(buf, "/path/to/home/.neo4j/foo.bar");

    ck_assert_int_eq(neo4j_dot_dir(NULL, 0, "foo.bar"), 28);
}
END_TEST


TCase* dotdir_tcase(void)
{
    TCase *tc = tcase_create("dotdir");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_neo4j_dot_dir_returns_default_dir);
    tcase_add_test(tc, test_neo4j_dot_dir_appends_dir);
    return tc;
}
