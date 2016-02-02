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
#include "buffering_iostream.h"
#include "ring_buffer.h"
#include "util.h"
#include <assert.h>
#include <stddef.h>
#include <unistd.h>


struct buffering_iostream {
    neo4j_iostream_t _iostream;

    neo4j_iostream_t *delegate;
    bool close_delegate;

    ring_buffer_t *rcvbuf;
    ring_buffer_t *sndbuf;
};


static ssize_t buffering_read(neo4j_iostream_t *stream,
        void *buf, size_t nbyte);
static ssize_t buffering_readv(neo4j_iostream_t *stream,
        const struct iovec *iov, unsigned int iovcnt);
static ssize_t buffering_write(neo4j_iostream_t *stream,
        const void *buf, size_t nbyte);
static ssize_t buffering_writev(neo4j_iostream_t *stream,
        const struct iovec *iov, unsigned int iovcnt);
static int buffering_flush(neo4j_iostream_t *stream);
static int buffering_close(neo4j_iostream_t *stream);


neo4j_iostream_t *neo4j_buffering_iostream(neo4j_iostream_t *delegate,
        bool close, size_t rcvbuf_size, size_t sndbuf_size)
{
    REQUIRE(delegate != NULL, NULL);
    REQUIRE(rcvbuf_size > 0 || sndbuf_size > 0, NULL);

    struct buffering_iostream *ios =
        calloc(1, sizeof(struct buffering_iostream));
    if (ios == NULL)
    {
        return NULL;
    }

    ios->delegate = delegate;
    ios->close_delegate = close;

    if (rcvbuf_size > 0)
    {
        ios->rcvbuf = rb_alloc(rcvbuf_size);
        if (ios->rcvbuf == NULL)
        {
            goto failure;
        }
    }
    if (sndbuf_size > 0)
    {
        ios->sndbuf = rb_alloc(sndbuf_size);
        if (ios->sndbuf == NULL)
        {
            goto failure;
        }
    }

    neo4j_iostream_t *iostream = &(ios->_iostream);
    iostream->read = buffering_read;
    iostream->readv = buffering_readv;
    iostream->write = buffering_write;
    iostream->writev = buffering_writev;
    iostream->flush = buffering_flush;
    iostream->close = buffering_close;
    return iostream;

    int errsv;
failure:
    errsv = errno;
    if (ios->rcvbuf != NULL)
    {
        rb_free(ios->rcvbuf);
    }
    if (ios->sndbuf != NULL)
    {
        rb_free(ios->sndbuf);
    }
    free(ios);
    errno = errsv;
    return NULL;
}


