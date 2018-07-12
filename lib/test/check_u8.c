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


// Examples of decoder tests from https://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt


START_TEST (test_u8clen_ascii)
{
    ck_assert_int_eq(neo4j_u8clen("a", SIZE_MAX), 1);
    ck_assert_int_eq(neo4j_u8clen("z", SIZE_MAX), 1);
    ck_assert_int_eq(neo4j_u8clen("multiple chars", SIZE_MAX), 1);
}
END_TEST


START_TEST (test_u8clen_boundaries)
{
    ck_assert_int_eq(neo4j_u8clen("", SIZE_MAX), 0);

    ck_assert_int_eq(neo4j_u8clen("\x01", SIZE_MAX), 1);
    ck_assert_int_eq(neo4j_u8clen("\x7F", SIZE_MAX), 1);

    ck_assert_int_eq(neo4j_u8clen("\xC2\x80", SIZE_MAX), 2);
    ck_assert_int_eq(neo4j_u8clen(u8"\U000007FF", SIZE_MAX), 2);

    ck_assert_int_eq(neo4j_u8clen(u8"\U00000800", SIZE_MAX), 3);
    ck_assert_int_eq(neo4j_u8clen(u8"\U0000D7FF", SIZE_MAX), 3);
    ck_assert_int_eq(neo4j_u8clen(u8"\U0000E000", SIZE_MAX), 3);
    ck_assert_int_eq(neo4j_u8clen(u8"\U0000FFFD", SIZE_MAX), 3);
    ck_assert_int_eq(neo4j_u8clen(u8"\U0000FFFF", SIZE_MAX), 3);

    ck_assert_int_eq(neo4j_u8clen(u8"\U00010000", SIZE_MAX), 4);
    ck_assert_int_eq(neo4j_u8clen(u8"\U0010FFFF", SIZE_MAX), 4);
}
END_TEST


START_TEST (test_u8clen_unexpected_continuation)
{
    for (unsigned char c = 0x80; c <= 0xBF; ++c)
    {
        ck_assert_int_eq(neo4j_u8clen((char *)&c, SIZE_MAX), -1);
    }
}
END_TEST


START_TEST (test_u8clen_lonely_start)
{
    char s[2] = "  ";
    for (unsigned char c = 0xC0; c <= 0xDF; ++c)
    {
        s[0] = c;
        ck_assert_int_eq(neo4j_u8clen(s, SIZE_MAX), -1);
    }

    for (unsigned char c = 0xE0; c <= 0xEF; ++c)
    {
        s[0] = c;
        ck_assert_int_eq(neo4j_u8clen(s, SIZE_MAX), -1);
    }

    for (unsigned char c = 0xF0; c <= 0xF7; ++c)
    {
        s[0] = c;
        ck_assert_int_eq(neo4j_u8clen(s, SIZE_MAX), -1);
    }
}
END_TEST


START_TEST (test_u8clen_missing_last)
{
    ck_assert_int_eq(neo4j_u8clen("\xC2\x80", 1), -1);
    ck_assert_int_eq(neo4j_u8clen(u8"\U000007FF", 1), -1);

    ck_assert_int_eq(neo4j_u8clen(u8"\U00000800", 2), -1);
    ck_assert_int_eq(neo4j_u8clen(u8"\U0000FFFF", 2), -1);

    ck_assert_int_eq(neo4j_u8clen(u8"\U00010000", 3), -1);
    ck_assert_int_eq(neo4j_u8clen(u8"\U0010FFFF", 3), -1);
}
END_TEST


START_TEST (test_u8clen_impossible_bytes)
{
    ck_assert_int_eq(neo4j_u8clen("\xFF", SIZE_MAX), -1);
    ck_assert_int_eq(neo4j_u8clen("\xFF", SIZE_MAX), -1);
}
END_TEST


