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
#include "../src/chunking_iostream.h"
#include "../src/connection.h"
#include "../src/deserialization.h"
#include "../src/messages.h"
#include "../src/serialization.h"
#include "../src/util.h"
#include "memiostream.h"
#include <check.h>
#include <errno.h>


static neo4j_iostream_t *stub_connect(struct neo4j_connection_factory *factory,
        const char *hostname, unsigned int port, neo4j_config_t *config,
        uint_fast32_t flags, struct neo4j_logger *logger);
static neo4j_message_type_t recv_message(neo4j_iostream_t *ios,
        neo4j_mpool_t *mpool, const neo4j_value_t **argv, uint16_t *argc);
static void queue_message(neo4j_iostream_t *ios, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc);
static void queue_run_success(neo4j_iostream_t *ios);
static void queue_record(neo4j_iostream_t *ios);
static void queue_stream_end_success(neo4j_iostream_t *ios);
static void queue_stream_end_success_with_counts(neo4j_iostream_t *ios);
static void queue_stream_end_success_with_profile(neo4j_iostream_t *ios);
static void queue_stream_end_success_with_plan(neo4j_iostream_t *ios);
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

    neo4j_value_t empty_map = neo4j_map(NULL, 0);
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, &empty_map, 1); // INIT

    connection = neo4j_connect("neo4j://localhost:7687", config, 0);
    ck_assert_ptr_ne(connection, NULL);

    rb_clear(out_rb);
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
    int result = neo4j_message_send(ios, type, argv, argc, NULL, 0, 1024);
    ck_assert_int_eq(result, 0);
}


void queue_run_success(neo4j_iostream_t *ios)
{
    neo4j_value_t result_fields[2] =
            { neo4j_string("field_one"), neo4j_string("field_two") };
    neo4j_map_entry_t fields =
        neo4j_map_entry("fields", neo4j_list(result_fields, 2));
    neo4j_value_t argv[1] = { neo4j_map(&fields, 1) };
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, argv, 1);
}


void queue_record(neo4j_iostream_t *ios)
{
    neo4j_value_t argv[1] = { neo4j_list(NULL, 0) };
    queue_message(server_ios, NEO4J_RECORD_MESSAGE, argv, 1);
}


void queue_stream_end_success(neo4j_iostream_t *ios)
{
    neo4j_map_entry_t fields[1] =
        { neo4j_map_entry("type", neo4j_string("rw")) };
    neo4j_value_t argv[1] = { neo4j_map(fields, 1) };
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, argv, 1);
}


void queue_stream_end_success_with_counts(neo4j_iostream_t *ios)
{
    neo4j_map_entry_t counts = neo4j_map_entry("nodes-created", neo4j_int(99));
    neo4j_map_entry_t fields[2] =
        { neo4j_map_entry("type", neo4j_string("rw")),
          neo4j_map_entry("stats", neo4j_map(&counts, 1)) };
    neo4j_value_t argv[1] = { neo4j_map(fields, 2) };
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, argv, 1);
}


void queue_stream_end_success_with_profile(neo4j_iostream_t *ios)
{
    neo4j_map_entry_t profargs[3] =
        { neo4j_map_entry("version", neo4j_string("CYPHER 3.0")),
          neo4j_map_entry("planner", neo4j_string("COST")),
          neo4j_map_entry("runtime", neo4j_string("INTERPRETTED")) };
    neo4j_value_t ids[1] = { neo4j_string("n") };

    neo4j_map_entry_t prof[6] =
        { neo4j_map_entry("args", neo4j_map(profargs, 3)),
          neo4j_map_entry("identifiers", neo4j_list(ids, 1)),
          neo4j_map_entry("dbHits", neo4j_int(42)),
          neo4j_map_entry("children", neo4j_list(NULL, 0)),
          neo4j_map_entry("rows", neo4j_int(1)),
          neo4j_map_entry("operatorType", neo4j_string("ProduceResults")),
        };

    neo4j_map_entry_t fields[2] =
        { neo4j_map_entry("type", neo4j_string("rw")),
          neo4j_map_entry("profile", neo4j_map(prof, 6)) };
    neo4j_value_t argv[1] = { neo4j_map(fields, 2) };

    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, argv, 1);
}


