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
#include "../src/memory.h"
#include "../src/neo4j-client.h"
#include <check.h>


struct test_allocator
{
    neo4j_memory_allocator_t allocator;
    int allocations;
    int releases;
};


#define TEST_BUFFER_SIZE 2048
static char test_buffer[TEST_BUFFER_SIZE];
static unsigned int test_buffer_used;

static struct test_allocator allocator;
static neo4j_mpool_t pool;
static const unsigned int block_size = 128;


static inline void *test_buffer_next(void)
{
    ck_assert(test_buffer_used < TEST_BUFFER_SIZE);
    char *ptr = test_buffer + test_buffer_used;
    *ptr = 'Z';
    test_buffer_used++;
    return ptr;
}


static inline bool test_buffer_overlap(char *ptr)
{
    return (ptr >= test_buffer) && (ptr < (test_buffer + sizeof(test_buffer)));
}


static void *test_alloc(neo4j_memory_allocator_t *allocator, void *context,
        size_t size)
{
    struct test_allocator *tallocator = (struct test_allocator *)allocator;
    (tallocator->allocations)++;
    return malloc(size);
}

static void *test_calloc(neo4j_memory_allocator_t *allocator, void *context,
        size_t count, size_t size)
{
    struct test_allocator *tallocator = (struct test_allocator *)allocator;
    (tallocator->allocations)++;
    return calloc(count, size);
}

static void test_free(neo4j_memory_allocator_t *allocator, void *ptr)
{
    struct test_allocator *tallocator = (struct test_allocator *)allocator;
    (tallocator->releases)++;
    if (test_buffer_overlap(ptr))
    {
        ck_assert(*((char *)ptr) == 'Z');
        *((char *)ptr) = '\0';
    }
    else
    {
        free(ptr);
    }
}

static void test_vfree(neo4j_memory_allocator_t *allocator, void **ptrs,
        size_t n)
{
    for (; n > 0; --n, ++ptrs)
    {
        void *ptr = *ptrs;
        test_free(allocator, ptr);
    }
}


static void setup(void)
{
    memset(test_buffer, 0, sizeof(test_buffer));
    test_buffer_used = 0;

    neo4j_memory_allocator_t tallocator =
        { .alloc = test_alloc, .calloc = test_calloc,
          .free = test_free, .vfree = test_vfree };
    allocator.allocator = tallocator;
    allocator.allocations = 0;
    allocator.releases = 0;

    pool = neo4j_mpool((neo4j_memory_allocator_t *)&allocator, block_size);
}


static void teardown(void)
{
    neo4j_mpool_drain(&pool);

    for (unsigned int i = 0; i < TEST_BUFFER_SIZE; ++i)
    {
        ck_assert(test_buffer[i] == '\0');
    }
}


START_TEST (fill_debounce_and_drain)
{
    for (int i = NEO4J_MPOOL_DEBOUNCE; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool, test_buffer_next()), 0);
    }

    ck_assert_ptr_eq(pool.ptrs, NULL);
    ck_assert_int_eq(pool.debounce_offset, NEO4J_MPOOL_DEBOUNCE);
    ck_assert_int_eq(pool.offset, block_size);

    neo4j_mpool_drain(&pool);
    ck_assert_int_eq(allocator.allocations, 0);
    ck_assert_int_eq(allocator.releases, NEO4J_MPOOL_DEBOUNCE);
    ck_assert_int_eq(neo4j_mpool_depth(pool), 0);
    ck_assert_ptr_eq(pool.ptrs, NULL);
    ck_assert_int_eq(pool.debounce_offset, 0);
    ck_assert_int_eq(pool.offset, block_size);
}
END_TEST


START_TEST (fill_debounce_and_part_drain)
{
    for (int i = NEO4J_MPOOL_DEBOUNCE; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool, test_buffer_next()), 0);
    }

    ck_assert_ptr_eq(pool.ptrs, NULL);
    ck_assert_int_eq(pool.debounce_offset, NEO4J_MPOOL_DEBOUNCE);
    ck_assert_int_eq(pool.offset, block_size);

    ck_assert(NEO4J_MPOOL_DEBOUNCE > 4);
    neo4j_mpool_drainto(&pool, 4);
    ck_assert_int_eq(allocator.allocations, 0);
    ck_assert_int_eq(allocator.releases, NEO4J_MPOOL_DEBOUNCE - 4);
    ck_assert_int_eq(neo4j_mpool_depth(pool), 4);
    ck_assert_ptr_eq(pool.ptrs, NULL);
    ck_assert_int_eq(pool.debounce_offset, 4);
    ck_assert_int_eq(pool.offset, block_size);
}
END_TEST


