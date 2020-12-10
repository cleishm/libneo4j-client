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
#ifndef NEO4J_OPENSSL_H
#define NEO4J_OPENSSL_H

#include "client_config.h"
// FIXME: openssl 1.1.0-pre4 has issues with cast-qual
// (and perhaps earlier versions?)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#include <openssl/ssl.h>
#pragma GCC diagnostic pop

/**
 * Initialize the OpenSSL library.
 *
 * Must be called at initialization. Not thread safe.
 *
 * @internal
 *
 * @return 0 on success, -1 on failure (errno will be set).
 */
__neo4j_must_check
int neo4j_openssl_init(void);

/**
 * Cleanup anything allocated by the OpenSSL library.
 *
 * Should be called before termination. Not thread safe.
 *
 * @internal
 *
 * @return 0 on success, -1 on failure (errno will be set).
 */
__neo4j_must_check
int neo4j_openssl_cleanup(void);

/**
 * Create a SSL BIO.
 *
 * @internal
 *
 * @param [delegate] A BIO for the cleartext stream.
 * @param [hostname] The hostname of the server.
 * @param [port] The port of the server.
 * @param [config] The client configuration.
 * @param [flags] A bitmask of flags to control connections.
 * @return An SSL BIO, or `NULL` on failure (errno will be set).
 */
__neo4j_must_check
BIO *neo4j_openssl_new_bio(BIO *delegate, const char *hostname, int port,
        const neo4j_config_t *config, uint_fast32_t flags);

#endif/*NEO4J_OPENSSL_H*/
