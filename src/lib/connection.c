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
#include "metadata.h"
#include "network.h"
#ifdef HAVE_OPENSSL
#include "openssl_iostream.h"
#endif
#include "posix_iostream.h"
#include "serialization.h"
#include "util.h"
#include <assert.h>
#include <unistd.h>


static int add_userinfo_to_config(const char *userinfo, neo4j_config_t *config,
        uint_fast32_t flags);
static neo4j_connection_t *establish_connection(const char *hostname,
        unsigned int port, neo4j_config_t *config, uint_fast32_t flags);
static neo4j_iostream_t *std_tcp_connect(
        struct neo4j_connection_factory *factory, const char *hostname,
        unsigned int port, neo4j_config_t *config, uint_fast32_t flags,
        struct neo4j_logger *logger);
static int negotiate_protocol_version(neo4j_iostream_t *iostream,
        uint32_t *protocol_version);

static bool interrupted(neo4j_connection_t *connection);
static int session_reset(neo4j_connection_t *connection);

static int send_requests(neo4j_connection_t *connection);
static int receive_responses(neo4j_connection_t *connection,
        const unsigned int *condition, bool interruptable);
static int drain_queued_requests(neo4j_connection_t *connection);

static struct neo4j_request *new_request(neo4j_connection_t *connection);
static void pop_request(neo4j_connection_t* connection);

static int initialize(neo4j_connection_t *connection);
static int initialize_callback(void *cdata, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc);
static int ack_failure(neo4j_connection_t *connection  );
static int ack_failure_callback(void *cdata, neo4j_message_type_t type,
       const neo4j_value_t *argv, uint16_t argc);


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
        goto failure;
    }

    if (uri->scheme == NULL ||
            (strcmp(uri->scheme, "neo4j") != 0 &&
             strcmp(uri->scheme, "bolt") != 0))
    {
        errno = NEO4J_UNKNOWN_URI_SCHEME;
        goto failure;
    }

    if (uri->userinfo != NULL)
    {
        if (!(flags & NEO4J_NO_URI_CREDENTIALS) &&
                add_userinfo_to_config(uri->userinfo, config, flags))
        {
            goto failure;
        }
        // clear any password in the URI
        size_t userinfolen = strlen(uri->userinfo);
        memset_s(uri->userinfo, userinfolen, 0, userinfolen);
    }

    unsigned int port = (uri->port > 0)? uri->port : NEO4J_DEFAULT_TCP_PORT;
    connection = establish_connection(uri->hostname, port, config, flags);
    if (connection == NULL)
    {
        goto failure;
    }
    config = NULL;

    if (initialize(connection))
    {
        goto failure;
    }

    free_uri(uri);
    return connection;

    int errsv;
failure:
    errsv = errno;
    if (connection != NULL)
    {
        neo4j_close(connection);
    }
    if (uri != NULL)
    {
        free_uri(uri);
    }
    if (config != NULL)
    {
        neo4j_config_free(config);
    }
    errno = errsv;
    return NULL;
}


int add_userinfo_to_config(const char *userinfo, neo4j_config_t *config,
        uint_fast32_t flags)
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
        if (!(flags & NEO4J_NO_URI_PASSWORD) &&
                neo4j_config_set_password(config, userinfo + username_len + 1))
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

    neo4j_connection_t *connection = establish_connection(hostname, port,
            config, flags);
    if (connection == NULL)
    {
        goto failure;
    }
    config = NULL;

    if (initialize(connection))
    {
        goto failure;
    }

    return connection;

    int errsv;
failure:
    errsv = errno;
    if (connection != NULL)
    {
        neo4j_close(connection);
    }
    if (config != NULL)
    {
        neo4j_config_free(config);
    }
    errno = errsv;
    return NULL;
}


/**
 * Establish the server connection.
 *
 * @internal
 *
 * @param [hostname] The hostname to connect to.
 * @param [port] The TCP port number to connect to.
 * @param [config] A pointer to a neo4j_config_t, which will be captured
 *         in the returned connection and should not be separately released.
 * @paran [flags] A bitmask of flags to control the connection.
 * @return A `neo4j_connection_t`, or `NULL` on error.
 */
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
    connection->request_queue_size = config->session_request_queue_size;

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


