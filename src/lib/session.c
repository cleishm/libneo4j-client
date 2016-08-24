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
#include "session.h"
#include "iostream.h"
#include "memory.h"
#include "metadata.h"
#include "serialization.h"
#include "util.h"
#include <assert.h>
#include <unistd.h>

static_assert(NEO4J_REQUEST_ARGV_PREALLOC >= 2,
        "NEO4J_REQUEST_ARGV_PREALLOC too small");


static int session_start(neo4j_session_t *session);
static int session_clear(neo4j_session_t *session);
static int session_reset(neo4j_session_t *session);

static int send_requests(neo4j_session_t *session);
static int receive_responses(neo4j_session_t *session,
        const unsigned int *condition, bool interruptable);
static int drain_queued_requests(neo4j_session_t *session);

static struct neo4j_request *new_request(neo4j_session_t *session);
static void pop_request(neo4j_session_t* session);

static int initialize(neo4j_session_t *session, unsigned int attempts);
static int initialize_callback(void *cdata, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc);
static int ack_failure(neo4j_session_t *session);
static int ack_failure_callback(void *cdata, neo4j_message_type_t type,
       const neo4j_value_t *argv, uint16_t argc);


neo4j_session_t *neo4j_new_session(neo4j_connection_t *connection)
{
    REQUIRE(connection != NULL, NULL);

    neo4j_session_t *session = NULL;
    neo4j_logger_t *logger = neo4j_get_logger(connection->config, "session");

    session = calloc(1, sizeof(neo4j_session_t));
    if (session == NULL)
    {
        neo4j_log_error_errno(logger, "malloc of neo4j_session_t failed");
        goto failure;
    }

    neo4j_log_debug(logger, "new session (%p) on %p",
            (void *)session, (void *)connection);

    session->connection = connection;
    session->logger = logger;
    neo4j_atomic_bool_init(&(session->processing), false);
    neo4j_atomic_bool_init(&(session->reset_requested), false);
    if (session_start(session))
    {
        goto failure;
    }

    neo4j_log_debug(logger, "session started (%p)", (void *)session);

    return session;

    int errsv;
failure:
    errsv = errno;
    if (session != NULL)
    {
        free(session);
    }
    neo4j_logger_release(logger);
    errno = errsv;
    return NULL;
}


static inline bool interrupted(neo4j_session_t *session)
{
    return neo4j_atomic_bool_get(&(session->reset_requested));
}


neo4j_connection_t *neo4j_session_connection(neo4j_session_t *session)
{
    return session->connection;
}


int session_start(neo4j_session_t *session)
{
    if (neo4j_attach_session(session->connection, session))
    {
        char ebuf[256];
        neo4j_log_debug(session->logger,
                "session (%p) cannot use connection %p: %s",
                (void *)session, (void *)(session->connection),
                neo4j_strerror(errno, ebuf, sizeof(ebuf)));
        return -1;
    }
    assert(session->request_queue != NULL);
    assert(session->request_queue_size > 0);
    assert(session->request_queue_depth == 0);

    if (initialize(session, 0))
    {
        assert(session->request_queue_depth <= 1);
        session->request_queue_depth = 0;
        goto failure;
    }

    return 0;

    int errsv;
failure:
    errsv = errno;
    neo4j_detach_session(session->connection, session, false);
    errno = errsv;
    return -1;
}


/**
 * End all jobs in the session and drain all requests
 *
 * @internal
 *
 * @param [session] The session.
 * @return 0 on success, <0 on error (errno will be set).
 */
int session_clear(neo4j_session_t *session)
{
    REQUIRE(session != NULL, -1);
    if (session->connection == NULL)
    {
        errno = NEO4J_SESSION_ENDED;
        return -1;
    }
    if (neo4j_atomic_bool_set(&(session->processing), true))
    {
        errno = NEO4J_SESSION_BUSY;
        return -1;
    }

    int err = 0;
    int errsv = errno;

    // notify all jobs first, so that they can handle received
    // responses appropriately
    for (neo4j_job_t *job = session->jobs; job != NULL;)
    {
        neo4j_job_abort(job, NEO4J_SESSION_ENDED);
        neo4j_job_t *next = job->next;
        job->next = NULL;
        job = next;
    }
    session->jobs = NULL;

    // Receive responses to inflight requests
    if (!session->failed && receive_responses(session, NULL, false))
    {
        err = -1;
        errsv = errno;
        session->failed = true;
    }

    // drain any remaining requests
    if (drain_queued_requests(session) && err == 0)
    {
        err = -1;
        errsv = errno;
        session->failed = true;
    }
    assert(session->request_queue_depth == 0);

    neo4j_atomic_bool_set(&(session->processing), false);
    errno = errsv;
    return err;
}