void queue_stream_end_success_with_plan(neo4j_iostream_t *ios)
{
    neo4j_map_entry_t s1_args[1] =
        { neo4j_map_entry("EstimatedRows", neo4j_float(9.9)) };
    neo4j_value_t s1_ids[1] = { neo4j_string("n") };

    neo4j_map_entry_t s1[4] =
        { neo4j_map_entry("args", neo4j_map(s1_args, 1)),
          neo4j_map_entry("identifiers", neo4j_list(s1_ids, 1)),
          neo4j_map_entry("children", neo4j_list(NULL, 0)),
          neo4j_map_entry("operatorType", neo4j_string("AllNodesScan"))
        };

    neo4j_map_entry_t s2_args[1] =
        { neo4j_map_entry("EstimatedRows", neo4j_float(10)) };
    neo4j_value_t s2_ids[1] = { neo4j_string("m") };

    neo4j_map_entry_t s2[4] =
        { neo4j_map_entry("args", neo4j_map(s2_args, 1)),
          neo4j_map_entry("identifiers", neo4j_list(s2_ids, 1)),
          neo4j_map_entry("children", neo4j_list(NULL, 0)),
          neo4j_map_entry("operatorType", neo4j_string("LabelScan"))
        };

    neo4j_map_entry_t profargs[4] =
        { neo4j_map_entry("version", neo4j_string("CYPHER 3.0")),
          neo4j_map_entry("planner", neo4j_string("RULE")),
          neo4j_map_entry("runtime", neo4j_string("INTERPRETTED")),
          neo4j_map_entry("EstimatedRows", neo4j_float(3.45)) };
    neo4j_value_t ids[2] = { neo4j_string("n"), neo4j_string("m") };
    neo4j_value_t sources[2] = { neo4j_map(s1, 4), neo4j_map(s2, 4) };

    neo4j_map_entry_t prof[4] =
        { neo4j_map_entry("args", neo4j_map(profargs, 4)),
          neo4j_map_entry("identifiers", neo4j_list(ids, 2)),
          neo4j_map_entry("children", neo4j_list(sources, 2)),
          neo4j_map_entry("operatorType", neo4j_string("ProduceResults")),
        };

    neo4j_map_entry_t fields[2] =
        { neo4j_map_entry("type", neo4j_string("r")),
          neo4j_map_entry("plan", neo4j_map(prof, 4)) };
    neo4j_value_t argv[1] = { neo4j_map(fields, 2) };

    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, argv, 1);
}


void queue_failure(neo4j_iostream_t *ios)
{
    neo4j_map_entry_t fields[2] =
        { neo4j_map_entry("code", neo4j_string("Neo.ClientError.Sample")),
          neo4j_map_entry("message", neo4j_string("Sample error")) };
    neo4j_value_t argv[1] = { neo4j_map(fields, 2) };
    queue_message(server_ios, NEO4J_FAILURE_MESSAGE, argv, 1);
}


START_TEST (test_run_returns_results_and_completes)
{
    neo4j_result_stream_t *results = neo4j_run(connection, "RETURN 1", neo4j_null);
    ck_assert_ptr_ne(results, NULL);
    ck_assert(rb_is_empty(out_rb)); // message is queued but not sent

    queue_run_success(server_ios); // RUN
    queue_record(server_ios); // PULL_ALL
    queue_record(server_ios); // PULL_ALL
    queue_stream_end_success_with_counts(server_ios); // PULL_ALL

    ck_assert_int_eq(neo4j_check_failure(results), 0);

    const neo4j_value_t *argv;
    uint16_t argc;
    neo4j_message_type_t type = recv_message(server_ios, &mpool,
            &argv, &argc);
    ck_assert(type == NEO4J_RUN_MESSAGE);
    ck_assert_int_eq(argc, 2);
    ck_assert(neo4j_type(argv[0]) == NEO4J_STRING);
    char buf[128];
    ck_assert_str_eq(neo4j_string_value(argv[0], buf, sizeof(buf)), "RETURN 1");
    ck_assert(neo4j_type(argv[1]) == NEO4J_MAP);
    ck_assert_int_eq(neo4j_map_size(argv[1]), 0);

    ck_assert_ptr_ne(neo4j_fetch_next(results), NULL);
    ck_assert_ptr_ne(neo4j_fetch_next(results), NULL);
    ck_assert_ptr_eq(neo4j_fetch_next(results), NULL);
    ck_assert_int_eq(errno, 0);

    ck_assert_int_eq(neo4j_check_failure(results), 0);

    ck_assert_int_eq(neo4j_statement_type(results), NEO4J_READ_WRITE_STATEMENT);
    struct neo4j_update_counts counts = neo4j_update_counts(results);
    ck_assert_int_eq(counts.nodes_created, 99);
    ck_assert_ptr_eq(neo4j_statement_plan(results), NULL);
    ck_assert_int_eq(errno, NEO4J_NO_PLAN_AVAILABLE);

    ck_assert_int_eq(neo4j_close_results(results), 0);

    ck_assert(rb_is_empty(in_rb));
}
END_TEST


