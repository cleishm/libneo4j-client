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
#include "logging.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>


static int rb_space_iovec(ring_buffer_t *rb,
        struct iovec iov[2], size_t nbytes);


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


int rb_space_iovec(ring_buffer_t *rb, struct iovec iov[2], size_t nbytes)
{
    if (rb == NULL)
    {
        errno = EINVAL;
        return -1;
    }
    rb_assert(rb);

    if (rb_is_full(rb))
    {
        errno = ENOBUFS;
        return -1;
    }

    size_t max = rb_space(rb);
    if (nbytes > max)
    {
        nbytes = max;
    }

    int iovcnt = 1;

    if (((rb->ptr - rb->buffer) + rb->used) >= rb->size)
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


ssize_t rb_append(ring_buffer_t *rb, const void *src, size_t nbytes)
{
    struct iovec iov[2];
    int iovcnt = rb_space_iovec(rb, iov, nbytes);
    if (iovcnt < 0)
    {
        return -1;
    }

    const uint8_t *src_bytes = src;
    ssize_t n = 0;
    for (int i = 0; i < iovcnt; ++i)
    {
        memcpy(iov[i].iov_base, src_bytes, iov[i].iov_len);
        src_bytes += iov[i].iov_len;
        rb->used += iov[i].iov_len;
        n += iov[i].iov_len;
    }

    return n;
}


ssize_t rb_read(ring_buffer_t *rb, int fd, size_t nbytes)
{
    struct iovec iov[2];
    int iovcnt = rb_space_iovec(rb, iov, nbytes);
    if (iovcnt < 0)
    {
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


int rb_data_iovec(ring_buffer_t *rb, struct iovec iov[2], size_t nbytes)
{
    if (rb == NULL)
    {
        errno = EINVAL;
        return -1;
    }
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

    int iovcnt = 1;
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


ssize_t rb_discard(ring_buffer_t *rb, size_t nbytes)
{
    if (rb == NULL)
    {
        errno = EINVAL;
        return -1;
    }
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


ssize_t rb_extract(ring_buffer_t *rb, void *dst, size_t nbytes)
{
    struct iovec iov[2];
    int iovcnt = rb_data_iovec(rb, iov, nbytes);
    if (iovcnt <= 0)
    {
        return iovcnt;
    }

    uint8_t *dst_bytes = dst;
    ssize_t n = 0;
    for (int i = 0; i < iovcnt; ++i)
    {
        assert(iov[i].iov_base == rb->ptr);

        memcpy(dst_bytes, iov[i].iov_base, iov[i].iov_len);
        dst_bytes += iov[i].iov_len;
        rb->used -= iov[i].iov_len;

        size_t tail_len = (rb->buffer + rb->size) - rb->ptr;
        if (iov[i].iov_len < tail_len)
        {
            rb->ptr += iov[i].iov_len;
        }
        else
        {
            assert(iov[i].iov_len == tail_len);
            rb->ptr = rb->buffer;
        }
        n += iov[i].iov_len;
    }

    if (rb->used == 0)
    {
        rb->ptr = rb->buffer;
    }

    return n;
}


ssize_t rb_write(ring_buffer_t *rb, int fd, size_t nbytes)
{
    struct iovec iov[2];
    int iovcnt = rb_data_iovec(rb, iov, nbytes);
    if (iovcnt <= 0)
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


void rb_clear(ring_buffer_t *rb)
{
    rb->ptr = rb->buffer;
    rb->used = 0;
}
