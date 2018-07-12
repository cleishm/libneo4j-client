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
#include "../src/ring_buffer.h"
#include <check.h>
#include <errno.h>
#include <unistd.h>


static char sample16[] = "0123456789ABCDEF";
static ring_buffer_t *rb;


static void setup(void)
{
    ck_assert_int_gt(sizeof(sample16), 16);
    rb = rb_alloc(16);
}


static void teardown(void)
{
    rb_free(rb);
}


START_TEST (test_to_rb_from_memory)
{
    size_t result = rb_append(rb, sample16, 10);
    ck_assert_int_eq(result, 10);

    ck_assert_int_eq(rb_used(rb), 10);
    ck_assert_int_eq(rb_space(rb), 6);
    ck_assert(!rb_is_empty(rb));
    ck_assert(!rb_is_full(rb));

    result = rb_append(rb, sample16, 10);
    ck_assert_int_eq(result, 6);

    ck_assert_int_eq(rb_used(rb), 16);
    ck_assert_int_eq(rb_space(rb), 0);
    ck_assert(!rb_is_empty(rb));
    ck_assert(rb_is_full(rb));

    ck_assert(memcmp(rb->buffer, "0123456789012345", 16) == 0);
}
END_TEST


START_TEST (test_to_rb_from_memory_wrapped_around)
{
    ck_assert_int_eq(rb_append(rb, sample16, 8), 8);
    ck_assert_int_eq(rb_discard(rb, 7), 7);
    ck_assert_int_eq(rb_used(rb), 1);

    size_t result = rb_append(rb, sample16, 16);
    ck_assert_int_eq(result, 15);
    ck_assert(!rb_is_empty(rb));
    ck_assert(rb_is_full(rb));

    ck_assert(memcmp(rb->buffer, "89ABCDE701234567", 16) == 0);
}
END_TEST


START_TEST (test_to_rb_from_scattered_memory)
{
    ck_assert_int_eq(rb_append(rb, sample16, 8), 8);
    ck_assert_int_eq(rb_discard(rb, 7), 7);
    ck_assert_int_eq(rb_used(rb), 1);

    struct iovec iov[3];
    iov[0].iov_base = sample16 + 4;
    iov[0].iov_len = 5;
    iov[1].iov_base = sample16;
    iov[1].iov_len = 4;
    iov[2].iov_base = sample16 + 9;
    iov[2].iov_len = 7;

    size_t result = rb_appendv(rb, iov, 3);
    ck_assert_int_eq(result, 15);
    ck_assert(!rb_is_empty(rb));
    ck_assert(rb_is_full(rb));

    ck_assert(memcmp(rb->buffer, "39ABCDE745678012", 16) == 0);
}
END_TEST


START_TEST (test_to_rb_from_memory_in_center)
{
    ck_assert_int_eq(rb_append(rb, sample16, 8), 8);
    ck_assert_int_eq(rb_discard(rb, 7), 7);
    ck_assert_int_eq(rb_append(rb, sample16, 11), 11);
    ck_assert_int_eq(rb_used(rb), 12);
    ck_assert_int_eq(rb_space(rb), 4);

    size_t result = rb_append(rb, sample16, 16);
    ck_assert_int_eq(result, 4);
    ck_assert(!rb_is_empty(rb));
    ck_assert(rb_is_full(rb));

    ck_assert(memcmp(rb->buffer, "89A0123701234567", 16) == 0);
}
END_TEST


START_TEST (test_to_rb_from_fd)
{
    int fds[2];
    ck_assert(pipe(fds) == 0);

    ck_assert(write(fds[1], sample16, 16) == 16);

    ck_assert(rb_is_empty(rb));
    ck_assert_int_eq(rb_used(rb), 0);
    ck_assert_int_eq(rb_space(rb), 16);

    ssize_t result = rb_read(rb, fds[0], 10);
    ck_assert_int_eq(result, 10);

    ck_assert_int_eq(rb_used(rb), 10);
    ck_assert_int_eq(rb_space(rb), 6);
    ck_assert(!rb_is_empty(rb));
    ck_assert(!rb_is_full(rb));

    result = rb_read(rb, fds[0], 10);
    ck_assert_int_eq(result, 6);

    ck_assert_int_eq(rb_used(rb), 16);
    ck_assert_int_eq(rb_space(rb), 0);
    ck_assert(!rb_is_empty(rb));
    ck_assert(rb_is_full(rb));

    ck_assert(memcmp(rb->buffer, "0123456789ABCDEF", 16) == 0);
}
END_TEST


START_TEST (test_return_enobufs_if_full)
{
    rb_append(rb, sample16, 16);

    ck_assert(rb_read(rb, 0, 1) < 0);
    ck_assert_int_eq(errno, ENOBUFS);
}
END_TEST