START_TEST (fill_1block_and_drain)
{
    for (int i = 100; i > 0; --i)
    {
        void *p = test_buffer_next();
        ck_assert_int_gt(neo4j_mpool_add(&pool, p), 0);
    }

    ck_assert_ptr_ne(pool.ptrs, NULL);
    ck_assert_ptr_eq(*(pool.ptrs), NULL);
    ck_assert_int_eq(pool.debounce_offset, 0);
    ck_assert_int_eq(pool.offset, 101);

    neo4j_mpool_drain(&pool);
    ck_assert_int_eq(allocator.allocations, 1);
    ck_assert_int_eq(allocator.releases, 101);
    ck_assert_int_eq(neo4j_mpool_depth(pool), 0);
    ck_assert_ptr_eq(pool.ptrs, NULL);
    ck_assert_int_eq(pool.debounce_offset, 0);
    ck_assert_int_eq(pool.offset, block_size);
}
END_TEST


START_TEST (fill_1block_and_part_drain)
{
    for (int i = 100; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool, test_buffer_next()), 0);
    }

    ck_assert_ptr_ne(pool.ptrs, NULL);
    ck_assert_ptr_eq(*(pool.ptrs), NULL);
    ck_assert_int_eq(pool.debounce_offset, 0);
    ck_assert_int_eq(pool.offset, 101);

    neo4j_mpool_drainto(&pool, 40);
    ck_assert_int_eq(allocator.allocations, 1);
    ck_assert_int_eq(allocator.releases, 60);
    ck_assert_int_eq(neo4j_mpool_depth(pool), 40);
    ck_assert_ptr_ne(pool.ptrs, NULL);
    ck_assert_ptr_eq(*(pool.ptrs), NULL);
    ck_assert_int_eq(pool.debounce_offset, 0);
    ck_assert_int_eq(pool.offset, 41);
}
END_TEST


START_TEST (fill_1block_plus_debounce_and_part_drain)
{
    for (int i = 132; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool, test_buffer_next()), 0);
    }

    ck_assert_ptr_ne(pool.ptrs, NULL);
    ck_assert_ptr_eq(*(pool.ptrs), NULL);
    ck_assert_int_eq(pool.debounce_offset, 5);
    ck_assert_int_eq(pool.offset, block_size);

    neo4j_mpool_drainto(&pool, 128);
    ck_assert_int_eq(allocator.allocations, 1);
    ck_assert_int_eq(allocator.releases, 4);
    ck_assert_int_eq(neo4j_mpool_depth(pool), 128);
    ck_assert_ptr_ne(pool.ptrs, NULL);
    ck_assert_ptr_eq(*(pool.ptrs), NULL);
    ck_assert_int_eq(pool.debounce_offset, 1);
    ck_assert_int_eq(pool.offset, 128);

    neo4j_mpool_drainto(&pool, 127);
    ck_assert_int_eq(allocator.allocations, 1);
    ck_assert_int_eq(allocator.releases, 5);
    ck_assert_int_eq(neo4j_mpool_depth(pool), 127);
    ck_assert_ptr_ne(pool.ptrs, NULL);
    ck_assert_ptr_eq(*(pool.ptrs), NULL);
    ck_assert_int_eq(pool.debounce_offset, 0);
    ck_assert_int_eq(pool.offset, 128);

    neo4j_mpool_drainto(&pool, 126);
    ck_assert_int_eq(allocator.allocations, 1);
    ck_assert_int_eq(allocator.releases, 6);
    ck_assert_int_eq(neo4j_mpool_depth(pool), 126);
    ck_assert_ptr_ne(pool.ptrs, NULL);
    ck_assert_ptr_eq(*(pool.ptrs), NULL);
    ck_assert_int_eq(pool.debounce_offset, 0);
    ck_assert_int_eq(pool.offset, 127);
}
END_TEST


START_TEST (fill_and_drain)
{
    int additions = sizeof(test_buffer) - 1;
    ck_assert_int_gt((additions % (block_size - 1)), NEO4J_MPOOL_DEBOUNCE);

    for (int i = additions; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool, test_buffer_next()), 0);
    }
    int expected_blocks = (additions / (block_size - 1)) + 1;

    neo4j_mpool_drain(&pool);
    ck_assert_int_eq(allocator.allocations, expected_blocks);
    ck_assert_int_eq(allocator.releases, additions + expected_blocks);

    ck_assert_int_eq(neo4j_mpool_depth(pool), 0);
    ck_assert_ptr_eq(pool.ptrs, NULL);
    ck_assert_int_eq(pool.debounce_offset, 0);
    ck_assert_int_eq(pool.offset, block_size);
}
END_TEST


