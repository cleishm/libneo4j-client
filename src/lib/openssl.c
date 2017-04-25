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
#include "openssl_iostream.h"
#include "thread.h"
#include "tofu.h"
#include "util.h"
#include <assert.h>
// FIXME: openssl 1.1.0-pre4 has issues with cast-qual
// (and perhaps earlier versions?)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#include <openssl/x509v3.h>
#include <openssl/err.h>
#pragma GCC diagnostic pop

#define NEO4J_CYPHER_LIST "HIGH:!EXPORT:!aNULL@STRENGTH"

static neo4j_mutex_t *thread_locks;

#ifndef HAVE_ASN1_STRING_GET0_DATA
#define ASN1_STRING_get0_data(x) ASN1_STRING_data(x)
#endif

#ifdef HAVE_CRYPTO_SET_LOCKING_CALLBACK
static void locking_callback(int mode, int type, const char *file, int line);
#endif
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
static int verify_hostname(X509* cert, const char *hostname,
        neo4j_logger_t *logger);
static int check_subject_alt_name(X509* cert, const char *hostname,
        neo4j_logger_t *logger);
static int check_common_name(X509 *cert, const char *hostname,
        neo4j_logger_t *logger);
static int openssl_error(neo4j_logger_t *logger, uint_fast8_t level,
        const char *file, unsigned int line);


int neo4j_openssl_init(void)
{
    SSL_library_init();
    SSL_load_error_strings();
    ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();

    if (neo4j_openssl_iostream_init())
    {
        return -1;
    }

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

#ifdef HAVE_CRYPTO_SET_LOCKING_CALLBACK
    if (CRYPTO_get_locking_callback() == NULL)
    {
        CRYPTO_set_locking_callback(locking_callback);
        CRYPTO_set_id_callback(neo4j_current_thread_id);
    }
#endif

    SSL_CTX *ctx = SSL_CTX_new(SSLv23_method());
    if (ctx == NULL)
    {
        errno = openssl_error(NULL, NEO4J_LOG_ERROR, __FILE__, __LINE__);
        return -1;
    }
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
    SSL_CTX_free(ctx);

    return 0;
}


int neo4j_openssl_cleanup(void)
{
#ifdef HAVE_CRYPTO_SET_LOCKING_CALLBACK
    if (CRYPTO_get_locking_callback() == locking_callback)
    {
        CRYPTO_set_locking_callback(NULL);
        CRYPTO_set_id_callback(NULL);
    }
#endif

    int num_locks = CRYPTO_num_locks();
    for (int i = 0; i < num_locks; i++)
    {
        neo4j_mutex_destroy(&(thread_locks[i]));
    }
    free(thread_locks);

    neo4j_openssl_iostream_cleanup();
    EVP_cleanup();
    return 0;
}


#ifdef HAVE_CRYPTO_SET_LOCKING_CALLBACK
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
#endif


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
#if OPENSSL_VERSION_NUMBER < 0x10100004
        if (result == 0)
#else
        // FIXME: when no handshake, openssl 1.1.0-pre4 reports result == -1
        // but error == 0 (and perhaps earlier versions?)
        if (result == 0 || ERR_peek_error() == 0)
