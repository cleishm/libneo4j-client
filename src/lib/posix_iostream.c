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
#include "posix_iostream.h"
#include "util.h"
#include <assert.h>
#include <stddef.h>
#include <unistd.h>


struct posix_iostream {
    neo4j_iostream_t _iostream;
    int fd;
};

static_assert(offsetof(struct posix_iostream, _iostream) == 0,
        "_iostream must be first field in struct posix_iostream");


static ssize_t posix_read(neo4j_iostream_t *stream, void *buf, size_t nbyte);
static ssize_t posix_readv(neo4j_iostream_t *stream,
        const struct iovec *iov, int iovcnt);
static ssize_t posix_write(neo4j_iostream_t *stream,
        const void *buf, size_t nbyte);
static ssize_t posix_writev(neo4j_iostream_t *stream,
        const struct iovec *iov, int iovcnt);
static int posix_flush(neo4j_iostream_t *stream);
static int posix_close(neo4j_iostream_t *stream);


neo4j_iostream_t *neo4j_posix_iostream(int fd)
{
    REQUIRE(fd >= 0, NULL);

    struct posix_iostream *ios = calloc(1, sizeof(struct posix_iostream));
    if (ios == NULL)
    {
        return NULL;
    }

    ios->fd = fd;

    neo4j_iostream_t *iostream = (neo4j_iostream_t *)ios;
    iostream->read = posix_read;
    iostream->readv = posix_readv;
    iostream->write = posix_write;
    iostream->writev = posix_writev;
    iostream->flush = posix_flush;
    iostream->close = posix_close;
    return iostream;
}


ssize_t posix_read(neo4j_iostream_t *stream, void *buf, size_t nbyte)
{
    struct posix_iostream *ios = (struct posix_iostream *)stream;
    if (ios->fd < 0)
    {
        errno = EPIPE;
        return -1;
    }
    return read(ios->fd, buf, nbyte);
}


ssize_t posix_readv(neo4j_iostream_t *stream,
        const struct iovec *iov, int iovcnt)
{
    struct posix_iostream *ios = (struct posix_iostream *)stream;
    if (ios->fd < 0)
    {
        errno = EPIPE;
        return -1;
    }
    return readv(ios->fd, iov, iovcnt);
}


ssize_t posix_write(neo4j_iostream_t *stream, const void *buf, size_t nbyte)
{
    struct posix_iostream *ios = (struct posix_iostream *)stream;
    if (ios->fd < 0)
    {
        errno = EPIPE;
        return -1;
    }
    return write(ios->fd, buf, nbyte);
}


ssize_t posix_writev(neo4j_iostream_t *stream,
        const struct iovec *iov, int iovcnt)
{
    struct posix_iostream *ios = (struct posix_iostream *)stream;
    if (ios->fd < 0)
    {
        errno = EPIPE;
        return -1;
    }
    return writev(ios->fd, iov, iovcnt);
}


int posix_flush(neo4j_iostream_t *stream)
{
    return 0;
}


int posix_close(neo4j_iostream_t *stream)
{
    struct posix_iostream *ios = (struct posix_iostream *)stream;
    if (ios->fd < 0)
    {
        errno = EPIPE;
        return -1;
    }
    int fd = ios->fd;
    ios->fd = -1;
    free(ios);
    return close(fd);
}
