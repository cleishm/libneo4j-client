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
#ifndef NEO4J_IOSTREAM_H
#define NEO4J_IOSTREAM_H

#include "neo4j-client.h"

typedef struct neo4j_iostream neo4j_iostream_t;


/**
 * Read from an iostream into a buffer.
 *
 * Note: may return before the buffer is filled.
 *
 * @internal
 *
 * @param [ios] The iostream to read from.
 * @param [buf] The buffer to read into.
 * @param [nbyte] The number of bytes to read into the buffer.
 * @return The number of bytes read, or -1 on failure (errno will be set).
 */
__neo4j_must_check
static inline ssize_t neo4j_ios_read(neo4j_iostream_t *ios,
        void *buf, size_t nbyte)
{
    return ios->read(ios, buf, nbyte);
}

/**
 * Read from an iostream into a vector of buffers.
 *
 * Note: may return before the buffers in the vector are filled.
 *
 * @internal
 *
 * @param [ios] The iostream to read from.
 * @param [iov] The vector of buffers to read into.
 * @param [iovcnt] The length of the vector.
 * @return The number of bytes read, or -1 on failure (errno will be set).
 */
__neo4j_must_check
static inline ssize_t neo4j_ios_readv(neo4j_iostream_t *ios,
        const struct iovec *iov, unsigned int iovcnt)
{
    return ios->readv(ios, iov, iovcnt);
}

/**
 * Read from an iostream into a buffer until full.
 *
 * Will return when the buffer is full or when the stream encounters an error.
 *
 * @internal
 *
 * @param [ios] The iostream to read from.
 * @param [buf] The buffer to read into.
 * @param [nbyte] The number of bytes to read into the buffer.
 * @param [received] `NULL` or a pointer to a `size_t`, which will be updated
 *         with the number of bytes actually written into the buffer.
 * @return 0 on success, -1 on failure (errno will be set).
 */
__neo4j_must_check
int neo4j_ios_read_all(neo4j_iostream_t *ios,
        void *buf, size_t nbyte, size_t *received);

/**
 * Read from an iostream into a vector of buffers until full.
 *
 * Will return when the buffers are full or when the stream encounters an error.
 *
 * @internal
 *
 * @param [ios] The iostream to read from.
 * @param [iov] The vector of buffers to read into.
 * @param [iovcnt] The length of the vector.
 * @param [received] `NULL` or a pointer to a `size_t`, which will be updated
 *         with the number of bytes actually written into the buffer.
 * @return 0 on success, -1 on failure (errno will be set).
 */
__neo4j_must_check
int neo4j_ios_readv_all(neo4j_iostream_t *ios,
        const struct iovec *iov, unsigned int iovcnt, size_t *received);

/**
 * Read from an iostream into a vector of buffers until full.
 *
 * Will return when the buffers are full or when the stream encounters an error.
 *
 * NOTE: this will modify the supplied I/O vector, even on failure.
 *
 * @internal
 *
 * @param [ios] The iostream to read from.
 * @param [iov] The vector of buffers to read into (will be modified!).
 * @param [iovcnt] The length of the vector.
 * @param [received] `NULL` or a pointer to a `size_t`, which will be updated
 *         with the number of bytes actually written into the buffer.
 * @return 0 on success, -1 on failure (errno will be set).
 */
__neo4j_must_check
int neo4j_ios_nonconst_readv_all(neo4j_iostream_t *ios,
        struct iovec *iov, unsigned int iovcnt, size_t *received);


/**
 * Write to an iostream from a buffer.
 *
 * Note: may return before all data in the buffer is written.
 *
 * @internal
 *
 * @param [ios] The iostream to write to.
 * @param [buf] The buffer containing data to write.
 * @param [nbyte] The number of bytes in the buffer to be written.
 * @return The number of bytes written, or -1 on failure (errno will be set).
 */
__neo4j_must_check
static inline ssize_t neo4j_ios_write(neo4j_iostream_t *ios,
        const void *buf, size_t nbyte)
{
    return ios->write(ios, buf, nbyte);
}

/**
 * Write to an iostream from a vector of buffers.
 *
 * Note: may return before all data in the vector of buffers is written.
 *
 * @internal
 *
 * @param [ios] The iostream to write to.
 * @param [iov] The vector of buffers containing data to write.
 * @param [iovcnt] The length of the vector.
 * @return The number of bytes written, or -1 on failure (errno will be set).
 */
__neo4j_must_check
static inline ssize_t neo4j_ios_writev(neo4j_iostream_t *ios,
        const struct iovec *iov, unsigned int iovcnt)
{
    return ios->writev(ios, iov, iovcnt);
}

/**
 * Write all data from a buffer to an iostream.
 *
 * Will return when all data from the buffer is written or when the stream
 * encounters an error.
 *
 * @internal
 *
 * @param [ios] The iostream to write to.
 * @param [buf] The buffer containing data to write.
 * @param [nbyte] The number of bytes in the buffer to be written.
 * @param [written] `NULL` or a pointer to a `size_t`, which will be updated
 *         with the number of bytes actually written to the iostream.
 * @return 0 on success, -1 on failure (errno will be set).
 */
__neo4j_must_check
int neo4j_ios_write_all(neo4j_iostream_t *ios,
        const void *buf, size_t nbyte, size_t *written);

/**
 * Write all data from a vector of buffers to an iostream.
 *
 * Will return when all data from the vector of buffers has been written or
 * when the stream encounters an error.
 *
 * @internal
 *
 * @param [ios] The iostream to write to.
 * @param [iov] The vector of buffers containing data to write.
 * @param [iovcnt] The length of the vector.
 * @param [written] `NULL` or a pointer to a `size_t`, which will be updated
 *         with the number of bytes actually written to the iostream.
 * @return 0 on success, -1 on failure (errno will be set).
 */
__neo4j_must_check
int neo4j_ios_writev_all(neo4j_iostream_t *ios,
        const struct iovec *iov, unsigned int iovcnt, size_t *written);

/**
 * Write all data from a vector of buffers to an iostream.
 *
 * Will return when all data from the vector of buffers has been written or
 * when the stream encounters an error.
 *
 * NOTE: this will modify the supplied I/O vector, even on failure.
 *
 * @internal
 *
 * @param [ios] The iostream to write to.
 * @param [iov] The vector of buffers containing data to write.
 * @param [iovcnt] The length of the vector.
 * @param [written] `NULL` or a pointer to a `size_t`, which will be updated
 *         with the number of bytes actually written to the iostream.
 * @return 0 on success, -1 on failure (errno will be set).
 */
__neo4j_must_check
int neo4j_ios_nonconst_writev_all(neo4j_iostream_t *ios,
        struct iovec *iov, unsigned int iovcnt, size_t *written);

/**
 * Flush the output buffer of the iostream.
 *
 * For unbuffered streams, this is a no-op.
 *
 * @internal
 *
 * @param [ios] The iostream to flush.
 * @return 0 on success, -1 on error (errno will be set).
 */
static inline int neo4j_ios_flush(neo4j_iostream_t *ios)
{
    return ios->flush(ios);
}

/**
 * Close the iostream.
 *
 * @internal
 *
 * @param [ios] The iostream to close. This iostream will be invalid and
 *         potentially deallocated after the function returns, even on error.
 * @return 0 on success, -1 on error (errno will be set).
 */
static inline int neo4j_ios_close(neo4j_iostream_t *ios)
{
    return ios->close(ios);
}

#endif/*NEO4J_IOSTREAM_H*/
