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
#include "../config.h"
#include "buffered_iostream.h"
#include "../src/lib/util.h"
#include <assert.h>
#include <unistd.h>


struct buffered_iostream {
    neo4j_iostream_t iostream;
    ring_buffer_t *inbuffer;
    ring_buffer_t *outbuffer;
};

static ssize_t buffered_read(neo4j_iostream_t *stream, void *buf, size_t nbyte);
static ssize_t buffered_readv(neo4j_iostream_t *stream,
        const struct iovec *iov, int iovcnt);
static ssize_t buffered_write(neo4j_iostream_t *stream,
        const void *buf, size_t nbyte);
static ssize_t buffered_writev(neo4j_iostream_t *stream,
        const struct iovec *iov, int iovcnt);
static int buffered_close(neo4j_iostream_t *stream);


neo4j_iostream_t *neo4j_buffered_iostream(ring_buffer_t *inbuffer,
        ring_buffer_t *outbuffer)
{
    REQUIRE(inbuffer != NULL, NULL);
    REQUIRE(outbuffer != NULL, NULL);

    struct buffered_iostream *ios = calloc(1, sizeof(struct buffered_iostream));
    if (ios == NULL)
    {
        return NULL;
    }

    ios->inbuffer = inbuffer;
    ios->outbuffer = outbuffer;
    ios->iostream.read = buffered_read;
    ios->iostream.readv = buffered_readv;
    ios->iostream.write = buffered_write;
    ios->iostream.writev = buffered_writev;
    ios->iostream.close = buffered_close;
    return (neo4j_iostream_t *)ios;
}


ssize_t buffered_read(neo4j_iostream_t *stream, void *buf, size_t nbyte)
{
    struct buffered_iostream *ios = (struct buffered_iostream *)stream;
    if (ios->inbuffer == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    return rb_extract(ios->inbuffer, buf, nbyte);
}


ssize_t buffered_readv(neo4j_iostream_t *stream,
        const struct iovec *iov, int iovcnt)
{
    struct buffered_iostream *ios = (struct buffered_iostream *)stream;
    if (ios->inbuffer == NULL)
    {
        errno = EPIPE;
        return -1;
    }

    ssize_t received = 0;
    for (int i = 0; i < iovcnt; ++i)
    {
        uint8_t *base = iov[i].iov_base;
        size_t len = iov[i].iov_len;
        while (len > 0)
        {
            ssize_t result = rb_extract(ios->inbuffer, base, len);
            if (result <= 0)
            {
                return (received > 0)? received : result;
            }
            assert((size_t)result <= len);
            received += result;
            len -= result;
        }
    }
    return received;
}


ssize_t buffered_write(neo4j_iostream_t *stream, const void *buf, size_t nbyte)
{
    struct buffered_iostream *ios = (struct buffered_iostream *)stream;
    if (ios->outbuffer == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    return rb_append(ios->outbuffer, buf, nbyte);
}


ssize_t buffered_writev(neo4j_iostream_t *stream,
        const struct iovec *iov, int iovcnt)
{
    struct buffered_iostream *ios = (struct buffered_iostream *)stream;
    if (ios->outbuffer == NULL)
    {
        errno = EPIPE;
        return -1;
    }

    ssize_t written = 0;
    for (int i = 0; i < iovcnt; ++i)
    {
        ssize_t result = rb_append(ios->outbuffer,
                iov[i].iov_base, iov[i].iov_len);
        if (result < 0)
        {
            return (written > 0)? written : result;
        }
        written += result;
    }
    return written;
}


int buffered_close(neo4j_iostream_t *stream)
{
    struct buffered_iostream *ios = (struct buffered_iostream *)stream;
    if (ios->inbuffer == NULL || ios->outbuffer == NULL)
    {
        errno = EPIPE;
        return -1;
    }

    ios->inbuffer = NULL;
    ios->outbuffer = NULL;
    free(ios);
    return 0;
}