START_TEST (fill_and_partially_drain)
{
    for (int i = 100; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool, test_buffer_next()), 0);
    }

    ck_assert_int_eq(neo4j_mpool_depth(pool), 100);
    neo4j_mpool_drainto(&pool, 50);

    ck_assert_int_eq(allocator.allocations, 1);
    ck_assert_int_eq(allocator.releases, 50);
    ck_assert_int_eq(neo4j_mpool_depth(pool), 50);
    allocator.allocations = 0;
    allocator.releases = 0;

    for (int i = 500; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool, test_buffer_next()), 0);
    }

    neo4j_mpool_drainto(&pool, 30);

    ck_assert_int_eq(allocator.allocations, 4);
    ck_assert_int_eq(allocator.releases, 524);
    allocator.allocations = 0;
    allocator.releases = 0;

    for (int i = 140; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool, test_buffer_next()), 0);
    }

    neo4j_mpool_drainto(&pool, 0);
    ck_assert_int_eq(allocator.allocations, 1);
    ck_assert_int_eq(allocator.releases, 172);
}
END_TEST


START_TEST (merge_with_empty_pool)
{
    ck_assert(pool.offset == pool.block_size);
    ck_assert(pool.debounce_offset == 0);
    ck_assert(neo4j_mpool_depth(pool) == 0);

    neo4j_mpool_t pool2 = neo4j_mpool(pool.allocator, pool.block_size);
    for (int i = 100; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool2, test_buffer_next()), 0);
    }

    ssize_t new_depth = neo4j_mpool_merge(&pool, &pool2);
    ck_assert(new_depth > 0);
    ck_assert((size_t)new_depth == 100);
    ck_assert(neo4j_mpool_depth(pool) == 100);
    ck_assert(neo4j_mpool_depth(pool2) == 0);
}
END_TEST


START_TEST (merge_with_full_pool)
{
    for (int i = 3*(pool.block_size-1); i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool, test_buffer_next()), 0);
    }
    ck_assert(pool.offset == pool.block_size);
    ck_assert(pool.debounce_offset == 0);
    size_t pdepth = neo4j_mpool_depth(pool);

    neo4j_mpool_t pool2 = neo4j_mpool(pool.allocator, pool.block_size);
    for (int i = 100; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool2, test_buffer_next()), 0);
    }

    ssize_t new_depth = neo4j_mpool_merge(&pool, &pool2);
    ck_assert(new_depth > 0);
    ck_assert((size_t)new_depth == pdepth+100);
    ck_assert(neo4j_mpool_depth(pool) == pdepth+100);
    ck_assert(neo4j_mpool_depth(pool2) == 0);
}
END_TEST


START_TEST (merge_with_underfull_pool)
{
    for (int i = 3*(pool.block_size-1) + (pool.block_size/2); i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool, test_buffer_next()), 0);
    }
    ck_assert(pool.offset < pool.block_size);
    ck_assert(pool.debounce_offset == 0);
    size_t pdepth = neo4j_mpool_depth(pool);

    neo4j_mpool_t pool2 = neo4j_mpool(pool.allocator, pool.block_size);
    unsigned int extra = 2*(pool.block_size-1) + 2*(pool.block_size/3);
    for (int i = extra; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool2, test_buffer_next()), 0);
    }

    ssize_t new_depth = neo4j_mpool_merge(&pool, &pool2);
    ck_assert(new_depth > 0);
    ck_assert((size_t)new_depth == pdepth+extra);
    ck_assert(neo4j_mpool_depth(pool) == pdepth+extra);
    ck_assert(neo4j_mpool_depth(pool2) == 0);
}
END_TEST


START_TEST (merge_with_underfull_below_offset_pool)
{
    for (int i = 3*(pool.block_size-1) + (pool.block_size/2); i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool, test_buffer_next()), 0);
    }
    ck_assert(pool.offset < pool.block_size);
    ck_assert(pool.debounce_offset == 0);
    size_t pdepth = neo4j_mpool_depth(pool);

    neo4j_mpool_t pool2 = neo4j_mpool(pool.allocator, pool.block_size);
    unsigned int extra = 2*(pool.block_size-1) + (pool.block_size/3);
    for (int i = extra; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool2, test_buffer_next()), 0);
    }

    ssize_t new_depth = neo4j_mpool_merge(&pool, &pool2);
    ck_assert(new_depth > 0);
    ck_assert((size_t)new_depth == pdepth+extra);
    ck_assert(neo4j_mpool_depth(pool) == pdepth+extra);
    ck_assert(neo4j_mpool_depth(pool2) == 0);
}
END_TEST


