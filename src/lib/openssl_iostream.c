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
#include "openssl_iostream.h"
#include "openssl.h"
#include "util.h"
#include <assert.h>
#include <limits.h>
#include <openssl/bio.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>


struct openssl_iostream {
    neo4j_iostream_t _iostream;
    BIO *bio;
    neo4j_iostream_t *delegate;
};


static ssize_t openssl_read(neo4j_iostream_t *self, void *buf, size_t nbyte);
static ssize_t openssl_readv(neo4j_iostream_t *self,
        const struct iovec *iov, unsigned int iovcnt);
static ssize_t openssl_write(neo4j_iostream_t *self,
        const void *buf, size_t nbyte);
static ssize_t openssl_writev(neo4j_iostream_t *self,
        const struct iovec *iov, unsigned int iovcnt);
static int openssl_flush(neo4j_iostream_t *self);
static int openssl_close(neo4j_iostream_t *self);

static int iostream_bio_write(BIO *bio, const char *buf, int nbyte);
static int iostream_bio_read(BIO *bio, char *buf, int nbyte);
static int iostream_bio_puts(BIO *bio, const char *s);
static int iostream_bio_gets(BIO *bio, char *s, int len);
static long iostream_bio_ctrl(BIO *bio, int cmd, long num, void *ptr);
static int iostream_bio_create(BIO *bio);
static int iostream_bio_destroy(BIO *bio);

#ifndef HAVE_BIO_METH_NEW
static BIO_METHOD _iostream_bio_method = {
    BIO_TYPE_FILTER,
    "neo4j_openssl_iostream",
    iostream_bio_write,
    iostream_bio_read,
    iostream_bio_puts,
    iostream_bio_gets,
    iostream_bio_ctrl,
    iostream_bio_create,
    iostream_bio_destroy
};
static BIO_METHOD *iostream_bio_method = &_iostream_bio_method;
#define BIO_set_data(bio, value) (bio->ptr = (value))
#define BIO_get_data(bio) (bio->ptr)
#define BIO_set_init(bio, value) (bio->init = (value))
#define BIO_get_init(bio) (bio->init)
#else
static BIO_METHOD *iostream_bio_method = NULL;
#endif


int neo4j_openssl_iostream_init(void)
{
#ifdef HAVE_BIO_METH_NEW
    iostream_bio_method = BIO_meth_new(BIO_TYPE_FILTER,
            "neo4j_openssl_iostream");
    if (iostream_bio_method == NULL)
    {
        return -1;
    }
    BIO_meth_set_write(iostream_bio_method, iostream_bio_write);
    BIO_meth_set_read(iostream_bio_method, iostream_bio_read);
    BIO_meth_set_puts(iostream_bio_method, iostream_bio_puts);
    BIO_meth_set_gets(iostream_bio_method, iostream_bio_gets);
    BIO_meth_set_ctrl(iostream_bio_method, iostream_bio_ctrl);
    BIO_meth_set_create(iostream_bio_method, iostream_bio_create);
    BIO_meth_set_destroy(iostream_bio_method, iostream_bio_destroy);
#endif
    return 0;
}


void neo4j_openssl_iostream_cleanup(void)
{
#ifdef HAVE_BIO_METH_NEW
    BIO_meth_free(iostream_bio_method);
    iostream_bio_method = NULL;
#endif
}


neo4j_iostream_t *neo4j_openssl_iostream(neo4j_iostream_t *delegate,
        const char *hostname, int port,
        const neo4j_config_t *config, uint_fast32_t flags)
{
    REQUIRE(delegate != NULL, NULL);
    REQUIRE(hostname != NULL, NULL);
    REQUIRE(config != NULL, NULL);

    assert(iostream_bio_method != NULL);
    BIO *iostream_bio = BIO_new(iostream_bio_method);
    if (iostream_bio == NULL)
    {
        return NULL;
    }
    BIO_set_data(iostream_bio, delegate);

    struct openssl_iostream *ios = NULL;

    BIO *ssl_bio = neo4j_openssl_new_bio(iostream_bio, hostname, port,
            config, flags);
    if (ssl_bio == NULL)
    {
        goto failure;
    }

    ios = calloc(1, sizeof(struct openssl_iostream));
    if (ios == NULL)
    {
        goto failure;
    }

    ios->bio = ssl_bio;
    ios->delegate = delegate;
    neo4j_iostream_t *iostream = &(ios->_iostream);
    iostream->read = openssl_read;
    iostream->readv = openssl_readv;
    iostream->write = openssl_write;
    iostream->writev = openssl_writev;
    iostream->flush = openssl_flush;
    iostream->close = openssl_close;
    return iostream;

    int errsv;
failure:
    errsv = errno;
    if (ssl_bio != NULL)
    {
        BIO_free(ssl_bio);
    }
    BIO_free(iostream_bio);
    errno = errsv;
    return NULL;
}