START_TEST (test_run_can_close_immediately_after_fetch)
{
    neo4j_result_stream_t *results = neo4j_run(connection, "RETURN 1", neo4j_map(NULL, 0));
    ck_assert_ptr_ne(results, NULL);
    ck_assert(rb_is_empty(out_rb)); // message is queued but not sent

    queue_run_success(server_ios); // RUN
    queue_record(server_ios); // PULL_ALL
    queue_stream_end_success(server_ios); // PULL_ALL

    ck_assert_ptr_ne(neo4j_fetch_next(results), NULL);
    ck_assert_int_eq(neo4j_close_results(results), 0);
}
END_TEST


START_TEST (test_run_returns_fieldnames)
{
    neo4j_result_stream_t *results = neo4j_run(connection, "RETURN 1", neo4j_null);
    ck_assert_ptr_ne(results, NULL);
    ck_assert(rb_is_empty(out_rb)); // message is queued but not sent

    queue_run_success(server_ios); // RUN
    queue_stream_end_success(server_ios); // PULL_ALL

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


START_TEST (test_run_returns_profile)
{
    neo4j_result_stream_t *results = neo4j_run(connection, "RETURN 1", neo4j_null);
    ck_assert_ptr_ne(results, NULL);
    ck_assert(rb_is_empty(out_rb)); // message is queued but not sent

    queue_run_success(server_ios); // RUN
    queue_stream_end_success_with_profile(server_ios); // PULL_ALL

    struct neo4j_statement_plan *plan = neo4j_statement_plan(results);
    ck_assert_ptr_ne(plan, NULL);

    ck_assert_str_eq(plan->version, "CYPHER 3.0");
    ck_assert_str_eq(plan->planner, "COST");
    ck_assert_str_eq(plan->runtime, "INTERPRETTED");
    ck_assert(plan->is_profile);
    ck_assert_ptr_ne(plan->output_step, NULL);
    ck_assert_str_eq(plan->output_step->operator_type, "ProduceResults");
    ck_assert_uint_eq(plan->output_step->nidentifiers, 1);
    ck_assert_str_eq(plan->output_step->identifiers[0], "n");
    ck_assert(plan->output_step->estimated_rows == 0.0);
    ck_assert_int_eq(plan->output_step->rows, 1);
    ck_assert_int_eq(plan->output_step->db_hits, 42);
    ck_assert_uint_eq(plan->output_step->nsources, 0);

    neo4j_statement_plan_release(plan);
    ck_assert_int_eq(neo4j_close_results(results), 0);
}
END_TEST


START_TEST (test_run_returns_plan)
{
    neo4j_result_stream_t *results = neo4j_run(connection, "RETURN 1", neo4j_null);
    ck_assert_ptr_ne(results, NULL);
    ck_assert(rb_is_empty(out_rb)); // message is queued but not sent

    queue_run_success(server_ios); // RUN
    queue_stream_end_success_with_plan(server_ios); // PULL_ALL

    struct neo4j_statement_plan *plan = neo4j_statement_plan(results);
    ck_assert_ptr_ne(plan, NULL);

    ck_assert_str_eq(plan->version, "CYPHER 3.0");
    ck_assert_str_eq(plan->planner, "RULE");
    ck_assert_str_eq(plan->runtime, "INTERPRETTED");
    ck_assert(!plan->is_profile);
    ck_assert_ptr_ne(plan->output_step, NULL);
    ck_assert_str_eq(plan->output_step->operator_type, "ProduceResults");
    ck_assert_uint_eq(plan->output_step->nidentifiers, 2);
    ck_assert_str_eq(plan->output_step->identifiers[0], "n");
    ck_assert_str_eq(plan->output_step->identifiers[1], "m");
    ck_assert(plan->output_step->estimated_rows == 3.45);
    ck_assert_int_eq(plan->output_step->rows, 0);
    ck_assert_int_eq(plan->output_step->db_hits, 0);
    ck_assert_uint_eq(plan->output_step->nsources, 2);

    struct neo4j_statement_execution_step *s1 = plan->output_step->sources[0];
    ck_assert_ptr_ne(s1, NULL);
    ck_assert_str_eq(s1->operator_type, "AllNodesScan");
    ck_assert_uint_eq(s1->nidentifiers, 1);
    ck_assert_str_eq(s1->identifiers[0], "n");
    ck_assert(s1->estimated_rows == 9.9);
    ck_assert_int_eq(s1->rows, 0);
    ck_assert_int_eq(s1->db_hits, 0);
    ck_assert_uint_eq(s1->nsources, 0);

    struct neo4j_statement_execution_step *s2 = plan->output_step->sources[1];
    ck_assert_ptr_ne(s2, NULL);
    ck_assert_str_eq(s2->operator_type, "LabelScan");
    ck_assert_uint_eq(s2->nidentifiers, 1);
    ck_assert_str_eq(s2->identifiers[0], "m");
    ck_assert(s2->estimated_rows == 10);
    ck_assert_int_eq(s2->rows, 0);
    ck_assert_int_eq(s2->db_hits, 0);
    ck_assert_uint_eq(s2->nsources, 0);

    neo4j_statement_plan_release(plan);
    ck_assert_int_eq(neo4j_close_results(results), 0);
}
END_TEST


START_TEST (test_run_returns_failure_when_statement_fails)
{
    queue_failure(server_ios); // RUN
    queue_message(server_ios, NEO4J_IGNORED_MESSAGE, NULL, 0); // PULL_ALL
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0); // ACK_FAILURE

    neo4j_result_stream_t *results = neo4j_run(connection, "badquery", neo4j_null);
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


START_TEST (test_run_returns_failure_during_streaming)
{
    neo4j_result_stream_t *results = neo4j_run(connection, "RETURN 1", neo4j_null);
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


START_TEST (test_run_skips_results_after_connection_close)
{
    neo4j_result_stream_t *results = neo4j_run(connection, "RETURN 1", neo4j_null);
    ck_assert_ptr_ne(results, NULL);

    queue_run_success(server_ios); // RUN
    queue_record(server_ios); // PULL_ALL
    queue_record(server_ios); // PULL_ALL
    queue_record(server_ios); // PULL_ALL
    queue_stream_end_success(server_ios); // PULL_ALL

    ck_assert_ptr_ne(neo4j_fetch_next(results), NULL);
    ck_assert_ptr_ne(neo4j_fetch_next(results), NULL);

    neo4j_close(connection);
    connection = NULL;

    ck_assert_ptr_eq(neo4j_fetch_next(results), NULL);
    ck_assert_int_eq(errno, NEO4J_SESSION_ENDED);
    ck_assert_int_eq(neo4j_check_failure(results), NEO4J_SESSION_ENDED);

    ck_assert_int_eq(neo4j_close_results(results), 0);
    ck_assert(rb_is_empty(in_rb));
}
END_TEST


START_TEST (test_run_skips_results_after_connection_reset)
{
    neo4j_result_stream_t *results = neo4j_run(connection, "RETURN 1", neo4j_null);
    ck_assert_ptr_ne(results, NULL);

    queue_run_success(server_ios); // RUN
    queue_record(server_ios); // PULL_ALL
    queue_record(server_ios); // PULL_ALL
    queue_record(server_ios); // PULL_ALL
    queue_stream_end_success(server_ios); // PULL_ALL
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0); // RESET

    ck_assert_ptr_ne(neo4j_fetch_next(results), NULL);

    const neo4j_value_t *argv;
    uint16_t argc;
    neo4j_message_type_t type = recv_message(server_ios, &mpool,
            &argv, &argc);
    ck_assert(type == NEO4J_RUN_MESSAGE);
    type = recv_message(server_ios, &mpool, &argv, &argc);
    ck_assert(type == NEO4J_PULL_ALL_MESSAGE);

    ck_assert_ptr_ne(neo4j_fetch_next(results), NULL);

    neo4j_reset(connection);

    ck_assert_ptr_eq(neo4j_fetch_next(results), NULL);
    ck_assert_int_eq(errno, NEO4J_SESSION_RESET);
    ck_assert_int_eq(neo4j_check_failure(results), NEO4J_SESSION_RESET);

    ck_assert_int_eq(neo4j_close_results(results), 0);
    ck_assert(rb_is_empty(in_rb));
}
END_TEST


START_TEST (test_run_returns_same_failure_after_connection_close)
{
    queue_failure(server_ios); // RUN
    queue_message(server_ios, NEO4J_IGNORED_MESSAGE, NULL, 0); // PULL_ALL
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0); // ACK_FAILURE

    neo4j_result_stream_t *results = neo4j_run(connection, "bad query",
            neo4j_map(NULL, 0));
    ck_assert_ptr_ne(results, NULL);

    int result = neo4j_check_failure(results);
    ck_assert_int_eq(result, NEO4J_STATEMENT_EVALUATION_FAILED);
    ck_assert_ptr_eq(neo4j_fetch_next(results), NULL);
    ck_assert_int_eq(errno, NEO4J_STATEMENT_EVALUATION_FAILED);

    neo4j_close(connection);
    connection = NULL;

    result = neo4j_check_failure(results);
    ck_assert_int_eq(result, NEO4J_STATEMENT_EVALUATION_FAILED);
    ck_assert_ptr_eq(neo4j_fetch_next(results), NULL);
    ck_assert_int_eq(errno, NEO4J_STATEMENT_EVALUATION_FAILED);

    ck_assert_int_eq(neo4j_close_results(results), 0);

    ck_assert(rb_is_empty(in_rb));
}
END_TEST