int neo4j_close(neo4j_connection_t *connection)
{
    REQUIRE(connection != NULL, -1);
    REQUIRE(connection->config != NULL, -1);

    if (neo4j_atomic_bool_set(&(connection->processing), true))
    {
        errno = NEO4J_SESSION_BUSY;
        return -1;
    }

    // notify all jobs first, so that they can handle received
    // responses appropriately
    for (neo4j_job_t *job = connection->jobs; job != NULL;)
    {
        neo4j_job_abort(job, NEO4J_SESSION_ENDED);
        neo4j_job_t *next = job->next;
        job->next = NULL;
        job = next;
    }
    connection->jobs = NULL;

    int err = 0;
    int errsv = errno;

    // receive responses to inflight requests
    if (!connection->failed && receive_responses(connection, NULL, false))
    {
        err = -1;
        errsv = errno;
        connection->failed = true;
    }

    // drain any remaining requests
    if (drain_queued_requests(connection) && err == 0)
    {
        err = -1;
        errsv = errno;
        connection->failed = true;
    }
    assert(connection->request_queue_depth == 0);

    neo4j_atomic_bool_set(&(connection->processing), false);

    if (connection->iostream != NULL &&
        neo4j_ios_close(connection->iostream) && err == 0)
    {
        err = -1;
        errsv = errno;
        connection->failed = true;
    }
    connection->iostream = NULL;

    if (err == 0)
    {
        neo4j_log_info(connection->logger, "disconnected %p",
                (void *)connection);
    }
    neo4j_logger_release(connection->logger);
    neo4j_config_free(connection->config);
    free(connection->server_id);
    free(connection->request_queue);
    free(connection->snd_buffer);
    free(connection->hostname);
    memset(connection, 0, sizeof(neo4j_connection_t));
    free(connection);
    errno = errsv;
    return err;
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


const char *neo4j_server_id(const neo4j_connection_t *connection)
{
    return connection->server_id;
}


bool neo4j_credentials_expired(const neo4j_connection_t *connection)
{
    return connection->credentials_expired;
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
                "Error sending message on %p: %s", (void *)connection,
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
                "Error receiving message on %p: %s", (void *)connection,
                neo4j_strerror(errno, ebuf, sizeof(ebuf)));
    }
    return res;
}


int neo4j_reset(neo4j_connection_t *connection)
{
    REQUIRE(connection != NULL, -1);
    if (connection->failed)
    {
        errno = NEO4J_SESSION_FAILED;
        return -1;
    }

    // immediately send RESET request onto the connection
    if (neo4j_connection_send(connection, NEO4J_RESET_MESSAGE, NULL, 0))
    {
        connection->failed = true;
        return -1;
    }

    neo4j_log_trace(connection->logger, "sent RESET in %p", (void *)connection);

    // Check and set the reset_requested sentinal and then check if processing
    // is already taking place.
    if (neo4j_atomic_bool_set(&(connection->reset_requested), true) ||
        neo4j_atomic_bool_set(&(connection->processing), true))
    {
        return 0;
    }

    int err = session_reset(connection);
    // clear reset_requested BEFORE ending processing, to ensure it is not
    // set if processing resumes
    neo4j_atomic_bool_set(&(connection->reset_requested), false);
    neo4j_atomic_bool_set(&(connection->processing), false);
    return err;
}


bool interrupted(neo4j_connection_t *connection)
{
    return neo4j_atomic_bool_get(&(connection->reset_requested));
}


