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

#define NEO4J_DEFAULT_MPOOL_BLOCK_SIZE 128
#define NEO4J_DEFAULT_RCVBUF_SIZE 4096
#define NEO4J_DEFAULT_SNDBUF_SIZE 4096
#define NEO4J_DEFAULT_SESSION_REQUEST_QUEUE_SIZE 256
#define NEO4J_DEFAULT_MAX_PIPELINED_REQUESTS 10
#define NEO4J_DEFAULT_RENDER_INSPECT_ROWS 100


#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_COLOR_GREY "\x1b[38;5;238m"
#define ANSI_COLOR_BLUE "\x1b[38;5;75m"
#define ANSI_COLOR_BRIGHT "\x1b[38;5;15m"


static struct neo4j_results_table_colors _neo4j_results_table_no_colors =
    { .border = { "", "" },
      .header = { "", "" },
      .cells = { "", "" } };

static struct neo4j_results_table_colors _neo4j_results_table_ansi_colors =
    { .border = { ANSI_COLOR_GREY, ANSI_COLOR_RESET },
      .header = { ANSI_COLOR_BRIGHT, ANSI_COLOR_RESET },
      .cells = { "", "" } };

const struct neo4j_results_table_colors *neo4j_results_table_no_colors =
        &_neo4j_results_table_no_colors;
const struct neo4j_results_table_colors *neo4j_results_table_ansi_colors =
        &_neo4j_results_table_ansi_colors;


static struct neo4j_plan_table_colors _neo4j_plan_table_no_colors =
    { .border = { "", "" },
      .header = { "", "" },
      .cells = { "", "" },
      .graph = { "", "" } };

static struct neo4j_plan_table_colors _neo4j_plan_table_ansi_colors =
    { .border = { ANSI_COLOR_GREY, ANSI_COLOR_RESET },
      .header = { ANSI_COLOR_BRIGHT, ANSI_COLOR_RESET },
      .cells = { "", "" },
      .graph = { ANSI_COLOR_BLUE, ANSI_COLOR_RESET } };

const struct neo4j_plan_table_colors *neo4j_plan_table_no_colors =
        &_neo4j_plan_table_no_colors;
const struct neo4j_plan_table_colors *neo4j_plan_table_ansi_colors =
        &_neo4j_plan_table_ansi_colors;


static ssize_t default_password_callback(void *userdata, char *buf, size_t n);


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
    config->mpool_block_size = NEO4J_DEFAULT_MPOOL_BLOCK_SIZE;
    config->client_id = libneo4j_client_id();
    config->io_rcvbuf_size = NEO4J_DEFAULT_RCVBUF_SIZE;
    config->io_sndbuf_size = NEO4J_DEFAULT_SNDBUF_SIZE;
    config->snd_min_chunk_size = 1024;
    config->snd_max_chunk_size = UINT16_MAX;
    config->session_request_queue_size =
            NEO4J_DEFAULT_SESSION_REQUEST_QUEUE_SIZE;
    config->max_pipelined_requests = NEO4J_DEFAULT_MAX_PIPELINED_REQUESTS;
    config->trust_known = true;
    config->render_inspect_rows = NEO4J_DEFAULT_RENDER_INSPECT_ROWS;
    config->results_table_colors = neo4j_results_table_no_colors;
    config->plan_table_colors = neo4j_plan_table_no_colors;
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
    ignore_unused_result(neo4j_config_set_username(config, NULL));
    ignore_unused_result(neo4j_config_set_password(config, NULL));
#ifdef HAVE_TLS
    ignore_unused_result(neo4j_config_set_TLS_private_key(config, NULL));
    ignore_unused_result(neo4j_config_set_TLS_ca_file(config, NULL));
    ignore_unused_result(neo4j_config_set_TLS_ca_dir(config, NULL));
#endif
    ignore_unused_result(neo4j_config_set_known_hosts_file(config, NULL));
    free(config);
}


void neo4j_config_set_client_id(neo4j_config_t *config, const char *client_id)
{
    config->client_id = client_id;
}


const char *neo4j_config_get_client_id(const neo4j_config_t *config)
{
    return config->client_id;
}


int neo4j_config_set_username(neo4j_config_t *config, const char *username)
{
    REQUIRE(config != NULL, -1);
    return replace_strptr_dup(&(config->username), username);
}