ssize_t buffering_read(neo4j_iostream_t *stream, void *buf, size_t nbyte)
{
    struct buffering_iostream *ios = container_of(stream,
            struct buffering_iostream, _iostream);
    if (ios->delegate == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    if (ios->rcvbuf == NULL)
    {
        return neo4j_ios_read(ios->delegate, buf, nbyte);
    }
    if (nbyte > SSIZE_MAX)
    {
        nbyte = SSIZE_MAX;
    }

    size_t extracted = rb_extract(ios->rcvbuf, buf, nbyte);
    assert(extracted <= nbyte);
    if (extracted == nbyte)
    {
        return extracted;
    }

    buf = (uint8_t *)buf + extracted;
    nbyte -= extracted;

    struct iovec iov[3];
    int iovcnt = 1;
    iov[0].iov_base = buf;
    iov[0].iov_len = nbyte;
    iovcnt += rb_space_iovec(ios->rcvbuf, iov + 1, rb_size(ios->rcvbuf));

    ssize_t n = neo4j_ios_readv(ios->delegate, iov, iovcnt);
    if (n < 0)
    {
        return (extracted > 0)? (ssize_t)extracted : -1;
    }
    if ((size_t)n <= nbyte)
    {
        return extracted + (size_t)n;
    }
    rb_advance(ios->rcvbuf, n - nbyte);
    return extracted + nbyte;
}


ssize_t buffering_readv(neo4j_iostream_t *stream,
        const struct iovec *iov, unsigned int iovcnt)
{
    struct buffering_iostream *ios = container_of(stream,
            struct buffering_iostream, _iostream);
    if (ios->delegate == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    if (ios->rcvbuf == NULL)
    {
        return neo4j_ios_readv(ios->delegate, iov, iovcnt);
    }
    if (iovcnt > IOV_MAX-2)
    {
        iovcnt = IOV_MAX-2;
    }

    size_t nbyte = iovlen(iov, iovcnt);

    size_t extracted = rb_extractv(ios->rcvbuf, iov, iovcnt);
    assert(extracted <= nbyte);
    if (extracted == nbyte)
    {
        return extracted;
    }

    nbyte -= extracted;

    ALLOC_IOVEC(diov, iovcnt+2);
    if (diov == NULL)
    {
        return (extracted > 0)? (ssize_t)extracted : -1;
    }
    unsigned int diovcnt = iov_skip(diov, iov, iovcnt, extracted);

    diovcnt += rb_space_iovec(ios->rcvbuf, diov + diovcnt,
            rb_size(ios->rcvbuf));

    ssize_t result = neo4j_ios_readv(ios->delegate, diov, diovcnt);
    if (result < 0)
    {
        result = (extracted > 0)? (ssize_t)extracted : -1;
        goto cleanup;
    }
    if ((size_t)result <= nbyte)
    {
        result += extracted;
        goto cleanup;
    }
    rb_advance(ios->rcvbuf, result - nbyte);
    result = extracted + nbyte;

    int errsv;
cleanup:
    errsv = errno;
    FREE_IOVEC(diov);
    errno = errsv;
    return result;
}


ssize_t buffering_write(neo4j_iostream_t *stream, const void *buf, size_t nbyte)
{
    struct buffering_iostream *ios = container_of(stream,
            struct buffering_iostream, _iostream);
    if (ios->delegate == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    if (ios->sndbuf == NULL)
    {
        return neo4j_ios_write(ios->delegate, buf, nbyte);
    }
    if (nbyte > SSIZE_MAX)
    {
        nbyte = SSIZE_MAX;
    }

    if (nbyte <= rb_space(ios->sndbuf))
    {
        return rb_append(ios->sndbuf, buf, nbyte);
    }

    size_t buffered = rb_used(ios->sndbuf);

    struct iovec iov[3];
    unsigned int iovcnt = rb_data_iovec(ios->sndbuf, iov, rb_size(ios->sndbuf));
    iov[iovcnt].iov_base = (uint8_t *)(intptr_t)buf;
    iov[iovcnt].iov_len = nbyte;
    ++iovcnt;

    ssize_t written = neo4j_ios_writev(ios->delegate, iov, iovcnt);
    if (written < 0)
    {
        return -1;
    }

    const uint8_t *rbytes;
    size_t remaining;

    if ((size_t)written < buffered)
    {
        rb_discard(ios->sndbuf, written);
        rbytes = buf;
        remaining = nbyte;
        written = 0;
    }
    else
    {
        rb_clear(ios->sndbuf);
        written -= buffered;
        assert((size_t)written <= nbyte);
        rbytes = (const uint8_t *)buf + written;
        remaining = nbyte - written;
    }

    if (remaining == 0)
    {
        return nbyte;
    }

    size_t appended = rb_append(ios->sndbuf, rbytes, remaining);
    return (size_t)written + appended;
}


ssize_t buffering_writev(neo4j_iostream_t *stream,
        const struct iovec *iov, unsigned int iovcnt)
{
    struct buffering_iostream *ios = container_of(stream,
            struct buffering_iostream, _iostream);
    if (ios->delegate == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    if (ios->sndbuf == NULL)
    {
        return neo4j_ios_writev(ios->delegate, iov, iovcnt);
    }
    if (iovcnt > IOV_MAX-2)
    {
        iovcnt = IOV_MAX-2;
    }

    size_t nbyte = iovlen(iov, iovcnt);

    if (nbyte <= rb_space(ios->sndbuf))
    {
        return rb_appendv(ios->sndbuf, iov, iovcnt);
    }

    size_t buffered = rb_used(ios->sndbuf);

    ALLOC_IOVEC(diov, iovcnt+2);
    if (diov == NULL)
    {
        return -1;
    }
    unsigned int diovcnt = rb_data_iovec(ios->sndbuf, diov,
            rb_size(ios->sndbuf));
    memcpy(diov+diovcnt, iov, iovcnt * sizeof(struct iovec));

    ssize_t written = neo4j_ios_writev(ios->delegate, diov, diovcnt + iovcnt);
    if (written < 0)
    {
        goto cleanup;
    }

    if ((size_t)written < buffered)
    {
        rb_discard(ios->sndbuf, written);
        memcpy(diov, iov, iovcnt * sizeof(struct iovec));
        diovcnt = iovcnt;
        written = 0;
    }
    else
    {
        rb_clear(ios->sndbuf);
        written -= buffered;
        assert((size_t)written <= nbyte);
        diovcnt = iov_skip(diov, iov, iovcnt, written);
    }

    if (diovcnt == 0)
    {
        written = nbyte;
        goto cleanup;
    }

    written += rb_appendv(ios->sndbuf, diov, diovcnt);

    int errsv;
cleanup:
    errsv = errno;
    FREE_IOVEC(diov);
    errno = errsv;
    return written;
}


int buffering_flush(neo4j_iostream_t *stream)
{
    struct buffering_iostream *ios = container_of(stream,
            struct buffering_iostream, _iostream);
    if (ios->delegate == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    if (!rb_is_empty(ios->sndbuf))
    {
        struct iovec iov[2];
        unsigned int iovcnt = rb_data_iovec(ios->sndbuf, iov,
                rb_size(ios->sndbuf));
        size_t written;
        if (neo4j_ios_writev_all(ios->delegate, iov, iovcnt, &written))
        {
            rb_discard(ios->sndbuf, written);
            return -1;
        }
        rb_clear(ios->sndbuf);
    }
    return neo4j_ios_flush(ios->delegate);
}


int buffering_close(neo4j_iostream_t *stream)
{
    struct buffering_iostream *ios = container_of(stream,
            struct buffering_iostream, _iostream);
    if (ios->delegate == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    neo4j_iostream_t *delegate = ios->delegate;
    ios->delegate = NULL;
    rb_free(ios->sndbuf);
    rb_free(ios->rcvbuf);
    free(ios);
    return neo4j_ios_close(delegate);
}