int session_reset(neo4j_connection_t *connection)
{
    assert(connection != NULL);

    const neo4j_config_t *config = connection->config;
    neo4j_mpool_t mpool =
            neo4j_mpool(config->allocator, config->mpool_block_size);
    int err = 0;
    int errsv = errno;

    // notify all jobs first, so that they can handle received
    // responses appropriately
    for (neo4j_job_t *job = connection->jobs; job != NULL;)
    {
        neo4j_job_abort(job, NEO4J_SESSION_RESET);
        neo4j_job_t *next = job->next;
        job->next = NULL;
        job = next;
    }
    connection->jobs = NULL;

    // process any already inflight requests
    if (receive_responses(connection, NULL, false) < 0)
    {
        err = -1;
        errsv = errno;
        connection->failed = true;
        goto cleanup;
    }

    // receive response to RESET
    neo4j_message_type_t type;
    const neo4j_value_t *argv;
    uint16_t argc;
    if (neo4j_connection_recv(connection, &mpool, &type, &argv, &argc))
    {
        neo4j_log_trace_errno(connection->logger,
                "neo4j_connection_recv failed");
        err = -1;
        errsv = errno;
        connection->failed = true;
        goto cleanup;
    }

    neo4j_log_debug(connection->logger, "rcvd %s in response to RESET in %p",
            neo4j_message_type_str(type), (void *)connection);

    if (type != NEO4J_SUCCESS_MESSAGE)
    {
        neo4j_log_error(connection->logger,
                "Unexpected %s message received in %p"
                " (expected SUCCESS in response to RESET)",
                neo4j_message_type_str(type), (void *)connection);
        err = -1;
        errsv = EPROTO;
        connection->failed = true;
        goto cleanup;
    }

cleanup:
    // ensure queue is empty
    if (drain_queued_requests(connection) && err == 0)
    {
        err = -1;
        errsv = errno;
        connection->failed = true;
    }

    neo4j_mpool_drain(&mpool);

    if (err == 0)
    {
        neo4j_log_debug(connection->logger, "connection reset (%p)", (void *)connection);
    }

    errno = errsv;
    return err;
}


int neo4j_attach_job(neo4j_connection_t *connection, neo4j_job_t *job)
{
    REQUIRE(connection != NULL, -1);
    REQUIRE(job != NULL, -1);
    REQUIRE(job->next == NULL, -1);

    if (connection->failed)
    {
        errno = NEO4J_SESSION_FAILED;
        return -1;
    }

    job->next = connection->jobs;
    connection->jobs = job;
    return 0;
}


int neo4j_detach_job(neo4j_connection_t *connection, neo4j_job_t *job)
{
    REQUIRE(connection != NULL, -1);
    REQUIRE(job != NULL, -1);

    if (connection->jobs == job)
    {
        connection->jobs = job->next;
        job->next = NULL;
        return 0;
    }

    REQUIRE(connection->jobs != NULL, -1);

    neo4j_job_t *it;
    for (it = connection->jobs;
            it->next != job && it->next != NULL;
            it = it->next)
        ;

    REQUIRE(it->next == job, -1);
    it->next = job->next;
    job->next = NULL;
    return 0;
}


/**
 * Process requests and responses for a session.
 *
 * @internal
 *
 * @param [connection] The connection.
 * @param [condition] A pointer to a condition flag, that must remain
 *         greater than zero for processing of responses to continue. This
 *         allows processing to be stopped when sufficient responses have
 *         been received to satisfy the current demands. If `NULL`, then
 *         processing will continue until there are no further outstanding
 *         requests (or a failure occurs).
 * @return 0 on success, -1 if a error occurs (errno will be set and all
 *         requests will be drained).
 */
int neo4j_session_sync(neo4j_connection_t *connection,
        const unsigned int *condition)
{
    REQUIRE(connection != NULL, -1);
    ENSURE_NOT_NULL(unsigned int, condition, 1);

    if (connection->failed)
    {
        errno = NEO4J_SESSION_FAILED;
        return -1;
    }
    if (neo4j_atomic_bool_set(&(connection->processing), true))
    {
        errno = NEO4J_SESSION_BUSY;
        return -1;
    }

    int err = -1;

    while (*condition > 0 && connection->request_queue_depth > 0 &&
            !interrupted(connection))
    {
        int result = receive_responses(connection, condition, true);
        if (result < 0)
        {
            goto cleanup;
        }
        if (result == 1)
        {
            break;
        }
        if (result > 0)
        {
            assert(connection->inflight_requests == 0);
            if (drain_queued_requests(connection))
            {
                assert(connection->request_queue_depth == 0);
                neo4j_atomic_bool_set(&(connection->processing), false);
                return -1;
            }
            assert(connection->request_queue_depth == 0);
            neo4j_atomic_bool_set(&(connection->processing), false);
            return ack_failure(connection);
        }

        if (send_requests(connection))
        {
            goto cleanup;
        }
    }

    if (interrupted(connection))
    {
        if (!session_reset(connection))
        {
            errno = NEO4J_SESSION_RESET;
        }
        neo4j_atomic_bool_set(&(connection->reset_requested), false);
    }
    else
    {
        err = 0;
    }

    int errsv;
cleanup:
    errsv = errno;
    if (err)
    {
        drain_queued_requests(connection);
        assert(connection->request_queue_depth == 0);
    }
    neo4j_atomic_bool_set(&(connection->processing), false);
    errno = errsv;
    return err;
}


