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
#ifndef NEO4J_CLIENT_CONFIG_H
#define NEO4J_CLIENT_CONFIG_H

#include "neo4j-client.h"
#include "memory.h"

struct neo4j_config
{
    struct neo4j_logger_provider *logger_provider;

    struct neo4j_connection_factory *connection_factory;
    struct neo4j_memory_allocator *allocator;
    unsigned int mpool_block_size;

    char *username;
    char *password;
    neo4j_basic_auth_callback_t basic_auth_callback;
    void *basic_auth_callback_userdata;

    const char *client_id;

    unsigned int so_rcvbuf_size;
    unsigned int so_sndbuf_size;
    time_t connect_timeout;

    size_t io_rcvbuf_size;
    size_t io_sndbuf_size;

    uint16_t snd_min_chunk_size;
    uint16_t snd_max_chunk_size;

    unsigned int session_request_queue_size;
    unsigned int max_pipelined_requests;

#ifdef HAVE_TLS
    char *tls_private_key_file;
    neo4j_password_callback_t tls_pem_pw_callback;
    void *tls_pem_pw_callback_userdata;
    char *tls_ca_file;
    char *tls_ca_dir;
#endif

    bool trust_known;
    char *known_hosts_file;

    neo4j_unverified_host_callback_t unverified_host_callback;
    void *unverified_host_callback_userdata;

    uint_fast32_t render_flags;
    unsigned int render_inspect_rows;
    const struct neo4j_results_table_colors *results_table_colors;
    const struct neo4j_plan_table_colors *plan_table_colors;
};


/**
 * Set the username in the config.
 *
 * Differs from `neo4j_config_set_username` as the username string
 * need not be null terminated as the length is supplied separately.
 *
 * @internal
 *
 * @param [config] The config to update.
 * @param [username] The string containing the username.
 * @param [n] The length of the string.
 * @return 0 on success, -1 on failure (errno will be set).
 */
int neo4j_config_nset_username(neo4j_config_t *config,
        const char *username, size_t n);


/**
 * Initialize a memory pool.
 *
 * @internal
 *
 * @param [config] The client configuration.
 * @return A memory pool.
 */
static inline neo4j_mpool_t neo4j_std_mpool(const neo4j_config_t *config)
{
    return neo4j_mpool(config->allocator, config->mpool_block_size);
}


/**
 * Ensure there are basic auth credentials.
 *
 * If a username and password is not configured in the client configuration,
 * attempts to obtain them via the basic_auth callback handler and otherwise
 * sets them to the empty string.
 *
 * @param [config] The client configuration.
 * @param [host] The host description.
 * @param 0 on success, -1 on error (errno will be set).
 */
int ensure_basic_auth_credentials(neo4j_config_t *config, const char *host);


#endif/*NEO4J_CLIENT_CONFIG_H*/
