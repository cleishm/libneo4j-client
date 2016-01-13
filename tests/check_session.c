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
#include "../config.h"
#include "../src/lib/chunking_iostream.h"
#include "../src/lib/connection.h"
#include "../src/lib/deserialization.h"
#include "../src/lib/messages.h"
#include "../src/lib/session.h"
#include "../src/lib/serialization.h"
#include "../src/lib/util.h"
#include "memiostream.h"
#include <check.h>
#include <errno.h>


struct received_response
{
    unsigned int condition;
    neo4j_message_type_t type;
};


static neo4j_iostream_t *stub_connect(struct neo4j_connection_factory *factory,
        const char *hostname, unsigned int port, neo4j_config_t *config,
        uint_fast32_t flags, struct neo4j_logger *logger);
static void queue_message(neo4j_iostream_t *ios, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc);
static neo4j_message_type_t recv_message(neo4j_iostream_t *ios,
        neo4j_mpool_t *mpool, const neo4j_value_t **argv, uint16_t *argc);
static int response_recv_callback(void *cdata, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc);


static struct neo4j_logger_provider *logger_provider;
static ring_buffer_t *in_rb;
static ring_buffer_t *out_rb;
static neo4j_iostream_t *client_ios;
static neo4j_iostream_t *server_ios;
static struct neo4j_connection_factory stub_factory;
static neo4j_config_t *config;
neo4j_connection_t *connection;
neo4j_mpool_t mpool;


static void setup(void)
{
    logger_provider = neo4j_std_logger_provider(stderr, NEO4J_LOG_ERROR, 0);
    in_rb = rb_alloc(1024);
    out_rb = rb_alloc(1024);
    client_ios = neo4j_memiostream(in_rb, out_rb);
    server_ios = neo4j_memiostream(out_rb, in_rb);

    stub_factory.tcp_connect = stub_connect;
    config = neo4j_new_config();
    neo4j_config_set_logger_provider(config, logger_provider);
    neo4j_config_set_connection_factory(config, &stub_factory);

    mpool = neo4j_std_mpool(config);

    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));

    connection = neo4j_connect("neo4j://localhost:7687", config, 0);
    ck_assert_ptr_ne(connection, NULL);
    rb_advance(out_rb, 4 + (4 * sizeof(uint32_t)));
}


static void teardown(void)
{
    neo4j_close(connection);
    neo4j_mpool_drain(&mpool);
    neo4j_ios_close(server_ios);
    neo4j_config_free(config);
    rb_free(in_rb);
    rb_free(out_rb);
    neo4j_std_logger_provider_free(logger_provider);
}


neo4j_iostream_t *stub_connect(struct neo4j_connection_factory *factory,
            const char *hostname, unsigned int port, neo4j_config_t *config,
            uint_fast32_t flags, struct neo4j_logger *logger)
{
    return client_ios;
}


void queue_message(neo4j_iostream_t *ios, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc)
{
    int result = neo4j_message_send(ios, type, argv, argc, 0, 1024);
    ck_assert_int_eq(result, 0);
}


neo4j_message_type_t recv_message(neo4j_iostream_t *ios, neo4j_mpool_t *mpool,
        const neo4j_value_t **argv, uint16_t *argc)
{
    neo4j_message_type_t type;
    int result = neo4j_message_recv(ios, mpool, &type, argv, argc);
    ck_assert_int_eq(result, 0);
    return type;
}


int response_recv_callback(void *cdata, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc)
{
    struct received_response *resp = (struct received_response *)cdata;
    resp->condition = 0;
    resp->type = type;
    return 0;
}


START_TEST (test_new_session_sends_init_containing_clientid)
{
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0);
    neo4j_session_t *session = neo4j_new_session(connection);
    ck_assert_ptr_ne(session, NULL);

    const neo4j_value_t *argv;
    uint16_t argc;
    neo4j_message_type_t type = recv_message(server_ios, &mpool, &argv, &argc);
    ck_assert(type == NEO4J_INIT_MESSAGE);
    ck_assert_int_eq(argc, 1);

    char buf[256];
    ck_assert(neo4j_type(argv[0]) == NEO4J_STRING);
    ck_assert_str_eq(neo4j_string_value(argv[0], buf, sizeof(buf)),
            config->client_id);

    neo4j_end_session(session);
}
END_TEST


