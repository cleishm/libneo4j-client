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
#include "../src/chunking_iostream.h"
#include "../src/iostream.h"
#include "../src/util.h"
#include "memiostream.h"
#include <check.h>
#include <errno.h>
#include <unistd.h>


static ring_buffer_t *rb;
static neo4j_iostream_t *loopback_stream;
static neo4j_iostream_t *chunking_stream;


static void setup(void)
{
    rb = rb_alloc(1024);
    loopback_stream = neo4j_loopback_iostream(rb);
    chunking_stream = neo4j_chunking_iostream(loopback_stream, 8, 64);
}


static void teardown(void)
{
    neo4j_ios_close(chunking_stream);
    neo4j_ios_close(loopback_stream);
    rb_free(rb);
}


START_TEST (receive_single_chunk)
{
    uint16_t length = htons(16);
    rb_append(rb, &length, sizeof(length));
    rb_append(rb, "0123456789abcdef", 16);
    length = 0;
    rb_append(rb, &length, sizeof(length));

    char chunk[16];
    size_t n = neo4j_ios_read(chunking_stream, chunk, 16);
    ck_assert_int_eq(n, 16);
    ck_assert(memcmp(chunk, "0123456789abcdef", 16) == 0);

    n = neo4j_ios_read(chunking_stream, chunk, 16);
    ck_assert_int_eq(n, 0);
}
END_TEST


START_TEST (receive_partial_chunk)
{
    uint16_t length = htons(16);
    rb_append(rb, &length, sizeof(length));
    rb_append(rb, "0123456789abcdef", 16);
    length = 0;
    rb_append(rb, &length, sizeof(length));

    char chunk[10];
    size_t n = neo4j_ios_read(chunking_stream, chunk, 10);
    ck_assert_int_eq(n, 10);
    ck_assert(memcmp(chunk, "0123456789", 10) == 0);

    n = neo4j_ios_read(chunking_stream, chunk, 10);
    ck_assert_int_eq(n, 6);
    ck_assert(memcmp(chunk, "abcdef", 6) == 0);

    n = neo4j_ios_read(chunking_stream, chunk, 10);
    ck_assert_int_eq(n, 0);
}
END_TEST


START_TEST (receive_multiple_chunks)
{
    uint16_t length = htons(16);
    rb_append(rb, &length, sizeof(length));
    rb_append(rb, "0123456789abcdef", 16);
    rb_append(rb, &length, sizeof(length));
    rb_append(rb, "0123456789abcdef", 16);
    length = 0;
    rb_append(rb, &length, sizeof(length));

    char chunk[24];
    size_t n = neo4j_ios_read(chunking_stream, chunk, 24);
    ck_assert_int_eq(n, 24);
    ck_assert(memcmp(chunk, "0123456789abcdef01234567", 24) == 0);

    n = neo4j_ios_read(chunking_stream, chunk, 10);
    ck_assert_int_eq(n, 8);
    ck_assert(memcmp(chunk, "89abcdef", 8) == 0);

    n = neo4j_ios_read(chunking_stream, chunk, 10);
    ck_assert_int_eq(n, 0);
}
END_TEST


START_TEST (receive_multiple_chunks_in_multiple_vectors)
{
    uint16_t length = htons(6);
    rb_append(rb, &length, sizeof(length));
    rb_append(rb, "012345", 6);
    length = htons(10);
    rb_append(rb, &length, sizeof(length));
    rb_append(rb, "6789abcdef", 10);
    length = htons(7);
    rb_append(rb, &length, sizeof(length));
    rb_append(rb, "ghijklm", 7);
    length = 0;
    rb_append(rb, &length, sizeof(length));

    struct iovec iov[3];
    char iov1[16];
    iov[0].iov_base = iov1;
    iov[0].iov_len = 16;
    char iov2[4];
    iov[1].iov_base = iov2;
    iov[1].iov_len = 4;
    char iov3[18];
    iov[2].iov_base = iov3;
    iov[2].iov_len = 18;
    size_t n = neo4j_ios_readv(chunking_stream, iov, 3);
    ck_assert_int_eq(n, 23);
    ck_assert(memcmp(iov1, "0123456789abcdef", 16) == 0);
    ck_assert(memcmp(iov2, "ghij", 4) == 0);
    ck_assert(memcmp(iov3, "klm", 3) == 0);
}
END_TEST