START_TEST (test_send_completes)
{
    neo4j_result_stream_t *results = neo4j_send(connection, "RETURN 1", neo4j_map(NULL, 0));
    ck_assert_ptr_ne(results, NULL);
    ck_assert(rb_is_empty(out_rb)); // message is queued but not sent

    queue_run_success(server_ios); // RUN
    queue_stream_end_success_with_counts(server_ios); // DISCARD_ALL

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

    ck_assert_ptr_eq(neo4j_fetch_next(results), NULL);
    ck_assert_int_eq(errno, 0);

    ck_assert_int_eq(neo4j_check_failure(results), 0);

    ck_assert_int_eq(neo4j_statement_type(results), NEO4J_READ_WRITE_STATEMENT);
    struct neo4j_update_counts counts = neo4j_update_counts(results);
    ck_assert_int_eq(counts.nodes_created, 99);
    ck_assert_ptr_eq(neo4j_statement_plan(results), NULL);
    ck_assert_int_eq(errno, NEO4J_NO_PLAN_AVAILABLE);

    ck_assert_int_eq(neo4j_close_results(results), 0);

    ck_assert(rb_is_empty(in_rb));
}
END_TEST


START_TEST (test_send_returns_fieldnames)
{
    neo4j_result_stream_t *results = neo4j_send(connection, "RETURN 1", neo4j_map(NULL, 0));
    ck_assert_ptr_ne(results, NULL);
    ck_assert(rb_is_empty(out_rb)); // message is queued but not sent

    queue_run_success(server_ios); // RUN
    queue_stream_end_success(server_ios); // DISCARD_ALL

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


START_TEST (test_send_returns_failure_when_statement_fails)
{
    queue_failure(server_ios); // RUN
    queue_message(server_ios, NEO4J_IGNORED_MESSAGE, NULL, 0); // DISCARD_ALL
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, NULL, 0); // ACK_FAILURE

    neo4j_result_stream_t *results = neo4j_send(connection, "bad query", neo4j_map(NULL, 0));
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


START_TEST (test_peek_retrieves_records_in_order)
{
    neo4j_result_stream_t *results = neo4j_run(connection, "RETURN 1", neo4j_null);
    ck_assert_ptr_ne(results, NULL);
    ck_assert(rb_is_empty(out_rb)); // message is queued but not sent

    queue_run_success(server_ios); // RUN
    queue_record(server_ios); // PULL_ALL
    queue_record(server_ios); // PULL_ALL
    queue_record(server_ios); // PULL_ALL
    queue_record(server_ios); // PULL_ALL
    queue_stream_end_success_with_counts(server_ios); // PULL_ALL

    ck_assert_int_eq(neo4j_check_failure(results), 0);

    neo4j_result_t *first = neo4j_retain(neo4j_peek(results, 0));
    ck_assert_ptr_ne(first, NULL);
    neo4j_result_t *third = neo4j_retain(neo4j_peek(results, 2));
    ck_assert_ptr_ne(third, NULL);

    ck_assert_ptr_eq(neo4j_fetch_next(results), first);
    ck_assert_ptr_ne(neo4j_fetch_next(results), NULL);

    neo4j_result_t *fourth = neo4j_retain(neo4j_peek(results, 1));
    ck_assert_ptr_ne(fourth, NULL);

    ck_assert_ptr_eq(neo4j_fetch_next(results), third);

    ck_assert_ptr_eq(neo4j_peek(results, 0), fourth);

    ck_assert_ptr_eq(neo4j_fetch_next(results), fourth);
    ck_assert_ptr_eq(neo4j_fetch_next(results), NULL);

    ck_assert_int_eq(errno, 0);

    ck_assert_int_eq(neo4j_check_failure(results), 0);

    neo4j_release(first);
    neo4j_release(third);
    neo4j_release(fourth);
    ck_assert_int_eq(neo4j_close_results(results), 0);

    ck_assert(rb_is_empty(in_rb));
}
END_TEST


START_TEST (test_peek_beyond_depth)
{
    neo4j_result_stream_t *results = neo4j_run(connection, "RETURN 1", neo4j_null);
    ck_assert_ptr_ne(results, NULL);
    ck_assert(rb_is_empty(out_rb)); // message is queued but not sent

    queue_run_success(server_ios); // RUN
    queue_record(server_ios); // PULL_ALL
    queue_record(server_ios); // PULL_ALL
    queue_stream_end_success_with_counts(server_ios); // PULL_ALL

    ck_assert_int_eq(neo4j_check_failure(results), 0);

    neo4j_result_t *first = neo4j_peek(results, 0);
    ck_assert_ptr_ne(first, NULL);
    neo4j_retain(first);
    ck_assert_ptr_eq(neo4j_peek(results, 2), NULL);

    ck_assert_ptr_eq(neo4j_fetch_next(results), first);
    ck_assert_ptr_ne(neo4j_fetch_next(results), NULL);
    ck_assert_ptr_eq(neo4j_fetch_next(results), NULL);
    ck_assert_int_eq(errno, 0);

    ck_assert_int_eq(neo4j_check_failure(results), 0);

    neo4j_release(first);
    ck_assert_int_eq(neo4j_close_results(results), 0);

    ck_assert(rb_is_empty(in_rb));
}
END_TEST


START_TEST (test_run_with_long_statement)
{
    char statement[65538];
    for (unsigned int i = 0; i < 65537; ++i)
    {
        statement[i] = (char) (random() & 0xff);
    }
    statement[65537] = '\0';

    neo4j_result_stream_t *results = neo4j_run(connection, statement, neo4j_null);
    ck_assert_ptr_ne(results, NULL);

    queue_run_success(server_ios); // RUN
    queue_stream_end_success(server_ios); // PULL_ALL

    ck_assert_int_eq(neo4j_check_failure(results), 0);

    const neo4j_value_t *argv;
    uint16_t argc;
    neo4j_message_type_t type = recv_message(server_ios, &mpool,
            &argv, &argc);
    ck_assert(type == NEO4J_RUN_MESSAGE);
    ck_assert_int_eq(argc, 2);
    ck_assert(neo4j_type(argv[0]) == NEO4J_STRING);
    char buf[131072];
    ck_assert_str_eq(neo4j_string_value(argv[0], buf, sizeof(buf)), statement);
    ck_assert(neo4j_type(argv[1]) == NEO4J_MAP);
    ck_assert_int_eq(neo4j_map_size(argv[1]), 0);

    ck_assert_int_eq(neo4j_close_results(results), 0);

    ck_assert(rb_is_empty(in_rb));
}
END_TEST


TCase* result_stream_tcase(void)
{
    TCase *tc = tcase_create("result stream");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_run_returns_results_and_completes);
    tcase_add_test(tc, test_run_can_close_immediately_after_fetch);
    tcase_add_test(tc, test_run_returns_fieldnames);
    tcase_add_test(tc, test_run_returns_profile);
    tcase_add_test(tc, test_run_returns_plan);
    tcase_add_test(tc, test_run_returns_failure_when_statement_fails);
    tcase_add_test(tc, test_run_returns_failure_during_streaming);
    tcase_add_test(tc, test_run_skips_results_after_connection_close);
    tcase_add_test(tc, test_run_skips_results_after_connection_reset);
    tcase_add_test(tc, test_run_returns_same_failure_after_connection_close);
    tcase_add_test(tc, test_send_completes);
    tcase_add_test(tc, test_send_returns_fieldnames);
    tcase_add_test(tc, test_send_returns_failure_when_statement_fails);
    tcase_add_test(tc, test_peek_retrieves_records_in_order);
    tcase_add_test(tc, test_peek_beyond_depth);
    tcase_add_test(tc, test_run_with_long_statement);
    return tc;
}
