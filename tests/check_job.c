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
#include "buffered_iostream.h"
#include "../src/lib/chunking_iostream.h"
#include "../src/lib/connection.h"
#include "../src/lib/deserialization.h"
#include "../src/lib/messages.h"
#include "../src/lib/session.h"
#include "../src/lib/serialization.h"
#include "../src/lib/util.h"
#include <check.h>
#include <errno.h>


static neo4j_iostream_t *stub_connect(struct neo4j_connection_factory *factory,
        const char *hostname, int port, neo4j_config_t *config,
        uint_fast32_t flags, struct neo4j_logger *logger);
static neo4j_message_type_t recv_message(neo4j_iostream_t *ios,
        neo4j_mpool_t *mpool, const neo4j_value_t **argv, uint16_t *argc);
static void queue_message(neo4j_iostream_t *ios, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc);
static void queue_run_success(neo4j_iostream_t *ios);
static void queue_record(neo4j_iostream_t *ios);
static void queue_pull_all_success(neo4j_iostream_t *ios);
static void queue_failure(neo4j_iostream_t *ios);


static struct neo4j_logger_provider *logger_provider;
static ring_buffer_t *in_rb;
static ring_buffer_t *out_rb;
static neo4j_iostream_t *client_ios;
static neo4j_iostream_t *server_ios;
static struct neo4j_connection_factory stub_factory;
static neo4j_config_t *config;
neo4j_mpool_t mpool;
neo4j_connection_t *connection;
neo4j_session_t *session;


static void setup(void)
{
    logger_provider = neo4j_std_logger_provider(stderr, NEO4J_LOG_ERROR, 0);
    in_rb = rb_alloc(1024);
    out_rb = rb_alloc(1024);
    client_ios = neo4j_buffered_iostream(in_rb, out_rb);
    server_ios = neo4j_buffered_iostream(out_rb, in_rb);

    stub_factory.tcp_connect = stub_connect;
    config = neo4j_new_config();
    neo4j_config_set_logger_provider(config, logger_provider);
    neo4j_config_set_connection_factory(config, &stub_factory);

    mpool = neo4j_std_mpool(config);

    uint32_t version = htonl(1);
    rb_append(in_rb, &version, sizeof(version));

    connection = neo4j_connect("neo4j://localhost:7687", config, 0);
    ck_assert_ptr_ne(connection, NULL);

    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0);
    session = neo4j_new_session(connection);
    ck_assert_ptr_ne(session, NULL);

    rb_clear(out_rb);
}


static void teardown(void)
{
    if (session != NULL)
    {
        neo4j_end_session(session);
    }
    neo4j_close(connection);
    neo4j_mpool_drain(&mpool);
    neo4j_ios_close(server_ios);
    neo4j_config_free(config);
    rb_free(in_rb);
    rb_free(out_rb);
    neo4j_std_logger_provider_free(logger_provider);
}


neo4j_iostream_t *stub_connect(struct neo4j_connection_factory *factory,
            const char *hostname, int port, neo4j_config_t *config,
            uint_fast32_t flags, struct neo4j_logger *logger)
{
    return client_ios;
}


neo4j_message_type_t recv_message(neo4j_iostream_t *ios, neo4j_mpool_t *mpool,
        const neo4j_value_t **argv, uint16_t *argc)
{
    neo4j_message_type_t type;
    int result = neo4j_message_recv(ios, mpool, &type, argv, argc);
    ck_assert_int_eq(result, 0);
    return type;
}


void queue_message(neo4j_iostream_t *ios, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc)
{
    int result = neo4j_message_send(ios, type, argv, argc, 0, 1024);
    ck_assert_int_eq(result, 0);
}


void queue_run_success(neo4j_iostream_t *ios)
{
    neo4j_value_t result_fields[2] =
            { neo4j_string("field_one"), neo4j_string("field_two") };
    neo4j_map_entry_t fields = neo4j_map_entry(
            neo4j_string("fields"), neo4j_list(result_fields, 2));
    neo4j_value_t argv[1] = { neo4j_map(&fields, 1) };
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, argv, 1);
}


void queue_record(neo4j_iostream_t *ios)
{
    neo4j_value_t argv[1] = { neo4j_list(NULL, 0) };
    queue_message(server_ios, NEO4J_RECORD_MESSAGE, argv, 1);
}


