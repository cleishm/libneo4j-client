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
#include <check.h>
#include <string.h>
#include <errno.h>


static const char *std_strerror_r(int errnum, char *buf, size_t buflen)
{
#ifdef STRERROR_R_CHAR_P
    return strerror_r(errnum, buf, buflen);
#else
    return (strerror_r(errnum, buf, buflen)) ? NULL : buf;
#endif
}



START_TEST (test_strerror_delegates_for_standard_errnums)
{
    char buf1[1024];
    char buf2[1024];

    const char *neo4j_err = neo4j_strerror(EINVAL, buf1, sizeof(buf1));
    ck_assert(neo4j_err != NULL);

    const char *std_err = std_strerror_r(EINVAL, buf2, sizeof(buf2));
    ck_assert_str_eq(neo4j_err, std_err);

    std_err = std_strerror_r(EPERM, buf2, sizeof(buf2));
    ck_assert_str_ne(neo4j_err, std_err);
}
END_TEST


START_TEST (test_strerror_invalid_arguments)
{
    ck_assert(neo4j_strerror(-1, NULL, 10) == NULL);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST


TCase* error_handling_tcase(void)
{
    TCase *tc = tcase_create("error handling");
    tcase_add_test(tc, test_strerror_delegates_for_standard_errnums);
    tcase_add_test(tc, test_strerror_invalid_arguments);
    return tc;
}
