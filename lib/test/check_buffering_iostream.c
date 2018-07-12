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
#include "../src/buffering_iostream.h"
#include "../src/iostream.h"
#include "../src/util.h"
#include "memiostream.h"
#include <check.h>
#include <errno.h>
#include <unistd.h>


static char sample16[] = "0123456789ABCDEF";
static ring_buffer_t *rcv_rb;
static ring_buffer_t *snd_rb;
static neo4j_iostream_t *ios;


static void setup(void)
{
    rcv_rb = rb_alloc(32);
    ck_assert(rcv_rb != NULL);
    snd_rb = rb_alloc(32);
    ck_assert(snd_rb != NULL);

    neo4j_iostream_t *sink = neo4j_memiostream(rcv_rb, snd_rb);
    ios = neo4j_buffering_iostream(sink, true, 8, 8);
}


static void teardown(void)
{
    neo4j_ios_close(ios);
    rb_free(snd_rb);
    rb_free(rcv_rb);
}


START_TEST (read_fills_buffer)
{
    rb_append(rcv_rb, sample16, 16);

    char buf[32];
    ck_assert_int_eq(neo4j_ios_read(ios, buf, 8), 8);
    ck_assert(rb_is_empty(rcv_rb));
    ck_assert(memcmp(buf, "01234567", 8) == 0);
}
END_TEST


START_TEST (readv_fills_buffer)
{
    rb_append(rcv_rb, sample16, 16);

    char buf[32];
    struct iovec iov[2];
    iov[0].iov_base = buf+4;
    iov[0].iov_len = 5;
    iov[1].iov_base = buf;
    iov[1].iov_len = 4;
    ck_assert_int_eq(neo4j_ios_readv(ios, iov, 2), 9);
    ck_assert(rb_is_empty(rcv_rb));
    ck_assert(memcmp(buf, "567801234", 9) == 0);
}
END_TEST


START_TEST (read_consumes_buffer_and_reads_extra)
{
    rb_append(rcv_rb, sample16, 16);

    char buf[32];
    ck_assert_int_eq(neo4j_ios_read(ios, buf, 2), 2);
    ck_assert_int_eq(rb_used(rcv_rb), 6);

    ck_assert_int_eq(neo4j_ios_read(ios, buf, sizeof(buf)), 14);
    ck_assert(rb_is_empty(rcv_rb));
    ck_assert(memcmp(buf, "23456789ABCDEF", 14) == 0);
}
END_TEST


START_TEST (readv_consumes_buffer_and_reads_extra)
{
    rb_append(rcv_rb, sample16, 16);

    char buf[32];
    ck_assert_int_eq(neo4j_ios_read(ios, buf, 2), 2);
    ck_assert_int_eq(rb_used(rcv_rb), 6);

    struct iovec iov[3];
    iov[0].iov_base = buf+6;
    iov[0].iov_len = 8;
    iov[1].iov_base = buf+5;
    iov[1].iov_len = 1;
    iov[2].iov_base = buf;
    iov[2].iov_len = 9;

    ck_assert_int_eq(neo4j_ios_readv(ios, iov, 3), 14);
    ck_assert(rb_is_empty(rcv_rb));
    ck_assert(memcmp(buf, "BCDEFA23456789", 14) == 0);
}
END_TEST


START_TEST (read_consumes_buffer_and_refills)
{
    rb_append(rcv_rb, sample16, 16);

    char buf[32];
    ck_assert_int_eq(neo4j_ios_read(ios, buf, 2), 2);
    ck_assert_int_eq(rb_used(rcv_rb), 6);

    ck_assert_int_eq(neo4j_ios_read(ios, buf, 9), 9);
    ck_assert(rb_is_empty(rcv_rb));
    ck_assert(memcmp(buf, "23456789A", 9) == 0);

    ck_assert_int_eq(neo4j_ios_read(ios, buf, sizeof(buf)), 5);
    ck_assert(memcmp(buf, "BCDEF", 5) == 0);
}
END_TEST


