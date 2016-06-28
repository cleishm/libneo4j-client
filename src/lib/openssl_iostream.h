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
#ifndef NEO4J_OPENSSL_IOSTREAM_H
#define NEO4J_OPENSSL_IOSTREAM_H

#include "neo4j-client.h"
#include "iostream.h"

/**
 * Initialize the openssl_iostream system.
 *
 * @internal
 *
 * @return 0 on success, -1 on failure (errno will be set).
 */
__neo4j_must_check
int neo4j_openssl_iostream_init(void);

/**
 * Cleanup the openssl_iostream system.
 */
void neo4j_openssl_iostream_cleanup(void);

/**
 * Create an iostream for an OpenSSL BIO.
 *
 * @internal
 *
 * @param [delegate] The iostream to establish SSL over.
 * @param [hostname] The hostname of the server the underlying iostream is
 *         connected to.
 * @param [port] The TCP port the underlying iostream is connected to.
 * @param [config] The neo4j client configuration in use for this connection.
 * @param [flags] A bitmask of flags for controling connections.
 * @return The SSL iostream, or `NULL` if an error occurred (errno will be set).
 */
__neo4j_must_check
neo4j_iostream_t *neo4j_openssl_iostream(neo4j_iostream_t *delegate,
        const char *hostname, int port,
        const neo4j_config_t *config, uint_fast32_t flags);

#endif/*NEO4J_OPENSSL_IOSTREAM_H*/