START_TEST (test_new_session_fails_on_init_failure)
{
    neo4j_config_set_logger_provider(connection->config, NULL);

    queue_message(server_ios, NEO4J_FAILURE_MESSAGE, NULL, 0);
    neo4j_session_t *session = neo4j_new_session(connection);
    ck_assert_ptr_eq(session, NULL);
    ck_assert_int_eq(errno, EPROTO);
}
END_TEST


START_TEST (test_new_session_fails_if_connection_is_dead)
{
    neo4j_config_set_logger_provider(connection->config, NULL);

    neo4j_session_t *session = neo4j_new_session(connection);
    ck_assert_ptr_eq(session, NULL);
    ck_assert_int_eq(errno, NEO4J_CONNECTION_CLOSED);
}
END_TEST


START_TEST (test_new_session_fails_if_session_active)
{
    neo4j_config_set_logger_provider(connection->config, NULL);

    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0);
    neo4j_session_t *session1 = neo4j_new_session(connection);
    ck_assert_ptr_ne(session1, NULL);

    neo4j_session_t *session2 = neo4j_new_session(connection);
    ck_assert_ptr_eq(session2, NULL);
    ck_assert_int_eq(errno, NEO4J_TOO_MANY_SESSIONS);

    neo4j_end_session(session1);
}
END_TEST


START_TEST (test_new_session_after_previous_is_closed)
{
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0);
    neo4j_session_t *session1 = neo4j_new_session(connection);
    ck_assert_ptr_ne(session1, NULL);
    neo4j_end_session(session1);

    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0);
    neo4j_session_t *session2 = neo4j_new_session(connection);
    ck_assert_ptr_ne(session2, NULL);
    neo4j_end_session(session2);
}
END_TEST


START_TEST (test_session_cant_start_after_previous_init_failure)
{
    neo4j_config_set_logger_provider(connection->config, NULL);

    queue_message(server_ios, NEO4J_FAILURE_MESSAGE, NULL, 0);
    neo4j_session_t *session1 = neo4j_new_session(connection);
    ck_assert_ptr_eq(session1, NULL);
    ck_assert_int_eq(errno, EPROTO);

    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0);
    neo4j_session_t *session2 = neo4j_new_session(connection);
    ck_assert_ptr_eq(session2, NULL);
    ck_assert_int_eq(errno, NEO4J_CONNECTION_CLOSED);
}
END_TEST


START_TEST (test_session_drains_outstanding_requests_on_close)
{
    neo4j_config_set_logger_provider(connection->config, NULL);

    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0);
    neo4j_session_t *session = neo4j_new_session(connection);
    ck_assert_ptr_ne(session, NULL);

    struct received_response resp = { 1, NULL };
    int result = neo4j_session_run(session, &mpool, "RETURN 1", NULL, 0,
            response_recv_callback, &resp);
    ck_assert_int_eq(result, 0);

    neo4j_end_session(session);
    ck_assert(resp.type == NEO4J_IGNORED_MESSAGE);
}
END_TEST


START_TEST (test_session_awaits_inflight_requests_on_close)
{
    neo4j_config_set_logger_provider(connection->config, NULL);

    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0);
    neo4j_session_t *session = neo4j_new_session(connection);
    ck_assert_ptr_ne(session, NULL);

    struct received_response resp1 = { 1, NULL };
    int result = neo4j_session_run(session, &mpool, "RETURN 1", NULL, 0,
            response_recv_callback, &resp1);
    ck_assert_int_eq(result, 0);

    struct received_response resp2 = { 1, NULL };
    result = neo4j_session_pull_all(session, &mpool,
            response_recv_callback, &resp2);
    ck_assert_int_eq(result, 0);

    // await only the first request (leaves the 2nd inflight)
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0);
    result = neo4j_session_sync(session, &(resp1.condition));
    ck_assert_int_eq(result, 0);
    ck_assert(resp1.type == NEO4J_SUCCESS_MESSAGE);
    ck_assert_int_eq(resp2.condition, 1);

    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0);
    neo4j_end_session(session);
    ck_assert(resp2.type == NEO4J_SUCCESS_MESSAGE);
}
END_TEST