/**
 * Send queued requests.
 *
 * @internal
 *
 * Sends requests, up to the maximum allowed for pipelining by the config.
 *
 * @param [connection] The connection.
 * @return 0 on success, -1 if an error occurs (errno will be set).
 */
int send_requests(neo4j_connection_t *connection)
{
    assert(connection != NULL);

    for (unsigned int i = connection->inflight_requests;
            i < connection->request_queue_depth &&
            i < connection->config->max_pipelined_requests &&
            !interrupted(connection); ++i)
    {
        int offset = (connection->request_queue_head + i) %
                connection->request_queue_size;
        struct neo4j_request *request = &(connection->request_queue[offset]);

        if (neo4j_connection_send(connection, request->type,
                    request->argv, request->argc))
        {
            return -1;
        }

        (connection->inflight_requests)++;
        neo4j_log_debug(connection->logger, "sent %s (%p) in %p",
                neo4j_message_type_str(request->type),
                (void *)request, (void *)connection);
    }

    return 0;
}


/**
 * Receive responses to inflight requests.
 *
 * @internal
 *
 * @param [connection] The connection.
 * @param [condition] A pointer to a condition flag, that must remain
 *         greater than zero for processing of responses to continue. This
 *         allows processing to be stopped when sufficient responses have
 *         been received to satisfy the current demands. If `NULL`, then
 *         processing will continue until there are no further outstanding
 *         requests (or a failure occurs).
 * @param [interruptable] If `true`, setting of `reset_requested` will interrupt
 *         receiving of results.
 * @return 0 on success, -1 if a error occurs (errno will be set), 1 if
 *         interrupted, and >1 if a valid FAILURE message is received (in which
 *         case, all inflight requests will have been drained).
 */
int receive_responses(neo4j_connection_t *connection, const unsigned int *condition,
        bool interruptable)
{
    assert(connection != NULL);
    ENSURE_NOT_NULL(unsigned int, condition, 1);

    bool failure = false;
    while ((failure || *condition > 0) && connection->inflight_requests > 0 &&
            (!interruptable || !interrupted(connection)))
    {
        neo4j_message_type_t type;
        const neo4j_value_t *argv;
        uint16_t argc;

        struct neo4j_request *request =
            &(connection->request_queue[connection->request_queue_head]);
        if (neo4j_connection_recv(connection, request->mpool,
                    &type, &argv, &argc))
        {
            neo4j_log_trace_errno(connection->logger,
                    "neo4j_connection_recv failed");
            return -1;
        }

        if (failure && type != NEO4J_IGNORED_MESSAGE)
        {
            neo4j_log_error(connection->logger,
                    "Unexpected %s message received in %p"
                    " (expected IGNORED after failure occurred)",
                    neo4j_message_type_str(type), (void *)connection);
            errno = EPROTO;
            connection->failed = true;
            return -1;
        }
        if (type == NEO4J_FAILURE_MESSAGE)
        {
            failure = true;
        }

        neo4j_log_debug(connection->logger, "rcvd %s in response to %s (%p)",
                neo4j_message_type_str(type),
                neo4j_message_type_str(request->type), (void *)request);

        int result = request->receive(request->cdata, type, argv, argc);
        int errsv = errno;
        if (result <= 0)
        {
            pop_request(connection);
            (connection->inflight_requests)--;
        }
        if (result < 0)
        {
            connection->failed = true;
            errno = errsv;
            return -1;
        }
    }

    if (interruptable && interrupted(connection))
    {
        return 1;
    }

    assert(!failure || connection->inflight_requests == 0);
    return failure? 2 : 0;
}


/**
 * Send IGNORED to all queued requests.
 *
 * @internal
 *
 * This will also generate IGNORED for all inflight requests, so this
 * method should only be called when there are no inflight requests or
 * when a terminal error has occured and the connection will be closed.
 *
 * @param [connection] The connection.
 * @return 0 on success, -1 if an error occurs (errno will be set).
 */
int drain_queued_requests(neo4j_connection_t *connection)
{
    assert(connection != NULL);

    int err = 0;
    int errsv = errno;
    while (connection->request_queue_depth > 0)
    {
        struct neo4j_request *request =
            &(connection->request_queue[connection->request_queue_head]);

        neo4j_log_trace(connection->logger, "draining %s (%p) from queue on %p",
                neo4j_message_type_str(request->type),
                (void *)request, (void *)connection);
        int result = request->receive(request->cdata, NULL, NULL, 0);
        assert(result <= 0);
        if (err == 0 && result < 0)
        {
            err = -1;
            errsv = errno;
        }
        pop_request(connection);
    }

    connection->inflight_requests = 0;
    errno = errsv;
    return err;
}