START_TEST (test_u8clen_overlong_sequence)
{
    ck_assert_int_eq(neo4j_u8clen("\xC0\x80", SIZE_MAX), -1);
    ck_assert_int_eq(neo4j_u8clen("\xC0\xAF", SIZE_MAX), -1);
    ck_assert_int_eq(neo4j_u8clen("\xC1\xBF", SIZE_MAX), -1);

    ck_assert_int_eq(neo4j_u8clen("\xE0\x80\x80", SIZE_MAX), -1);
    ck_assert_int_eq(neo4j_u8clen("\xE0\x80\xAF", SIZE_MAX), -1);
    ck_assert_int_eq(neo4j_u8clen("\xE0\x9F\xBF", SIZE_MAX), -1);

    ck_assert_int_eq(neo4j_u8clen("\xF0\x80\x80\x80", SIZE_MAX), -1);
    ck_assert_int_eq(neo4j_u8clen("\xF0\x80\x80\xAF", SIZE_MAX), -1);
    ck_assert_int_eq(neo4j_u8clen("\xF0\x8F\xBF\xBF", SIZE_MAX), -1);
}
END_TEST


START_TEST (test_u8clen_utf16_surrogates)
{
    ck_assert_int_eq(neo4j_u8clen("\xED\xA0\x80", SIZE_MAX), -1);
    ck_assert_int_eq(neo4j_u8clen("\xED\xAD\xBF", SIZE_MAX), -1);
    ck_assert_int_eq(neo4j_u8clen("\xED\xAE\x80", SIZE_MAX), -1);
    ck_assert_int_eq(neo4j_u8clen("\xED\xAF\xBF", SIZE_MAX), -1);
    ck_assert_int_eq(neo4j_u8clen("\xED\xB0\x80", SIZE_MAX), -1);
    ck_assert_int_eq(neo4j_u8clen("\xED\xBE\x80", SIZE_MAX), -1);
    ck_assert_int_eq(neo4j_u8clen("\xED\xBF\xBF", SIZE_MAX), -1);
}
END_TEST


START_TEST (test_u8cwidth_ascii)
{
    ck_assert_int_eq(neo4j_u8cwidth("a", SIZE_MAX), 1);
    ck_assert_int_eq(neo4j_u8cwidth("z", SIZE_MAX), 1);
    ck_assert_int_eq(neo4j_u8cwidth("multiple chars", SIZE_MAX), 1);
}
END_TEST


START_TEST (test_u8cwidth_8_bit_control)
{
    ck_assert_int_eq(neo4j_u8cwidth("\a", SIZE_MAX), -1);
    ck_assert_int_eq(neo4j_u8cpwidth(0x07), -1);
    ck_assert_int_eq(neo4j_u8cwidth("\n", SIZE_MAX), -1);
    ck_assert_int_eq(neo4j_u8cwidth("\x1b", SIZE_MAX), -1);
    ck_assert_int_eq(neo4j_u8cpwidth(127), -1);
    ck_assert_int_eq(neo4j_u8cpwidth(159), -1);
}
END_TEST


START_TEST (test_u8cswidth)
{
    ck_assert_int_eq(neo4j_u8cswidth("abcde", SIZE_MAX), 5);
    ck_assert_int_eq(neo4j_u8cswidth("abc\nde", SIZE_MAX), -1);
    ck_assert_int_eq(neo4j_u8cswidth(u8"a\u0102cd", SIZE_MAX), 4);
    ck_assert_int_eq(neo4j_u8cswidth(u8"a\uACFFb", SIZE_MAX), 4);
}
END_TEST


TCase* u8_tcase(void)
{
    TCase *tc = tcase_create("u8");
    tcase_add_test(tc, test_u8clen_ascii);
    tcase_add_test(tc, test_u8clen_boundaries);
    tcase_add_test(tc, test_u8clen_unexpected_continuation);
    tcase_add_test(tc, test_u8clen_lonely_start);
    tcase_add_test(tc, test_u8clen_missing_last);
    tcase_add_test(tc, test_u8clen_impossible_bytes);
    tcase_add_test(tc, test_u8clen_overlong_sequence);
    tcase_add_test(tc, test_u8clen_utf16_surrogates);
    tcase_add_test(tc, test_u8cwidth_ascii);
    tcase_add_test(tc, test_u8cwidth_8_bit_control);
    tcase_add_test(tc, test_u8cswidth);
    return tc;
}