START_TEST (readv_consumes_buffer_and_refills)
{
    rb_append(rcv_rb, sample16, 16);

    char buf[32];
    ck_assert_int_eq(neo4j_ios_read(ios, buf, 2), 2);
    ck_assert_int_eq(rb_used(rcv_rb), 6);

    struct iovec iov[3];
    iov[0].iov_base = buf+3;
    iov[0].iov_len = 6;
    iov[1].iov_base = buf;
    iov[1].iov_len = 3;

    ck_assert_int_eq(neo4j_ios_readv(ios, iov, 2), 9);
    ck_assert(rb_is_empty(rcv_rb));
    ck_assert(memcmp(buf, "89A234567", 9) == 0);

    ck_assert_int_eq(neo4j_ios_read(ios, buf, sizeof(buf)), 5);
    ck_assert(memcmp(buf, "BCDEF", 5) == 0);
}
END_TEST


START_TEST (small_write_goes_to_buffer)
{
    ck_assert_int_eq(neo4j_ios_write(ios, sample16, 4), 4);
    ck_assert(rb_is_empty(snd_rb));

    neo4j_ios_flush(ios);
    ck_assert_int_eq(rb_used(snd_rb), 4);

    char outbuf[32];
    ck_assert_int_eq(rb_extract(snd_rb, outbuf, sizeof(outbuf)), 4);
    ck_assert(memcmp(outbuf, "0123", 4) == 0);
}
END_TEST


START_TEST (small_writev_goes_to_buffer)
{
    struct iovec iov[2];
    iov[0].iov_base = sample16+3;
    iov[0].iov_len = 2;
    iov[1].iov_base = sample16;
    iov[1].iov_len = 3;
    ck_assert_int_eq(neo4j_ios_writev(ios, iov, 2), 5);
    ck_assert(rb_is_empty(snd_rb));

    neo4j_ios_flush(ios);
    ck_assert_int_eq(rb_used(snd_rb), 5);

    char outbuf[32];
    ck_assert_int_eq(rb_extract(snd_rb, outbuf, sizeof(outbuf)), 5);
    ck_assert(memcmp(outbuf, "34012", 5) == 0);
}
END_TEST


START_TEST (overfilling_buffer_with_write_causes_flush)
{
    ck_assert_int_eq(neo4j_ios_write(ios, sample16, 4), 4);
    ck_assert(rb_is_empty(snd_rb));

    ck_assert_int_eq(neo4j_ios_write(ios, sample16, 16), 16);
    ck_assert_int_eq(rb_used(snd_rb), 20);

    char outbuf[32];
    ck_assert_int_eq(rb_extract(snd_rb, outbuf, sizeof(outbuf)), 20);
    ck_assert(memcmp(outbuf, "01230123456789ABCDEF", 20) == 0);
}
END_TEST


START_TEST (overfilling_buffer_with_writev_causes_flush)
{
    ck_assert_int_eq(neo4j_ios_write(ios, sample16, 4), 4);
    ck_assert(rb_is_empty(snd_rb));

    struct iovec iov[2];
    iov[0].iov_base = sample16+9;
    iov[0].iov_len = 7;
    iov[1].iov_base = sample16;
    iov[1].iov_len = 9;
    ck_assert_int_eq(neo4j_ios_writev(ios, iov, 2), 16);
    ck_assert_int_eq(rb_used(snd_rb), 20);

    char outbuf[32];
    ck_assert_int_eq(rb_extract(snd_rb, outbuf, sizeof(outbuf)), 20);
    ck_assert(memcmp(outbuf, "01239ABCDEF012345678", 20) == 0);
}
END_TEST


START_TEST (large_write_skips_buffer)
{
    ck_assert_int_eq(neo4j_ios_write(ios, sample16, 16), 16);
    ck_assert_int_eq(rb_used(snd_rb), 16);

    char outbuf[32];
    ck_assert_int_eq(rb_extract(snd_rb, outbuf, sizeof(outbuf)), 16);
    ck_assert(memcmp(outbuf, "0123456789ABCDEF", 16) == 0);
}
END_TEST


START_TEST (large_writev_skips_buffer)
{
    struct iovec iov[3];
    iov[0].iov_base = sample16;
    iov[0].iov_len = 7;
    iov[1].iov_base = sample16+12;
    iov[1].iov_len = 4;
    iov[2].iov_base = sample16;
    iov[2].iov_len = 8;
    ck_assert_int_eq(neo4j_ios_writev(ios, iov, 3), 19);
    ck_assert_int_eq(rb_used(snd_rb), 19);

    char outbuf[32];
    ck_assert_int_eq(rb_extract(snd_rb, outbuf, sizeof(outbuf)), 19);
    ck_assert(memcmp(outbuf, "0123456CDEF01234567", 19) == 0);
}
END_TEST


