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
#include "memiostream.h"
#include "../src/util.h"
#include <assert.h>
#include <unistd.h>


struct memiostream {
    neo4j_iostream_t iostream;
    ring_buffer_t *inbuffer;
    ring_buffer_t *outbuffer;
};

static ssize_t memios_read(neo4j_iostream_t *stream, void *buf, size_t nbyte);
static ssize_t memios_readv(neo4j_iostream_t *stream,
        const struct iovec *iov, unsigned int iovcnt);
static ssize_t memios_write(neo4j_iostream_t *stream,
        const void *buf, size_t nbyte);
static ssize_t memios_writev(neo4j_iostream_t *stream,
        const struct iovec *iov, unsigned int iovcnt);
static int memios_flush(neo4j_iostream_t *stream);
static int memios_close(neo4j_iostream_t *stream);


neo4j_iostream_t *neo4j_memiostream(ring_buffer_t *inbuffer,
        ring_buffer_t *outbuffer)
{
    REQUIRE(inbuffer != NULL, NULL);
    REQUIRE(outbuffer != NULL, NULL);

    struct memiostream *ios = calloc(1, sizeof(struct memiostream));
    if (ios == NULL)
    {
        return NULL;
    }

    ios->inbuffer = inbuffer;
    ios->outbuffer = outbuffer;
    ios->iostream.read = memios_read;
    ios->iostream.readv = memios_readv;
    ios->iostream.write = memios_write;
    ios->iostream.writev = memios_writev;
    ios->iostream.flush = memios_flush;
    ios->iostream.close = memios_close;
    return (neo4j_iostream_t *)ios;
}


ssize_t memios_read(neo4j_iostream_t *stream, void *buf, size_t nbyte)
{
    struct memiostream *ios = (struct memiostream *)stream;
    if (ios->inbuffer == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    return rb_extract(ios->inbuffer, buf, nbyte);
}


ssize_t memios_readv(neo4j_iostream_t *stream,
        const struct iovec *iov, unsigned int iovcnt)
{
    struct memiostream *ios = (struct memiostream *)stream;
    if (ios->inbuffer == NULL)
    {
        errno = EPIPE;
        return -1;
    }

    return rb_extractv(ios->inbuffer, iov, iovcnt);
}


ssize_t memios_write(neo4j_iostream_t *stream, const void *buf, size_t nbyte)
{
    struct memiostream *ios = (struct memiostream *)stream;
    if (ios->outbuffer == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    return rb_append(ios->outbuffer, buf, nbyte);
}


ssize_t memios_writev(neo4j_iostream_t *stream,
        const struct iovec *iov, unsigned int iovcnt)
{
    struct memiostream *ios = (struct memiostream *)stream;
    if (ios->outbuffer == NULL)
    {
        errno = EPIPE;
        return -1;
    }

    return rb_appendv(ios->outbuffer, iov, iovcnt);
}


int memios_flush(neo4j_iostream_t *stream)
{
    return 0;
}


int memios_close(neo4j_iostream_t *stream)
{
    struct memiostream *ios = (struct memiostream *)stream;
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