int session_reset(neo4j_session_t *session)
{
    assert(session != NULL);
    assert(session->connection != NULL);

    const neo4j_config_t *config = neo4j_session_config(session);
    neo4j_mpool_t mpool =
            neo4j_mpool(config->allocator, config->mpool_block_size);
    int err = 0;
    int errsv = errno;

    // notify all jobs first, so that they can handle received
    // responses appropriately
    for (neo4j_job_t *job = session->jobs; job != NULL;)
    {
        neo4j_job_abort(job, NEO4J_SESSION_RESET);
        neo4j_job_t *next = job->next;
        job->next = NULL;
        job = next;
    }
    session->jobs = NULL;

    // process any already inflight requests
    if (receive_responses(session, NULL, false) < 0)
    {
        err = -1;
        errsv = errno;
        session->failed = true;
        goto cleanup;
    }

    // receive response to RESET
    neo4j_message_type_t type;
    const neo4j_value_t *argv;
    uint16_t argc;
    if (neo4j_connection_recv(session->connection, &mpool, &type, &argv, &argc))
    {
        neo4j_log_trace_errno(session->logger,
                "neo4j_connection_recv failed");
        err = -1;
        errsv = errno;
        session->failed = true;
        goto cleanup;
    }

    neo4j_log_debug(session->logger, "rcvd %s in response to RESET in %p",
            neo4j_message_type_str(type), (void *)session);

    if (type != NEO4J_SUCCESS_MESSAGE)
    {
        neo4j_log_error(session->logger,
                "unexpected %s message received in %p"
                " (expected SUCCESS in response to RESET)",
                neo4j_message_type_str(type), (void *)session);
        err = -1;
        errsv = EPROTO;
        session->failed = true;
        goto cleanup;
    }

cleanup:
    // ensure queue is empty
    if (drain_queued_requests(session) && err == 0)
    {
        err = -1;
        errsv = errno;
        session->failed = true;
    }

    neo4j_mpool_drain(&mpool);

    if (err == 0)
    {
        neo4j_log_debug(session->logger, "session reset (%p)", (void *)session);
    }

    errno = errsv;
    return err;
}


int neo4j_end_session(neo4j_session_t *session)
{
    REQUIRE(session != NULL, -1);
    REQUIRE(session->connection != NULL, -1);
    int err = 0;
    int errsv = errno;

    if (session_clear(session))
    {
        err = -1;
        errsv = errno;
    }

    int result = neo4j_detach_session(session->connection, session,
            !session->failed);
    if (result && err == 0)
    {
        err = -1;
        errsv = errno;
    }

    neo4j_log_debug(session->logger, "session ended (%p)", (void *)session);

    session->connection = NULL;
    neo4j_logger_release(session->logger);
    session->logger = NULL;
    free(session);
    errno = errsv;
    return err;
}


int neo4j_reset_session(neo4j_session_t *session)
{
    REQUIRE(session != NULL, -1);
    if (session->connection == NULL)
    {
        errno = NEO4J_SESSION_ENDED;
        return -1;
    }
    if (session->failed)
    {
        errno = NEO4J_SESSION_FAILED;
        return -1;
    }

    // immediately send RESET request onto the connection
    if (neo4j_connection_send(session->connection, NEO4J_RESET_MESSAGE, NULL, 0))
    {
        session->failed = true;
        return -1;
    }

    neo4j_log_trace(session->logger, "sent RESET in %p", (void *)session);

    // Check and set the reset_requested sentinal and then check if processing
    // is already taking place.
    if (neo4j_atomic_bool_set(&(session->reset_requested), true) ||
        neo4j_atomic_bool_set(&(session->processing), true))
    {
        return 0;
    }

    int err = session_reset(session);
    // clear reset_requested BEFORE ending processing, to ensure it is not
    // set if processing resumes
    neo4j_atomic_bool_set(&(session->reset_requested), false);
    neo4j_atomic_bool_set(&(session->processing), false);
    return err;
}


bool neo4j_credentials_expired(const neo4j_session_t *session)
{
    return session->credentials_expired;
}


