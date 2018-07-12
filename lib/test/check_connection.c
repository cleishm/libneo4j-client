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
#include "../src/connection.h"
#include "../src/util.h"
#include "memiostream.h"
#include <check.h>
#include <errno.h>


struct received_response
{
    unsigned int condition;
    neo4j_message_type_t type;
};


#define STUB_FAILURE_CODE -99

static neo4j_iostream_t *stub_connect(
        struct neo4j_connection_factory *factory,
        const char *hostname, unsigned int port, neo4j_config_t *config,
        uint_fast32_t flags, struct neo4j_logger *logger);
static neo4j_iostream_t *stub_failing_connect(
        struct neo4j_connection_factory *factory,
        const char *hostname, unsigned int port, neo4j_config_t *config,
        uint_fast32_t flags, struct neo4j_logger *logger);
static int ios_noop_close(struct neo4j_iostream *self);


static neo4j_value_t empty_map;
static neo4j_value_t failure_metadata;
static neo4j_map_entry_t failure_metadata_entries[2];
static struct neo4j_logger_provider *logger_provider;
static ring_buffer_t *in_rb;
static ring_buffer_t *out_rb;
static neo4j_iostream_t *client_ios;
static neo4j_iostream_t *server_ios;
static struct neo4j_connection_factory stub_factory;
static const char *username = "user";
static const char *password = "pass";
static neo4j_config_t *config;
static int (*client_ios_close)(struct neo4j_iostream *self);
static neo4j_mpool_t mpool;


static void setup(void)
{
    empty_map = neo4j_map(NULL, 0);
    failure_metadata_entries[0] =
            neo4j_map_entry("code", neo4j_string("unknown"));
    failure_metadata_entries[1] =
            neo4j_map_entry("message", neo4j_string("unknown"));
    failure_metadata = neo4j_map(failure_metadata_entries, 2);

    logger_provider = neo4j_std_logger_provider(stderr, NEO4J_LOG_ERROR, 0);
    in_rb = rb_alloc(1024);
    out_rb = rb_alloc(1024);
    client_ios = neo4j_memiostream(in_rb, out_rb);
    client_ios_close = client_ios->close;
    client_ios->close = ios_noop_close;
    server_ios = neo4j_memiostream(out_rb, in_rb);

    stub_factory.tcp_connect = stub_connect;
    config = neo4j_new_config();
    neo4j_config_set_logger_provider(config, logger_provider);
    neo4j_config_set_connection_factory(config, &stub_factory);
    ck_assert_int_eq(neo4j_config_set_username(config, username), 0);
    ck_assert_int_eq(neo4j_config_set_password(config, password), 0);

    mpool = neo4j_std_mpool(config);
}


static void teardown(void)
{
    neo4j_config_free(config);
    client_ios_close(client_ios);
    neo4j_ios_close(server_ios);
    rb_free(in_rb);
    rb_free(out_rb);
    neo4j_mpool_drain(&mpool);
    neo4j_std_logger_provider_free(logger_provider);
}


neo4j_iostream_t *stub_connect(struct neo4j_connection_factory *factory,
        const char *hostname, unsigned int port, neo4j_config_t *config,
        uint_fast32_t flags, struct neo4j_logger *logger)
{
    if (strcmp(hostname, "localhost") != 0)
    {
        errno = EHOSTDOWN;
        return NULL;
    }
    if (port != 7687)
    {
        errno = ECONNRESET;
        return NULL;
    }
    if (strcmp(config->username, username) != 0 ||
            strcmp(config->password, password) != 0)
    {
        errno = NEO4J_INVALID_CREDENTIALS;
        return NULL;
    }
    return client_ios;
}


neo4j_iostream_t *stub_failing_connect(
        struct neo4j_connection_factory *factory,
        const char *hostname, unsigned int port, neo4j_config_t *config,
        uint_fast32_t flags, struct neo4j_logger *logger)
{
    errno = STUB_FAILURE_CODE;
    return NULL;
}


static int ios_noop_close(struct neo4j_iostream *self)
{
    return 0;
}


static void queue_message(neo4j_iostream_t *ios, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc)
{
    int result = neo4j_message_send(ios, type, argv, argc, NULL, 0, 1024);
    ck_assert_int_eq(result, 0);
}


static neo4j_message_type_t recv_message(neo4j_iostream_t *ios,
        neo4j_mpool_t *mpool, const neo4j_value_t **argv, uint16_t *argc)
{
    neo4j_message_type_t type;
    int result = neo4j_message_recv(ios, mpool, &type, argv, argc);
    ck_assert_int_eq(result, 0);
    return type;
}


