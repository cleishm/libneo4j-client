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
#include "chunking_iostream.h"
#include "util.h"
#include <assert.h>
#include <limits.h>


static ssize_t chunking_read(neo4j_iostream_t *self, void *buf, size_t nbyte);
static ssize_t chunking_readv(neo4j_iostream_t *self,
        const struct iovec *iov, unsigned int iovcnt);
static ssize_t chunking_write(neo4j_iostream_t *self,
        const void *buf, size_t nbyte);
static ssize_t chunking_writev(neo4j_iostream_t *self,
        const struct iovec *iov, unsigned int iovcnt);
static int chunking_flush(neo4j_iostream_t *self);
static int chunking_close(neo4j_iostream_t *self);
static int chunking_dealloc_close(neo4j_iostream_t *self);
static unsigned int chunk_iovec(const struct iovec *iov, unsigned int iovcnt,
        uint16_t max_chunk, size_t *nbytes, unsigned int *nchunks);


neo4j_iostream_t *neo4j_chunking_iostream(neo4j_iostream_t *delegate,
        uint16_t snd_min_chunk, uint16_t snd_max_chunk)
{
    REQUIRE(delegate != NULL, NULL);
    REQUIRE(snd_max_chunk > 0, NULL);

    struct neo4j_chunking_iostream *ios = malloc(
            sizeof(struct neo4j_chunking_iostream));
    if (ios == NULL)
    {
        return NULL;
    }

    uint8_t *snd_buffer = NULL;
    if (snd_min_chunk > 0)
    {
        snd_buffer = malloc(snd_min_chunk);
        if (snd_buffer == NULL)
        {
            int errsv = errno;
            free(ios);
            errno = errsv;
            return NULL;
        }
    }

    neo4j_iostream_t *iostream = neo4j_chunking_iostream_init(ios, delegate,
            snd_buffer, snd_min_chunk, snd_max_chunk);
    iostream->close = chunking_dealloc_close;
    return iostream;
}


neo4j_iostream_t *neo4j_chunking_iostream_init(
        struct neo4j_chunking_iostream *ios, neo4j_iostream_t *delegate,
        uint8_t *buffer, uint16_t bsize, uint16_t max_chunk)
{
    REQUIRE(ios != NULL, NULL);
    REQUIRE(delegate != NULL, NULL);
    REQUIRE(bsize == 0 || buffer != NULL, NULL);
    REQUIRE(max_chunk > 0, NULL);

    memset(ios, 0, sizeof(struct neo4j_chunking_iostream));
    ios->snd_buffer = buffer;
    ios->snd_buffer_size = bsize;
    ios->snd_buffer_used = 0;
    ios->snd_max_chunk = max_chunk;
    ios->delegate = delegate;

    neo4j_iostream_t *iostream = &(ios->_iostream);
    iostream->read = chunking_read;
    iostream->readv = chunking_readv;
    iostream->write = chunking_write;
    iostream->writev = chunking_writev;
    iostream->flush = chunking_flush;
    iostream->close = chunking_close;
    return iostream;
}


