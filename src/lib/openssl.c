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
#include "openssl.h"
#include "errno.h"
#include "logging.h"
#include "thread.h"
#include "tofu.h"
#include "util.h"
#include <assert.h>
#include <openssl/err.h>

static neo4j_mutex_t *thread_locks;

static void locking_callback(int mode, int type, const char *file, int line);
static SSL_CTX *new_ctx(const neo4j_config_t *config, neo4j_logger_t *logger);
static int load_private_key(SSL_CTX *ctx, const neo4j_config_t *config,
        neo4j_logger_t *logger);
static int pem_pw_callback(char *buf, int size, int rwflag, void *userdata);
static int load_certificate_authorities(SSL_CTX *ctx,
        const neo4j_config_t *config, neo4j_logger_t *logger);
static int verify(SSL *ssl, const char *hostname, int port,
        const neo4j_config_t *config, uint_fast32_t flags,
        neo4j_logger_t *logger);
static int cert_fingerprint(X509* cert, char *buf, size_t n,
        neo4j_logger_t *logger);
static int sha512_digest(unsigned char *buf, unsigned int *np,
        const void *s, size_t n, neo4j_logger_t *logger);
static int openssl_error(neo4j_logger_t *logger, uint_fast8_t level,
        const char *file, unsigned int line);


int neo4j_openssl_init(void)
{
    SSL_library_init();
    SSL_load_error_strings();
    ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();

    int num_locks = CRYPTO_num_locks();
    thread_locks = calloc(num_locks, sizeof(neo4j_mutex_t));
    if (thread_locks == NULL)
    {
        return -1;
    }

    for (int i = 0; i < num_locks; i++)
    {
        int err = neo4j_mutex_init(&(thread_locks[i]));
        if (err)
        {
            for (; i > 0; --i)
            {
                neo4j_mutex_destroy(&(thread_locks[i-1]));
            }
            free(thread_locks);
            errno = err;
            return -1;
        }
    }

    if (CRYPTO_get_locking_callback() == NULL)
    {
        CRYPTO_set_locking_callback(locking_callback);
        CRYPTO_set_id_callback(neo4j_current_thread_id);
    }

    SSL_CTX *ctx = SSL_CTX_new(TLSv1_method());
    if (ctx == NULL)
    {
        errno = openssl_error(NULL, NEO4J_LOG_ERROR, __FILE__, __LINE__);
        return -1;
    }
    SSL_CTX_free(ctx);

    return 0;
}


int neo4j_openssl_cleanup(void)
{
    if (CRYPTO_get_locking_callback() == locking_callback)
    {
        CRYPTO_set_locking_callback(NULL);
        CRYPTO_set_id_callback(NULL);
    }

    int num_locks = CRYPTO_num_locks();
    for (int i = 0; i < num_locks; i++)
    {
        neo4j_mutex_destroy(&(thread_locks[i]));
    }
    free(thread_locks);

    EVP_cleanup();
    return 0;
}


void locking_callback(int mode, int type, const char *file, int line)
{
    if (mode & CRYPTO_LOCK)
    {
        neo4j_mutex_lock(&thread_locks[type]);
    }
    else
    {
        neo4j_mutex_unlock(&thread_locks[type]);
    }
}


BIO *neo4j_openssl_new_bio(BIO *delegate, const char *hostname, int port,
        const neo4j_config_t *config, uint_fast32_t flags)
{
    neo4j_logger_t *logger = neo4j_get_logger(config, "tls");

    SSL_CTX *ctx = new_ctx(config, logger);
    if (ctx == NULL)
    {
        neo4j_logger_release(logger);
        return NULL;
    }

    BIO *ssl_bio = BIO_new_ssl(ctx, 1);
    if (ssl_bio == NULL)
    {
        errno = openssl_error(logger, NEO4J_LOG_ERROR, __FILE__, __LINE__);
        SSL_CTX_free(ctx);
        goto failure;
    }

    SSL_CTX_free(ctx);

    BIO_push(ssl_bio, delegate);
    if (BIO_set_close(ssl_bio, BIO_CLOSE) != 1)
    {
        errno = openssl_error(logger, NEO4J_LOG_ERROR, __FILE__, __LINE__);
        goto failure;
    }

    int result = BIO_do_handshake(ssl_bio);
    if (result != 1)
    {
        if (result == 0)
        {
            errno = NEO4J_NO_SERVER_TLS_SUPPORT;
            goto failure;
        }
        errno = openssl_error(logger, NEO4J_LOG_ERROR, __FILE__, __LINE__);
        goto failure;
    }

    SSL *ssl = NULL;
    BIO_get_ssl(ssl_bio, &ssl);
    assert(ssl != NULL);
    if (verify(ssl, hostname, port, config, flags, logger))
    {
        goto failure;
    }

    neo4j_logger_release(logger);
    return ssl_bio;

    int errsv;
failure:
    errsv = errno;
    BIO_free(ssl_bio);
    neo4j_logger_release(logger);
    errno = errsv;
    return NULL;
}