START_TEST (test_session_drains_requests_and_acks_after_failure)
{
    neo4j_config_set_logger_provider(connection->config, NULL);

    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0);
    neo4j_session_t *session = neo4j_new_session(connection);
    ck_assert_ptr_ne(session, NULL);

    neo4j_message_type_t type = recv_message(server_ios, &mpool, NULL, NULL);
    ck_assert(type == NEO4J_INIT_MESSAGE);

    struct received_response resp1 = { 1, NULL };
    int result = neo4j_session_run(session, &mpool, "RETURN 1", NULL, 0,
            response_recv_callback, &resp1);
    ck_assert_int_eq(result, 0);

    struct received_response resp2 = { 1, NULL };
    result = neo4j_session_pull_all(session, &mpool,
            response_recv_callback, &resp2);
    ck_assert_int_eq(result, 0);

    queue_message(server_ios, NEO4J_FAILURE_MESSAGE, NULL, 0);
    queue_message(server_ios, NEO4J_IGNORED_MESSAGE, NULL, 0);
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0);
    result = neo4j_session_sync(session, &(resp1.condition));
    ck_assert_int_eq(result, 0);
    ck_assert(resp1.type == NEO4J_FAILURE_MESSAGE);
    ck_assert(resp2.type == NEO4J_IGNORED_MESSAGE);

    type = recv_message(server_ios, &mpool, NULL, NULL);
    ck_assert(type == NEO4J_RUN_MESSAGE);

    type = recv_message(server_ios, &mpool, NULL, NULL);
    ck_assert(type == NEO4J_PULL_ALL_MESSAGE);

    type = recv_message(server_ios, &mpool, NULL, NULL);
    ck_assert(type == NEO4J_ACK_FAILURE_MESSAGE);

    neo4j_end_session(session);
}
END_TEST


START_TEST (test_session_cant_start_after_eproto_in_failure)
{
    neo4j_config_set_logger_provider(connection->config, NULL);

    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0);
    neo4j_session_t *session1 = neo4j_new_session(connection);
    ck_assert_ptr_ne(session1, NULL);

    struct received_response resp1 = { 1, NULL };
    int result = neo4j_session_run(session1, &mpool, "RETURN 1", NULL, 0,
            response_recv_callback, &resp1);
    ck_assert_int_eq(result, 0);

    struct received_response resp2 = { 1, NULL };
    result = neo4j_session_pull_all(session1, &mpool,
            response_recv_callback, &resp2);
    ck_assert_int_eq(result, 0);

    queue_message(server_ios, NEO4J_FAILURE_MESSAGE, NULL, 0);
    queue_message(server_ios, NEO4J_FAILURE_MESSAGE, NULL, 0);
    result = neo4j_session_sync(session1, NULL);
    ck_assert_int_eq(result, -1);
    ck_assert_int_eq(errno, EPROTO);
    ck_assert(resp1.type == NEO4J_FAILURE_MESSAGE);
    ck_assert(resp2.type == NEO4J_IGNORED_MESSAGE);

    neo4j_end_session(session1);

    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0);
    neo4j_session_t *session2 = neo4j_new_session(connection);
    ck_assert_ptr_eq(session2, NULL);
    ck_assert_int_eq(errno, NEO4J_CONNECTION_CLOSED);
}
END_TEST