START_TEST (receive_broken_chunk)
{
    uint16_t length = htons(16);
    rb_append(rb, &length, sizeof(length));
    rb_append(rb, "0123456789abcdef", 16);
    rb_append(rb, &length, sizeof(length));
    rb_append(rb, "0123456789", 10);

    char chunk[32];
    size_t n = neo4j_ios_read(chunking_stream, chunk, 32);
    ck_assert_int_eq(n, 26);
    ck_assert(memcmp(chunk, "0123456789abcdef0123456789", 26) == 0);

    n = neo4j_ios_read(chunking_stream, chunk, 0);
    ck_assert_int_eq(n, -1);
    ck_assert_int_eq(errno, NEO4J_CONNECTION_CLOSED);
}
END_TEST


START_TEST (receive_broken_sequence)
{
    uint16_t length = htons(16);
    rb_append(rb, &length, sizeof(length));
    rb_append(rb, "0123456789abcdef", 16);
    rb_append(rb, &length, sizeof(length));
    rb_append(rb, "0123456789abcdef", 16);

    char chunk[32];
    size_t n = neo4j_ios_read(chunking_stream, chunk, 32);
    ck_assert_int_eq(n, 32);
    ck_assert(memcmp(chunk, "0123456789abcdef0123456789abcdef", 32) == 0);

    n = neo4j_ios_read(chunking_stream, chunk, 0);
    ck_assert_int_eq(n, -1);
    ck_assert_int_eq(errno, NEO4J_CONNECTION_CLOSED);
}
END_TEST


START_TEST (write_nothing)
{
    neo4j_iostream_t *chunking_stream = neo4j_chunking_iostream(
            loopback_stream, 8, 64);
    neo4j_ios_close(chunking_stream);

    ck_assert(rb_is_empty(rb));
}
END_TEST


START_TEST (write_single_chunk)
{
    neo4j_iostream_t *chunking_stream = neo4j_chunking_iostream(
            loopback_stream, 8, 64);

    size_t n = neo4j_ios_write(chunking_stream, "0123456789abcdef", 16);
    ck_assert_int_eq(n, 16);
    ck_assert_int_eq(rb_used(rb), 18);

    neo4j_ios_close(chunking_stream);
    ck_assert_int_eq(rb_used(rb), 20);

    uint16_t length;
    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 16);

    char chunk[16];
    n = rb_extract(rb, chunk, 16);
    ck_assert_int_eq(n, 16);
    ck_assert(memcmp(chunk, "0123456789abcdef", 16) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 0);

    ck_assert(rb_is_empty(rb));
}
END_TEST


START_TEST (write_undersized_chunk_and_flush_on_next_write)
{
    neo4j_iostream_t *chunking_stream = neo4j_chunking_iostream(
            loopback_stream, 16, 64);

    size_t n = neo4j_ios_write(chunking_stream, "0123456789", 10);
    ck_assert_int_eq(n, 10);
    ck_assert_int_eq(rb_used(rb), 0);

    n = neo4j_ios_write(chunking_stream, "abcdef", 6);
    ck_assert_int_eq(n, 6);
    ck_assert_int_eq(rb_used(rb), 18);

    neo4j_ios_close(chunking_stream);
    ck_assert_int_eq(rb_used(rb), 20);

    uint16_t length;
    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 16);

    char chunk[16];
    n = rb_extract(rb, chunk, 16);
    ck_assert_int_eq(n, 16);
    ck_assert(memcmp(chunk, "0123456789abcdef", 16) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 0);

    ck_assert(rb_is_empty(rb));
}
END_TEST


START_TEST (write_undersized_chunk_and_flush_on_close)
{
    neo4j_iostream_t *chunking_stream = neo4j_chunking_iostream(
            loopback_stream, 32, 64);

    size_t n = neo4j_ios_write(chunking_stream, "0123456789", 10);
    ck_assert_int_eq(n, 10);
    ck_assert_int_eq(rb_used(rb), 0);
    n = neo4j_ios_write(chunking_stream, "abcdef", 6);
    ck_assert_int_eq(n, 6);
    ck_assert_int_eq(rb_used(rb), 0);

    neo4j_ios_close(chunking_stream);
    ck_assert_int_eq(rb_used(rb), 20);

    uint16_t length;
    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 16);

    char chunk[16];
    n = rb_extract(rb, chunk, 16);
    ck_assert_int_eq(n, 16);
    ck_assert(memcmp(chunk, "0123456789abcdef", 16) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 0);

    ck_assert(rb_is_empty(rb));
}
END_TEST


