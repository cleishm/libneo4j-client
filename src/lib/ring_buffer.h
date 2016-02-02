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
#ifndef LIBRING_BUFFER_H
#define LIBRING_BUFFER_H

#include <stdint.h>
#include <sys/uio.h>
#include <unistd.h>

#if __GNUC__ > 3
#define __librb_malloc __attribute__((malloc))
#else
#define __librb_malloc /*malloc*/
#endif

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
__librb_malloc
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
 * @return The number of bytes appended, which may be 0 if the buffer is full.
 */
size_t rb_append(ring_buffer_t *rb, const void *src, size_t nbytes);

/**
 * Append data to a ring buffer.
 *
 * @param [rb] The ring buffer.
 * @param [iov] A vector of buffers to append from.
 * @param [iovcnt] The length of the vector.
 * @return The number of bytes appended, which may be 0 if the buffer is full.
 */
size_t rb_appendv(ring_buffer_t *rb,
        const struct iovec *iov, unsigned int iovcnt);

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
 * @return The number of bytes extracted, which may be 0 if the buffer is empty.
 */
size_t rb_extract(ring_buffer_t *rb, void *dst, size_t nbytes);

/**
 * Extract data from a ring buffer.
 *
 * @param [rb] The ring buffer.
 * @param [iov] A vector of buffers to extract into.
 * @param [iovcnt] The length of the vector.
 * @return The number of bytes extracted, which may be 0 if the buffer is empty.
 */
size_t rb_extractv(ring_buffer_t *rb,
        const struct iovec *iov, unsigned int iovcnt);

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
 * Obtain an I/O vector covering data stored in a ring buffer.
 *
 * @param [rb] The ring buffer.
 * @param [iov] An I/O vector to be updated.
 * @param [nbytes] The number of bytes to obtain a vector for.
 * @return The number of entries used in the I/O vector (up to 2).
 */
unsigned int rb_data_iovec(ring_buffer_t *rb, struct iovec iov[2],
        size_t nbytes);

/**
 * Obtain an I/O vector covering free space in a ring buffer.
 *
 * @param [rb] The ring buffer.
 * @param [iov] An I/O vector to be updated.
 * @param [nbytes] The number of bytes to obtain a vector for.
 * @return The number of entries used in the I/O vector (up to 2). Will be 0
 *         only if the buffer is full.
 */
unsigned int rb_space_iovec(ring_buffer_t *rb, struct iovec iov[2],
        size_t nbytes);

/**
 * Increase the used space of a buffer.
 *
 * @warning This method MUST only be used after writing data into a vector
 * obtained via `rb_space_iovec(...)`, as it will mark data in the buffer as
 * valid. If this data isn't first written then it will contain data that is
 * either random or contains stale content from previous writes to the buffer,
 * which could have serious security implications.
 *
 * @param [rb] The ring buffer.
 * @param [nbytes] The number of bytes to to advance the buffer, which must
 *         be the same amount written into the vector obtained from
 *         `rb_space_iovec(...)`.
 * @return The number of bytes advanced.
 */
size_t rb_advance(ring_buffer_t *rb, size_t nbytes);

/**
 * Discard bytes from a ring buffer.
 *
 * @param [rb] The ring buffer.
 * @param [nbytes] The number of bytes to discard.
 * @return The number of bytes discarded.
 */
size_t rb_discard(ring_buffer_t *rb, size_t nbytes);

/**
 * Clear a ring buffer.
 *
 * @param [rb] The ring buffer.
 */
static inline void rb_clear(ring_buffer_t *rb)
{
    rb->ptr = rb->buffer;
    rb->used = 0;
}

#endif/*LIBRING_BUFFER_H*/
