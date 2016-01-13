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
#include "../src/lib/connection.h"
#include "../src/lib/util.h"
#include "memiostream.h"
#include <check.h>
#include <errno.h>


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


static struct neo4j_logger_provider *logger_provider;
static ring_buffer_t *in_rb;
static ring_buffer_t *out_rb;
static neo4j_iostream_t *client_ios;
static struct neo4j_connection_factory stub_factory;
static neo4j_config_t *config;
static const char *username = "username";
static const char *password = "password";
static int (*ios_close)(struct neo4j_iostream *self);


static void setup(void)
{
    logger_provider = neo4j_std_logger_provider(stderr, NEO4J_LOG_ERROR, 0);
    in_rb = rb_alloc(1024);
    out_rb = rb_alloc(1024);
    client_ios = neo4j_memiostream(in_rb, out_rb);
    ios_close = client_ios->close;
    client_ios->close = ios_noop_close;

    stub_factory.tcp_connect = stub_connect;
    config = neo4j_new_config();
    neo4j_config_set_logger_provider(config, logger_provider);
    neo4j_config_set_connection_factory(config, &stub_factory);
    neo4j_config_set_username(config, username);
    neo4j_config_set_password(config, password);
}


static void teardown(void)
{
    neo4j_config_free(config);
    ios_close(client_ios);
    rb_free(in_rb);
    rb_free(out_rb);
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


START_TEST (test_connects_URI_and_establishes_protocol)
{
    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));

    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_ne(connection, NULL);
    ck_assert_ptr_eq(connection->iostream, client_ios);

    ck_assert_int_eq(rb_used(out_rb), 20);
    uint8_t expected_hello[4] = { 0x60, 0x60, 0xB0, 0x17 };
    uint8_t hello[4];
    rb_extract(out_rb, hello, 4);
    ck_assert(memcmp(hello, expected_hello, 4) == 0);

    uint32_t expected_versions[4] = { htonl(1), 0, 0, 0 };
    uint32_t versions[4];
    rb_extract(out_rb, versions, 16);
    ck_assert(memcmp(versions, expected_versions, 16) == 0);

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


START_TEST (test_connects_URI_containing_credentials)
{
    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));

    username = "john";
    password = "smith";
    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://john:smith@localhost:7687", config, 0);
    ck_assert_ptr_ne(connection, NULL);
    ck_assert_ptr_eq(connection->iostream, client_ios);

    ck_assert_int_eq(rb_used(out_rb), 20);
    uint8_t expected_hello[4] = { 0x60, 0x60, 0xB0, 0x17 };
    uint8_t hello[4];
    rb_extract(out_rb, hello, 4);
    ck_assert(memcmp(hello, expected_hello, 4) == 0);

    uint32_t expected_versions[4] = { htonl(1), 0, 0, 0 };
    uint32_t versions[4];
    rb_extract(out_rb, versions, 16);
    ck_assert(memcmp(versions, expected_versions, 16) == 0);

    neo4j_close(connection);
}
END_TEST


START_TEST (test_returns_einval_for_invalid_close_argument)
{
    ck_assert_int_eq(neo4j_close(NULL), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST


START_TEST (test_connects_tcp_and_establishes_protocol)
{
    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));

    neo4j_connection_t *connection = neo4j_tcp_connect(
            "localhost", 7687, config, 0);
    ck_assert_ptr_ne(connection, NULL);
    ck_assert_ptr_eq(connection->iostream, client_ios);

    ck_assert_int_eq(rb_used(out_rb), 20);
    uint8_t expected_hello[4] = { 0x60, 0x60, 0xB0, 0x17 };
    uint8_t hello[4];
    rb_extract(out_rb, hello, 4);
    ck_assert(memcmp(hello, expected_hello, 4) == 0);

    uint32_t expected_versions[4] = { htonl(1), 0, 0, 0 };
    uint32_t versions[4];
    rb_extract(out_rb, versions, 16);
    ck_assert(memcmp(versions, expected_versions, 16) == 0);

    neo4j_close(connection);
}
END_TEST


START_TEST (test_fails_if_connection_factory_fails)
{
    struct neo4j_connection_factory stub_failing_factory =
    {
        .tcp_connect = stub_failing_connect
    };

    neo4j_config_set_logger_provider(config, NULL);
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
    neo4j_config_set_logger_provider(config, NULL);

    uint32_t version = htonl(0);
    rb_append(in_rb, &version, sizeof(version));

    neo4j_connection_t *connection = neo4j_connect(
            "neo4j://localhost:7687", config, 0);
    ck_assert_ptr_eq(connection, NULL);
    ck_assert_int_eq(errno, NEO4J_PROTOCOL_NEGOTIATION_FAILED);

    neo4j_close(connection);
}
END_TEST


TCase* connection_tcase(void)
{
    TCase *tc = tcase_create("connection");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_connects_URI_and_establishes_protocol);
    tcase_add_test(tc, test_fails_invalid_URI);
    tcase_add_test(tc, test_fails_unknown_URI_scheme);
    tcase_add_test(tc, test_connects_URI_containing_credentials);
    tcase_add_test(tc, test_returns_einval_for_invalid_close_argument);
    tcase_add_test(tc, test_connects_tcp_and_establishes_protocol);
    tcase_add_test(tc, test_fails_if_connection_factory_fails);
    tcase_add_test(tc, test_fails_if_unknown_protocol);
    return tc;
}
