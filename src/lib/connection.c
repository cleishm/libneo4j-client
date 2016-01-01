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
#include "connection.h"
#include "chunking_iostream.h"
#include "client_config.h"
#include "deserialization.h"
#include "logging.h"
#include "memory.h"
#include "network.h"
#ifdef HAVE_OPENSSL
#include "openssl_iostream.h"
#endif
#include "posix_iostream.h"
#include "serialization.h"
#include "util.h"
#include <assert.h>
#include <unistd.h>


static int add_userinfo_to_config(const char *userinfo, neo4j_config_t *config);
static neo4j_connection_t *establish_connection(const char *hostname, int port,
        const char *connection_name, neo4j_config_t *config,
        uint_fast32_t flags);
static neo4j_iostream_t *std_tcp_connect(
        struct neo4j_connection_factory *factory, const char *hostname,
        int port, neo4j_config_t *config, uint_fast32_t flags,
        struct neo4j_logger *logger);
static int negotiate_protocol_version(neo4j_iostream_t *iostream,
        uint32_t *protocol_version);
static int disconnect(neo4j_connection_t *connection);


struct neo4j_connection_factory neo4j_std_connection_factory =
{
    .tcp_connect = &std_tcp_connect
};


neo4j_connection_t *neo4j_connect(const char *uri_string,
        neo4j_config_t *config, uint_fast32_t flags)
{
    REQUIRE(uri_string != NULL, NULL);
    config = neo4j_config_dup(config);
    if (config == NULL)
    {
        return NULL;
    }

    neo4j_connection_t *connection = NULL;

    struct uri *uri = parse_uri(uri_string, NULL);
    if (uri == NULL)
    {
        if (errno == EINVAL)
        {
            errno = NEO4J_INVALID_URI;
        }
        goto cleanup;
    }

    if (uri->scheme == NULL ||
            (strcmp(uri->scheme, "neo4j") != 0 &&
             strcmp(uri->scheme, "bolt") != 0))
    {
        errno = NEO4J_UNKNOWN_URI_SCHEME;
        goto cleanup;
    }

    if (uri->userinfo != NULL)
    {
        if (add_userinfo_to_config(uri->userinfo, config))
        {
            goto cleanup;
        }
    }

    connection = establish_connection(
            uri->hostname, uri->port, uri_string, config, flags);

    int errsv;
cleanup:
    errsv = errno;
    if (uri != NULL)
    {
        free_uri(uri);
    }
    if (config != NULL && connection == NULL)
    {
        neo4j_config_free(config);
    }
    errno = errsv;
    return connection;
}


int add_userinfo_to_config(const char *userinfo, neo4j_config_t *config)
{
    size_t username_len = strcspn(userinfo, ":");
    if (*(userinfo + username_len) == '\0')
    {
        if (neo4j_config_set_username(config, userinfo))
        {
            return -1;
        }
    }
    else
    {
        char *username = strndup(userinfo, username_len);
        if (username == NULL)
        {
            return -1;
        }
        if (neo4j_config_set_username(config, username))
        {
            free(username);
            return -1;
        }
        free(username);
        if (neo4j_config_set_password(config, userinfo + username_len + 1))
        {
            return -1;
        }
    }
    return 0;
}


neo4j_connection_t *neo4j_tcp_connect(const char *hostname, unsigned int port,
        neo4j_config_t *config, uint_fast32_t flags)
{
    REQUIRE(hostname != NULL, NULL);

    config = neo4j_config_dup(config);
    if (config == NULL)
    {
        return NULL;
    }

    char namebuf[1024];
    if (snprintf(namebuf, sizeof(namebuf), "%s:%d", hostname, port) <= 0)
    {
        return NULL;
    }
    return establish_connection(hostname, port, namebuf, config, flags);
}


