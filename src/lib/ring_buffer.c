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
#include "ring_buffer.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>


#ifndef NDEBUG
static inline void rb_assert(const ring_buffer_t *rb)
{
    assert(rb != NULL);
    assert(rb->buffer != NULL);
    assert(rb->ptr != NULL);
    assert(rb->ptr >= rb->buffer);
    assert(rb->ptr < rb->buffer + rb->size);
    assert(rb->used <= rb->size);
}
#else
#define rb_assert(RB) do {} while(0)
#endif


ring_buffer_t *rb_alloc(size_t size)
{
    if (size == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    ring_buffer_t *rb = calloc(1, sizeof(ring_buffer_t));
    if (rb == NULL)
    {
        return NULL;
    }

    rb->buffer = malloc(size);
    if (rb->buffer == NULL)
    {
        int errsv = errno;
        free(rb);
        errno = errsv;
        return NULL;
    }
    rb->size = size;
    rb->ptr = rb->buffer;
    rb->used = 0;

    return rb;
}


void rb_free(ring_buffer_t *rb)
{
    rb_assert(rb);
    free(rb->buffer);
    free(rb);
}


size_t rb_append(ring_buffer_t *rb, const void *src, size_t nbytes)
{
    struct iovec diov[2];
    unsigned int diovcnt = rb_space_iovec(rb, diov, nbytes);
    if (diovcnt == 0)
    {
        return 0;
    }

    size_t appended = memcpy_to_iov(diov, diovcnt, src, nbytes);
    rb_advance(rb, appended);
    return appended;
}


size_t rb_appendv(ring_buffer_t *rb,
        const struct iovec *siov, unsigned int siovcnt)
{
    if (siovcnt == 0)
    {
        return 0;
    }

    struct iovec iov[2];
    unsigned int iovcnt = rb_space_iovec(rb, iov, rb->size);
    if (iovcnt == 0)
    {
        return 0;
    }

    size_t appended = memcpy_from_iov_to_iov(iov, iovcnt, siov, siovcnt);
    rb_advance(rb, appended);
    return appended;
}


ssize_t rb_read(ring_buffer_t *rb, int fd, size_t nbytes)
{
    struct iovec iov[2];
    unsigned int iovcnt = rb_space_iovec(rb, iov, nbytes);
    if (iovcnt == 0)
    {
        errno = ENOBUFS;
        return -1;
    }

    ssize_t n = readv(fd, iov, iovcnt);
    if (n <= 0)
    {
        return n;
    }

    rb->used += n;
    return n;
}


unsigned int rb_data_iovec(ring_buffer_t *rb, struct iovec iov[2],
        size_t nbytes)
{
    rb_assert(rb);

    if (rb_is_empty(rb))
    {
        return 0;
    }

    size_t max = rb_used(rb);
    if (nbytes > max)
    {
        nbytes = max;
    }

    unsigned int iovcnt = 1;
    iov[0].iov_base = rb->ptr;
    if ((size_t)((rb->buffer + rb->size) - rb->ptr) >= nbytes)
    {
        iov[0].iov_len = nbytes;
    }
    else
    {
        size_t tail_len = (rb->buffer + rb->size) - rb->ptr;
        iov[0].iov_len = tail_len;
        iov[1].iov_base = rb->buffer;
        iov[1].iov_len = nbytes - tail_len;
        iovcnt++;
    }

    return iovcnt;
}


unsigned int rb_space_iovec(ring_buffer_t *rb, struct iovec iov[2],
        size_t nbytes)
{
    rb_assert(rb);

    if (rb_is_full(rb))
    {
        return 0;
    }

    size_t max = rb_space(rb);
    if (nbytes > max)
    {
        nbytes = max;
    }

    unsigned int iovcnt = 1;

    if (rb_is_empty(rb))
    {
        assert(rb->ptr == rb->buffer);
        iov[0].iov_base = rb->buffer;
        iov[0].iov_len = nbytes;
    }
    else if (((rb->ptr - rb->buffer) + rb->used) >= rb->size)
    {
        iov[0].iov_base = rb->ptr - (rb->size - rb->used);
        iov[0].iov_len = nbytes;
    }
    else
    {
        size_t tail_len = (rb->buffer + rb->size) - (rb->ptr + rb->used);
        iov[0].iov_base = rb->ptr + rb->used;

        if (tail_len > nbytes)
        {
            iov[0].iov_len = nbytes;
        }
        else
        {
            iov[0].iov_len = tail_len;
            iov[1].iov_base = rb->buffer;
            iov[1].iov_len = nbytes - tail_len;
            iovcnt++;
        }
    }

    return iovcnt;
}


size_t rb_advance(ring_buffer_t *rb, size_t nbytes)
{
    rb_assert(rb);

    size_t space = rb_space(rb);
    if (nbytes > space)
    {
        nbytes = space;
    }

    rb->used += nbytes;
    return nbytes;
}


size_t rb_discard(ring_buffer_t *rb, size_t nbytes)
{
    rb_assert(rb);

    if (nbytes > rb->used)
    {
        nbytes = rb->used;
    }

    rb->used -= nbytes;
    if (rb->used == 0)
    {
        rb->ptr = rb->buffer;
    }
    else if ((size_t)((rb->buffer + rb->size) - rb->ptr) >= nbytes)
    {
        rb->ptr += nbytes;
    }
    else
    {
        size_t tail_len = (rb->buffer + rb->size) - rb->ptr;
        rb->ptr = rb->buffer + (nbytes - tail_len);
    }

    return nbytes;
}


size_t rb_extract(ring_buffer_t *rb, void *dst, size_t nbytes)
{
    struct iovec siov[2];
    unsigned int siovcnt = rb_data_iovec(rb, siov, nbytes);
    if (siovcnt == 0)
    {
        return 0;
    }

    size_t extracted = memcpy_from_iov(dst, nbytes, siov, siovcnt);
    rb_discard(rb, extracted);
    return extracted;
}


size_t rb_extractv(ring_buffer_t *rb,
        const struct iovec *iov, unsigned int iovcnt)
{
    if (iovcnt == 0)
    {
        return 0;
    }

    struct iovec siov[2];
    unsigned int siovcnt = rb_data_iovec(rb, siov, rb->size);
    if (siovcnt == 0)
    {
        return 0;
    }

    size_t extracted = memcpy_from_iov_to_iov(iov, iovcnt, siov, siovcnt);
    rb_discard(rb, extracted);
    return extracted;
}


ssize_t rb_write(ring_buffer_t *rb, int fd, size_t nbytes)
{
    struct iovec iov[2];
    unsigned int iovcnt = rb_data_iovec(rb, iov, nbytes);
    if (iovcnt == 0)
    {
        return iovcnt;
    }

    ssize_t n = writev(fd, iov, iovcnt);
    if (n <= 0)
    {
        return n;
    }

    rb->used -= n;
    if (rb->used == 0)
    {
        rb->ptr = rb->buffer;
    }
    else if ((size_t)((rb->ptr - rb->buffer) + n) < rb->size)
    {
        rb->ptr += n;
    }
    else
    {
        rb->ptr -= rb->size - n;
    }

    return n;
}