#endif
        {
            ERR_get_error(); // pop the error
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
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_method());
    if (ctx == NULL)
    {
        errno = openssl_error(logger, NEO4J_LOG_ERROR, __FILE__, __LINE__);
        goto failure;
    }
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

    if (SSL_CTX_set_cipher_list(ctx, NEO4J_CYPHER_LIST) != 1)
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
        neo4j_log_error(logger, "Server did not present a TLS certificate");
        errno = NEO4J_TLS_VERIFICATION_FAILED;
        return -1;
    }

    int result = -1;

    char fingerprint[SHA512_DIGEST_LENGTH * 2 + 1];
    if (cert_fingerprint(cert, fingerprint, sizeof(fingerprint), logger))
    {
        goto cleanup;
    }

    neo4j_log_debug(logger, "server cert fingerprint: %s", fingerprint);

    long verification = SSL_get_verify_result(ssl);
    const char *verification_msg = X509_verify_cert_error_string(verification);
    switch (verification)
    {
    case X509_V_OK:
        result = verify_hostname(cert, hostname, logger);
        if (result < 0)
        {
            goto cleanup;
        }
        if (result == 0)
        {
            neo4j_log_debug(logger, "certificate verified using CA");
            break;
        }
        verification_msg = "certificate does not match hostname";
        // fall through
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
        if (!config->trust_known)
        {
            break;
        }
        neo4j_log_debug(logger, "TLS certificate verification failed: %s",
                verification_msg);
        // attempt use of TOFU
        result = neo4j_check_known_hosts(hostname, port,
                fingerprint, config, flags);
        if (result == 1)
        {
            neo4j_log_error(logger,
                    "Server fingerprint not in known hosts and "
                    "TLS certificate verification failed: %s",
                    verification_msg);
        }
        if (result > 0)
        {
            result = -1;
            errno = NEO4J_TLS_VERIFICATION_FAILED;
        }
        goto cleanup;
    case X509_V_ERR_OUT_OF_MEM:
        errno = ENOMEM;
        goto cleanup;
    default:
        break;
    }

    if (result)
    {
        neo4j_log_error(logger, "TLS certificate verification failed: %s",
                verification_msg);
        errno = NEO4J_TLS_VERIFICATION_FAILED;
    }

    int errsv;
cleanup:
    errsv = errno;
    X509_free(cert);
    errno = errsv;
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


int verify_hostname(X509* cert, const char *hostname,
        neo4j_logger_t *logger)
{
    int result = check_subject_alt_name(cert, hostname, logger);
    if (result <= 0)
    {
        return result;
    }
    return check_common_name(cert, hostname, logger);
}


int check_subject_alt_name(X509* cert, const char *hostname,
        neo4j_logger_t *logger)
{
    STACK_OF(GENERAL_NAME) *names = X509_get_ext_d2i(cert,
            NID_subject_alt_name, NULL, NULL);
    if (names == NULL)
    {
        return 1;
    }

    int result = 1;

    int nnames = sk_GENERAL_NAME_num(names);
    for (int i = 0; i < nnames; ++i)
    {
        const GENERAL_NAME *name = sk_GENERAL_NAME_value(names, i);
        if (name->type != GEN_DNS)
        {
            continue;
        }

        const char *name_str =
                (const char *)ASN1_STRING_get0_data(name->d.dNSName);
        // check that there isn't a null in the asn1 string
        if (strlen(name_str) != (size_t)ASN1_STRING_length(name->d.dNSName))
        {
            result = -1;
            errno = NEO4J_TLS_MALFORMED_CERTIFICATE;
            goto cleanup;
        }
        neo4j_log_trace(logger, "checking against certificate "
                "subject alt name '%s'", name_str);
        if (hostname_matches(hostname, name_str))
        {
            result = 0;
            goto cleanup;
        }
    }

cleanup:
    sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free);
    return result;
}


int check_common_name(X509 *cert, const char *hostname, neo4j_logger_t *logger)
{
    int i = X509_NAME_get_index_by_NID(X509_get_subject_name(cert), NID_commonName, -1);
    if (i < 0)
    {
        return -1;
    }
    X509_NAME_ENTRY *cn = X509_NAME_get_entry(X509_get_subject_name(cert), i);
    if (cn == NULL)
    {
        return -1;
    }
    ASN1_STRING *asn1 = X509_NAME_ENTRY_get_data(cn);
    if (asn1 == NULL)
    {
        return -1;
    }
    const char *cn_str = (const char *)ASN1_STRING_get0_data(asn1);
    // check that there isn't a null in the asn1 string
    if (strlen(cn_str) != (size_t)ASN1_STRING_length(asn1))
    {
        errno = NEO4J_TLS_MALFORMED_CERTIFICATE;
        return -1;
    }
    neo4j_log_trace(logger, "checking against certificate common name '%s'",
            cn_str);
    return hostname_matches(hostname, cn_str)? 0 : 1;
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