static int response_recv_callback(void *cdata, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc)
{
    struct received_response *resp = (struct received_response *)cdata;
    resp->condition = 0;
    resp->type = type;
    return 0;
}


START_TEST (test_connects_URI_and_sends_init)
{
    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));
    neo4j_map_entry_t init_metadata_entries[1];
    init_metadata_entries[0] =
        neo4j_map_entry("server", neo4j_string("neo4j/1.2.3"));
    neo4j_value_t map = neo4j_map(init_metadata_entries, 1);
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, &map, 1); // INIT

    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_ne(connection, NULL);
    ck_assert_ptr_eq(connection->iostream, client_ios);

    // check resulting connection
    ck_assert(!neo4j_credentials_expired(connection));
    ck_assert_str_eq(neo4j_server_id(connection), "neo4j/1.2.3");

    // check protocol HELLO was sent
    uint8_t expected_hello[4] = { 0x60, 0x60, 0xB0, 0x17 };
    uint8_t hello[4];
    rb_extract(out_rb, hello, 4);
    ck_assert(memcmp(hello, expected_hello, 4) == 0);

    // check expected versions was sent
    uint32_t expected_versions[4] = { htonl(1), 0, 0, 0 };
    uint32_t versions[4];
    rb_extract(out_rb, versions, 16);
    ck_assert(memcmp(versions, expected_versions, 16) == 0);

    // check content sent in INIT message
    const neo4j_value_t *argv;
    uint16_t argc;
    neo4j_message_type_t type = recv_message(server_ios, &mpool, &argv, &argc);
    ck_assert(type == NEO4J_INIT_MESSAGE);
    ck_assert_int_eq(argc, 2);

    char buf[256];
    ck_assert(neo4j_type(argv[0]) == NEO4J_STRING);
    ck_assert_str_eq(neo4j_string_value(argv[0], buf, sizeof(buf)),
            config->client_id);

    ck_assert(neo4j_type(argv[1]) == NEO4J_MAP);
    ck_assert_str_eq(neo4j_string_value(
            neo4j_map_get(argv[1], "scheme"), buf, sizeof(buf)),
            "basic");
    ck_assert_str_eq(neo4j_string_value(
            neo4j_map_get(argv[1], "principal"), buf, sizeof(buf)),
            "user");
    ck_assert_str_eq(neo4j_string_value(
            neo4j_map_get(argv[1], "credentials"), buf, sizeof(buf)),
            "pass");

    neo4j_close(connection);
}
END_TEST


START_TEST (test_connects_URI_containing_credentials_and_sends_init)
{
    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));
    neo4j_map_entry_t init_metadata_entries[1];
    init_metadata_entries[0] =
        neo4j_map_entry("server", neo4j_string("neo4j/1.2.3"));
    neo4j_value_t map = neo4j_map(init_metadata_entries, 1);
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, &map, 1); // INIT

    username = "john";
    password = "smith";
    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://john:smith@localhost:7687", config, 0);
    ck_assert_ptr_ne(connection, NULL);
    ck_assert_ptr_eq(connection->iostream, client_ios);

    // check resulting connection
    ck_assert(!neo4j_credentials_expired(connection));
    ck_assert_str_eq(neo4j_server_id(connection), "neo4j/1.2.3");

    // check protocol HELLO was sent
    uint8_t expected_hello[4] = { 0x60, 0x60, 0xB0, 0x17 };
    uint8_t hello[4];
    rb_extract(out_rb, hello, 4);
    ck_assert(memcmp(hello, expected_hello, 4) == 0);

    // check expected versions was sent
    uint32_t expected_versions[4] = { htonl(1), 0, 0, 0 };
    uint32_t versions[4];
    rb_extract(out_rb, versions, 16);
    ck_assert(memcmp(versions, expected_versions, 16) == 0);

    // check content sent in INIT message
    const neo4j_value_t *argv;
    uint16_t argc;
    neo4j_message_type_t type = recv_message(server_ios, &mpool, &argv, &argc);
    ck_assert(type == NEO4J_INIT_MESSAGE);
    ck_assert_int_eq(argc, 2);

    char buf[256];
    ck_assert(neo4j_type(argv[0]) == NEO4J_STRING);
    ck_assert_str_eq(neo4j_string_value(argv[0], buf, sizeof(buf)),
            config->client_id);

    ck_assert(neo4j_type(argv[1]) == NEO4J_MAP);
    ck_assert_str_eq(neo4j_string_value(
            neo4j_map_get(argv[1], "scheme"), buf, sizeof(buf)),
            "basic");
    ck_assert_str_eq(neo4j_string_value(
            neo4j_map_get(argv[1], "principal"), buf, sizeof(buf)),
            "john");
    ck_assert_str_eq(neo4j_string_value(
            neo4j_map_get(argv[1], "credentials"), buf, sizeof(buf)),
            "smith");

    neo4j_close(connection);
}
END_TEST


