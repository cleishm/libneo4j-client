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
#include "client_config.h"
#include "memory.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>


static size_t default_password_callback(void *userdata, char *buf, size_t n);


const char *libneo4j_client_id(void)
{
    return PACKAGE_NAME "/" PACKAGE_VERSION;
}


const char *libneo4j_client_version(void)
{
    return PACKAGE_VERSION;
}


neo4j_config_t *neo4j_new_config()
{
    neo4j_config_t *config = calloc(1, sizeof(neo4j_config_t));
    if (config == NULL)
    {
        return NULL;
    }
    config->connection_factory = &neo4j_std_connection_factory;
    config->allocator = &neo4j_std_memory_allocator;
    config->mpool_block_size = 128;
    config->client_id = libneo4j_client_id();
    config->io_rcvbuf_size = 4096;
    config->io_sndbuf_size = 4096;
    config->snd_min_chunk_size = 1024;
    config->snd_max_chunk_size = UINT16_MAX;
    config->session_request_queue_size = 256;
    config->max_pipelined_requests = 10;
    config->trust_known = true;
    return config;
}


neo4j_config_t *neo4j_config_dup(const neo4j_config_t *config)
{
    if (config == NULL)
    {
        return neo4j_new_config();
    }
    neo4j_config_t *dup = malloc(sizeof(neo4j_config_t));
    if (config == NULL)
    {
        return NULL;
    }

    memcpy(dup, config, sizeof(neo4j_config_t));
    if (strdup_null(&(dup->username), config->username))
    {
        goto failure;
    }
    if (strdup_null(&(dup->password), config->password))
    {
        goto failure;
    }

#ifdef HAVE_TLS
    if (strdup_null(&(dup->tls_private_key_file), config->tls_private_key_file))
    {
        goto failure;
    }
    if (strdup_null(&(dup->tls_ca_file), config->tls_ca_file))
    {
        goto failure;
    }
    if (strdup_null(&(dup->tls_ca_dir), config->tls_ca_dir))
    {
        goto failure;
    }
#endif
    if (strdup_null(&(dup->known_hosts_file), config->known_hosts_file))
    {
        goto failure;
    }

    return dup;

    int errsv;
failure:
    errsv = errno;
    neo4j_config_free(dup);
    errno = errsv;
    return NULL;
}


void neo4j_config_free(neo4j_config_t *config)
{
    if (config == NULL)
    {
        return;
    }
    free(config->username);
    free(config->password);
#ifdef HAVE_TLS
    free(config->tls_private_key_file);
    free(config->tls_ca_file);
    free(config->tls_ca_dir);
#endif
    free(config->known_hosts_file);
    free(config);
}


void neo4j_config_set_client_id(neo4j_config_t *config, const char *client_id)
{
    config->client_id = client_id;
}


int neo4j_config_set_username(neo4j_config_t *config, const char *username)
{
    REQUIRE(config != NULL, -1);
    return replace_strptr_dup(&(config->username), username);
}


int neo4j_config_nset_username(neo4j_config_t *config,
        const char *username, size_t n)
{
    REQUIRE(config != NULL, -1);
    return replace_strptr_ndup(&(config->username), username, n);
}


int neo4j_config_set_password(neo4j_config_t *config, const char *password)
{
    REQUIRE(config != NULL, -1);
    return replace_strptr_dup(&(config->password), password);
}


int neo4j_config_set_TLS_private_key(neo4j_config_t *config, const char *path)
{
    REQUIRE(config != NULL, -1);
#ifdef HAVE_TLS
    return replace_strptr_dup(&(config->tls_private_key_file), path);
#else
    errno = NEO4J_TLS_NOT_SUPPORTED;
    return -1;
#endif
}


int neo4j_config_set_TLS_private_key_password_callback(neo4j_config_t *config,
        neo4j_password_callback_t callback, void *userdata)
{
    REQUIRE(config != NULL, -1);
#ifdef HAVE_TLS
    config->tls_pem_pw_callback = callback;
    config->tls_pem_pw_callback_userdata = userdata;
    return 0;
#else
    errno = NEO4J_TLS_NOT_SUPPORTED;
    return -1;
#endif
}


int neo4j_config_set_TLS_private_key_password(neo4j_config_t *config,
        const char *password)
{
    REQUIRE(config != NULL, -1);
    return neo4j_config_set_TLS_private_key_password_callback(config,
            default_password_callback, (void *)(intptr_t)password);
}


int neo4j_config_set_TLS_ca_file(neo4j_config_t *config, const char *path)
{
    REQUIRE(config != NULL, -1);
#ifdef HAVE_TLS
    return replace_strptr_dup(&(config->tls_ca_file), path);
#else
    errno = NEO4J_TLS_NOT_SUPPORTED;
    return -1;
#endif
}


int neo4j_config_set_TLS_ca_dir(neo4j_config_t *config, const char *path)
{
    REQUIRE(config != NULL, -1);
#ifdef HAVE_TLS
    return replace_strptr_dup(&(config->tls_ca_dir), path);
#else
    errno = NEO4J_TLS_NOT_SUPPORTED;
    return -1;
#endif
}


int neo4j_config_set_trust_known_hosts(neo4j_config_t *config, bool enable)
{
    REQUIRE(config != NULL, -1);
    config->trust_known = enable;
    return 0;
}


int neo4j_config_set_known_hosts_file(neo4j_config_t *config,
        const char *path)
{
    REQUIRE(config != NULL, -1);
    return replace_strptr_dup(&(config->known_hosts_file), path);
}

int neo4j_config_set_unverified_host_callback(neo4j_config_t *config,
        neo4j_unverified_host_callback_t callback, void *userdata)
{
    REQUIRE(config != NULL, -1);
    config->unverified_host_callback = callback;
    config->unverified_host_callback_userdata = userdata;
    return 0;
}


size_t default_password_callback(void *userdata, char *buf, size_t n)
{
    const char *password = (const char *)userdata;
    size_t pwlen = strlen(password);

    if (n < pwlen)
    {
        return 0;
    }

    memcpy(buf, password, pwlen);
    return pwlen;
}


int neo4j_config_set_sndbuf_size(neo4j_config_t *config, size_t size)
{
    REQUIRE(config != NULL, -1);
    config->io_sndbuf_size = size;
    return 0;
}


int neo4j_config_set_rcvbuf_size(neo4j_config_t *config, size_t size)
{
    REQUIRE(config != NULL, -1);
    config->io_rcvbuf_size = size;
    return 0;
}


void neo4j_config_set_logger_provider(neo4j_config_t *config,
        struct neo4j_logger_provider *logger_provider)
{
    config->logger_provider = logger_provider;
}


int neo4j_config_set_so_sndbuf_size(neo4j_config_t *config, unsigned int size)
{
    REQUIRE(config != NULL, -1);
    if (size > INT_MAX)
    {
        errno = ERANGE;
        return -1;
    }
    config->so_sndbuf_size = size;
    return 0;
}


int neo4j_config_set_so_rcvbuf_size(neo4j_config_t *config, unsigned int size)
{
    REQUIRE(config != NULL, -1);
    if (size > INT_MAX)
    {
        errno = ERANGE;
        return -1;
    }
    config->so_rcvbuf_size = size;
    return 0;
}


void neo4j_config_set_connection_factory(neo4j_config_t *config,
        struct neo4j_connection_factory *connection_factory)
{
    config->connection_factory = connection_factory;
}


void neo4j_config_set_memory_allocator(neo4j_config_t *config,
        struct neo4j_memory_allocator *allocator)
{
    config->allocator = allocator;
}
