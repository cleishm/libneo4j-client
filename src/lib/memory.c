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
#include "memory.h"
#include "util.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>


static void *system_alloc(neo4j_memory_allocator_t *allocator, void *context,
        size_t size);
static void *system_calloc(neo4j_memory_allocator_t *allocator, void *context,
                size_t count, size_t size);
static void system_free(neo4j_memory_allocator_t *allocator, void *ptr);
static void system_vfree(neo4j_memory_allocator_t *allocator, void **ptrs,
        size_t n);

struct neo4j_memory_allocator neo4j_std_memory_allocator =
{
    .alloc = system_alloc,
    .calloc = system_calloc,
    .free = system_free,
    .vfree = system_vfree
};


void *system_alloc(neo4j_memory_allocator_t *allocator, void *context,
        size_t size)
{
    assert(allocator != NULL);
    return malloc(size);
}


void *system_calloc(neo4j_memory_allocator_t *allocator, void *context,
        size_t count, size_t size)
{
    assert(allocator != NULL);
    return calloc(count, size);
}


void system_free(neo4j_memory_allocator_t *allocator, void *ptr)
{
    assert(allocator != NULL);
    free(ptr);
}


void system_vfree(neo4j_memory_allocator_t *allocator, void **ptrs, size_t n)
{
    assert(allocator != NULL);
    for (; n > 0; --n, ++ptrs)
    {
        free(*ptrs);
    }
}


static int remove_debounce(neo4j_mpool_t *pool);
static int resize_pool(neo4j_mpool_t *npool, neo4j_mpool_t *pool,
        unsigned int block_size);
static int merge_pools(neo4j_mpool_t *pool1, neo4j_mpool_t *pool2);
static int concat_pools(neo4j_mpool_t *pool1, neo4j_mpool_t *pool2);


neo4j_mpool_t neo4j_mpool(neo4j_memory_allocator_t *allocator,
        unsigned int block_size)
{
    assert(allocator != NULL);
    neo4j_mpool_t pool;

    memset(&pool, 0, sizeof(neo4j_mpool_t));
    pool.allocator = allocator;
    pool.block_size = maxu(block_size, NEO4J_MPOOL_DEBOUNCE+2);
    pool.offset = pool.block_size;
    return pool;
}


ssize_t neo4j_mpool_add(neo4j_mpool_t *pool, void *ptr)
{
    assert(pool != NULL);
    assert(ptr != NULL);

    if (pool->offset >= pool->block_size)
    {
        assert(pool->offset == pool->block_size);

        // overflow into debounce_ptrs
        if (pool->debounce_offset < NEO4J_MPOOL_DEBOUNCE)
        {
            pool->debounce_ptrs[(pool->debounce_offset)++] = ptr;
            return ++(pool->depth);
        }

        if (remove_debounce(pool))
        {
            return -1;
        }
    }

    pool->ptrs[(pool->offset)++] = ptr;
    return ++(pool->depth);
}


int remove_debounce(neo4j_mpool_t *pool)
{
    void **block = neo4j_alloc(pool->allocator, pool,
            pool->block_size * sizeof(void *));
    if (block == NULL)
    {
        return -1;
    }

    *block = pool->ptrs;
    memcpy(block+1, pool->debounce_ptrs,
            pool->debounce_offset * sizeof(void *));
    pool->ptrs = block;
    pool->offset = pool->debounce_offset+1;
    pool->debounce_offset = 0;
    return 0;
}


void neo4j_mpool_drainto(neo4j_mpool_t *pool, size_t depth)
{
    if (pool->depth <= depth)
    {
        return;
    }

    size_t todrain = pool->depth - depth;

    if (pool->debounce_offset > 0)
    {
        // drain debounce region
        unsigned int debounce_drain = min(pool->debounce_offset, todrain);
        void **ptrs =
            pool->debounce_ptrs + (pool->debounce_offset - debounce_drain);
        neo4j_vfree(pool->allocator, ptrs, debounce_drain);
        pool->debounce_offset -= debounce_drain;
        todrain -= debounce_drain;
    }

    while (pool->ptrs != NULL && todrain >= (pool->offset-1))
    {
        // drain entire block
        void **block = pool->ptrs;
        pool->ptrs = *block;

        neo4j_vfree(pool->allocator, block+1, pool->offset-1);
        neo4j_free(pool->allocator, block);
        todrain -= (pool->offset-1);
        pool->offset = pool->block_size;
    }

    if (todrain > 0)
    {
        // drain part of block
        assert(todrain < (pool->offset-1));
        pool->offset -= todrain;
        neo4j_vfree(pool->allocator, pool->ptrs + pool->offset, todrain);
    }
    pool->depth = depth;
}


ssize_t neo4j_mpool_merge(neo4j_mpool_t *pool1, neo4j_mpool_t *pool2)
{
    assert(pool1 != NULL);
    assert(pool2 != NULL);

    if (pool2->depth == 0)
    {
        return pool1->depth;
    }

    if (pool1->debounce_offset > 0 && remove_debounce(pool1))
    {
        return -1;
    }

    neo4j_mpool_t tpool;
    if (pool1->block_size != pool2->block_size ||
            pool1->allocator != pool2->allocator)
    {
        if (resize_pool(&tpool, pool2, pool1->block_size))
        {
            return -1;
        }
        pool2 = &tpool;
    }

    if (pool1->offset == pool1->block_size)
    {
        // shortcut
        return concat_pools(pool1, pool2);
    }
    return merge_pools(pool1, pool2);
}