START_TEST (test_connects_tcp_and_sends_init)
{
    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));
    neo4j_map_entry_t init_metadata_entries[1];
    init_metadata_entries[0] =
        neo4j_map_entry("server", neo4j_string("neo4j/1.2.3"));
    neo4j_value_t map = neo4j_map(init_metadata_entries, 1);
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, &map, 1); // INIT

    neo4j_connection_t *connection = neo4j_tcp_connect(
            "localhost", 7687, config, 0);
    ck_assert_ptr_ne(connection, NULL);
    ck_assert_ptr_eq(connection->iostream, client_ios);

    // check resulting connection
    ck_assert(!neo4j_credentials_expired(connection));
    ck_assert_str_eq(neo4j_server_id(connection), "neo4j/1.2.3");

    // check protocol HELLO was sent
    uint8_t expected_hello[4] = { 0x60, 0x60, 0xB0, 0x17 };
    uint8_t hello[4];
    rb_extract(out_rb, hello, 4);
    ck_assert(memcmp(hello, expected_hello, 4) == 0);

    // check expected versions was sent
    uint32_t expected_versions[4] = { htonl(1), 0, 0, 0 };
    uint32_t versions[4];
    rb_extract(out_rb, versions, 16);
    ck_assert(memcmp(versions, expected_versions, 16) == 0);

    // check content sent in INIT message
    const neo4j_value_t *argv;
    uint16_t argc;
    neo4j_message_type_t type = recv_message(server_ios, &mpool, &argv, &argc);
    ck_assert(type == NEO4J_INIT_MESSAGE);
    ck_assert_int_eq(argc, 2);

    char buf[256];
    ck_assert(neo4j_type(argv[0]) == NEO4J_STRING);
    ck_assert_str_eq(neo4j_string_value(argv[0], buf, sizeof(buf)),
            config->client_id);

    ck_assert(neo4j_type(argv[1]) == NEO4J_MAP);
    ck_assert_str_eq(neo4j_string_value(
            neo4j_map_get(argv[1], "scheme"), buf, sizeof(buf)),
            "basic");
    ck_assert_str_eq(neo4j_string_value(
            neo4j_map_get(argv[1], "principal"), buf, sizeof(buf)),
            username);
    ck_assert_str_eq(neo4j_string_value(
            neo4j_map_get(argv[1], "credentials"), buf, sizeof(buf)),
            password);

    neo4j_close(connection);
}
END_TEST


START_TEST (test_expired_credentials)
{
    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));
    neo4j_map_entry_t init_metadata_entries[1];
    init_metadata_entries[0] =
        neo4j_map_entry("credentials_expired", neo4j_bool(true));
    neo4j_value_t map = neo4j_map(init_metadata_entries, 1);
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, &map, 1); // INIT

    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_ne(connection, NULL);
    ck_assert_ptr_eq(connection->iostream, client_ios);

    ck_assert(neo4j_credentials_expired(connection));

    neo4j_close(connection);
}
END_TEST


START_TEST (test_fails_invalid_URI)
{
    neo4j_connection_t *connection = neo4j_connect(
            "neo4j:/localhost:7687", config, 0);
    ck_assert_ptr_eq(connection, NULL);
    ck_assert_int_eq(errno, NEO4J_INVALID_URI);
}
END_TEST


START_TEST (test_fails_unknown_URI_scheme)
{
    neo4j_connection_t *connection = neo4j_connect(
            "foo://localhost:7687", config, 0);
    ck_assert_ptr_eq(connection, NULL);
    ck_assert_int_eq(errno, NEO4J_UNKNOWN_URI_SCHEME);
}
END_TEST