START_TEST (write_multiple_chunks)
{
    neo4j_iostream_t *chunking_stream = neo4j_chunking_iostream(
            loopback_stream, 8, 64);

    size_t n = neo4j_ios_write(chunking_stream, "0123456", 7);
    ck_assert_int_eq(n, 7);
    ck_assert_int_eq(rb_used(rb), 0);

    n = neo4j_ios_write(chunking_stream, "789", 3);
    ck_assert_int_eq(n, 3);
    ck_assert_int_eq(rb_used(rb), 12);

    n = neo4j_ios_write(chunking_stream, "abcdef", 6);
    ck_assert_int_eq(n, 6);
    ck_assert_int_eq(rb_used(rb), 12);

    neo4j_ios_close(chunking_stream);
    ck_assert_int_eq(rb_used(rb), 22);

    uint16_t length;
    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 10);

    char chunk1[10];
    n = rb_extract(rb, chunk1, 10);
    ck_assert_int_eq(n, 10);
    ck_assert(memcmp(chunk1, "0123456789", 10) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 6);

    char chunk2[6];
    n = rb_extract(rb, chunk2, 6);
    ck_assert_int_eq(n, 6);
    ck_assert(memcmp(chunk2, "abcdef", 6) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 0);

    ck_assert(rb_is_empty(rb));
}
END_TEST


START_TEST (write_oversized_chunk)
{
    neo4j_iostream_t *chunking_stream = neo4j_chunking_iostream(
            loopback_stream, 8, 8);

    size_t n = neo4j_ios_write(chunking_stream, "01234", 5);
    ck_assert_int_eq(n, 5);
    ck_assert_int_eq(rb_used(rb), 0);

    n = neo4j_ios_write(chunking_stream, "56789abcdef", 11);
    ck_assert_int_eq(n, 11);
    ck_assert_int_eq(rb_used(rb), 20);

    neo4j_ios_close(chunking_stream);
    ck_assert_int_eq(rb_used(rb), 22);

    uint16_t length;
    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 8);

    char chunk1[8];
    n = rb_extract(rb, chunk1, 8);
    ck_assert_int_eq(n, 8);
    ck_assert(memcmp(chunk1, "01234567", 8) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 8);

    char chunk2[8];
    n = rb_extract(rb, chunk2, 8);
    ck_assert_int_eq(n, 8);
    ck_assert(memcmp(chunk2, "89abcdef", 8) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 0);

    ck_assert(rb_is_empty(rb));
}
END_TEST


START_TEST (writev_single_chunk)
{
    neo4j_iostream_t *chunking_stream = neo4j_chunking_iostream(
            loopback_stream, 8, 64);

    struct iovec iov[1];
    iov[0].iov_base = "0123456789abcdef";
    iov[0].iov_len = 16;
    size_t n = neo4j_ios_writev(chunking_stream, iov, 1);
    ck_assert_int_eq(n, 16);
    ck_assert_int_eq(rb_used(rb), 18);

    neo4j_ios_close(chunking_stream);
    ck_assert_int_eq(rb_used(rb), 20);

    uint16_t length;
    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 16);

    char chunk[16];
    n = rb_extract(rb, chunk, 16);
    ck_assert_int_eq(n, 16);
    ck_assert(memcmp(chunk, "0123456789abcdef", 16) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 0);

    ck_assert(rb_is_empty(rb));
}
END_TEST


START_TEST (writev_oversized_chunk)
{
    neo4j_iostream_t *chunking_stream = neo4j_chunking_iostream(
            loopback_stream, 8, 8);

    size_t n = neo4j_ios_write(chunking_stream, "ABCDE", 5);
    ck_assert_int_eq(n, 5);
    ck_assert_int_eq(rb_used(rb), 0);

    struct iovec iov[2];
    iov[0].iov_base = "0123456";
    iov[0].iov_len = 7;
    iov[1].iov_base = "789abcdef";
    iov[1].iov_len = 9;

    n = neo4j_ios_writev(chunking_stream, iov, 2);
    ck_assert_int_eq(n, 16);
    ck_assert_int_eq(rb_used(rb), 20);

    neo4j_ios_close(chunking_stream);
    ck_assert_int_eq(rb_used(rb), 29);

    uint16_t length;
    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 8);

    char chunk1[8];
    n = rb_extract(rb, chunk1, 8);
    ck_assert_int_eq(n, 8);
    ck_assert(memcmp(chunk1, "ABCDE012", 8) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 8);

    char chunk2[8];
    n = rb_extract(rb, chunk2, 8);
    ck_assert_int_eq(n, 8);
    ck_assert(memcmp(chunk2, "3456789a", 8) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 5);

    char chunk3[5];
    n = rb_extract(rb, chunk3, 5);
    ck_assert_int_eq(n, 5);
    ck_assert(memcmp(chunk3, "bcdef", 5) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 0);

    ck_assert(rb_is_empty(rb));
}
END_TEST