START_TEST (merge_overfull_with_underfull_pool)
{
    for (int i = 3*(pool.block_size-1) + (pool.block_size/2); i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool, test_buffer_next()), 0);
    }
    ck_assert(pool.offset < pool.block_size);
    ck_assert(pool.debounce_offset == 0);
    size_t pdepth = neo4j_mpool_depth(pool);

    neo4j_mpool_t pool2 = neo4j_mpool(pool.allocator, pool.block_size);
    unsigned int extra = 2*(pool.block_size-1) + (NEO4J_MPOOL_DEBOUNCE/2);
    for (int i = extra; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool2, test_buffer_next()), 0);
    }

    ssize_t new_depth = neo4j_mpool_merge(&pool, &pool2);
    ck_assert(new_depth > 0);
    ck_assert((size_t)new_depth == pdepth+extra);
    ck_assert(neo4j_mpool_depth(pool) == pdepth+extra);
    ck_assert(neo4j_mpool_depth(pool2) == 0);
}
END_TEST


START_TEST (merge_overfull_with_underfull_below_debounce_pool)
{
    for (int i = 4*(pool.block_size-1) - (NEO4J_MPOOL_DEBOUNCE/3); i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool, test_buffer_next()), 0);
    }
    ck_assert(pool.offset < pool.block_size);
    ck_assert(pool.debounce_offset == 0);
    size_t pdepth = neo4j_mpool_depth(pool);

    neo4j_mpool_t pool2 = neo4j_mpool(pool.allocator, pool.block_size);
    unsigned int extra = 2*(pool.block_size-1) + (NEO4J_MPOOL_DEBOUNCE/2);
    for (int i = extra; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool2, test_buffer_next()), 0);
    }

    ssize_t new_depth = neo4j_mpool_merge(&pool, &pool2);
    ck_assert(new_depth > 0);
    ck_assert((size_t)new_depth == pdepth+extra);
    ck_assert(neo4j_mpool_depth(pool) == pdepth+extra);
    ck_assert(neo4j_mpool_depth(pool2) == 0);
}
END_TEST


START_TEST (merge_with_overfull_pool)
{
    for (int i = 3*(pool.block_size-1) + (NEO4J_MPOOL_DEBOUNCE/2); i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool, test_buffer_next()), 0);
    }
    ck_assert(pool.offset == pool.block_size);
    ck_assert(pool.debounce_offset > 0);
    size_t pdepth = neo4j_mpool_depth(pool);

    neo4j_mpool_t pool2 = neo4j_mpool(pool.allocator, pool.block_size);
    unsigned int extra = 2*(pool.block_size-1) + 2*(pool.block_size/3);
    for (int i = extra; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool2, test_buffer_next()), 0);
    }

    ssize_t new_depth = neo4j_mpool_merge(&pool, &pool2);
    ck_assert(new_depth > 0);
    ck_assert((size_t)new_depth == pdepth+extra);
    ck_assert(neo4j_mpool_depth(pool) == pdepth+extra);
    ck_assert(neo4j_mpool_depth(pool2) == 0);
}
END_TEST


START_TEST (merge_overfull_with_overfull_pool)
{
    for (int i = 3*(pool.block_size-1) + (NEO4J_MPOOL_DEBOUNCE/2); i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool, test_buffer_next()), 0);
    }
    ck_assert(pool.offset == pool.block_size);
    ck_assert(pool.debounce_offset > 0);
    size_t pdepth = neo4j_mpool_depth(pool);

    neo4j_mpool_t pool2 = neo4j_mpool(pool.allocator, pool.block_size);
    unsigned int extra = 2*(pool.block_size-1) + 2*(NEO4J_MPOOL_DEBOUNCE/3);
    for (int i = extra; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool2, test_buffer_next()), 0);
    }

    ssize_t new_depth = neo4j_mpool_merge(&pool, &pool2);
    ck_assert(new_depth > 0);
    ck_assert((size_t)new_depth == pdepth+extra);
    ck_assert(neo4j_mpool_depth(pool) == pdepth+extra);
    ck_assert(neo4j_mpool_depth(pool2) == 0);
}
END_TEST


START_TEST (merge_with_empty_pool_of_smaller_blocksize)
{
    ck_assert(pool.offset == pool.block_size);
    ck_assert(pool.debounce_offset == 0);
    ck_assert(neo4j_mpool_depth(pool) == 0);

    neo4j_mpool_t pool2 = neo4j_mpool(pool.allocator, 2*(pool.block_size/3));
    for (int i = 300; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool2, test_buffer_next()), 0);
    }

    ssize_t new_depth = neo4j_mpool_merge(&pool, &pool2);
    ck_assert(new_depth > 0);
    ck_assert((size_t)new_depth == 300);
    ck_assert(neo4j_mpool_depth(pool) == 300);
    ck_assert(neo4j_mpool_depth(pool2) == 0);
}
END_TEST