START_TEST (test_returns_einval_for_invalid_close_argument)
{
    ck_assert_int_eq(neo4j_close(NULL), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST


START_TEST (test_fails_if_connection_factory_fails)
{
    struct neo4j_connection_factory stub_failing_factory =
    {
        .tcp_connect = stub_failing_connect
    };

    neo4j_config_set_connection_factory(config, &stub_failing_factory);

    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_eq(connection, NULL);
    ck_assert_int_eq(errno, STUB_FAILURE_CODE);

    neo4j_close(connection);
}
END_TEST


START_TEST (test_fails_if_unknown_protocol)
{
    uint32_t version = htonl(0);
    rb_append(in_rb, &version, sizeof(version));

    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_eq(connection, NULL);
    ck_assert_int_eq(errno, NEO4J_PROTOCOL_NEGOTIATION_FAILED);

    neo4j_close(connection);
}
END_TEST


START_TEST (test_fails_if_init_failure)
{
    neo4j_config_set_logger_provider(config, NULL);

    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));

    failure_metadata_entries[0] = neo4j_map_entry("code",
            neo4j_string("Neo.ClientError.Security.Unauthorized"));
    queue_message(server_ios, NEO4J_FAILURE_MESSAGE,
            &failure_metadata, 1); // INIT
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0); // ACK_FAILURE

    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_eq(connection, NULL);
    ck_assert_int_eq(errno, NEO4J_INVALID_CREDENTIALS);
}
END_TEST


START_TEST (test_fails_if_init_failure_and_close)
{
    neo4j_config_set_logger_provider(config, NULL);

    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));

    failure_metadata_entries[0] = neo4j_map_entry("code",
            neo4j_string("Neo.ClientError.Security.Unauthorized"));
    queue_message(server_ios, NEO4J_FAILURE_MESSAGE,
            &failure_metadata, 1); // INIT
    // No response to ACK_FAILURE

    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_eq(connection, NULL);
    ck_assert_int_eq(errno, NEO4J_INVALID_CREDENTIALS);
}
END_TEST


START_TEST (test_fails_if_connection_closes)
{
    neo4j_config_set_logger_provider(config, NULL);

    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_eq(connection, NULL);
    ck_assert_int_eq(errno, NEO4J_PROTOCOL_NEGOTIATION_FAILED);

    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));

    connection = neo4j_connect("neo4j://localhost:7687", config, 0);
    ck_assert_ptr_eq(connection, NULL);
    ck_assert_int_eq(errno, NEO4J_CONNECTION_CLOSED);
}
END_TEST


START_TEST (test_drains_outstanding_requests_on_close)
{
    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, &empty_map, 1); // INIT
    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_ne(connection, NULL);

    struct received_response resp = { 1, NULL };
    int result = neo4j_session_run(connection, &mpool, "RETURN 1", neo4j_null,
            response_recv_callback, &resp);
    ck_assert_int_eq(result, 0);

    neo4j_close(connection);

    ck_assert(resp.type == NULL);
}
END_TEST


START_TEST (test_awaits_inflight_requests_on_close)
{
    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, &empty_map, 1); // INIT
    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_ne(connection, NULL);

    struct received_response resp1 = { 1, NULL };
    int result = neo4j_session_run(connection, &mpool, "RETURN 1", neo4j_null,
            response_recv_callback, &resp1);
    ck_assert_int_eq(result, 0);

    struct received_response resp2 = { 1, NULL };
    result = neo4j_session_pull_all(connection, &mpool,
            response_recv_callback, &resp2);
    ck_assert_int_eq(result, 0);

    // await only the first request (leaves the 2nd inflight)
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0); // RUN
    result = neo4j_session_sync(connection, &(resp1.condition));
    ck_assert_int_eq(result, 0);
    ck_assert(resp1.type == NEO4J_SUCCESS_MESSAGE);
    ck_assert_int_eq(resp2.condition, 1);

    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0); // PULL_ALL
    neo4j_close(connection);
    ck_assert(resp2.type == NEO4J_SUCCESS_MESSAGE);
}
END_TEST


START_TEST (test_sends_reset_on_reset)
{
    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, &empty_map, 1); // INIT
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0); // RESET

    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_ne(connection, NULL);

    neo4j_reset(connection);

    // skip HELLO and protocol negotiation
    rb_discard(out_rb, 20);

    // INIT msg
    const neo4j_value_t *argv;
    uint16_t argc;
    neo4j_message_type_t type = recv_message(server_ios, &mpool, &argv, &argc);
    ck_assert(type == NEO4J_INIT_MESSAGE);
    ck_assert_int_eq(argc, 2);

    // RESET msg
    type = recv_message(server_ios, &mpool, &argv, &argc);
    ck_assert(type == NEO4J_RESET_MESSAGE);
    ck_assert_int_eq(argc, 0);

    neo4j_close(connection);
}
END_TEST