START_TEST (unwritten_write_is_pushed_to_buffer)
{
    rb_advance(snd_rb, 24); // content is garbage

    ck_assert_int_eq(neo4j_ios_write(ios, sample16, 16), 16);
    ck_assert(rb_is_full(snd_rb));

    rb_clear(snd_rb);
    neo4j_ios_flush(ios);
    ck_assert_int_eq(rb_used(snd_rb), 8);

    char outbuf[32];
    ck_assert_int_eq(rb_extract(snd_rb, outbuf, sizeof(outbuf)), 8);
    ck_assert(memcmp(outbuf, "89ABCDEF", 8) == 0);
}
END_TEST


START_TEST (unwritten_writev_is_pushed_to_buffer)
{
    rb_advance(snd_rb, 24); // content is garbage

    struct iovec iov[2];
    iov[0].iov_base = sample16+9;
    iov[0].iov_len = 7;
    iov[1].iov_base = sample16;
    iov[1].iov_len = 9;
    ck_assert_int_eq(neo4j_ios_writev(ios, iov, 2), 16);
    ck_assert(rb_is_full(snd_rb));

    rb_clear(snd_rb);
    neo4j_ios_flush(ios);
    ck_assert_int_eq(rb_used(snd_rb), 8);

    char outbuf[32];
    ck_assert_int_eq(rb_extract(snd_rb, outbuf, sizeof(outbuf)), 8);
    ck_assert(memcmp(outbuf, "12345678", 8) == 0);
}
END_TEST


START_TEST (unwritten_write_is_pushed_to_buffer_until_full)
{
    rb_advance(snd_rb, 30); // content is garbage

    ck_assert_int_eq(neo4j_ios_write(ios, sample16, 16), 10);
    ck_assert(rb_is_full(snd_rb));

    rb_clear(snd_rb);
    neo4j_ios_flush(ios);
    ck_assert_int_eq(rb_used(snd_rb), 8);

    char outbuf[32];
    ck_assert_int_eq(rb_extract(snd_rb, outbuf, sizeof(outbuf)), 8);
    ck_assert(memcmp(outbuf, "23456789", 8) == 0);
}
END_TEST


START_TEST (unwritten_writev_is_pushed_to_buffer_until_full)
{
    rb_advance(snd_rb, 30); // content is garbage

    struct iovec iov[3];
    iov[0].iov_base = sample16;
    iov[0].iov_len = 2;
    iov[1].iov_base = sample16+12;
    iov[1].iov_len = 4;
    iov[2].iov_base = sample16;
    iov[2].iov_len = 8;
    ck_assert_int_eq(neo4j_ios_writev(ios, iov, 3), 10);
    ck_assert(rb_is_full(snd_rb));

    rb_clear(snd_rb);
    neo4j_ios_flush(ios);
    ck_assert_int_eq(rb_used(snd_rb), 8);

    char outbuf[32];
    ck_assert_int_eq(rb_extract(snd_rb, outbuf, sizeof(outbuf)), 8);
    ck_assert(memcmp(outbuf, "CDEF0123", 8) == 0);
}
END_TEST


TCase* buffering_iostream_tcase(void)
{
    TCase *tc = tcase_create("buffering_iostream");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, read_fills_buffer);
    tcase_add_test(tc, readv_fills_buffer);
    tcase_add_test(tc, read_consumes_buffer_and_reads_extra);
    tcase_add_test(tc, readv_consumes_buffer_and_reads_extra);
    tcase_add_test(tc, read_consumes_buffer_and_refills);
    tcase_add_test(tc, readv_consumes_buffer_and_refills);
    tcase_add_test(tc, small_write_goes_to_buffer);
    tcase_add_test(tc, small_writev_goes_to_buffer);
    tcase_add_test(tc, overfilling_buffer_with_write_causes_flush);
    tcase_add_test(tc, overfilling_buffer_with_writev_causes_flush);
    tcase_add_test(tc, large_write_skips_buffer);
    tcase_add_test(tc, large_writev_skips_buffer);
    tcase_add_test(tc, unwritten_write_is_pushed_to_buffer);
    tcase_add_test(tc, unwritten_writev_is_pushed_to_buffer);
    tcase_add_test(tc, unwritten_write_is_pushed_to_buffer_until_full);
    tcase_add_test(tc, unwritten_writev_is_pushed_to_buffer_until_full);
    return tc;
}