const char *neo4j_config_get_username(const neo4j_config_t *config)
{
    return config->username;
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
    if (config->password != NULL)
    {
        size_t plen = strlen(config->password);
        memset_s(config->password, plen, 0, plen);
    }
    return replace_strptr_dup(&(config->password), password);
}


int neo4j_config_set_basic_auth_callback(neo4j_config_t *config,
        neo4j_basic_auth_callback_t callback, void *userdata)
{
    REQUIRE(config != NULL, -1);
    config->basic_auth_callback = callback;
    config->basic_auth_callback_userdata = userdata;
    return 0;
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

const char *neo4j_config_get_TLS_private_key(const neo4j_config_t *config)
{
    REQUIRE(config != NULL, NULL);
#ifdef HAVE_TLS
    return config->tls_private_key_file;
#else
    return NULL;
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


const char *neo4j_config_get_TLS_ca_file(const neo4j_config_t *config)
{
    REQUIRE(config != NULL, NULL);
#ifdef HAVE_TLS
    return config->tls_ca_file;
#else
    return NULL;
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


const char *neo4j_config_get_TLS_ca_dir(const neo4j_config_t *config)
{
    REQUIRE(config != NULL, NULL);
#ifdef HAVE_TLS
    return config->tls_ca_dir;
#else
    return NULL;
#endif
}


int neo4j_config_set_trust_known_hosts(neo4j_config_t *config, bool enable)
{
    REQUIRE(config != NULL, -1);
    config->trust_known = enable;
    return 0;
}


bool neo4j_config_get_trust_known_hosts(const neo4j_config_t *config)
{
    REQUIRE(config != NULL, -1);
    return config->trust_known;
}


int neo4j_config_set_known_hosts_file(neo4j_config_t *config,
        const char *path)
{
    REQUIRE(config != NULL, -1);
    return replace_strptr_dup(&(config->known_hosts_file), path);
}


const char *neo4j_config_get_known_hosts_file(const neo4j_config_t *config)
{
    REQUIRE(config != NULL, NULL);
    return config->known_hosts_file;
}


int neo4j_config_set_unverified_host_callback(neo4j_config_t *config,
        neo4j_unverified_host_callback_t callback, void *userdata)
{
    REQUIRE(config != NULL, -1);
    config->unverified_host_callback = callback;
    config->unverified_host_callback_userdata = userdata;
    return 0;
}


ssize_t default_password_callback(void *userdata, char *buf, size_t n)
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


size_t neo4j_config_get_sndbuf_size(const neo4j_config_t *config)
{
    REQUIRE(config != NULL, 0);
    return config->io_sndbuf_size;
}


int neo4j_config_set_rcvbuf_size(neo4j_config_t *config, size_t size)
{
    REQUIRE(config != NULL, -1);
    config->io_rcvbuf_size = size;
    return 0;
}


size_t neo4j_config_get_rcvbuf_size(const neo4j_config_t *config)
{
    REQUIRE(config != NULL, 0);
    return config->io_rcvbuf_size;
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


unsigned int neo4j_config_get_so_sndbuf_size(const neo4j_config_t *config)
{
    return config->so_sndbuf_size;
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


unsigned int neo4j_config_get_so_rcvbuf_size(const neo4j_config_t *config)
{
    return config->so_rcvbuf_size;
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


struct neo4j_memory_allocator *neo4j_config_get_memory_allocator(
        const neo4j_config_t *config)
{
    return config->allocator;
}


void neo4j_config_set_max_pipelined_requests(neo4j_config_t *config,
        unsigned int n)
{
    config->max_pipelined_requests = n;
}


unsigned int neo4j_config_get_max_pipelined_requests(
        const neo4j_config_t *config)
{
    return config->max_pipelined_requests;
}


int ensure_basic_auth_credentials(neo4j_config_t *config, const char *host)
{
    if (config->username != NULL && config->password != NULL)
    {
        return 0;
    }

    char username_buf[NEO4J_MAXUSERNAMELEN + 1];
    char password_buf[NEO4J_MAXPASSWORDLEN + 1];
    strncpy(username_buf, (config->username != NULL)? config->username : "",
            sizeof(username_buf) - 1);
    username_buf[sizeof(username_buf) - 1] = '\0';
    strncpy(password_buf, (config->password != NULL)? config->password : "",
            sizeof(password_buf) - 1);
    password_buf[sizeof(password_buf) - 1] = '\0';

    int err = -1;

    if (config->basic_auth_callback != NULL && config->basic_auth_callback(
            config->basic_auth_callback_userdata, host,
            username_buf, sizeof(username_buf),
            password_buf, sizeof(password_buf)))
    {
        goto cleanup;
    }

    if (neo4j_config_set_username(config, username_buf))
    {
        goto cleanup;
    }

    if (neo4j_config_set_password(config, password_buf))
    {
        goto cleanup;
    }

    err = 0;

    int errsv;
cleanup:
    errsv = errno;
    memset_s(username_buf, sizeof(username_buf), 0, sizeof(username_buf));
    memset_s(password_buf, sizeof(password_buf), 0, sizeof(password_buf));
    errno = errsv;
    return err;
}


void neo4j_config_set_render_nulls(neo4j_config_t *config, bool enable)
{
    if (enable)
    {
        config->render_flags |= NEO4J_RENDER_SHOW_NULLS;
    }
    else
    {
        config->render_flags &= ~NEO4J_RENDER_SHOW_NULLS;
    }
}


bool neo4j_config_get_render_nulls(const neo4j_config_t *config)
{
    return config->render_flags & NEO4J_RENDER_SHOW_NULLS;
}


void neo4j_config_set_render_quoted_strings(neo4j_config_t *config,
        bool enable)
{
    if (enable)
    {
        config->render_flags |= NEO4J_RENDER_QUOTE_STRINGS;
    }
    else
    {
        config->render_flags &= ~NEO4J_RENDER_QUOTE_STRINGS;
    }
}


bool neo4j_config_get_render_quoted_strings(const neo4j_config_t *config)
{
    return config->render_flags & NEO4J_RENDER_QUOTE_STRINGS;
}


void neo4j_config_set_render_ascii(neo4j_config_t *config, bool enable)
{
    if (enable)
    {
        config->render_flags |= NEO4J_RENDER_ASCII;
    }
    else
    {
        config->render_flags &= ~NEO4J_RENDER_ASCII;
    }
}


bool neo4j_config_get_render_ascii(const neo4j_config_t *config)
{
    return config->render_flags & NEO4J_RENDER_ASCII;
}


void neo4j_config_set_render_rowlines(neo4j_config_t *config, bool enable)
{
    if (enable)
    {
        config->render_flags |= NEO4J_RENDER_ROWLINES;
    }
    else
    {
        config->render_flags &= ~NEO4J_RENDER_ROWLINES;
    }
}


bool neo4j_config_get_render_rowlines(const neo4j_config_t *config)
{
    return config->render_flags & NEO4J_RENDER_ROWLINES;
}


void neo4j_config_set_render_wrapped_values(neo4j_config_t *config,
        bool enable)
{
    if (enable)
    {
        config->render_flags |= NEO4J_RENDER_WRAP_VALUES;
    }
    else
    {
        config->render_flags &= ~NEO4J_RENDER_WRAP_VALUES;
    }
}


bool neo4j_config_get_render_wrapped_values(const neo4j_config_t *config)
{
    return config->render_flags & NEO4J_RENDER_WRAP_VALUES;
}


void neo4j_config_set_render_wrap_markers(neo4j_config_t *config, bool enable)
{
    if (enable)
    {
        config->render_flags &= ~NEO4J_RENDER_NO_WRAP_MARKERS;
    }
    else
    {
        config->render_flags |= NEO4J_RENDER_NO_WRAP_MARKERS;
    }
}


bool neo4j_config_get_render_wrap_markers(const neo4j_config_t *config)
{
    return !(config->render_flags & NEO4J_RENDER_NO_WRAP_MARKERS);
}


void neo4j_config_set_render_inspect_rows(neo4j_config_t *config,
        unsigned int rows)
{
    config->render_inspect_rows = rows;
}


unsigned int neo4j_config_get_render_inspect_rows(const neo4j_config_t *config)
{
    return config->render_inspect_rows;
}


void neo4j_config_set_results_table_colors(neo4j_config_t *config,
        const struct neo4j_results_table_colors *colors)
{
    config->results_table_colors = colors;
}


const struct neo4j_results_table_colors *neo4j_config_get_results_table_colors(
        const neo4j_config_t *config)
{
    return config->results_table_colors;
}


void neo4j_config_set_plan_table_colors(neo4j_config_t *config,
        const struct neo4j_plan_table_colors *colors)
{
    config->plan_table_colors = colors;
}


const struct neo4j_plan_table_colors *neo4j_config_get_plan_table_colorization(
        const neo4j_config_t *config)
{
    return config->plan_table_colors;
}