START_TEST (writev_multivec_chunk)
{
    neo4j_iostream_t *chunking_stream = neo4j_chunking_iostream(
            loopback_stream, 8, 8);

    size_t n = neo4j_ios_write(chunking_stream, "ABCDE", 5);
    ck_assert_int_eq(n, 5);
    ck_assert_int_eq(rb_used(rb), 0);

    struct iovec iov[3];
    iov[0].iov_base = "0123456";
    iov[0].iov_len = 7;
    iov[1].iov_base = "789a";
    iov[1].iov_len = 4;
    iov[2].iov_base = "bcdef";
    iov[2].iov_len = 5;

    n = neo4j_ios_writev(chunking_stream, iov, 3);
    ck_assert_int_eq(n, 16);
    ck_assert_int_eq(rb_used(rb), 20);

    neo4j_ios_close(chunking_stream);
    ck_assert_int_eq(rb_used(rb), 29);

    uint16_t length;
    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 8);

    char chunk1[8];
    n = rb_extract(rb, chunk1, 8);
    ck_assert_int_eq(n, 8);
    ck_assert(memcmp(chunk1, "ABCDE012", 8) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 8);

    char chunk2[8];
    n = rb_extract(rb, chunk2, 8);
    ck_assert_int_eq(n, 8);
    ck_assert(memcmp(chunk2, "3456789a", 8) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 5);

    char chunk3[5];
    n = rb_extract(rb, chunk3, 5);
    ck_assert_int_eq(n, 5);
    ck_assert(memcmp(chunk3, "bcdef", 5) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 0);

    ck_assert(rb_is_empty(rb));
}
END_TEST


START_TEST (writev_large_multivec_chunk)
{
    neo4j_iostream_t *chunking_stream = neo4j_chunking_iostream(
            loopback_stream, 8, 8);

    struct iovec iov[4];
    iov[0].iov_base = "0123456";
    iov[0].iov_len = 7;
    iov[1].iov_base = "789abcdef";
    iov[1].iov_len = 9;
    iov[2].iov_base = "ghijklmnop";
    iov[2].iov_len = 10;
    iov[3].iov_base = "qrstuvwxyz";
    iov[3].iov_len = 10;

    size_t n = neo4j_ios_writev(chunking_stream, iov, 4);
    ck_assert_int_eq(n, 36);
    ck_assert_int_eq(rb_used(rb), 40);

    neo4j_ios_close(chunking_stream);
    ck_assert_int_eq(rb_used(rb), 48);

    uint16_t length;
    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 8);

    char chunk1[8];
    n = rb_extract(rb, chunk1, 8);
    ck_assert_int_eq(n, 8);
    ck_assert(memcmp(chunk1, "01234567", 8) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 8);

    char chunk2[8];
    n = rb_extract(rb, chunk2, 8);
    ck_assert_int_eq(n, 8);
    ck_assert(memcmp(chunk2, "89abcdef", 8) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 8);

    char chunk3[8];
    n = rb_extract(rb, chunk3, 8);
    ck_assert_int_eq(n, 8);
    ck_assert(memcmp(chunk3, "ghijklmn", 8) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 8);

    char chunk4[8];
    n = rb_extract(rb, chunk4, 8);
    ck_assert_int_eq(n, 8);
    ck_assert(memcmp(chunk4, "opqrstuv", 8) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 4);

    char chunk5[4];
    n = rb_extract(rb, chunk5, 4);
    ck_assert_int_eq(n, 4);
    ck_assert(memcmp(chunk5, "wxyz", 4) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 0);

    ck_assert(rb_is_empty(rb));
}
END_TEST


