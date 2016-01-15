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
#include "iostream.h"
#include "util.h"
#include <assert.h>
#include <limits.h>
#include <unistd.h>


int neo4j_ios_read_all(neo4j_iostream_t *stream,
        void *buf, size_t nbyte, size_t *received)
{
    assert(stream != NULL);
    assert(buf != NULL);
    assert(nbyte > 0);
    ENSURE_NOT_NULL(size_t, received, 0);
    if (nbyte > SSIZE_MAX)
    {
        errno = EMSGSIZE;
        return -1;
    }

    *received = 0;
    ssize_t result;
    do
    {
        result = neo4j_ios_read(stream, buf, nbyte);
        if (result < 0 && errno == EINTR)
        {
            continue;
        }
        if (result < 0)
        {
            // only partially received
            return -1;
        }
        if (result == 0 && nbyte > 0)
        {
            errno = NEO4J_CONNECTION_CLOSED;
            return -1;
        }
        assert((size_t)result <= nbyte);
        *received += result;
        buf = ((uint8_t *)buf) + result;
        nbyte -= result;
    } while (nbyte > 0);

    return 0;
}


int neo4j_ios_readv_all(neo4j_iostream_t *stream,
        const struct iovec *iov, unsigned int iovcnt, size_t *received)
{
    assert(stream != NULL);
    assert(iov != NULL);
    assert(iovcnt > 0);
    ENSURE_NOT_NULL(size_t, received, 0);
    size_t total = iovlen(iov, iovcnt);
    if (total > SSIZE_MAX || iovcnt > IOV_MAX)
    {
        errno = EMSGSIZE;
        return -1;
    }

    // first try to read with the provided iovec
    ssize_t result;
    do
    {
        result = neo4j_ios_readv(stream, iov, iovcnt);
    } while (result < 0 && errno == EINTR);

    if (result < 0)
    {
        return -1;
    }
    *received = result;
    if ((size_t)result == total)
    {
        return 0;
    }
    if (result == 0)
    {
        errno = NEO4J_CONNECTION_CLOSED;
        return -1;
    }

    // read isn't complete - duplicate the iovec and do a nonconst read
    ALLOC_IOVEC(diov, iovcnt);
    if (diov == NULL)
    {
        return -1;
    }

    unsigned int diovcnt = iov_skip(diov, iov, iovcnt, *received);
    assert(diovcnt > 0);
    size_t additional;
    int r = neo4j_ios_nonconst_readv_all(stream, diov, diovcnt, &additional);
    int errsv = errno;
    *received += additional;
    FREE_IOVEC(diov);
    errno = errsv;
    return r;
}


int neo4j_ios_nonconst_readv_all(neo4j_iostream_t *stream,
        struct iovec *iov, unsigned int iovcnt, size_t *received)
{
    assert(stream != NULL);
    assert(iov != NULL);
    assert(iovcnt > 0);
    ENSURE_NOT_NULL(size_t, received, 0);
    size_t total = iovlen(iov, iovcnt);
    if (total > SSIZE_MAX)
    {
        errno = EMSGSIZE;
        return -1;
    }

    *received = 0;
    do
    {
        ssize_t result = neo4j_ios_readv(stream, iov, iovcnt);
        if (result < 0 && errno == EINTR)
        {
            continue;
        }
        if (result < 0)
        {
            return -1;
        }
        if (result == 0 && total > 0)
        {
            errno = NEO4J_CONNECTION_CLOSED;
            return -1;
        }

        *received += result;
        assert(*received <= total);
        iovcnt = iov_skip(iov, iov, iovcnt, result);
    } while (iovcnt > 0);

    assert(*received == total);
    return 0;
}


int neo4j_ios_write_all(neo4j_iostream_t *stream,
        const void *buf, size_t nbyte, size_t *written)
{
    assert(stream != NULL);
    assert(buf != NULL);
    assert(nbyte > 0);
    ENSURE_NOT_NULL(size_t, written, 0);
    if (nbyte > SSIZE_MAX)
    {
        errno = EMSGSIZE;
        return -1;
    }

    *written = 0;
    ssize_t result;
    do
    {
        result = neo4j_ios_write(stream, buf, nbyte);
        if (result < 0 && errno == EINTR)
        {
            continue;
        }
        if (result < 0)
        {
            // only partially written
            return -1;
        }
        assert((size_t)result <= nbyte);
        *written += result;
        buf = ((const uint8_t *)buf) + result;
        nbyte -= result;
    } while (nbyte > 0);

    return 0;
}


int neo4j_ios_writev_all(neo4j_iostream_t *stream,
        const struct iovec *iov, unsigned int iovcnt, size_t *written)
{
    assert(stream != NULL);
    assert(iov != NULL);
    assert(iovcnt > 0);
    ENSURE_NOT_NULL(size_t, written, 0);
    size_t total = iovlen(iov, iovcnt);
    if (total > SSIZE_MAX || iovcnt > IOV_MAX)
    {
        errno = EMSGSIZE;
        return -1;
    }

    // first try to write with the provided iovec
    ssize_t result;
    do
    {
        result = neo4j_ios_writev(stream, iov, iovcnt);
    } while (result < 0 && errno == EINTR);

    if (result < 0)
    {
        return -1;
    }
    *written = result;
    if ((size_t)result == total)
    {
        return 0;
    }

    // write isn't complete - duplicate the iovec and do a nonconst write
    ALLOC_IOVEC(diov, iovcnt);
    if (diov == NULL)
    {
        return -1;
    }

    assert(*written > 0);
    unsigned int diovcnt = iov_skip(diov, iov, iovcnt, *written);
    assert(diovcnt > 0);
    size_t additional;
    int n = neo4j_ios_nonconst_writev_all(stream, diov, diovcnt, &additional);
    *written += additional;
    FREE_IOVEC(diov);
    return n;
}


int neo4j_ios_nonconst_writev_all(neo4j_iostream_t *stream,
        struct iovec *iov, unsigned int iovcnt, size_t *written)
{
    assert(stream != NULL);
    assert(iov != NULL);
    assert(iovcnt > 0);
    ENSURE_NOT_NULL(size_t, written, 0);
    size_t total = iovlen(iov, iovcnt);
    if (total > SSIZE_MAX)
    {
        errno = EMSGSIZE;
        return -1;
    }

    *written = 0;
    do
    {
        ssize_t result = neo4j_ios_writev(stream, iov, iovcnt);
        if (result < 0 && errno == EINTR)
        {
            continue;
        }
        if (result < 0)
        {
            return -1;
        }

        *written += result;
        assert(*written <= total);
        iovcnt = iov_skip(iov, iov, iovcnt, result);
    } while (iovcnt > 0);

    assert(*written == total);
    return 0;
}