START_TEST (test_session_cant_start_after_eproto_in_ack_failure)
{
    neo4j_config_set_logger_provider(connection->config, NULL);

    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0);
    neo4j_session_t *session1 = neo4j_new_session(connection);
    ck_assert_ptr_ne(session1, NULL);

    struct received_response resp1 = { 1, NULL };
    int result = neo4j_session_run(session1, &mpool, "RETURN 1", NULL, 0,
            response_recv_callback, &resp1);
    ck_assert_int_eq(result, 0);

    struct received_response resp2 = { 1, NULL };
    result = neo4j_session_pull_all(session1, &mpool,
            response_recv_callback, &resp2);
    ck_assert_int_eq(result, 0);

    queue_message(server_ios, NEO4J_FAILURE_MESSAGE, NULL, 0);
    queue_message(server_ios, NEO4J_IGNORED_MESSAGE, NULL, 0);
    queue_message(server_ios, NEO4J_FAILURE_MESSAGE, NULL, 0);
    result = neo4j_session_sync(session1, NULL);
    ck_assert_int_eq(result, -1);
    ck_assert_int_eq(errno, EPROTO);
    ck_assert(resp1.type == NEO4J_FAILURE_MESSAGE);
    ck_assert(resp2.type == NEO4J_IGNORED_MESSAGE);

    neo4j_end_session(session1);

    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0);
    neo4j_session_t *session2 = neo4j_new_session(connection);
    ck_assert_ptr_eq(session2, NULL);
    ck_assert_int_eq(errno, NEO4J_CONNECTION_CLOSED);
}
END_TEST


START_TEST (test_session_drains_acks_when_closed)
{
    neo4j_config_set_logger_provider(connection->config, NULL);

    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0);
    neo4j_session_t *session = neo4j_new_session(connection);
    ck_assert_ptr_ne(session, NULL);

    neo4j_message_type_t type = recv_message(server_ios, &mpool, NULL, NULL);
    ck_assert(type == NEO4J_INIT_MESSAGE);

    struct received_response resp1 = { 1, NULL };
    int result = neo4j_session_run(session, &mpool, "RETURN 1", NULL, 0,
            response_recv_callback, &resp1);
    ck_assert_int_eq(result, 0);

    struct received_response resp2 = { 1, NULL };
    result = neo4j_session_pull_all(session, &mpool,
            response_recv_callback, &resp2);
    ck_assert_int_eq(result, 0);

    queue_message(server_ios, NEO4J_FAILURE_MESSAGE, NULL, 0);
    queue_message(server_ios, NEO4J_IGNORED_MESSAGE, NULL, 0);
    // no queued response for the ACK_FAILURE => connection closed

    result = neo4j_session_sync(session, &(resp1.condition));
    ck_assert_int_eq(result, -1);
    ck_assert_int_eq(errno, NEO4J_CONNECTION_CLOSED);
    ck_assert(resp1.type == NEO4J_FAILURE_MESSAGE);
    ck_assert(resp2.type == NEO4J_IGNORED_MESSAGE);

    type = recv_message(server_ios, &mpool, NULL, NULL);
    ck_assert(type == NEO4J_RUN_MESSAGE);

    type = recv_message(server_ios, &mpool, NULL, NULL);
    ck_assert(type == NEO4J_PULL_ALL_MESSAGE);

    type = recv_message(server_ios, &mpool, NULL, NULL);
    ck_assert(type == NEO4J_ACK_FAILURE_MESSAGE);

    neo4j_end_session(session);
}
END_TEST


TCase* session_tcase(void)
{
    TCase *tc = tcase_create("session");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_new_session_sends_init_containing_clientid);
    tcase_add_test(tc, test_new_session_fails_on_init_failure);
    tcase_add_test(tc, test_new_session_fails_if_connection_is_dead);
    tcase_add_test(tc, test_new_session_fails_if_session_active);
    tcase_add_test(tc, test_new_session_after_previous_is_closed);
    tcase_add_test(tc, test_session_cant_start_after_previous_init_failure);
    tcase_add_test(tc, test_session_drains_outstanding_requests_on_close);
    tcase_add_test(tc, test_session_awaits_inflight_requests_on_close);
    tcase_add_test(tc, test_session_drains_requests_and_acks_after_failure);
    tcase_add_test(tc, test_session_cant_start_after_eproto_in_failure);
    tcase_add_test(tc, test_session_cant_start_after_eproto_in_ack_failure);
    tcase_add_test(tc, test_session_drains_acks_when_closed);
    return tc;
}