/**
 * Add a queued request.
 *
 * @internal
 *
 * The returned pointer will be to a request struct already added to the tail
 * of the queue. It MUST be populated with valid attributes before any other
 * connection methods are invoked.
 *
 * @param [connection] The connection.
 * @return The queued request, which MUST be populated with valid attributes,
 *         or `NULL` if an error occurs (errno will be set).
 */
struct neo4j_request *new_request(neo4j_connection_t *connection)
{
    assert(connection != NULL);
    const neo4j_config_t *config = connection->config;

    if (connection->failed)
    {
        errno = NEO4J_SESSION_FAILED;
        return NULL;
    }

    if (connection->request_queue_depth >= connection->request_queue_size)
    {
        assert(connection->request_queue_depth == connection->request_queue_size);
        errno = ENOBUFS;
        return NULL;
    }

    unsigned int request_queue_free =
        connection->request_queue_size - connection->request_queue_depth;
    unsigned int request_queue_tail =
        (request_queue_free > connection->request_queue_head)?
        connection->request_queue_head + connection->request_queue_depth :
        connection->request_queue_head - request_queue_free;

    (connection->request_queue_depth)++;
    struct neo4j_request *req =
        &(connection->request_queue[request_queue_tail]);
    req->_mpool = neo4j_mpool(config->allocator, config->mpool_block_size);
    req->mpool = &(req->_mpool);
    return req;
}


/**
 * Pop a request off the head of the queue.
 *
 * @internal
 *
 * @param [connection] The connection.
 */
void pop_request(neo4j_connection_t* connection)
{
    assert(connection != NULL);
    assert(connection->request_queue_depth > 0);

    struct neo4j_request *req =
        &(connection->request_queue[connection->request_queue_head]);
    neo4j_mpool_drain(&(req->_mpool));
    memset(req, 0, sizeof(struct neo4j_request));

    (connection->request_queue_depth)--;
    (connection->request_queue_head)++;
    if (connection->request_queue_head >= connection->request_queue_size)
    {
        assert(connection->request_queue_head == connection->request_queue_size);
        connection->request_queue_head = 0;
    }
}


struct init_cdata
{
    neo4j_connection_t *connection;
    int error;
};


int initialize(neo4j_connection_t *connection)
{
    assert(connection != NULL);
    neo4j_config_t *config = connection->config;

    char host[NEO4J_MAXHOSTLEN];
    if (describe_host(host, sizeof(host), connection->hostname,
            connection->port))
    {
        return -1;
    }

    if (ensure_basic_auth_credentials(config, host))
    {
        return -1;
    }

    int err = -1;

    struct neo4j_request *req = new_request(connection);
    if (req == NULL)
    {
        goto cleanup;
    }

    struct init_cdata cdata = { .connection = connection, .error = 0 };

    req->type = NEO4J_INIT_MESSAGE;
    req->_argv[0] = neo4j_string(config->client_id);
    neo4j_map_entry_t auth_token[3] =
        { neo4j_map_entry("scheme", neo4j_string("basic")),
          neo4j_map_entry("principal", neo4j_string(config->username)),
          neo4j_map_entry("credentials", neo4j_string(config->password)) };
    req->_argv[1] = neo4j_map(auth_token, 3);
    req->argv = req->_argv;
    req->argc = 2;
    req->receive = initialize_callback;
    req->cdata = &cdata;

    neo4j_log_trace(connection->logger,
            "enqu INIT{\"%s\", {scheme: basic, principal: \"%s\", "
            "credentials: ****}} (%p) in %p",
            config->client_id, config->username,
            (void *)req, (void *)connection);

    if (neo4j_session_sync(connection, NULL))
    {
        if (cdata.error != 0)
        {
            errno = cdata.error;
        }
        goto cleanup;
    }

    if (cdata.error != 0)
    {
        assert(cdata.error == NEO4J_INVALID_CREDENTIALS ||
                cdata.error == NEO4J_AUTH_RATE_LIMIT);
        errno = cdata.error;
        goto cleanup;
    }

    err = 0;

    int errsv;
cleanup:
    errsv = errno;
    // clear password out of connection config
    ignore_unused_result(neo4j_config_set_password(connection->config, NULL));
    errno = errsv;
    return err;
}


