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
#include "connection.h"
#include "iostream.h"
#include "memory.h"
#include "messages.h"
#include "metadata.h"
#include "serialization.h"
#include "util.h"
#include <assert.h>
#include <unistd.h>

static_assert(NEO4J_REQUEST_ARGV_PREALLOC >= 2,
        "NEO4J_REQUEST_ARGV_PREALLOC too small");


static int session_start(neo4j_session_t *session);
static int session_clear(neo4j_session_t *session);
static int send_requests(neo4j_session_t *session);
static int receive_responses(neo4j_session_t *session,
        const unsigned int *condition);
static int drain_queued_requests(neo4j_session_t *session);

static struct neo4j_request *new_request(neo4j_session_t *session);
static void pop_request(neo4j_session_t* session);

static int initialize(neo4j_session_t *session);
static int initialize_callback(void *cdata, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc);
static int reset(neo4j_session_t *session);
static int reset_callback(void *cdata, neo4j_message_type_t type,
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
    session->config = connection->config;
    session->logger = logger;
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

    if (initialize(session))
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


static int session_clear(neo4j_session_t *session)
{
    REQUIRE(session != NULL, -1);
    REQUIRE(session->connection != NULL, -1);
    int err = 0;
    int errsv = errno;

    for (neo4j_job_t *job = session->jobs; job != NULL; job = job->next)
    {
        neo4j_job_notify_session_ending(job);
    }

    if (!session->failed && receive_responses(session, NULL))
    {
        err = -1;
        errsv = errno;
        session->failed = true;
    }

    if (drain_queued_requests(session) && err == 0)
    {
        err = -1;
        errsv = errno;
        session->failed = true;
    }
    assert(session->request_queue_depth == 0);

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
    session->config = NULL;
    neo4j_logger_release(session->logger);
    session->logger = NULL;
    free(session);
    errno = errsv;
    return err;
}


int neo4j_reset_session(neo4j_session_t *session)
{
    if (session_clear(session))
    {
        return -1;
    }
    if (reset(session))
    {
        return -1;
    }
    neo4j_log_debug(session->logger, "session reset (%p)", (void *)session);
    return 0;
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


int neo4j_session_sync(neo4j_session_t *session, const unsigned int *condition)
{
    REQUIRE(session != NULL, -1);
    ENSURE_NOT_NULL(unsigned int, condition, 1);

    if (session->failed)
    {
        errno = NEO4J_SESSION_FAILED;
        return -1;
    }

    while (*condition > 0 && session->request_queue_depth > 0)
    {
        int result = receive_responses(session, condition);
        if (result < 0)
        {
            goto error;
        }
        if (result > 0)
        {
            assert(session->inflight_requests == 0);
            if (drain_queued_requests(session))
            {
                assert(session->request_queue_depth == 0);
                return -1;
            }
            assert(session->request_queue_depth == 0);
            return reset(session);
        }

        if (send_requests(session))
        {
            goto error;
        }
    }

    return 0;

    int errsv;
error:
    errsv = errno;
    drain_queued_requests(session);
    assert(session->request_queue_depth == 0);
    errno = errsv;
    return -1;
}


int send_requests(neo4j_session_t *session)
{
    assert(session != NULL);
    neo4j_connection_t *connection = session->connection;
    const neo4j_config_t *config = session->config;

    for (unsigned int i = session->inflight_requests;
            i < session->request_queue_depth &&
            i < config->max_pipelined_requests; ++i)
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


int receive_responses(neo4j_session_t *session, const unsigned int *condition)
{
    assert(session != NULL);
    ENSURE_NOT_NULL(unsigned int, condition, 1);
    neo4j_connection_t *connection = session->connection;

    bool failure = false;
    while ((failure || *condition > 0) && session->inflight_requests > 0)
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

    return failure? 1 : 0;
}


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


struct neo4j_request *new_request(neo4j_session_t *session)
{
    assert(session != NULL);
    const neo4j_config_t *config = session->config;

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


int initialize(neo4j_session_t *session)
{
    assert(session != NULL);
    const neo4j_config_t *config = session->config;
    const char *client_id = config->client_id;

    struct neo4j_request *req = new_request(session);
    if (req == NULL)
    {
        return -1;
    }
    req->type = NEO4J_INIT_MESSAGE;
    req->_argv[0] = neo4j_string(client_id);
    req->argv = req->_argv;
    req->argc = 1;
    req->receive = initialize_callback;
    req->cdata = session;

    neo4j_log_trace(session->logger, "enqu INIT{\"%s\"} (%p) in %p",
            client_id, (void *)req, (void *)session);

    return neo4j_session_sync(session, NULL);
}


int initialize_callback(void *cdata, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc)
{
    assert(cdata != NULL);
    neo4j_session_t *session = (neo4j_session_t *)cdata;

    if (type == NEO4J_SUCCESS_MESSAGE)
    {
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
    char description[128];
    snprintf(description, sizeof(description),
            "FAILURE message received in %p (in response to INIT)",
            (void *)session);
    const neo4j_value_t *metadata = neo4j_validate_metadata(argv, argc,
            description, session->logger);
    if (metadata == NULL)
    {
        return -1;
    }

    const neo4j_config_t *config = session->config;
    const char *code;
    const char *message;
    neo4j_mpool_t mpool =
        neo4j_mpool(config->allocator, config->mpool_block_size);
    if (neo4j_meta_failure_details(&code, &message, *metadata, &mpool,
                description, session->logger))
    {
        return -1;
    }

    if (strcmp(code, "Neo.ClientError.Security.EncryptionRequired") == 0)
    {
        errno = NEO4J_SERVER_REQUIRES_SECURE_CONNECTION;
        return -1;
    }

    neo4j_log_error(session->logger, "session initalization failed: %s",
            message);
    errno = NEO4J_UNEXPECTED_ERROR;
    return -1;
}


int reset(neo4j_session_t *session)
{
    assert(session != NULL);

    struct neo4j_request *req = new_request(session);
    if (req == NULL)
    {
        return -1;
    }
    req->type = NEO4J_RESET_MESSAGE;
    req->argc = 0;
    req->receive = reset_callback;
    req->cdata = session;

    neo4j_log_trace(session->logger, "enqu RESET (%p) in %p",
            (void *)req, (void *)session);

    return neo4j_session_sync(session, NULL);
}


int reset_callback(void *cdata, neo4j_message_type_t type,
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
                " (expected SUCCESS in response to RESET)",
                neo4j_message_type_str(type), (void *)session);
        errno = EPROTO;
        return -1;
    }

    neo4j_log_trace(session->logger, "RESET complete in %p",
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

    struct neo4j_request *req = new_request(session);
    if (req == NULL)
    {
        return -1;
    }
    req->type = NEO4J_RUN_MESSAGE;
    req->_argv[0] = neo4j_string(statement);
    req->_argv[1] = neo4j_is_null(params)? neo4j_map(NULL, 0) : params;
    req->argv = req->_argv;
    req->argc = 2;
    req->mpool = mpool;
    req->receive = callback;
    req->cdata = cdata;

    neo4j_log_trace(session->logger, "enqu RUN{\"%s\"} (%p) in %p",
            statement, (void *)req, (void *)session);

    return 0;
}


int neo4j_session_pull_all(neo4j_session_t *session, neo4j_mpool_t *mpool,
        neo4j_response_recv_t callback, void *cdata)
{
    REQUIRE(session != NULL, -1);
    REQUIRE(mpool != NULL, -1);
    REQUIRE(callback != NULL, -1);

    struct neo4j_request *req = new_request(session);
    if (req == NULL)
    {
        return -1;
    }

    req->type = NEO4J_PULL_ALL_MESSAGE;
    req->argv = NULL;
    req->argc = 0;
    req->mpool = mpool;
    req->receive = callback;
    req->cdata = cdata;

    neo4j_log_trace(session->logger, "enqu PULL_ALL (%p) in %p",
            (void *)req, (void *)session);

    return 0;
}


int neo4j_session_discard_all(neo4j_session_t *session, neo4j_mpool_t *mpool,
        neo4j_response_recv_t callback, void *cdata)
{
    REQUIRE(session != NULL, -1);
    REQUIRE(mpool != NULL, -1);
    REQUIRE(callback != NULL, -1);

    struct neo4j_request *req = new_request(session);
    if (req == NULL)
    {
        return -1;
    }

    req->type = NEO4J_DISCARD_ALL_MESSAGE;
    req->argv = NULL;
    req->argc = 0;
    req->mpool = mpool;
    req->receive = callback;
    req->cdata = cdata;

    neo4j_log_trace(session->logger, "enqu DISCARD_ALL (%p) in %p",
            (void *)req, (void *)session);

    return 0;
}