START_TEST (writev_multiple_mixed_chunks)
{
    neo4j_iostream_t *chunking_stream = neo4j_chunking_iostream(
            loopback_stream, 8, 8);

    struct iovec iov[4];
    iov[0].iov_base = "0123";
    iov[0].iov_len = 4;
    size_t n = neo4j_ios_writev(chunking_stream, iov, 1);
    ck_assert_int_eq(n, 4);
    ck_assert_int_eq(rb_used(rb), 0);

    iov[0].iov_base = "456";
    iov[0].iov_len = 3;
    n = neo4j_ios_writev(chunking_stream, iov, 1);
    ck_assert_int_eq(n, 3);
    ck_assert_int_eq(rb_used(rb), 0);

    iov[0].iov_base = "7";
    iov[0].iov_len = 1;
    n = neo4j_ios_writev(chunking_stream, iov, 1);
    ck_assert_int_eq(n, 1);
    ck_assert_int_eq(rb_used(rb), 10);

    iov[0].iov_base = "89a";
    iov[0].iov_len = 3;
    iov[1].iov_base = "bc";
    iov[1].iov_len = 2;
    n = neo4j_ios_writev(chunking_stream, iov, 2);
    ck_assert_int_eq(n, 5);
    ck_assert_int_eq(rb_used(rb), 10);

    iov[0].iov_base = "defghi";
    iov[0].iov_len = 6;
    iov[1].iov_base = "jklmn";
    iov[1].iov_len = 5;
    n = neo4j_ios_writev(chunking_stream, iov, 2);
    ck_assert_int_eq(n, 11);
    ck_assert_int_eq(rb_used(rb), 30);

    iov[0].iov_base = "opq";
    iov[0].iov_len = 3;
    iov[1].iov_base = "r";
    iov[1].iov_len = 1;
    iov[2].iov_base = "st";
    iov[2].iov_len = 2;
    n = neo4j_ios_writev(chunking_stream, iov, 3);
    ck_assert_int_eq(n, 6);
    ck_assert_int_eq(rb_used(rb), 30);

    iov[0].iov_base = "uvw";
    iov[0].iov_len = 3;
    iov[1].iov_base = "xyz";
    iov[1].iov_len = 3;
    n = neo4j_ios_writev(chunking_stream, iov, 2);
    ck_assert_int_eq(n, 6);
    ck_assert_int_eq(rb_used(rb), 40);

    neo4j_ios_close(chunking_stream);
    ck_assert_int_eq(rb_used(rb), 48);

    uint16_t length;
    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 8);

    char chunk1[8];
    n = rb_extract(rb, chunk1, 8);
    ck_assert_int_eq(n, 8);
    ck_assert(memcmp(chunk1, "01234567", 8) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 8);

    char chunk2[8];
    n = rb_extract(rb, chunk2, 8);
    ck_assert_int_eq(n, 8);
    ck_assert(memcmp(chunk2, "89abcdef", 8) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 8);

    char chunk3[8];
    n = rb_extract(rb, chunk3, 8);
    ck_assert_int_eq(n, 8);
    ck_assert(memcmp(chunk3, "ghijklmn", 8) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 8);

    char chunk4[8];
    n = rb_extract(rb, chunk4, 8);
    ck_assert_int_eq(n, 8);
    ck_assert(memcmp(chunk4, "opqrstuv", 8) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 4);

    char chunk5[4];
    n = rb_extract(rb, chunk5, 4);
    ck_assert_int_eq(n, 4);
    ck_assert(memcmp(chunk5, "wxyz", 4) == 0);

    n = rb_extract(rb, &length, sizeof(uint16_t));
    ck_assert_int_eq(n, sizeof(uint16_t));
    ck_assert_int_eq(ntohs(length), 0);

    ck_assert(rb_is_empty(rb));
}
END_TEST


TCase* chunking_iostream_tcase(void)
{
    TCase *tc = tcase_create("chunking_iostream");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, receive_single_chunk);
    tcase_add_test(tc, receive_partial_chunk);
    tcase_add_test(tc, receive_multiple_chunks);
    tcase_add_test(tc, receive_multiple_chunks_in_multiple_vectors);
    tcase_add_test(tc, receive_broken_chunk);
    tcase_add_test(tc, receive_broken_sequence);
    tcase_add_test(tc, write_nothing);
    tcase_add_test(tc, write_single_chunk);
    tcase_add_test(tc, write_undersized_chunk_and_flush_on_next_write);
    tcase_add_test(tc, write_undersized_chunk_and_flush_on_close);
    tcase_add_test(tc, write_multiple_chunks);
    tcase_add_test(tc, write_oversized_chunk);
    tcase_add_test(tc, writev_single_chunk);
    tcase_add_test(tc, writev_oversized_chunk);
    tcase_add_test(tc, writev_multivec_chunk);
    tcase_add_test(tc, writev_large_multivec_chunk);
    tcase_add_test(tc, writev_multiple_mixed_chunks);
    return tc;
}