ssize_t openssl_read(neo4j_iostream_t *self, void *buf, size_t nbyte)
{
    struct openssl_iostream *ios = container_of(self,
            struct openssl_iostream, _iostream);
    if (ios->bio == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    // TODO: check if BIO_read sets errno on error
    int len = (nbyte < INT_MAX)? nbyte : INT_MAX;
    return BIO_read(ios->bio, buf, len);
}


ssize_t openssl_readv(neo4j_iostream_t *self,
        const struct iovec *iov, unsigned int iovcnt)
{
    REQUIRE(iovcnt > 0 && iov[0].iov_len > 0, -1);
    struct openssl_iostream *ios = container_of(self,
            struct openssl_iostream, _iostream);
    if (ios->bio == NULL)
    {
        errno = EPIPE;
        return -1;
    }

    // TODO: check if BIO_read sets errno on error
    // TODO: if iovcnt > 1, this will always be a short read, so instead
    // consider reading entire vector
    int len = (iov[0].iov_len < INT_MAX)? iov[0].iov_len : INT_MAX;
    return BIO_read(ios->bio, iov[0].iov_base, len);
}


ssize_t openssl_write(neo4j_iostream_t *self, const void *buf, size_t nbyte)
{
    struct openssl_iostream *ios = container_of(self,
            struct openssl_iostream, _iostream);
    if (ios->bio == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    // TODO: check if BIO_write sets errno on error
    int len = (nbyte < INT_MAX)? nbyte : INT_MAX;
    return BIO_write(ios->bio, buf, len);
}


ssize_t openssl_writev(neo4j_iostream_t *self,
        const struct iovec *iov, unsigned int iovcnt)
{
    REQUIRE(iovcnt > 0 && iov[0].iov_len > 0, -1);
    struct openssl_iostream *ios = container_of(self,
            struct openssl_iostream, _iostream);
    if (ios->bio == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    // TODO: check if BIO_write sets errno on error
    // TODO: if iovcnt > 1, this will always be a short write, so instead
    // consider writing entire vector
    int len = (iov[0].iov_len < INT_MAX)? iov[0].iov_len : INT_MAX;
    return BIO_write(ios->bio, iov[0].iov_base, len);
}


int openssl_flush(neo4j_iostream_t *self)
{
    struct openssl_iostream *ios = container_of(self,
            struct openssl_iostream, _iostream);
    if (ios->bio == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    // TODO: check if BIO_flush sets errno on error
    return (BIO_flush(ios->bio) == 1)? 0 : -1;
}

int openssl_close(neo4j_iostream_t *self)
{
    struct openssl_iostream *ios = container_of(self,
            struct openssl_iostream, _iostream);
    if (ios->bio == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    BIO_free_all(ios->bio);
    ios->bio = NULL;
    neo4j_ios_close(ios->delegate);
    ios->delegate = NULL;
    free(ios);
    return 0;
}


int iostream_bio_write(BIO *bio, const char *buf, int nbyte)
{
    neo4j_iostream_t *ios = (neo4j_iostream_t *)BIO_get_data(bio);
    if (ios == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    ssize_t result = neo4j_ios_write(ios, buf, nbyte);
    assert(result < INT_MAX);
    return result;
}


int iostream_bio_read(BIO *bio, char *buf, int nbyte)
{
    neo4j_iostream_t *ios = (neo4j_iostream_t *)BIO_get_data(bio);
    if (ios == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    ssize_t result = neo4j_ios_read(ios, buf, nbyte);
    assert(result < INT_MAX);
    return result;
}


int iostream_bio_puts(BIO *bio, const char *s)
{
    neo4j_iostream_t *ios = (neo4j_iostream_t *)BIO_get_data(bio);
    if (ios == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    ssize_t result = neo4j_ios_write(ios, s, strlen(s));
    assert(result < INT_MAX);
    return result;
}


int iostream_bio_gets(BIO *bio, char *s, int len)
{
    errno = ENOTSUP;
    return -2;
}


long iostream_bio_ctrl(BIO *bio, int cmd, long num, void *ptr)
{
    switch (cmd)
    {
    case BIO_CTRL_FLUSH:
        return 1;
    default:
        return 0;
    }
}


int iostream_bio_create(BIO *bio)
{
    BIO_set_init(bio, 1);
    BIO_set_data(bio, NULL);
    return 1;
}


int iostream_bio_destroy(BIO *bio)
{
    neo4j_iostream_t *ios = (neo4j_iostream_t *)BIO_get_data(bio);
    if (ios == NULL)
    {
        errno = EPIPE;
        return -1;
    }
    BIO_set_data(bio, NULL);
    BIO_set_init(bio, 0);
    return 1;
}
