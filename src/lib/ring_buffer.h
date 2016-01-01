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
#ifndef NEO4J_RING_BUFFER_H
#define NEO4J_RING_BUFFER_H

#include <stdint.h>
#include <sys/uio.h>
#include <unistd.h>

typedef struct ring_buffer
{
    uint8_t *buffer;
    size_t size;
    uint8_t *ptr;
    size_t used;
} ring_buffer_t;

/**
 * Allocate a ring buffer.
 *
 * @param [size] The size of the buffer (in bytes).
 * @return A newly allocated buffer.
 */
ring_buffer_t *rb_alloc(size_t size);

/**
 * Deallocate a ring buffer.
 *
 * @param [rb] The buffer to deallocate.
 */
void rb_free(ring_buffer_t *rb);

/**
 * @fn size_t rb_size(const ring_buffer_t* rb)
 * @brief Get the size of a ring buffer.
 * @param [rb] The ring buffer.
 * @return The size of the buffer.
 */
#define rb_size(RB) ((RB)->size)

/**
 * @fn size_t rb_used(const ring_buffer_t* rb)
 * @brief Get the number of bytes in a ring buffer.
 * @param [rb] The ring buffer.
 * @return The number of bytes in the buffer.
 */
#define rb_used(RB) ((RB)->used)

/**
 * @fn size_t rb_space(const ring_buffer_t* rb)
 * @brief Get the number of bytes free in a ring buffer.
 * @param [rb] The ring buffer.
 * @return The number of bytes free in the buffer.
 */
#define rb_space(RB) ((RB)->size - (RB)->used)

/**
 * @fn bool rb_is_empty(const ring_buffer_t* rb)
 * @brief Check if a ring buffer is empty.
 * @param [rb] The ring buffer.
 * @return `true` if the buffer is empty, and `false` otherwise.
 */
#define rb_is_empty(RB) (rb_used(RB) == 0)

/**
 * @fn bool rb_is_full(const ring_buffer_t* rb)
 * @brief Check if a ring buffer is full.
 * @param [rb] The ring buffer.
 * @return `true` if the buffer is full, and `false` otherwise.
 */
#define rb_is_full(RB) (rb_space(RB) == 0)

/**
 * Append data to a ring buffer.
 *
 * @param [rb] The ring buffer.
 * @param [src] A pointer to memory for appending into the buffer.
 * @param [nbytes] The number of bytes to be appended.
 * @return The number of bytes appended, or -1 on error (errno will be set).
 */
ssize_t rb_append(ring_buffer_t *rb, const void *src, size_t nbytes);

/**
 * Read bytes from a file descriptor and append to a buffer.
 *
 * @param [rb] The ring buffer.
 * @param [fd] The file descriptor to read from.
 * @param [nbytes] The number of bytes to be read and appended.
 * @return The number of bytes appended, or -1 on error (errno will be set).
 */
ssize_t rb_read(ring_buffer_t * rb, int fd, size_t nbytes);

/**
 * Extract data from a ring buffer.
 *
 * @param [rb] The ring buffer.
 * @param [dst] A pointer to a buffer extracted bytes will be copied to.
 * @param [nbytes] The number of bytes to be extracted.
 * @return The number of bytes extracted, or -1 on error (errno will be set).
 */
ssize_t rb_extract(ring_buffer_t *rb, void *dst, size_t nbytes);

/**
 * Extract data from a ring buffer and write to a file descriptor.
 *
 * @param [rb] The ring buffer.
 * @param [fd] The file descriptor to write to.
 * @param [nbytes] The number of bytes to be extracted and written.
 * @return The number of bytes written, or -1 on error (errno will be set).
 */
ssize_t rb_write(ring_buffer_t *rb, int fd, size_t nbytes);

/**
 * Obtain an iovector covering data stored in a ring buffer.
 *
 * @param [rb] The ring buffer.
 * @param [iov] An iovector to be updated.
 * @param [nbytes] The number of bytes to obtain a vector for.
 * @return The number of entries used in the iovector (up to 2).
 */
int rb_data_iovec(ring_buffer_t *rb, struct iovec iov[2], size_t nbytes);

/**
 * Discard bytes from a ring buffer.
 *
 * @param [rb] The ring buffer.
 * @param [nbytes] The number of bytes to discard.
 * @return The number of bytes discarded.
 */
ssize_t rb_advance(ring_buffer_t *rb, size_t nbytes);

/**
 * Clear a ring buffer.
 *
 * @param [rb] The ring buffer.
 */
void rb_clear(ring_buffer_t *rb);

#endif/*NEO4J_RING_BUFFER_H*/