int initialize_callback(void *cdata, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc)
{
    if (type == NULL)
    {
        return 0;
    }

    assert(cdata != NULL);
    neo4j_connection_t *connection = ((struct init_cdata *)cdata)->connection;

    char description[128];

    if (type == NEO4J_SUCCESS_MESSAGE)
    {
        snprintf(description, sizeof(description),
                "SUCCESS in %p (response to INIT)", (void *)connection);
        const neo4j_value_t *metadata = neo4j_validate_metadata(argv, argc,
                description, connection->logger);
        if (metadata == NULL)
        {
            return -1;
        }
        if (neo4j_log_is_enabled(connection->logger, NEO4J_LOG_TRACE))
        {
            neo4j_metadata_log(connection->logger, NEO4J_LOG_TRACE, description,
                    *metadata);
        }
        neo4j_value_t ce = neo4j_map_get(*metadata, "credentials_expired");
        connection->credentials_expired =
                (neo4j_type(ce) == NEO4J_BOOL && neo4j_bool_value(ce));
        neo4j_value_t si = neo4j_map_get(*metadata, "server");
        if (neo4j_type(si) == NEO4J_STRING)
        {
            connection->server_id = strndup(neo4j_ustring_value(si),
                    neo4j_string_length(si));
            if (connection->server_id == NULL)
            {
                return -1;
            }
        }
        return 0;
    }

    if (type != NEO4J_FAILURE_MESSAGE)
    {
        neo4j_log_error(connection->logger,
                "Unexpected %s message received in %p"
                " (expected SUCCESS in response to INIT)",
                neo4j_message_type_str(type), (void *)connection);
        errno = EPROTO;
        return -1;
    }

    // handle failure
    snprintf(description, sizeof(description),
            "FAILURE in %p (response to INIT)", (void *)connection);
    const neo4j_value_t *metadata = neo4j_validate_metadata(argv, argc,
            description, connection->logger);
    if (metadata == NULL)
    {
        return -1;
    }

    if (neo4j_log_is_enabled(connection->logger, NEO4J_LOG_TRACE))
    {
        neo4j_metadata_log(connection->logger, NEO4J_LOG_TRACE, description,
                *metadata);
    }

    const neo4j_config_t *config = connection->config;
    struct neo4j_failure_details details;
    neo4j_mpool_t mpool =
        neo4j_mpool(config->allocator, config->mpool_block_size);
    if (neo4j_meta_failure_details(&details, *metadata, &mpool,
                description, connection->logger))
    {
        return -1;
    }

    int result = -1;

    if (strcmp("Neo.ClientError.Security.EncryptionRequired",
            details.code) == 0)
    {
        errno = NEO4J_SERVER_REQUIRES_SECURE_CONNECTION;
        goto cleanup;
    }
    if (strcmp("Neo.ClientError.Security.Unauthorized", details.code) == 0)
    {
        ((struct init_cdata *)cdata)->error = NEO4J_INVALID_CREDENTIALS;
        result = 0;
        goto cleanup;
    }
    if (strcmp("Neo.ClientError.Security.AuthenticationRateLimit",
            details.code) == 0)
    {
        ((struct init_cdata *)cdata)->error = NEO4J_AUTH_RATE_LIMIT;
        result = 0;
        goto cleanup;
    }

    neo4j_log_error(connection->logger, "Session initialization failed: %s",
            details.message);
    errno = NEO4J_UNEXPECTED_ERROR;

cleanup:
    neo4j_mpool_drain(&mpool);
    return result;
}


int ack_failure(neo4j_connection_t *connection)
{
    assert(connection != NULL);

    struct neo4j_request *req = new_request(connection);
    if (req == NULL)
    {
        return -1;
    }
    req->type = NEO4J_ACK_FAILURE_MESSAGE;
    req->argc = 0;
    req->receive = ack_failure_callback;
    req->cdata = connection;

    neo4j_log_trace(connection->logger, "enqu ACK_FAILURE (%p) in %p",
            (void *)req, (void *)connection);

    return neo4j_session_sync(connection, NULL);
}