void queue_pull_all_success(neo4j_iostream_t *ios)
{
    neo4j_map_entry_t fields = neo4j_map_entry(
            neo4j_string("type"), neo4j_string("r"));
    neo4j_value_t argv[1] = { neo4j_map(&fields, 1) };
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, argv, 1);
}


void queue_failure(neo4j_iostream_t *ios)
{
    neo4j_map_entry_t fields[2] =
        { neo4j_map_entry(
                neo4j_string("code"), neo4j_string("Neo.ClientError.Sample")),
          neo4j_map_entry(
                neo4j_string("message"), neo4j_string("Sample error")) };
    neo4j_value_t argv[1] = { neo4j_map(fields, 2) };
    queue_message(server_ios, NEO4J_FAILURE_MESSAGE, argv, 1);
}


START_TEST (test_job_returns_results_and_completes)
{
    neo4j_result_stream_t *results = neo4j_run(session, "RETURN 1", NULL, 0);
    ck_assert_ptr_ne(results, NULL);
    ck_assert(rb_is_empty(out_rb)); // message is queued but not sent

    queue_run_success(server_ios); // RUN
    queue_record(server_ios); // PULL_ALL
    queue_record(server_ios); // PULL_ALL
    queue_pull_all_success(server_ios); // PULL_ALL

    ck_assert_int_eq(neo4j_check_failure(results), 0);

    const neo4j_value_t *argv;
    uint16_t argc;
    neo4j_message_type_t type = recv_message(server_ios, &mpool,
            &argv, &argc);
    ck_assert(type == NEO4J_RUN_MESSAGE);
    ck_assert_int_eq(argc, 2);
    ck_assert(neo4j_type(argv[0]) == NEO4J_STRING);
    char buf[128];
    ck_assert_str_eq(neo4j_string_value(argv[0], buf, sizeof(buf)),
            "RETURN 1");
    ck_assert(neo4j_type(argv[1]) == NEO4J_MAP);
    ck_assert_int_eq(neo4j_map_size(argv[1]), 0);

    ck_assert_ptr_ne(neo4j_fetch_next(results), NULL);
    ck_assert_ptr_ne(neo4j_fetch_next(results), NULL);
    ck_assert_ptr_eq(neo4j_fetch_next(results), NULL);
    ck_assert_int_eq(errno, 0);

    ck_assert_int_eq(neo4j_check_failure(results), 0);
    ck_assert_int_eq(neo4j_close_results(results), 0);

    ck_assert(rb_is_empty(in_rb));
}
END_TEST


START_TEST (test_job_returns_run_metadata)
{
    neo4j_result_stream_t *results = neo4j_run(session, "RETURN 1", NULL, 0);
    ck_assert_ptr_ne(results, NULL);
    ck_assert(rb_is_empty(out_rb)); // message is queued but not sent

    queue_run_success(server_ios); // RUN
    queue_pull_all_success(server_ios); // PULL_ALL

    ck_assert_int_eq(neo4j_nfields(results), 2);
    ck_assert_str_eq(neo4j_fieldname(results, 0), "field_one");
    ck_assert_str_eq(neo4j_fieldname(results, 1), "field_two");

    ck_assert_ptr_eq(neo4j_fetch_next(results), NULL);
    ck_assert_int_eq(errno, 0);

    ck_assert_int_eq(neo4j_nfields(results), 2);
    ck_assert_str_eq(neo4j_fieldname(results, 0), "field_one");
    ck_assert_str_eq(neo4j_fieldname(results, 1), "field_two");
    ck_assert_int_eq(neo4j_check_failure(results), 0);
    ck_assert_int_eq(neo4j_close_results(results), 0);

    ck_assert(rb_is_empty(in_rb));
}
END_TEST


START_TEST (test_job_returns_failure_when_statement_fails)
{
    queue_failure(server_ios); // RUN
    queue_message(server_ios, NEO4J_IGNORED_MESSAGE, NULL, 0); // PULL_ALL
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0); // ACK_FAILURE

    neo4j_result_stream_t *results = neo4j_run(session, "bad query", NULL, 0);
    ck_assert_ptr_ne(results, NULL);

    int result = neo4j_check_failure(results);
    ck_assert_int_eq(result, NEO4J_STATEMENT_EVALUATION_FAILED);

    ck_assert_ptr_eq(neo4j_fetch_next(results), NULL);
    ck_assert_int_eq(errno, NEO4J_STATEMENT_EVALUATION_FAILED);
    result = neo4j_check_failure(results);
    ck_assert_int_eq(result, NEO4J_STATEMENT_EVALUATION_FAILED);

    ck_assert_int_eq(neo4j_close_results(results), 0);

    ck_assert(rb_is_empty(in_rb));
}
END_TEST