START_TEST (test_to_memory_from_rb)
{
    rb_append(rb, sample16, 16);

    char outbuf[32];
    size_t result = rb_extract(rb, outbuf, 10);
    ck_assert_int_eq(result, 10);

    ck_assert_int_eq(rb_space(rb), 10);
    ck_assert(!rb_is_empty(rb));
    ck_assert(!rb_is_full(rb));

    result = rb_extract(rb, outbuf + 10, 10);
    ck_assert_int_eq(result, 6);
    ck_assert(rb_is_empty(rb));
    ck_assert(!rb_is_full(rb));

    ck_assert(memcmp(outbuf, "0123456789ABCDEF", 16) == 0);
}
END_TEST


START_TEST (test_to_memory_from_rb_wrapped_around)
{
    rb_append(rb, sample16, 16);
    rb_discard(rb, 10);
    rb_append(rb, sample16, 6);
    ck_assert_int_eq(rb_space(rb), 4);

    char outbuf[32];
    size_t result = rb_extract(rb, outbuf, 16);
    ck_assert_int_eq(result, 12);

    ck_assert(rb_is_empty(rb));
    ck_assert(!rb_is_full(rb));

    ck_assert(memcmp(outbuf, "ABCDEF012345", 12) == 0);
}
END_TEST


START_TEST (test_to_scattered_memory_from_rb)
{
    rb_append(rb, sample16, 4);
    rb_append(rb, sample16, 8);
    rb_discard(rb, 4);
    rb_append(rb, sample16+8, 8);

    char outbuf[32];
    struct iovec iov[3];
    iov[0].iov_base = outbuf+12;
    iov[0].iov_len = 2;
    iov[1].iov_base = outbuf+7;
    iov[1].iov_len = 5;
    iov[2].iov_base = outbuf;
    iov[2].iov_len = 7;

    size_t result = rb_extractv(rb, iov, 3);
    ck_assert_int_eq(result, 14);

    ck_assert_int_eq(rb_space(rb), 14);
    ck_assert(!rb_is_empty(rb));
    ck_assert(!rb_is_full(rb));

    ck_assert(memcmp(outbuf, "789ABCD2345601", 14) == 0);
}
END_TEST


START_TEST (test_to_fd_from_rb)
{
    int fds[2];
    ck_assert(pipe(fds) == 0);

    rb_append(rb, sample16, 16);
    ck_assert(rb_is_full(rb));

    ssize_t result = rb_write(rb, fds[1], 10);
    ck_assert_int_eq(result, 10);

    ck_assert_int_eq(rb_used(rb), 6);
    ck_assert_int_eq(rb_space(rb), 10);

    char outbuf[32];
    ck_assert_int_eq(read(fds[0], outbuf, sizeof(outbuf)), 10);
    ck_assert(memcmp(outbuf, "0123456789", 10) == 0);

    result = rb_write(rb, fds[1], 10);
    ck_assert_int_eq(result, 6);
    ck_assert(rb_is_empty(rb));

    ck_assert_int_eq(read(fds[0], outbuf, sizeof(outbuf)), 6);
    ck_assert(memcmp(outbuf, "ABCDEF", 6) == 0);
}
END_TEST


START_TEST (test_advance)
{
    rb_append(rb, sample16, 12);
    rb_discard(rb, 4);

    struct iovec iov[2];
    ck_assert_int_eq(rb_space_iovec(rb, iov, 16), 2);
    ck_assert_int_eq(iov[0].iov_len, 4);
    ck_assert_int_eq(iov[1].iov_len, 4);

    memcpy(iov[0].iov_base, sample16, 4);
    memcpy(iov[1].iov_base, sample16, 2);
    rb_advance(rb, 6);

    char outbuf[32];
    ck_assert_int_eq(rb_extract(rb, outbuf, sizeof(outbuf)), 14);
    ck_assert(memcmp(outbuf, "456789AB012301", 14) == 0);
}
END_TEST


START_TEST (test_discard)
{
    rb_append(rb, sample16, 16);

    size_t result = rb_discard(rb, 8);
    ck_assert_int_eq(result, 8);

    char outbuf[32];
    ck_assert_int_eq(rb_extract(rb, outbuf, sizeof(outbuf)), 8);
    ck_assert(memcmp(outbuf, "89ABCDEF", 8) == 0);
}
END_TEST


START_TEST (test_clear)
{
    rb_append(rb, sample16, 16);
    ck_assert(rb_is_full(rb));

    rb_clear(rb);
    ck_assert(rb_is_empty(rb));
}
END_TEST


TCase* ring_buffer_tcase(void)
{
    TCase *tc = tcase_create("ring_buffer");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_to_rb_from_memory);
    tcase_add_test(tc, test_to_rb_from_memory_wrapped_around);
    tcase_add_test(tc, test_to_rb_from_memory_in_center);
    tcase_add_test(tc, test_to_rb_from_scattered_memory);
    tcase_add_test(tc, test_to_rb_from_fd);
    tcase_add_test(tc, test_return_enobufs_if_full);
    tcase_add_test(tc, test_to_memory_from_rb);
    tcase_add_test(tc, test_to_memory_from_rb_wrapped_around);
    tcase_add_test(tc, test_to_scattered_memory_from_rb);
    tcase_add_test(tc, test_to_fd_from_rb);
    tcase_add_test(tc, test_advance);
    tcase_add_test(tc, test_discard);
    tcase_add_test(tc, test_clear);
    return tc;
}