neo4j_connection_t *establish_connection(const char *hostname, int port,
        const char *connection_name, neo4j_config_t *config,
        uint_fast32_t flags)
{
    neo4j_logger_t *logger = neo4j_get_logger(config, "connection");

    neo4j_iostream_t *iostream = NULL;
    struct neo4j_request *request_queue = NULL;

    neo4j_connection_t *connection = calloc(1, sizeof(neo4j_connection_t));
    if (connection == NULL)
    {
        neo4j_log_error_errno(logger, "malloc of neo4j_connection_t failed");
        goto failure;
    }

    request_queue = calloc(config->session_request_queue_size,
            sizeof(struct neo4j_request));
    if (request_queue == NULL)
    {
        char ebuf[256];
        neo4j_log_error(logger, "malloc of request_queue[%d] failed: %s",
                config->session_request_queue_size,
                neo4j_strerror(errno, ebuf, sizeof(ebuf)));
        goto failure;
    }

    iostream = config->connection_factory->tcp_connect(
            config->connection_factory, hostname, port, config, flags, logger);
    if (iostream == NULL)
    {
        goto failure;
    }

    uint32_t protocol_version;
    if (negotiate_protocol_version(iostream, &protocol_version))
    {
        char ebuf[256];
        neo4j_log_error(logger,
                "failed to negotiate a protocol version with '%s': %s",
                connection_name, neo4j_strerror(errno, ebuf, sizeof(ebuf)));
        goto failure;
    }
    if (protocol_version != 1)
    {
        errno = NEO4J_PROTOCOL_NEGOTIATION_FAILED;
        neo4j_log_error(logger,
                "unable to agree on a protocol version with '%s'",
                connection_name);
        goto failure;
    }

    connection->config = config;
    connection->logger = logger;
    connection->iostream = iostream;
    connection->version = protocol_version;
#ifdef HAVE_TLS
    connection->insecure = flags & NEO4J_INSECURE;
#else
    connection->insecure = true;
#endif
    connection->request_queue = request_queue;

    neo4j_log_info(logger, "connected (%p) to '%s'%s", connection,
            connection_name, connection->insecure? " (insecure)" : "");
    neo4j_log_debug(logger, "connection %p using protocol version %d",
            connection, protocol_version);

    return connection;

    int errsv;
failure:
    errsv = errno;
    if (connection != NULL)
    {
        free(connection);
    }
    if (request_queue != NULL)
    {
        free(request_queue);
    }
    if (iostream != NULL)
    {
        neo4j_ios_close(iostream);
    }
    neo4j_logger_release(logger);
    errno = errsv;
    return NULL;
}


neo4j_iostream_t *std_tcp_connect(struct neo4j_connection_factory *factory,
        const char *hostname, int port,
        neo4j_config_t *config, uint_fast32_t flags,
        struct neo4j_logger *logger)
{
    REQUIRE(factory != NULL, NULL);
    REQUIRE(config != NULL, NULL);
    REQUIRE(hostname != NULL, NULL);
    REQUIRE(port >= 0, NULL);

    char servname[16];
    snprintf(servname, sizeof(servname), "%d", port);
    int fd = neo4j_connect_tcp_socket(hostname, servname, config, logger);
    if (fd < 0)
    {
        return NULL;
    }

    neo4j_log_trace(logger, "opened socket to %s [%d] (fd=%d)",
            hostname, port, fd);

    neo4j_iostream_t *ios = neo4j_posix_iostream(fd);
    if (ios == NULL)
    {
        goto failure;
    }

#ifdef HAVE_TLS
    if (!(flags & NEO4J_INSECURE))
    {
#ifdef HAVE_OPENSSL
        neo4j_log_trace(logger, "initialiting TLS (fd=%d)", fd);
        neo4j_iostream_t *tls_ios = neo4j_openssl_iostream(ios, hostname, port,
                config, flags);
        if (tls_ios == NULL)
        {
            goto failure;
        }
        ios = tls_ios;
#endif
    }
#endif

    return ios;

    int errsv;
failure:
    errsv = errno;
    if (ios != NULL)
    {
        neo4j_ios_close(ios);
    }
    close(fd);
    errno = errsv;
    return NULL;
}