START_TEST (merge_with_pool_of_smaller_blocksize)
{
    for (int i = 3*(pool.block_size-1) + (pool.block_size/2); i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool, test_buffer_next()), 0);
    }
    ck_assert(pool.offset < pool.block_size);
    ck_assert(pool.debounce_offset == 0);
    size_t pdepth = neo4j_mpool_depth(pool);

    neo4j_mpool_t pool2 = neo4j_mpool(pool.allocator, 2*(pool.block_size/3));
    unsigned int extra = 2*(pool.block_size-1) + 2*(pool.block_size/3);
    for (int i = extra; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool2, test_buffer_next()), 0);
    }

    ssize_t new_depth = neo4j_mpool_merge(&pool, &pool2);
    ck_assert(new_depth > 0);
    ck_assert((size_t)new_depth == pdepth+extra);
    ck_assert(neo4j_mpool_depth(pool) == pdepth+extra);
    ck_assert(neo4j_mpool_depth(pool2) == 0);
}
END_TEST


START_TEST (merge_with_empty_pool_of_larger_blocksize)
{
    ck_assert(pool.offset == pool.block_size);
    ck_assert(pool.debounce_offset == 0);
    ck_assert(neo4j_mpool_depth(pool) == 0);

    neo4j_mpool_t pool2 = neo4j_mpool(pool.allocator, 3*(pool.block_size/2));
    for (int i = 100; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool2, test_buffer_next()), 0);
    }

    ssize_t new_depth = neo4j_mpool_merge(&pool, &pool2);
    ck_assert(new_depth > 0);
    ck_assert((size_t)new_depth == 100);
    ck_assert(neo4j_mpool_depth(pool) == 100);
    ck_assert(neo4j_mpool_depth(pool2) == 0);
}
END_TEST


START_TEST (merge_with_pool_of_larger_blocksize)
{
    for (int i = 3*(pool.block_size-1) + (pool.block_size/2); i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool, test_buffer_next()), 0);
    }
    ck_assert(pool.offset < pool.block_size);
    ck_assert(pool.debounce_offset == 0);
    size_t pdepth = neo4j_mpool_depth(pool);

    neo4j_mpool_t pool2 = neo4j_mpool(pool.allocator, 3*(pool.block_size/2));
    unsigned int extra = 2*(pool.block_size-1) + 2*(pool.block_size/3);
    for (int i = extra; i > 0; --i)
    {
        ck_assert_int_gt(neo4j_mpool_add(&pool2, test_buffer_next()), 0);
    }

    ssize_t new_depth = neo4j_mpool_merge(&pool, &pool2);
    ck_assert(new_depth > 0);
    ck_assert((size_t)new_depth == pdepth+extra);
    ck_assert(neo4j_mpool_depth(pool) == pdepth+extra);
    ck_assert(neo4j_mpool_depth(pool2) == 0);
}
END_TEST


TCase* memory_tcase(void)
{
    TCase *tc = tcase_create("memory");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, fill_debounce_and_drain);
    tcase_add_test(tc, fill_debounce_and_part_drain);
    tcase_add_test(tc, fill_1block_and_drain);
    tcase_add_test(tc, fill_1block_and_part_drain);
    tcase_add_test(tc, fill_1block_plus_debounce_and_part_drain);
    tcase_add_test(tc, fill_and_drain);
    tcase_add_test(tc, fill_and_partially_drain);
    tcase_add_test(tc, merge_with_empty_pool);
    tcase_add_test(tc, merge_with_full_pool);
    tcase_add_test(tc, merge_with_underfull_pool);
    tcase_add_test(tc, merge_with_underfull_below_offset_pool);
    tcase_add_test(tc, merge_overfull_with_underfull_pool);
    tcase_add_test(tc, merge_overfull_with_underfull_below_debounce_pool);
    tcase_add_test(tc, merge_with_overfull_pool);
    tcase_add_test(tc, merge_overfull_with_overfull_pool);
    tcase_add_test(tc, merge_with_empty_pool_of_smaller_blocksize);
    tcase_add_test(tc, merge_with_pool_of_smaller_blocksize);
    tcase_add_test(tc, merge_with_empty_pool_of_larger_blocksize);
    tcase_add_test(tc, merge_with_pool_of_larger_blocksize);
    return tc;
}