int resize_pool(neo4j_mpool_t *npool, neo4j_mpool_t *pool,
        unsigned int block_size)
{
    assert(pool->depth > 0);

    if (pool->debounce_offset > 0 && remove_debounce(pool))
    {
        return -1;
    }

    neo4j_mpool_t tpool = neo4j_mpool(pool->allocator, block_size);

    tpool.ptrs = neo4j_alloc(tpool.allocator, npool,
            block_size * sizeof(void *));
    if (tpool.ptrs == NULL)
    {
        return -1;
    }
    *(tpool.ptrs) = NULL;

    // first block
    void **nblock = tpool.ptrs;
    unsigned int space = (pool->depth % (block_size - 1));
    tpool.offset = space + 1;

    void **block = pool->ptrs;
    unsigned int available = pool->offset - 1;

    // loop through remaining
    do
    {
        unsigned int tocopy = minu(space, available);

        memcpy(nblock+1+space-tocopy, block+1+available-tocopy,
                tocopy * sizeof(void *));

        space -= tocopy;
        available -= tocopy;

        if (available == 0)
        {
            block = *block;
            available = pool->block_size - 1;
        }
        else if (space == 0)
        {
            *nblock = neo4j_alloc(tpool.allocator,
                    npool, block_size * sizeof(void *));
            if (*nblock == NULL)
            {
                goto cleanup;
            }
            nblock = *nblock;
            *nblock = NULL;
            space = block_size - 1;
        }
    } while (block != NULL);
    assert(space == 0);

    tpool.depth = pool->depth;

    // deallocate original blocks
    do
    {
        void **block = pool->ptrs;
        pool->ptrs = *block;
        neo4j_free(pool->allocator, block);
    } while (pool->ptrs != NULL);
    pool->depth = 0;

    memcpy(npool, &tpool, sizeof(tpool));
    return 0;

    int errsv;
cleanup:
    errsv = errno;
    do
    {
        void **block = tpool.ptrs;
        tpool.ptrs = *block;
        neo4j_free(tpool.allocator, block);
    }
    while (tpool.ptrs != NULL);
    errno = errsv;
    return -1;
}


int merge_pools(neo4j_mpool_t *pool1, neo4j_mpool_t *pool2)
{
    assert(pool1->allocator == pool2->allocator);
    assert(pool1->debounce_offset == 0);

    unsigned int space = pool1->block_size - pool1->offset;
    unsigned int used = pool1->offset - 1;

    void **block = pool2->ptrs;
    if (block != NULL)
    {
        // reverse pool2 blocks
        void **prev = NULL;
        while (*block != NULL)
        {
            void **next = *block;
            *block = prev;
            prev = block;
            block = next;
        }
        *block = prev;

        // shift blocks into pool1
        prev = pool1->ptrs;
        while (*block != NULL)
        {
            memcpy(prev+1+used, block+1, space * sizeof(void *));
            memmove(block+1, block+1+space,
                    (pool2->block_size-1-space) * sizeof(void *));
            void **next = *block;
            *block = prev;
            prev = block;
            block = next;
        }

        if (space < pool2->offset)
        {
            memcpy(prev+1+used, block+1, space * sizeof(void *));
            memmove(block+1, block+1+space,
                    (pool2->offset-1-space) * sizeof(void *));
            *block = prev;
            pool1->ptrs = block;
            pool1->offset = pool2->offset - space;
        }
        else
        {
            memcpy(prev+1+used, block+1, pool2->offset * sizeof(void *));
            neo4j_free(pool1->allocator, block);
            pool1->ptrs = prev;
            pool1->offset += pool2->offset - 1;
        }
    }

    if (pool2->debounce_offset > 0)
    {
        unsigned int tocopy = minu(pool1->block_size - pool1->offset,
                pool2->debounce_offset);
        memcpy(pool1->ptrs + pool1->offset, pool2->debounce_ptrs,
                tocopy * sizeof(void *));
        pool1->offset += tocopy;
        pool1->debounce_offset = pool2->debounce_offset - tocopy;
        memcpy(pool1->debounce_ptrs, pool2->debounce_ptrs + tocopy,
                pool1->debounce_offset * sizeof(void *));
    }

    pool1->depth += pool2->depth;

    pool2->debounce_offset = 0;
    pool2->ptrs = NULL;
    pool2->offset = pool2->block_size;
    pool2->depth = 0;

    return pool1->depth;
}


int concat_pools(neo4j_mpool_t *pool1, neo4j_mpool_t *pool2)
{
    assert(pool1->allocator == pool2->allocator);
    assert(pool1->debounce_offset == 0);
    assert(pool1->offset == pool1->block_size);
    assert(pool2->depth > 0);
    assert(pool1->block_size == pool2->block_size);

    void **last_block = pool2->ptrs;
    for (; *last_block != NULL; last_block = *last_block)
        ;
    *last_block = pool1->ptrs;
    pool1->ptrs = pool2->ptrs;
    pool1->offset = pool2->offset;

    if (pool2->debounce_offset > 0)
    {
        memcpy(pool1->debounce_ptrs, pool2->debounce_ptrs,
                pool2->debounce_offset * sizeof(void *));
        pool1->debounce_offset = pool2->debounce_offset;
    }

    pool1->depth += pool2->depth;

    pool2->debounce_offset = 0;
    pool2->ptrs = NULL;
    pool2->offset = pool2->block_size;
    pool2->depth = 0;
    return pool1->depth;
}