int neo4j_close(neo4j_connection_t *connection)
{
    REQUIRE(connection != NULL, -1);
    REQUIRE(connection->config != NULL, -1);

    if (connection->session != NULL)
    {
        errno = NEO4J_SESSION_ACTIVE;
        return -1;
    }
    int result = disconnect(connection);
    int errsv = errno;
    if (result == 0)
    {
        neo4j_log_info(connection->logger, "disconnected %p", connection);
    }
    neo4j_logger_release(connection->logger);
    neo4j_config_free(connection->config);
    free(connection->request_queue);
    connection->logger = NULL;
    connection->config = NULL;
    connection->request_queue = NULL;
    connection->iostream = NULL;
    free(connection);
    errno = errsv;
    return result;
}


int disconnect(neo4j_connection_t *connection)
{
    assert(connection != NULL);
    if (connection->iostream == NULL)
    {
        return 0;
    }
    int result = neo4j_ios_close(connection->iostream);
    connection->iostream = NULL;
    return result;
}


int negotiate_protocol_version(neo4j_iostream_t *iostream,
        uint32_t *protocol_version)
{
    uint8_t hello[] = { 0x60, 0x60, 0xB0, 0x17 };
    if (neo4j_ios_write_all(iostream, hello,
                sizeof(hello), NULL) < 0)
    {
        return -1;
    }

    uint32_t supported_versions[4] = { htonl(1), 0, 0, 0 };
    if (neo4j_ios_write_all(iostream, supported_versions,
                sizeof(supported_versions), NULL) < 0)
    {
        return -1;
    }

    uint32_t agreed_version;
    ssize_t result = neo4j_ios_read_all(iostream, &agreed_version,
            sizeof(agreed_version), NULL);
    if (result < 0)
    {
        return -1;
    }

    *protocol_version = ntohl(agreed_version);
    return 0;
}


int neo4j_connection_send(neo4j_connection_t *connection,
        neo4j_message_type_t type, const neo4j_value_t *argv, uint16_t argc)
{
    REQUIRE(connection != NULL, -1);
    if (connection->iostream == NULL)
    {
        errno = NEO4J_CONNECTION_CLOSED;
        return -1;
    }

    const neo4j_config_t *config = connection->config;
    int res = neo4j_message_send(connection->iostream, type, argv, argc,
            config->snd_min_chunk_size, config->snd_max_chunk_size);
    if (res && errno != NEO4J_CONNECTION_CLOSED)
    {
        char ebuf[256];
        neo4j_log_error(connection->logger,
                "error sending message on %p: %s\n", connection,
                neo4j_strerror(errno, ebuf, sizeof(ebuf)));
    }
    return res;
}


int neo4j_connection_recv(neo4j_connection_t *connection, neo4j_mpool_t *mpool,
        neo4j_message_type_t *type, const neo4j_value_t **argv, uint16_t *argc)
{
    REQUIRE(connection != NULL, -1);
    if (connection->iostream == NULL)
    {
        errno = NEO4J_CONNECTION_CLOSED;
        return -1;
    }

    int res = neo4j_message_recv(connection->iostream, mpool, type, argv, argc);
    if (res && errno != NEO4J_CONNECTION_CLOSED)
    {
        char ebuf[256];
        neo4j_log_error(connection->logger,
                "error receiving message on %p: %s\n", connection,
                neo4j_strerror(errno, ebuf, sizeof(ebuf)));
    }
    return res;
}


int neo4j_attach_session(neo4j_connection_t *connection,
        neo4j_session_t *session)
{
    REQUIRE(connection != NULL, -1);
    REQUIRE(session != NULL, -1);
    if (connection->iostream == NULL)
    {
        errno = NEO4J_CONNECTION_CLOSED;
        return -1;
    }

    if (connection->session != NULL)
    {
        errno = NEO4J_TOO_MANY_SESSIONS;
        return -1;
    }
    session->request_queue = connection->request_queue;
    session->request_queue_size =
        connection->config->session_request_queue_size;
    connection->session = session;
    return 0;
}


int neo4j_detach_session(neo4j_connection_t *connection,
        neo4j_session_t *session, bool reusable)
{
    REQUIRE(connection != NULL, -1);
    REQUIRE(session != NULL, -1);
    REQUIRE(connection->session == session, -1);
    int result = 0;

    connection->session = NULL;
    if (!reusable)
    {
        result = disconnect(connection);
    }
    session->request_queue = NULL;
    session->request_queue_size = 0;
    return result;
}
