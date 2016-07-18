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
#include "buffering_iostream.h"
#include "chunking_iostream.h"
#include "client_config.h"
#include "deserialization.h"
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
static neo4j_connection_t *establish_connection(const char *hostname,
        unsigned int port, neo4j_config_t *config, uint_fast32_t flags);
static neo4j_iostream_t *std_tcp_connect(
        struct neo4j_connection_factory *factory, const char *hostname,
        unsigned int port, neo4j_config_t *config, uint_fast32_t flags,
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

    unsigned int port = (uri->port > 0)? uri->port : NEO4J_DEFAULT_TCP_PORT;
    connection = establish_connection(uri->hostname, port, config, flags);

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
    REQUIRE(port <= UINT16_MAX, NULL);

    if (port == 0)
    {
        port = NEO4J_DEFAULT_TCP_PORT;
    }

    config = neo4j_config_dup(config);
    if (config == NULL)
    {
        return NULL;
    }

    return establish_connection(hostname, port, config, flags);
}


neo4j_connection_t *establish_connection(const char *hostname,
        unsigned int port, neo4j_config_t *config, uint_fast32_t flags)
{
    neo4j_logger_t *logger = neo4j_get_logger(config, "connection");

    neo4j_iostream_t *iostream = NULL;
    uint8_t *snd_buffer = NULL;
    struct neo4j_request *request_queue = NULL;

    neo4j_connection_t *connection = calloc(1, sizeof(neo4j_connection_t));
    if (connection == NULL)
    {
        goto failure;
    }

    snd_buffer = malloc(config->snd_min_chunk_size);
    if (snd_buffer == NULL)
    {
        goto failure;
    }

    request_queue = calloc(config->session_request_queue_size,
            sizeof(struct neo4j_request));
    if (request_queue == NULL)
    {
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
        errno = NEO4J_PROTOCOL_NEGOTIATION_FAILED;
        goto failure;
    }
    if (protocol_version != 1)
    {
        errno = NEO4J_PROTOCOL_NEGOTIATION_FAILED;
        goto failure;
    }

    connection->config = config;
    connection->logger = logger;
    connection->hostname = strdup(hostname);
    if (connection->hostname == NULL)
    {
        goto failure;
    }
    connection->port = port;
    connection->iostream = iostream;
    connection->version = protocol_version;
#ifdef HAVE_TLS
    connection->insecure = flags & NEO4J_INSECURE;
#else
    connection->insecure = true;
#endif
    connection->snd_buffer = snd_buffer;
    connection->request_queue = request_queue;

    neo4j_log_info(logger, "connected (%p) to %s:%u%s", (void *)connection,
            hostname, port, connection->insecure? " (insecure)" : "");
    neo4j_log_debug(logger, "connection %p using protocol version %d",
            (void *)connection, protocol_version);

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
    if (snd_buffer != NULL)
    {
        free(snd_buffer);
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
        const char *hostname, unsigned int port, neo4j_config_t *config,
        uint_fast32_t flags, struct neo4j_logger *logger)
{
    REQUIRE(factory != NULL, NULL);
    REQUIRE(config != NULL, NULL);
    REQUIRE(hostname != NULL, NULL);
    REQUIRE(port <= UINT16_MAX, NULL);

    char servname[MAXSERVNAMELEN];
    snprintf(servname, sizeof(servname), "%u", port);
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

    if (config->io_sndbuf_size > 0 || config->io_rcvbuf_size > 0)
    {
        neo4j_iostream_t *buffering_ios = neo4j_buffering_iostream(ios, true,
                config->io_sndbuf_size, config->io_rcvbuf_size);
        if (buffering_ios == NULL)
        {
            goto failure;
        }
        ios = buffering_ios;
    }

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
        neo4j_log_info(connection->logger, "disconnected %p",
                (void *)connection);
    }
    neo4j_logger_release(connection->logger);
    neo4j_config_free(connection->config);
    free(connection->request_queue);
    free(connection->snd_buffer);
    free(connection->hostname);
    memset(connection, 0, sizeof(neo4j_connection_t));
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

    if (neo4j_ios_flush(iostream))
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


const char *neo4j_connection_hostname(const neo4j_connection_t *connection)
{
    return connection->hostname;
}


unsigned int neo4j_connection_port(const neo4j_connection_t *connection)
{
    return connection->port;
}


const char *neo4j_connection_username(const neo4j_connection_t *connection)
{
    return connection->config->username;
}


bool neo4j_connection_is_secure(const neo4j_connection_t *connection)
{
    return !(connection->insecure);
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
            connection->snd_buffer, config->snd_min_chunk_size,
            config->snd_max_chunk_size);
    if (res && errno != NEO4J_CONNECTION_CLOSED)
    {
        char ebuf[256];
        neo4j_log_error(connection->logger,
                "error sending message on %p: %s\n", (void *)connection,
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
                "error receiving message on %p: %s\n", (void *)connection,
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
