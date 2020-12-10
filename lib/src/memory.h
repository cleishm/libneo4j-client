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
#ifndef NEO4J_MEMORY_H
#define NEO4J_MEMORY_H

#include "neo4j-client.h"
#include <errno.h>


typedef struct neo4j_memory_allocator neo4j_memory_allocator_t;


/**
 * Allocate memory using the specified allocator.
 *
 * @internal
 *
 * @param [allocator] The allocator to use.
 * @param [context] An opaque 'context' for the allocation, which an
 *         allocator may use to try an optimize storage as memory allocated
 *         with the same context is likely (but not guaranteed) to be all
 *         deallocated at the same time. Context may be `NULL`, in which
 *         case it does not offer any guidance on deallocation.
 * @param [size] The number of bytes to allocate.
 * @return A pointer the newly allocated memory, or `NULL` if an error occurs
 *         (errno will be set).
 */
__neo4j_malloc
static inline void *neo4j_alloc(neo4j_memory_allocator_t *allocator,
        void *context, size_t size)
{
    return allocator->alloc(allocator, context, size);
}


/**
 * Allocate memory for consecutive objects using the specified allocator.
 *
 * The memory will be filled with bytes of value zero.
 *
 * @internal
 *
 * @param [allocator] The allocator to use.
 * @param [context] An opaque 'context' for the allocation, which an
 *         allocator may use to try an optimize storage as memory allocated
 *         with the same context is likely (but not guaranteed) to be all
 *         deallocated at the same time. Context may be `NULL`, in which
 *         case it does not offer any guidance on deallocation.
 * @param [count] The number of objects to allocate contiguous space for.
 * @param [size] The size of each object.
 * @return A pointer the newly allocated memory, or `NULL` if an error occurs
 *         (errno will be set).
 */
__neo4j_malloc
static inline void *neo4j_calloc(neo4j_memory_allocator_t *allocator,
        void *context, size_t count, size_t size)
{
    return allocator->calloc(allocator, context, count, size);
}


/**
 * Return memory to the specified allocator.
 *
 * @internal
 *
 * @param [allocator] The allocator to use.
 * @param [ptr] A pointer to the memory being returned.
 */
static inline void neo4j_free(neo4j_memory_allocator_t *allocator, void *ptr)
{
    allocator->free(allocator, ptr);
}


/**
 * Return multiple memory regions to the specified allocator.
 *
 * @internal
 *
 * @param [allocator] The allocator to use.
 * @param [ptrs] An array of pointers to memory for returning.
 * @param [n] The length of the pointer array.
 */
static inline void neo4j_vfree(neo4j_memory_allocator_t *allocator, void **ptrs,
        size_t n)
{
    allocator->vfree(allocator, ptrs, n);
}


#define NEO4J_MPOOL_DEBOUNCE 8

typedef struct neo4j_mpool
{
    neo4j_memory_allocator_t *allocator;
    unsigned int block_size;
    void *debounce_ptrs[NEO4J_MPOOL_DEBOUNCE];
    unsigned int debounce_offset;
    void **ptrs;
    unsigned int offset;
    size_t depth;
} neo4j_mpool_t;


/**
 * Initialize a new memory pool.
 *
 * @internal
 *
 * @param [allocator] The allocator to use with the pool.
 * @param [block_size] The number of memory pointers to hold in each block.
 *         The pool will allocate a new block when the previous has been filled.
 * @return A memory pool.
 */
neo4j_mpool_t neo4j_mpool(neo4j_memory_allocator_t *allocator,
        unsigned int block_size);

/**
 * Add memory to a memory pool.
 *
 * The memory added to the pool will be automatically deallocated when the pool
 * is drained to the current depth or lower.
 *
 * @internal
 *
 * @param [pool] A pointer to the pool.
 * @param [ptr] The pointer to be added.
 * @return The new pool depth on success, or -1 on failure (errno will be set).
 */
__neo4j_must_check
ssize_t neo4j_mpool_add(neo4j_mpool_t *pool, void *ptr);

/**
 * Drain a memory pool to the specified depth.
 *
 * If the pool is already below the specified depth, no change to the pool is
 * made.
 *
 * @internal
 *
 * @param [pool] A pointer to the pool to drain.
 * @param [depth] The depth to drain to.
 */
void neo4j_mpool_drainto(neo4j_mpool_t *pool, size_t depth);

/**
 * Completely drain a memory pool.
 *
 * This deallocates all memory added to the pool.
 *
 * @internal
 *
 * @param [pool] A pointer to the pool to drain.
 */
static inline void neo4j_mpool_drain(neo4j_mpool_t *pool)
{
    neo4j_mpool_drainto(pool, 0);
}

/**
 * @fn size_t neo4j_mpool_depth(const neo4j_mpool_t pool)
 * @brief Get the depth of a memory pool.
 *
 * @internal
 *
 * @param [pool] The pool to check the depth of.
 * @return The depth of the pool.
 */
#define neo4j_mpool_depth(pool) (_neo4j_mpool_depth(&(pool)))
static inline size_t _neo4j_mpool_depth(const neo4j_mpool_t *pool)
{
    return pool->depth;
}

/**
 * Allocate memory and add it to a memory pool.
 *
 * Memory will be allocated using the allocator the pool was created with.
 *
 * @internal
 *
 * @param [pool] The pool to allocate memory using.
 * @return A pointer the newly allocated memory, or `NULL` if an error occurs
 *         (errno will be set).
 */
__neo4j_malloc
static inline void *neo4j_mpool_alloc(neo4j_mpool_t *pool, size_t size)
{
    void *ptr = neo4j_alloc(pool->allocator, pool, size);
    if (neo4j_mpool_add(pool, ptr) < 0)
    {
        int errsv = errno;
        neo4j_free(pool->allocator, ptr);
        errno = errsv;
        return NULL;
    }
    return ptr;
}

/**
 * Allocate memory for consecutive objects and add to a memory pool.
 *
 * Memory will be allocated using the allocator the pool was created with,
 * and will be filled with bytes of value zero.
 *
 * @internal
 *
 * @param [pool] The pool to allocate memory using.
 * @param [count] The number of objects to allocate contiguous space for.
 * @param [size] The size of each object.
 * @return A pointer the newly allocated memory, or `NULL` if an error occurs
 *         (errno will be set).
 */
__neo4j_malloc
static inline void *neo4j_mpool_calloc(neo4j_mpool_t *pool,
        size_t count, size_t size)
{
    void *ptr = neo4j_calloc(pool->allocator, pool, count, size);
    if (neo4j_mpool_add(pool, ptr) < 0)
    {
        int errsv = errno;
        neo4j_free(pool->allocator, ptr);
        errno = errsv;
        return NULL;
    }
    return ptr;
}

/**
 * Combine two memory pools.
 *
 * The memory in the second pool is added on top of the memory in the
 * first pool, increasing the depth of the first pool and emptying the second.
 *
 * @internal
 *
 * @param [pool1] A pointer to the first pool.
 * @param [pool2] A pointer to the second pool.
 * @return The new pool1 depth on success, or -1 on failure (errno will be set).
 */
__neo4j_must_check
ssize_t neo4j_mpool_merge(neo4j_mpool_t *pool1, neo4j_mpool_t *pool2);


#endif/*NEO4J_MEMORY_H*/