START_TEST (test_drains_outstanding_requests_on_reset)
{
    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, &empty_map, 1); // INIT

    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_ne(connection, NULL);

    struct received_response resp = { 1, NULL };
    int result = neo4j_session_run(connection, &mpool, "RETURN 1", neo4j_null,
            response_recv_callback, &resp);
    ck_assert_int_eq(result, 0);

    neo4j_reset(connection);
    ck_assert(resp.type == NULL);

    neo4j_close(connection);
}
END_TEST


START_TEST (test_awaits_inflight_requests_on_reset)
{
    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, &empty_map, 1); // INIT

    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_ne(connection, NULL);

    struct received_response resp1 = { 1, NULL };
    int result = neo4j_session_run(connection, &mpool, "RETURN 1", neo4j_null,
            response_recv_callback, &resp1);
    ck_assert_int_eq(result, 0);

    struct received_response resp2 = { 1, NULL };
    result = neo4j_session_pull_all(connection, &mpool,
            response_recv_callback, &resp2);
    ck_assert_int_eq(result, 0);

    // await only the first request (leaves the 2nd inflight)
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0); // RUN
    result = neo4j_session_sync(connection, &(resp1.condition));
    ck_assert_int_eq(result, 0);
    ck_assert(resp1.type == NEO4J_SUCCESS_MESSAGE);
    ck_assert_int_eq(resp2.condition, 1);

    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0); // PULL_ALL
    neo4j_reset(connection);
    ck_assert(resp2.type == NEO4J_SUCCESS_MESSAGE);

    neo4j_close(connection);
}
END_TEST


START_TEST (test_drains_requests_and_acks_after_failure)
{
    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, &empty_map, 1); // INIT

    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_ne(connection, NULL);

    struct received_response resp1 = { 1, NULL };
    int result = neo4j_session_run(connection, &mpool, "RETURN 1", neo4j_null,
            response_recv_callback, &resp1);
    ck_assert_int_eq(result, 0);

    struct received_response resp2 = { 1, NULL };
    result = neo4j_session_pull_all(connection, &mpool,
            response_recv_callback, &resp2);
    ck_assert_int_eq(result, 0);

    struct received_response resp3 = { 1, NULL };
    result = neo4j_session_run(connection, &mpool, "RETURN 2", neo4j_null,
            response_recv_callback, &resp3);
    ck_assert_int_eq(result, 0);

    queue_message(server_ios, NEO4J_FAILURE_MESSAGE, &failure_metadata, 1); // RUN
    queue_message(server_ios, NEO4J_IGNORED_MESSAGE, NULL, 0); // PULL_ALL
    queue_message(server_ios, NEO4J_IGNORED_MESSAGE, NULL, 0); // RUN
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0); // ACK_FAILURE
    result = neo4j_session_sync(connection, &(resp1.condition));
    ck_assert_int_eq(result, 0);
    ck_assert(resp1.type == NEO4J_FAILURE_MESSAGE);
    ck_assert(resp2.type == NEO4J_IGNORED_MESSAGE);
    ck_assert(resp3.type == NEO4J_IGNORED_MESSAGE);

    neo4j_close(connection);
}
END_TEST


START_TEST (test_cant_continue_after_eproto_in_failure)
{
    neo4j_config_set_logger_provider(config, NULL);

    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, &empty_map, 1); // INIT

    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_ne(connection, NULL);

    struct received_response resp1 = { 1, NULL };
    int result = neo4j_session_run(connection, &mpool, "RETURN 1", neo4j_null,
            response_recv_callback, &resp1);
    ck_assert_int_eq(result, 0);

    struct received_response resp2 = { 1, NULL };
    result = neo4j_session_pull_all(connection, &mpool,
            response_recv_callback, &resp2);
    ck_assert_int_eq(result, 0);

    queue_message(server_ios, NEO4J_FAILURE_MESSAGE, &failure_metadata, 1); // RUN
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0); // PULL_ALL
    result = neo4j_session_sync(connection, NULL);
    ck_assert_int_eq(result, -1);
    ck_assert_int_eq(errno, EPROTO);
    ck_assert(resp1.type == NEO4J_FAILURE_MESSAGE);
    ck_assert(resp2.type == NULL);

    result = neo4j_session_run(connection, &mpool, "RETURN 2", neo4j_null,
            response_recv_callback, &resp1);
    ck_assert_int_eq(result, -1);
    ck_assert_int_eq(errno, NEO4J_SESSION_FAILED);

    neo4j_close(connection);
}
END_TEST