SSL_CTX *new_ctx(const neo4j_config_t *config, neo4j_logger_t *logger)
{
    SSL_CTX *ctx = SSL_CTX_new(TLSv1_method());
    if (ctx == NULL)
    {
        errno = openssl_error(logger, NEO4J_LOG_ERROR, __FILE__, __LINE__);
        return NULL;
    }

    if (SSL_CTX_set_cipher_list(ctx, "HIGH:!EXPORT:!aNULL@STRENGTH") != 1)
    {
        errno = openssl_error(logger, NEO4J_LOG_ERROR, __FILE__, __LINE__);
        goto failure;
    }

    // Necessary when using blocking sockets
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);

    // Caching should be done at the protocol layer anyway
    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);

    if (load_private_key(ctx, config, logger))
    {
        goto failure;
    }

    if (load_certificate_authorities(ctx, config, logger))
    {
        goto failure;
    }

    return ctx;

    int errsv;
failure:
    errsv = errno;
    SSL_CTX_free(ctx);
    errno = errsv;
    return NULL;
}


int load_private_key(SSL_CTX *ctx, const neo4j_config_t *config,
        neo4j_logger_t *logger)
{
    const char *private_key = config->tls_private_key_file;
    if (private_key == NULL)
    {
        return 0;
    }

    if (SSL_CTX_use_certificate_chain_file(ctx, private_key) != 1)
    {
        errno = openssl_error(logger, NEO4J_LOG_ERROR, __FILE__, __LINE__);
        return -1;
    }

    if (config->tls_pem_pw_callback != NULL)
    {
        SSL_CTX_set_default_passwd_cb_userdata(ctx, (void *)(intptr_t)config);
        SSL_CTX_set_default_passwd_cb(ctx, pem_pw_callback);
    }

    return 0;
}


int pem_pw_callback(char *buf, int size, int rwflag, void *userdata)
{
    const neo4j_config_t *config = (const neo4j_config_t *)userdata;
    if (config->tls_pem_pw_callback != NULL)
    {
        return 0;
    }
    return config->tls_pem_pw_callback(config->tls_pem_pw_callback_userdata,
            buf, size);
}


int load_certificate_authorities(SSL_CTX *ctx, const neo4j_config_t *config,
        neo4j_logger_t *logger)
{
    const char *ca_file = config->tls_ca_file;
    const char *ca_dir = config->tls_ca_dir;

    if (ca_file == NULL && ca_dir == NULL)
    {
        return 0;
    }

    if (SSL_CTX_load_verify_locations(ctx, ca_file, ca_dir) != 1)
    {
        errno = openssl_error(logger, NEO4J_LOG_ERROR, __FILE__, __LINE__);
        return -1;
    }

    return 0;
}