ssize_t chunking_read(neo4j_iostream_t *self, void *buf, size_t nbyte)
{
    REQUIRE(buf != NULL, -1);
    struct neo4j_chunking_iostream *ios = container_of(self,
            struct neo4j_chunking_iostream, _iostream);
    if (ios->delegate == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    if (nbyte == 0)
    {
        errno = ios->rcv_errno;
        return (ios->rcv_errno != 0)? -1 : 0;
    }

    struct iovec iov[2];
    uint16_t length;

    ssize_t received = 0;
    do
    {
        unsigned int iovcnt;
        size_t l;

        if (ios->rcv_chunk_remaining == 0)
        {
            iov[0].iov_base = &length;
            iov[0].iov_len = sizeof(length);
            iovcnt = 1;
            l = 0;
        }
        else if (nbyte < (size_t)ios->rcv_chunk_remaining)
        {
            iov[0].iov_base = buf;
            iov[0].iov_len = nbyte;
            iovcnt = 1;
            l = nbyte;
        }
        else
        {
            // since the chunk will be exhausted,
            // also read the next chunk length
            iov[0].iov_base = buf;
            iov[0].iov_len = (size_t)ios->rcv_chunk_remaining;
            iov[1].iov_base = &length;
            iov[1].iov_len = sizeof(length);
            iovcnt = 2;
            l = (size_t)ios->rcv_chunk_remaining;
        }

        size_t result;
        if (neo4j_ios_readv_all(ios->delegate, iov, iovcnt, &result) < 0)
        {
            // only report reading up to the next chunk length
            received += minzu(result, l);
            ios->rcv_errno = errno;
            ios->rcv_chunk_remaining = -1;
            return received;
        }

        if (result <= (size_t)ios->rcv_chunk_remaining)
        {
            received += result;
            ios->rcv_chunk_remaining -= result;
            return received;
        }

        // we received a complete chunk and the next chunk length
        received += ios->rcv_chunk_remaining;
        assert((result - ios->rcv_chunk_remaining) == sizeof(length));

        // if the next chunk length is zero, then we're done
        if (length == 0)
        {
            ios->rcv_chunk_remaining = -1;
            return received;
        }

        nbyte -= l;
        buf = ((uint8_t *)buf) + l;
        ios->rcv_chunk_remaining = ntohs(length);
    } while (nbyte > 0);

    return received;
}


ssize_t chunking_readv(neo4j_iostream_t *self,
        const struct iovec *iov, unsigned int iovcnt)
{
    if (iovcnt == 1)
    {
        return chunking_read(self, iov[0].iov_base, iov[0].iov_len);
    }
    else if (iovcnt > IOV_MAX-1)
    {
        iovcnt = IOV_MAX-1;
    }

    REQUIRE(iov != NULL, -1);
    struct neo4j_chunking_iostream *ios = container_of(self,
            struct neo4j_chunking_iostream, _iostream);
    if (ios->delegate == NULL)
    {
        errno = EPIPE;
        return -1;
    }

    if (ios->rcv_chunk_remaining < 0 || iovcnt == 0)
    {
        errno = ios->rcv_errno;
        return (ios->rcv_errno > 0)? -1 : 0;
    }

    // duplicate the iovector, as it will be modified
    ALLOC_IOVEC(diov, iovcnt);
    if (diov == NULL)
    {
        return -1;
    }
    memcpy(diov, iov, iovcnt * sizeof(struct iovec));

    ALLOC_IOVEC(riov, iovcnt+1);
    if (riov == NULL)
    {
        FREE_IOVEC(diov);
        return -1;
    }

    uint16_t length;
    ssize_t received = 0;
    do
    {
        // populate riov with enough to read whatever is left
        // for the current chunk
        unsigned int riovcnt = iov_limit(riov, diov, iovcnt,
                ios->rcv_chunk_remaining);
        size_t total = iovlen(riov, riovcnt);

        assert(total <= (size_t)ios->rcv_chunk_remaining);
        if (total == (size_t)ios->rcv_chunk_remaining)
        {
            // since the chunk will be exhausted,
            // also read the next chunk length
            assert(riovcnt < iovcnt + 1);
            riov[riovcnt].iov_base = &length;
            riov[riovcnt].iov_len = sizeof(length);
            riovcnt++;
        }

        size_t result;
        if (neo4j_ios_readv_all(ios->delegate, riov, riovcnt, &result) < 0)
        {
            // only report reading up to the next chunk length
            received += minzu(result, total);
            ios->rcv_errno = errno;
            ios->rcv_chunk_remaining = -1;
            goto cleanup;
        }

        if (result <= (size_t)ios->rcv_chunk_remaining)
        {
            received += result;
            ios->rcv_chunk_remaining -= result;
            goto cleanup;
        }

        // we received a complete chunk and the next chunk length
        received += ios->rcv_chunk_remaining;
        iovcnt = iov_skip(diov, diov, iovcnt, ios->rcv_chunk_remaining);
        assert((result - ios->rcv_chunk_remaining) == sizeof(length));

        // if the next chunk length is zero, then we're done
        if (length == 0)
        {
            ios->rcv_chunk_remaining = -1;
            goto cleanup;
        }

        ios->rcv_chunk_remaining = ntohs(length);
    } while (iovcnt > 0);

    int errsv;
cleanup:
    errsv = errno;
    FREE_IOVEC(diov);
    FREE_IOVEC(riov);
    errno = errsv;
    return received;
}


ssize_t chunking_write(neo4j_iostream_t *self, const void *buf, size_t nbyte)
{
    REQUIRE(buf != NULL, -1);
    REQUIRE(nbyte > 0, -1);

    struct iovec iov[1];
    iov[0].iov_base = (void *)(uintptr_t)buf;
    iov[0].iov_len = nbyte;
    return chunking_writev(self, iov, 1);
}


ssize_t chunking_writev(neo4j_iostream_t *self,
        const struct iovec *iov, unsigned int iovcnt)
{
    REQUIRE(iov != NULL, -1);
    struct neo4j_chunking_iostream *ios = container_of(self,
            struct neo4j_chunking_iostream, _iostream);
    if (ios->delegate == NULL)
    {
        errno = EPIPE;
        return -1;
    }

    // determine number of iovectors needed to write all data and
    // chunk length markers, accounting for data already buffered
    size_t nbytes = ios->snd_buffer_used;
    unsigned int nchunks;
    unsigned int niovcnt;

    for (;;)
    {
        niovcnt = chunk_iovec(iov, iovcnt, ios->snd_max_chunk,
                &nbytes, &nchunks);
        if (niovcnt <= IOV_MAX)
        {
            break;
        }
        --iovcnt;
    }

    if (nbytes == 0)
    {
        return 0;
    }

    // buffer less than snd_buffer_size data instead
    if (nbytes < ios->snd_buffer_size)
    {
        size_t result = memcpy_from_iov(ios->snd_buffer + ios->snd_buffer_used,
                USHRT_MAX - ios->snd_buffer_used, iov, iovcnt);
        assert(result > 0);
        ios->snd_buffer_used += result;
        return result;
    }

    // create a new iovec and populate
    // note: every chunk but the last must be snd_max_chunk in size, thus
    // we can reuse the 2 byte length in several iovectors
    ALLOC_IOVEC(diov, niovcnt);
    if (diov == NULL)
    {
        return -1;
    }
    uint16_t full_chunk_len = htons(ios->snd_max_chunk);
    uint16_t tail_len = ((nbytes - 1) % ios->snd_max_chunk) + 1;
    uint16_t tail_chunk_len = htons(tail_len);
    unsigned int chunk = 0;
    unsigned int cbytes = 0;

    diov[0].iov_base = (chunk < nchunks-1)? &full_chunk_len : &tail_chunk_len;
    diov[0].iov_len = sizeof(uint16_t);
    unsigned int diovcnt = 1;
    unsigned int tail_chunk_voff = niovcnt;

    if (ios->snd_buffer_used > 0)
    {
        diov[diovcnt].iov_base = ios->snd_buffer;
        diov[diovcnt].iov_len = ios->snd_buffer_used;
        diovcnt++;
        cbytes += ios->snd_buffer_used;
    }

    for (unsigned int i = 0; i < iovcnt; ++i)
    {
        uint8_t *base = (uint8_t *)(iov[i].iov_base);
        size_t iov_len = iov[i].iov_len;
        do
        {
            assert(diovcnt < niovcnt);
            if (cbytes == ios->snd_max_chunk)
            {
                chunk++;
                if ((chunk == nchunks-1) && (tail_len < ios->snd_buffer_size))
                {
                    assert(tail_chunk_voff == niovcnt);
                    tail_chunk_voff = diovcnt;
                }
                assert(chunk < nchunks);
                diov[diovcnt].iov_base =
                    (chunk < nchunks-1)? &full_chunk_len : &tail_chunk_len;
                diov[diovcnt].iov_len = sizeof(uint16_t);
                diovcnt++;
                assert(diovcnt < niovcnt);
                cbytes = 0;
            }
            size_t diov_len = minzu(ios->snd_max_chunk - cbytes, iov_len);
            diov[diovcnt].iov_base = base;
            diov[diovcnt].iov_len = diov_len;
            diovcnt++;
            base += diov_len;
            iov_len -= diov_len;
            cbytes += diov_len;
        } while (iov_len > 0);
    }

    // do the write
    size_t written;
    if (neo4j_ios_writev_all(ios->delegate, diov, tail_chunk_voff, &written))
    {
        FREE_IOVEC(diov);
        return -1;
    }
    ios->data_sent = true;

    unsigned int chunks_written =
        (tail_len < ios->snd_buffer_size)? nchunks-1 : nchunks;

    assert((size_t)written > (size_t)chunks_written * sizeof(uint16_t));
    written -= (size_t)chunks_written * sizeof(uint16_t);
    assert((size_t)written > (size_t)ios->snd_buffer_used);
    written -= ios->snd_buffer_used;
    nbytes -= ios->snd_buffer_used;
    ios->snd_buffer_used = 0;

    if (tail_len < ios->snd_buffer_size)
    {
        // last chunk is small enough to be buffered
        assert((tail_chunk_voff + 1) < niovcnt);
        ssize_t result = memcpy_from_iov(ios->snd_buffer, USHRT_MAX,
                &(diov[tail_chunk_voff + 1]), niovcnt - tail_chunk_voff - 1);
        assert(result >= 0);
        ios->snd_buffer_used = result;
        written += result;
    }
    assert((size_t)written == nbytes);
    FREE_IOVEC(diov);
    return written;
}


int chunking_flush(neo4j_iostream_t *self)
{
    struct neo4j_chunking_iostream *ios = container_of(self,
            struct neo4j_chunking_iostream, _iostream);
    if (ios->delegate == NULL)
    {
        errno = EPIPE;
        return -1;
    }

    return neo4j_ios_flush(ios->delegate);
}


int chunking_close(neo4j_iostream_t *self)
{
    struct neo4j_chunking_iostream *ios = container_of(self,
            struct neo4j_chunking_iostream, _iostream);
    if (ios->delegate == NULL)
    {
        errno = EPIPE;
        return -1;
    }

    if (!ios->data_sent && ios->snd_buffer_used == 0)
    {
        return 0;
    }

    struct iovec iov[3];
    unsigned int iovcnt = 0;

    uint16_t nsize;
    if (ios->snd_buffer_used > 0)
    {
        nsize = htons(ios->snd_buffer_used);
        iov[0].iov_base = &nsize;
        iov[0].iov_len = sizeof(uint16_t);
        iov[1].iov_base = ios->snd_buffer;
        iov[1].iov_len = ios->snd_buffer_used;
        iovcnt = 2;
    }

    uint16_t end = 0;
    iov[iovcnt].iov_base = &end;
    iov[iovcnt].iov_len = sizeof(uint16_t);
    iovcnt++;

    int result = neo4j_ios_writev_all(ios->delegate, iov, iovcnt, NULL);
    if (neo4j_ios_flush(ios->delegate) && result == 0)
    {
        result = -1;
    }
    ios->snd_buffer = NULL;
    ios->data_sent = false;
    ios->delegate = NULL;
    return result;
}


int chunking_dealloc_close(neo4j_iostream_t *self)
{
    struct neo4j_chunking_iostream *ios = container_of(self,
            struct neo4j_chunking_iostream, _iostream);
    uint8_t *snd_buffer = ios->snd_buffer;
    int result = chunking_close(self);
    int errsv = errno;
    free(snd_buffer);
    free(ios);
    errno = errsv;
    return result;
}


unsigned int chunk_iovec(const struct iovec *iov, unsigned int iovcnt,
        uint16_t max_chunk, size_t *nbytes, unsigned int *nchunks)
{
    *nchunks = 0;
    unsigned int niovcnt = 0;
    unsigned int cbytes = 0;

    if (*nbytes > 0)
    {
        assert(*nbytes < max_chunk);
        niovcnt = 1;
        cbytes += *nbytes;
    }

    for (unsigned int i = 0; i < iovcnt; ++i)
    {
        assert(cbytes <= max_chunk);

        if (cbytes < max_chunk)
        {
            // hav to use some of iov[i] to fill the chunk
            niovcnt++;
        }

        // add cbytes after div to avoid overflow
        lldiv_t div = lldiv(iov[i].iov_len - 1, max_chunk);
        div.rem += cbytes;
        if (div.rem >= max_chunk)
        {
            div.quot++;
            div.rem -= max_chunk;
        }
        div.rem += 1;

        cbytes = div.rem;
        *nchunks += div.quot;
        niovcnt += div.quot * 2;
        *nbytes += iov[i].iov_len;
    }

    if (cbytes > 0)
    {
        (*nchunks)++;
        niovcnt++;
    }

    return niovcnt;
}