int ack_failure_callback(void *cdata, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc)
{
    assert(cdata != NULL);
    neo4j_connection_t *connection = (neo4j_connection_t *)cdata;

    if (type == NULL)
    {
        // only when draining after connection close
        return 0;
    }
    if (type != NEO4J_SUCCESS_MESSAGE)
    {
        neo4j_log_error(connection->logger,
                "Unexpected %s message received in %p"
                " (expected SUCCESS in response to ACK_FAILURE)",
                neo4j_message_type_str(type), (void *)connection);
        errno = EPROTO;
        return -1;
    }

    neo4j_log_trace(connection->logger, "ACK_FAILURE complete in %p",
            (void *)connection);
    return 0;
}


int neo4j_session_run(neo4j_connection_t *connection, neo4j_mpool_t *mpool,
        const char *statement, neo4j_value_t params,
        neo4j_response_recv_t callback, void *cdata)
{
    REQUIRE(connection != NULL, -1);
    REQUIRE(mpool != NULL, -1);
    REQUIRE(statement != NULL, -1);
    REQUIRE(neo4j_type(params) == NEO4J_MAP || neo4j_is_null(params), -1);
    REQUIRE(callback != NULL, -1);

    if (neo4j_atomic_bool_set(&(connection->processing), true))
    {
        errno = NEO4J_SESSION_BUSY;
        return -1;
    }

    int err = -1;
    struct neo4j_request *req = new_request(connection);
    if (req == NULL)
    {
        goto cleanup;
    }
    req->type = NEO4J_RUN_MESSAGE;
    req->_argv[0] = neo4j_string(statement);
    req->_argv[1] = neo4j_is_null(params)? neo4j_map(NULL, 0) : params;
    req->argv = req->_argv;
    req->argc = 2;
    req->mpool = mpool;
    req->receive = callback;
    req->cdata = cdata;

    if (neo4j_log_is_enabled(connection->logger, NEO4J_LOG_TRACE))
    {
        char buf[1024];
        neo4j_log_trace(connection->logger, "enqu RUN{\"%s\", %s} (%p) in %p",
                statement, neo4j_tostring(req->argv[1], buf, sizeof(buf)),
                (void *)req, (void *)connection);
    }

    err = 0;

cleanup:
    neo4j_atomic_bool_set(&(connection->processing), false);
    return err;
}


int neo4j_session_pull_all(neo4j_connection_t *connection, neo4j_mpool_t *mpool,
        neo4j_response_recv_t callback, void *cdata)
{
    REQUIRE(connection != NULL, -1);
    REQUIRE(mpool != NULL, -1);
    REQUIRE(callback != NULL, -1);

    if (neo4j_atomic_bool_set(&(connection->processing), true))
    {
        errno = NEO4J_SESSION_BUSY;
        return -1;
    }

    int err = -1;
    struct neo4j_request *req = new_request(connection);
    if (req == NULL)
    {
        goto cleanup;
    }

    req->type = NEO4J_PULL_ALL_MESSAGE;
    req->argv = NULL;
    req->argc = 0;
    req->mpool = mpool;
    req->receive = callback;
    req->cdata = cdata;

    neo4j_log_trace(connection->logger, "enqu PULL_ALL (%p) in %p",
            (void *)req, (void *)connection);

    err = 0;

cleanup:
    neo4j_atomic_bool_set(&(connection->processing), false);
    return err;
}


int neo4j_session_discard_all(neo4j_connection_t *connection,
        neo4j_mpool_t *mpool, neo4j_response_recv_t callback, void *cdata)
{
    REQUIRE(connection != NULL, -1);
    REQUIRE(mpool != NULL, -1);
    REQUIRE(callback != NULL, -1);

    if (neo4j_atomic_bool_set(&(connection->processing), true))
    {
        errno = NEO4J_SESSION_BUSY;
        return -1;
    }

    int err = -1;
    struct neo4j_request *req = new_request(connection);
    if (req == NULL)
    {
        goto cleanup;
    }

    req->type = NEO4J_DISCARD_ALL_MESSAGE;
    req->argv = NULL;
    req->argc = 0;
    req->mpool = mpool;
    req->receive = callback;
    req->cdata = cdata;

    neo4j_log_trace(connection->logger, "enqu DISCARD_ALL (%p) in %p",
            (void *)req, (void *)connection);

    err = 0;

cleanup:
    neo4j_atomic_bool_set(&(connection->processing), false);
    return err;
}