START_TEST (test_job_returns_failure_during_streaming)
{
    neo4j_result_stream_t *results = neo4j_run(session, "RETURN 1", NULL, 0);
    ck_assert_ptr_ne(results, NULL);
    ck_assert(rb_is_empty(out_rb)); // message is queued but not sent

    queue_run_success(server_ios); // RUN
    queue_record(server_ios); // PULL_ALL
    queue_failure(server_ios); // PULL_ALL
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0); // ACK_FAILURE

    ck_assert_int_eq(neo4j_check_failure(results), 0);

    ck_assert_ptr_ne(neo4j_fetch_next(results), NULL);
    ck_assert_ptr_eq(neo4j_fetch_next(results), NULL);
    ck_assert_int_eq(errno, NEO4J_STATEMENT_EVALUATION_FAILED);

    int result = neo4j_check_failure(results);
    ck_assert_int_eq(result, NEO4J_STATEMENT_EVALUATION_FAILED);

    ck_assert_ptr_eq(neo4j_fetch_next(results), NULL);
    ck_assert_int_eq(errno, NEO4J_STATEMENT_EVALUATION_FAILED);
    result = neo4j_check_failure(results);
    ck_assert_int_eq(result, NEO4J_STATEMENT_EVALUATION_FAILED);

    ck_assert_int_eq(neo4j_close_results(results), 0);

    ck_assert(rb_is_empty(in_rb));
}
END_TEST


START_TEST (test_job_skips_results_after_session_close)
{
    neo4j_result_stream_t *results = neo4j_run(session, "RETURN 1", NULL, 0);
    ck_assert_ptr_ne(results, NULL);

    queue_run_success(server_ios); // RUN
    queue_record(server_ios); // PULL_ALL
    queue_record(server_ios); // PULL_ALL
    queue_record(server_ios); // PULL_ALL
    queue_pull_all_success(server_ios); // PULL_ALL

    ck_assert_ptr_ne(neo4j_fetch_next(results), NULL);
    ck_assert_ptr_ne(neo4j_fetch_next(results), NULL);

    neo4j_end_session(session);
    session = NULL;

    ck_assert_ptr_eq(neo4j_fetch_next(results), NULL);
    ck_assert_int_eq(errno, NEO4J_SESSION_ENDED);
    ck_assert_int_eq(neo4j_check_failure(results), NEO4J_SESSION_ENDED);

    ck_assert_int_eq(neo4j_close_results(results), 0);
    ck_assert(rb_is_empty(in_rb));
}
END_TEST


START_TEST (test_job_returns_same_failure_after_session_close)
{
    queue_failure(server_ios); // RUN
    queue_message(server_ios, NEO4J_IGNORED_MESSAGE, NULL, 0); // PULL_ALL
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0); // ACK_FAILURE

    neo4j_result_stream_t *results = neo4j_run(session, "bad query", NULL, 0);
    ck_assert_ptr_ne(results, NULL);

    int result = neo4j_check_failure(results);
    ck_assert_int_eq(result, NEO4J_STATEMENT_EVALUATION_FAILED);
    ck_assert_ptr_eq(neo4j_fetch_next(results), NULL);
    ck_assert_int_eq(errno, NEO4J_STATEMENT_EVALUATION_FAILED);

    neo4j_end_session(session);
    session = NULL;

    result = neo4j_check_failure(results);
    ck_assert_int_eq(result, NEO4J_STATEMENT_EVALUATION_FAILED);
    ck_assert_ptr_eq(neo4j_fetch_next(results), NULL);
    ck_assert_int_eq(errno, NEO4J_STATEMENT_EVALUATION_FAILED);

    ck_assert_int_eq(neo4j_close_results(results), 0);

    ck_assert(rb_is_empty(in_rb));
}
END_TEST


TCase* job_tcase(void)
{
    TCase *tc = tcase_create("job");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_job_returns_results_and_completes);
    tcase_add_test(tc, test_job_returns_run_metadata);
    tcase_add_test(tc, test_job_returns_failure_when_statement_fails);
    tcase_add_test(tc, test_job_returns_failure_during_streaming);
    tcase_add_test(tc, test_job_skips_results_after_session_close);
    tcase_add_test(tc, test_job_returns_same_failure_after_session_close);
    return tc;
}