int neo4j_attach_job(neo4j_session_t *session, neo4j_job_t *job)
{
    REQUIRE(session != NULL, -1);
    REQUIRE(job != NULL, -1);
    REQUIRE(job->next == NULL, -1);

    if (session->failed)
    {
        errno = NEO4J_SESSION_FAILED;
        return -1;
    }

    job->next = session->jobs;
    session->jobs = job;
    return 0;
}


int neo4j_detach_job(neo4j_session_t *session, neo4j_job_t *job)
{
    REQUIRE(session != NULL, -1);
    REQUIRE(job != NULL, -1);

    if (session->jobs == job)
    {
        session->jobs = job->next;
        job->next = NULL;
        return 0;
    }

    REQUIRE(session->jobs != NULL, -1);

    neo4j_job_t *it;
    for (it = session->jobs; it->next != job && it->next != NULL; it = it->next)
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
 * @param [session] The session.
 * @param [condition] A pointer to a condition flag, that must remain
 *         greater than zero for processing of responses to continue. This
 *         allows processing to be stopped when sufficient responses have
 *         been received to satisfy the current demands. If `NULL`, then
 *         processing will continue until there are no further outstanding
 *         requests (or a failure occurs).
 * @return 0 on success, -1 if a error occurs (errno will be set and all
 *         requests will be drained).
 */
int neo4j_session_sync(neo4j_session_t *session, const unsigned int *condition)
{
    REQUIRE(session != NULL, -1);
    ENSURE_NOT_NULL(unsigned int, condition, 1);

    if (session->failed)
    {
        errno = NEO4J_SESSION_FAILED;
        return -1;
    }
    if (neo4j_atomic_bool_set(&(session->processing), true))
    {
        errno = NEO4J_SESSION_BUSY;
        return -1;
    }

    int err = -1;

    while (*condition > 0 && session->request_queue_depth > 0 &&
            !interrupted(session))
    {
        int result = receive_responses(session, condition, true);
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
            assert(session->inflight_requests == 0);
            if (drain_queued_requests(session))
            {
                assert(session->request_queue_depth == 0);
                neo4j_atomic_bool_set(&(session->processing), false);
                return -1;
            }
            assert(session->request_queue_depth == 0);
            neo4j_atomic_bool_set(&(session->processing), false);
            return ack_failure(session);
        }

        if (send_requests(session))
        {
            goto cleanup;
        }
    }

    if (interrupted(session))
    {
        if (!session_reset(session))
        {
            errno = NEO4J_SESSION_RESET;
        }
        neo4j_atomic_bool_set(&(session->reset_requested), false);
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
        drain_queued_requests(session);
        assert(session->request_queue_depth == 0);
    }
    neo4j_atomic_bool_set(&(session->processing), false);
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
 * @param [session] The session.
 * @return 0 on success, -1 if an error occurs (errno will be set).
 */
int send_requests(neo4j_session_t *session)
{
    assert(session != NULL);
    neo4j_connection_t *connection = session->connection;
    const neo4j_config_t *config = neo4j_session_config(session);

    for (unsigned int i = session->inflight_requests;
            i < session->request_queue_depth &&
            i < config->max_pipelined_requests && !interrupted(session); ++i)
    {
        int offset =
            (session->request_queue_head + i) % session->request_queue_size;
        struct neo4j_request *request = &(session->request_queue[offset]);

        if (neo4j_connection_send(connection, request->type,
                    request->argv, request->argc))
        {
            return -1;
        }

        (session->inflight_requests)++;
        neo4j_log_debug(session->logger, "sent %s (%p) in %p",
                neo4j_message_type_str(request->type),
                (void *)request, (void *)session);
    }

    return 0;
}


/**
 * Receive responses to inflight requests.
 *
 * @internal
 *
 * @param [session] The session.
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
int receive_responses(neo4j_session_t *session, const unsigned int *condition,
        bool interruptable)
{
    assert(session != NULL);
    ENSURE_NOT_NULL(unsigned int, condition, 1);
    neo4j_connection_t *connection = session->connection;

    bool failure = false;
    while ((failure || *condition > 0) && session->inflight_requests > 0 &&
            (!interruptable || !interrupted(session)))
    {
        neo4j_message_type_t type;
        const neo4j_value_t *argv;
        uint16_t argc;

        struct neo4j_request *request =
            &(session->request_queue[session->request_queue_head]);
        if (neo4j_connection_recv(connection, request->mpool,
                    &type, &argv, &argc))
        {
            neo4j_log_trace_errno(session->logger,
                    "neo4j_connection_recv failed");
            return -1;
        }

        if (failure && type != NEO4J_IGNORED_MESSAGE)
        {
            neo4j_log_error(session->logger,
                    "unexpected %s message received in %p"
                    " (expected IGNORED after failure occurred)",
                    neo4j_message_type_str(type), (void *)session);
            errno = EPROTO;
            session->failed = true;
            return -1;
        }
        if (type == NEO4J_FAILURE_MESSAGE)
        {
            failure = true;
        }

        neo4j_log_debug(session->logger, "rcvd %s in response to %s (%p)",
                neo4j_message_type_str(type),
                neo4j_message_type_str(request->type), (void *)request);

        int result = request->receive(request->cdata, type, argv, argc);
        int errsv = errno;
        if (result <= 0)
        {
            pop_request(session);
            (session->inflight_requests)--;
        }
        if (result < 0)
        {
            session->failed = true;
            errno = errsv;
            return -1;
        }
    }

    if (interruptable && interrupted(session))
    {
        return 1;
    }

    assert(!failure || session->inflight_requests == 0);
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
 * @param [session] The session.
 * @return 0 on success, -1 if an error occurs (errno will be set).
 */
int drain_queued_requests(neo4j_session_t *session)
{
    assert(session != NULL);

    int err = 0;
    int errsv = errno;
    while (session->request_queue_depth > 0)
    {
        struct neo4j_request *request =
            &(session->request_queue[session->request_queue_head]);

        neo4j_log_trace(session->logger, "draining %s (%p) from queue on %p",
                neo4j_message_type_str(request->type),
                (void *)request, (void *)session);
        int result = request->receive(request->cdata,
                NEO4J_IGNORED_MESSAGE, NULL, 0);
        assert(result <= 0);
        if (err == 0 && result < 0)
        {
            err = -1;
            errsv = errno;
        }
        pop_request(session);
    }

    session->inflight_requests = 0;
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
 * session methods are invoked.
 *
 * @param [session] The session.
 * @return The queued request, which MUST be populated with valid attributes,
 *         or `NULL` if an error occurs (errno will be set).
 */
struct neo4j_request *new_request(neo4j_session_t *session)
{
    assert(session != NULL);
    const neo4j_config_t *config = neo4j_session_config(session);

    if (session->failed)
    {
        errno = NEO4J_SESSION_FAILED;
        return NULL;
    }

    if (session->request_queue_depth >= session->request_queue_size)
    {
        assert(session->request_queue_depth == session->request_queue_size);
        errno = ENOBUFS;
        return NULL;
    }

    unsigned int request_queue_free =
        session->request_queue_size - session->request_queue_depth;
    unsigned int request_queue_tail =
        (request_queue_free > session->request_queue_head)?
        session->request_queue_head + session->request_queue_depth :
        session->request_queue_head - request_queue_free;

    (session->request_queue_depth)++;
    struct neo4j_request *req =
        &(session->request_queue[request_queue_tail]);
    req->_mpool = neo4j_mpool(config->allocator, config->mpool_block_size);
    req->mpool = &(req->_mpool);
    return req;
}


/**
 * Pop a request off the head of the queue.
 *
 * @internal
 *
 * @param [session] The session.
 */
void pop_request(neo4j_session_t* session)
{
    assert(session != NULL);
    assert(session->request_queue_depth > 0);

    struct neo4j_request *req =
        &(session->request_queue[session->request_queue_head]);
    neo4j_mpool_drain(&(req->_mpool));
    memset(req, 0, sizeof(struct neo4j_request));

    (session->request_queue_depth)--;
    (session->request_queue_head)++;
    if (session->request_queue_head >= session->request_queue_size)
    {
        assert(session->request_queue_head == session->request_queue_size);
        session->request_queue_head = 0;
    }
}


struct init_cdata
{
    neo4j_session_t *session;
    int error;
};


int initialize(neo4j_session_t *session, unsigned int attempts)
{
    assert(session != NULL);
    neo4j_config_t *config = neo4j_session_config(session);
    const char *client_id = config->client_id;

    struct init_cdata cdata = { .session = session, .error = 0 };

    const char *username = (config->username != NULL)? config->username : "";
    const char *password = (config->password != NULL)? config->password : "";

    if (attempts > 0 || config->auth_reattempt_callback == NULL ||
            config->password != NULL || config->attempt_empty_password)
    {
        struct neo4j_request *req = new_request(session);
        if (req == NULL)
        {
            return -1;
        }

        req->type = NEO4J_INIT_MESSAGE;
        req->_argv[0] = neo4j_string(client_id);
        neo4j_map_entry_t auth_token[3] =
            { neo4j_map_entry("scheme", neo4j_string("basic")),
              neo4j_map_entry("principal", neo4j_string(username)),
              neo4j_map_entry("credentials", neo4j_string(password)) };
        req->_argv[1] = neo4j_map(auth_token, 3);
        req->argv = req->_argv;
        req->argc = 2;
        req->receive = initialize_callback;
        req->cdata = &cdata;

        neo4j_log_trace(session->logger,
                "enqu INIT{\"%s\", {scheme: basic, principal: \"%s\", "
                "credentials: ****}} (%p) in %p",
                client_id, username, (void *)req, (void *)session);

        if (neo4j_session_sync(session, NULL))
        {
            return -1;
        }

        if (cdata.error == 0)
        {
            return 0;
        }

        assert(cdata.error == NEO4J_INVALID_CREDENTIALS ||
                cdata.error == NEO4J_AUTH_RATE_LIMIT);

        if (config->auth_reattempt_callback == NULL)
        {
            errno = cdata.error;
            return -1;
        }

        ++attempts;
    }

    char host[NEO4J_MAXHOSTLEN];
    if (describe_host(host, sizeof(host),
            session->connection->hostname, session->connection->port))
    {
        return -1;
    }

    char username_buf[NEO4J_MAXUSERNAMELEN];
    strncpy(username_buf, username, sizeof(username_buf));
    char password_buf[NEO4J_MAXPASSWORDLEN];
    strncpy(password_buf, password, sizeof(password_buf));

    int r = config->auth_reattempt_callback(
            config->auth_reattempt_callback_userdata, host, attempts,
            cdata.error, username_buf, sizeof(username_buf),
            password_buf, sizeof(password_buf));
    if (r < 0)
    {
        return -1;
    }
    else if (r > 0)
    {
        errno = cdata.error;
        if (cdata.error == 0)
        {
            neo4j_log_error(session->logger,
                    "authentication callback returned NEO4J_AUTHENTICATION_FAIL"
                    " before first authentication attempt (in %p)",
                    (void *)session);
            errno = NEO4J_UNEXPECTED_ERROR;
        }
        return -1;
    }

    if (neo4j_config_set_username(config, username_buf))
    {
        return -1;
    }
    if (neo4j_config_set_password(config, password_buf))
    {
        return -1;
    }

    return initialize(session, attempts);
}


int initialize_callback(void *cdata, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc)
{
    assert(cdata != NULL);
    neo4j_session_t *session = ((struct init_cdata *)cdata)->session;

    char description[128];

    if (type == NEO4J_SUCCESS_MESSAGE)
    {
        snprintf(description, sizeof(description),
                "SUCCESS in %p (response to INIT)", (void *)session);
        const neo4j_value_t *metadata = neo4j_validate_metadata(argv, argc,
                description, session->logger);
        if (metadata == NULL)
        {
            return -1;
        }
        if (neo4j_log_is_enabled(session->logger, NEO4J_LOG_TRACE))
        {
            neo4j_metadata_log(session->logger, NEO4J_LOG_TRACE, description,
                    *metadata);
        }
        neo4j_value_t ce = neo4j_map_get(*metadata, "credentials_expired");
        session->credentials_expired =
                (neo4j_type(ce) == NEO4J_BOOL && neo4j_bool_value(ce));
        return 0;
    }

    if (type != NEO4J_FAILURE_MESSAGE)
    {
        neo4j_log_error(session->logger,
                "unexpected %s message received in %p"
                " (expected SUCCESS in response to INIT)",
                neo4j_message_type_str(type), (void *)session);
        errno = EPROTO;
        return -1;
    }

    // handle failure
    snprintf(description, sizeof(description),
            "FAILURE in %p (response to INIT)", (void *)session);
    const neo4j_value_t *metadata = neo4j_validate_metadata(argv, argc,
            description, session->logger);
    if (metadata == NULL)
    {
        return -1;
    }

    if (neo4j_log_is_enabled(session->logger, NEO4J_LOG_TRACE))
    {
        neo4j_metadata_log(session->logger, NEO4J_LOG_TRACE, description,
                *metadata);
    }

    const neo4j_config_t *config = neo4j_session_config(session);
    struct neo4j_failure_details details;
    neo4j_mpool_t mpool =
        neo4j_mpool(config->allocator, config->mpool_block_size);
    if (neo4j_meta_failure_details(&details, *metadata, &mpool,
                description, session->logger))
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

    neo4j_log_error(session->logger, "session initialization failed: %s",
            details.message);
    errno = NEO4J_UNEXPECTED_ERROR;

cleanup:
    neo4j_mpool_drain(&mpool);
    return result;
}


int ack_failure(neo4j_session_t *session)
{
    assert(session != NULL);

    struct neo4j_request *req = new_request(session);
    if (req == NULL)
    {
        return -1;
    }
    req->type = NEO4J_ACK_FAILURE_MESSAGE;
    req->argc = 0;
    req->receive = ack_failure_callback;
    req->cdata = session;

    neo4j_log_trace(session->logger, "enqu ACK_FAILURE (%p) in %p",
            (void *)req, (void *)session);

    return neo4j_session_sync(session, NULL);
}


int ack_failure_callback(void *cdata, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc)
{
    assert(cdata != NULL);
    neo4j_session_t *session = (neo4j_session_t *)cdata;

    if (type == NEO4J_IGNORED_MESSAGE)
    {
        // only when draining after connection close
        return 0;
    }
    if (type != NEO4J_SUCCESS_MESSAGE)
    {
        neo4j_log_error(session->logger,
                "unexpected %s message received in %p"
                " (expected SUCCESS in response to ACK_FAILURE)",
                neo4j_message_type_str(type), (void *)session);
        errno = EPROTO;
        return -1;
    }

    neo4j_log_trace(session->logger, "ACK_FAILURE complete in %p",
            (void *)session);
    return 0;
}


int neo4j_session_run(neo4j_session_t *session, neo4j_mpool_t *mpool,
        const char *statement, neo4j_value_t params,
        neo4j_response_recv_t callback, void *cdata)
{
    REQUIRE(session != NULL, -1);
    REQUIRE(mpool != NULL, -1);
    REQUIRE(statement != NULL, -1);
    REQUIRE(neo4j_type(params) == NEO4J_MAP || neo4j_is_null(params), -1);
    REQUIRE(callback != NULL, -1);

    if (neo4j_atomic_bool_set(&(session->processing), true))
    {
        errno = NEO4J_SESSION_BUSY;
        return -1;
    }

    int err = -1;
    struct neo4j_request *req = new_request(session);
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

    if (neo4j_log_is_enabled(session->logger, NEO4J_LOG_TRACE))
    {
        char buf[1024];
        neo4j_log_trace(session->logger, "enqu RUN{\"%s\", %s} (%p) in %p",
                statement, neo4j_tostring(req->argv[1], buf, sizeof(buf)),
                (void *)req, (void *)session);
    }

    err = 0;

cleanup:
    neo4j_atomic_bool_set(&(session->processing), false);
    return err;
}


int neo4j_session_pull_all(neo4j_session_t *session, neo4j_mpool_t *mpool,
        neo4j_response_recv_t callback, void *cdata)
{
    REQUIRE(session != NULL, -1);
    REQUIRE(mpool != NULL, -1);
    REQUIRE(callback != NULL, -1);

    if (neo4j_atomic_bool_set(&(session->processing), true))
    {
        errno = NEO4J_SESSION_BUSY;
        return -1;
    }

    int err = -1;
    struct neo4j_request *req = new_request(session);
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

    neo4j_log_trace(session->logger, "enqu PULL_ALL (%p) in %p",
            (void *)req, (void *)session);

    err = 0;

cleanup:
    neo4j_atomic_bool_set(&(session->processing), false);
    return err;
}


int neo4j_session_discard_all(neo4j_session_t *session, neo4j_mpool_t *mpool,
        neo4j_response_recv_t callback, void *cdata)
{
    REQUIRE(session != NULL, -1);
    REQUIRE(mpool != NULL, -1);
    REQUIRE(callback != NULL, -1);

    if (neo4j_atomic_bool_set(&(session->processing), true))
    {
        errno = NEO4J_SESSION_BUSY;
        return -1;
    }

    int err = -1;
    struct neo4j_request *req = new_request(session);
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

    neo4j_log_trace(session->logger, "enqu DISCARD_ALL (%p) in %p",
            (void *)req, (void *)session);

    err = 0;

cleanup:
    neo4j_atomic_bool_set(&(session->processing), false);
    return err;
}