int verify(SSL *ssl, const char *hostname, int port,
        const neo4j_config_t *config, uint_fast32_t flags,
        neo4j_logger_t *logger)
{
    X509 *cert = SSL_get_peer_certificate(ssl);
    if (cert == NULL)
    {
        errno = NEO4J_TLS_VERIFICATION_FAILED;
        return -1;
    }

    int result = -1;

    char fingerprint[SHA512_DIGEST_LENGTH * 2];
    if (cert_fingerprint(cert, fingerprint, sizeof(fingerprint), logger))
    {
        goto cleanup;
    }

    neo4j_log_debug(logger, "server cert fingerprint: %s", fingerprint);

    long verification = SSL_get_verify_result(ssl);
    switch (verification)
    {
    case X509_V_OK:
        // TODO: verify that the certificate matches hostname/port
        neo4j_log_debug(logger, "certificate verified using CA");
        result = 0;
        goto cleanup;
    // TODO: check other verification codes for unacceptable certificates
    default:
        break;
    }

    if (!(config->trust_known))
    {
        errno = NEO4J_TLS_VERIFICATION_FAILED;
        goto cleanup;
    }

    result = neo4j_check_known_hosts(hostname, port, fingerprint, config, flags);
    if (result > 0)
    {
        errno = NEO4J_TLS_VERIFICATION_FAILED;
        result = -1;
    }

cleanup:
    X509_free(cert);
    return result;
}


int cert_fingerprint(X509* cert, char *buf, size_t n, neo4j_logger_t *logger)
{
    unsigned char *der = NULL;
    int derlen = i2d_X509(cert, &der);
    if (derlen < 0)
    {
        errno = openssl_error(logger, NEO4J_LOG_ERROR, __FILE__, __LINE__);
        return -1;
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int dlen;
    if (sha512_digest(digest, &dlen, der, derlen, logger))
    {
        free(der);
        return -1;
    }
    assert(dlen <= EVP_MAX_MD_SIZE);

    size_t c = 0;
    for (unsigned int i = 0; i < dlen && c < n; i++)
    {
        snprintf(buf + c, n - c, "%02x", digest[i]);
        c += 2;
    }

    return 0;
}


int sha512_digest(unsigned char *buf, unsigned int *np, const void *s, size_t n,
        neo4j_logger_t *logger)
{
    const EVP_MD *md = EVP_get_digestbyname("SHA512");
    assert(md != NULL);
    if (md == NULL)
    {
        neo4j_log_error(logger, "OpenSSL failed to load digest SHA512");
        errno = NEO4J_UNEXPECTED_ERROR;
        return -1;
    }

    EVP_MD_CTX *mdctx = EVP_MD_CTX_create();
    assert(mdctx != NULL);
    if (mdctx == NULL)
    {
        neo4j_log_error(logger, "OpenSSL `EVP_MD_CTX_create` failed");
        errno = NEO4J_UNEXPECTED_ERROR;
        return -1;
    }
    if (EVP_DigestInit_ex(mdctx, md, NULL) != 1)
    {
        neo4j_log_error(logger, "OpenSSL `EVP_DigestInit_ex` failed");
        errno = NEO4J_UNEXPECTED_ERROR;
        goto failure;
    }
    if (EVP_DigestUpdate(mdctx, s, n) != 1)
    {
        neo4j_log_error(logger, "OpenSSL `EVP_DigestUpdate` failed");
        errno = NEO4J_UNEXPECTED_ERROR;
        goto failure;
    }
    if (EVP_DigestFinal_ex(mdctx, buf, np) != 1)
    {
        neo4j_log_error(logger, "OpenSSL `EVP_DigestFinal` failed");
        errno = NEO4J_UNEXPECTED_ERROR;
        goto failure;
    }
    EVP_MD_CTX_destroy(mdctx);
    return 0;

    int errsv;
failure:
    errsv = errno;
    EVP_MD_CTX_destroy(mdctx);
    errno = errsv;
    return -1;
}


int openssl_error(neo4j_logger_t *logger, uint_fast8_t level,
        const char *file, unsigned int line)
{
    unsigned long code = ERR_get_error();
    if (code == 0)
    {
        neo4j_log_error(logger, "OpenSSL error not available (%s:%d)",
                file, line);
        return NEO4J_UNEXPECTED_ERROR;
    }

    if (ERR_get_error() != 0)
    {
        neo4j_log_error(logger, "OpenSSL error stack too deep (%s:%d)",
                file, line);
        return NEO4J_UNEXPECTED_ERROR;
    }

    char ebuf[256];
    ERR_error_string_n(code, ebuf, sizeof(ebuf));
    neo4j_log(logger, level, "OpenSSL error: %lu:%s:%s:%s", code,
            ERR_lib_error_string(code),
            ERR_func_error_string(code),
            ERR_reason_error_string(code));

    return NEO4J_UNEXPECTED_ERROR;
}