START_TEST (test_cant_continue_after_eproto_in_ack_failure)
{
    neo4j_config_set_logger_provider(config, NULL);

    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, &empty_map, 1); // INIT

    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_ne(connection, NULL);

    struct received_response resp1 = { 1, NULL };
    int result = neo4j_session_run(connection, &mpool, "RETURN 1", neo4j_null,
            response_recv_callback, &resp1);
    ck_assert_int_eq(result, 0);

    struct received_response resp2 = { 1, NULL };
    result = neo4j_session_pull_all(connection, &mpool,
            response_recv_callback, &resp2);
    ck_assert_int_eq(result, 0);

    queue_message(server_ios, NEO4J_FAILURE_MESSAGE, &failure_metadata, 1); // RUN
    queue_message(server_ios, NEO4J_IGNORED_MESSAGE, NULL, 0); // PULL_ALL
    queue_message(server_ios, NEO4J_FAILURE_MESSAGE, &failure_metadata, 1); // ACK_FAILURE
    result = neo4j_session_sync(connection, NULL);
    ck_assert_int_eq(result, -1);
    ck_assert_int_eq(errno, EPROTO);
    ck_assert(resp1.type == NEO4J_FAILURE_MESSAGE);
    ck_assert(resp2.type == NEO4J_IGNORED_MESSAGE);

    result = neo4j_session_run(connection, &mpool, "RETURN 2", neo4j_null,
            response_recv_callback, &resp1);
    ck_assert_int_eq(result, -1);
    ck_assert_int_eq(errno, NEO4J_SESSION_FAILED);

    neo4j_close(connection);
}
END_TEST


START_TEST (test_drains_acks_when_closed)
{
    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, &empty_map, 1); // INIT

    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_ne(connection, NULL);

    struct received_response resp1 = { 1, NULL };
    int result = neo4j_session_run(connection, &mpool, "RETURN 1", neo4j_null,
            response_recv_callback, &resp1);
    ck_assert_int_eq(result, 0);

    struct received_response resp2 = { 1, NULL };
    result = neo4j_session_pull_all(connection, &mpool,
            response_recv_callback, &resp2);
    ck_assert_int_eq(result, 0);

    queue_message(server_ios, NEO4J_FAILURE_MESSAGE, &failure_metadata, 1); // RUN
    queue_message(server_ios, NEO4J_IGNORED_MESSAGE, NULL, 0); // PULL_ALL
    // no queued response for the ACK_FAILURE => connection closed

    result = neo4j_session_sync(connection, &(resp1.condition));
    ck_assert_int_eq(result, -1);
    ck_assert_int_eq(errno, NEO4J_CONNECTION_CLOSED);
    ck_assert(resp1.type == NEO4J_FAILURE_MESSAGE);
    ck_assert(resp2.type == NEO4J_IGNORED_MESSAGE);

    neo4j_close(connection);
}
END_TEST


TCase* connection_tcase(void)
{
    TCase *tc = tcase_create("connection");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_connects_URI_and_sends_init);
    tcase_add_test(tc, test_connects_URI_containing_credentials_and_sends_init);
    tcase_add_test(tc, test_connects_tcp_and_sends_init);
    tcase_add_test(tc, test_expired_credentials);
    tcase_add_test(tc, test_fails_invalid_URI);
    tcase_add_test(tc, test_fails_unknown_URI_scheme);
    tcase_add_test(tc, test_returns_einval_for_invalid_close_argument);
    tcase_add_test(tc, test_fails_if_connection_factory_fails);
    tcase_add_test(tc, test_fails_if_unknown_protocol);
    tcase_add_test(tc, test_fails_if_init_failure);
    tcase_add_test(tc, test_fails_if_init_failure_and_close);
    tcase_add_test(tc, test_fails_if_connection_closes);
    tcase_add_test(tc, test_drains_outstanding_requests_on_close);
    tcase_add_test(tc, test_awaits_inflight_requests_on_close);
    tcase_add_test(tc, test_sends_reset_on_reset);
    tcase_add_test(tc, test_drains_outstanding_requests_on_reset);
    tcase_add_test(tc, test_awaits_inflight_requests_on_reset);
    tcase_add_test(tc, test_drains_requests_and_acks_after_failure);
    tcase_add_test(tc, test_cant_continue_after_eproto_in_failure);
    tcase_add_test(tc, test_cant_continue_after_eproto_in_ack_failure);
    tcase_add_test(tc, test_drains_acks_when_closed);
    return tc;
}
