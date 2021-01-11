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
#include "../src/neo4j-client.h"
#include "../src/chunking_iostream.h"
#include "../src/connection.h"
#include "../src/deserialization.h"
#include "../src/messages.h"
#include "../src/serialization.h"
#include "../src/util.h"
#include "../src/transaction.h"
#include "memiostream.h"
#include <check.h>
#include <assert.h>
#include <errno.h>


static neo4j_iostream_t *stub_connect(struct neo4j_connection_factory *factory,
        const char *hostname, unsigned int port, neo4j_config_t *config,
        uint_fast32_t flags, struct neo4j_logger *logger);
static neo4j_message_type_t recv_message(neo4j_iostream_t *ios,
        neo4j_mpool_t *mpool, const neo4j_value_t **argv, uint16_t *argc);
static void queue_message(neo4j_iostream_t *ios, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc);
static void queue_run_success(neo4j_iostream_t *ios);
static void queue_begin_success(neo4j_iostream_t *ios);
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
    logger_provider = neo4j_std_logger_provider(stderr, NEO4J_LOG_DEBUG, 0);
    in_rb = rb_alloc(1024);
    out_rb = rb_alloc(1024);
    client_ios = neo4j_memiostream(in_rb, out_rb);
    server_ios = neo4j_memiostream(out_rb, in_rb);

    stub_factory.tcp_connect = stub_connect;
    config = neo4j_new_config();
    neo4j_config_set_logger_provider(config, logger_provider);
    neo4j_config_set_connection_factory(config, &stub_factory);

    mpool = neo4j_std_mpool(config);

    uint32_t version = htonl(4);
    rb_append(in_rb, &version, sizeof(version)); // server response version

    neo4j_value_t empty_map = neo4j_map(NULL, 0);
    queue_message(server_ios, NEO4J_SUCCESS_MESSAGE, &empty_map, 1); // response to INIT
    connection = neo4j_connect("neo4j://localhost:7687", config, 0); // sends INIT
    // ck_assert_ptr_ne(connection, NULL);
    assert( connection != NULL);

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


// recv_message retrieves a message (from the server) sent by a client call
neo4j_message_type_t recv_message(neo4j_iostream_t *ios, neo4j_mpool_t *mpool,
        const neo4j_value_t **argv, uint16_t *argc)
{
    neo4j_message_type_t type;
    int result = neo4j_message_recv(ios, mpool, &type, argv, argc);
    // ck_assert_int_eq(result, 0);
    assert( result == 0 );
    printf("%s\n",type->name);
    return type;
}


void queue_message(neo4j_iostream_t *ios, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc)
{
    int result = neo4j_message_send(ios, type, argv, argc, NULL, 0, 1024);
    // ck_assert_int_eq(result, 0);
    assert( result == 0 );
}

void queue_begin_success(neo4j_iostream_t *ios) {
  neo4j_value_t empty_map = neo4j_map(NULL, 0);
  //neo4j_map_entry_t ent = neo4j_map_entry("key", neo4j_string("value"));
  //neo4j_value_t argv[1] = { neo4j_map(&ent,1) };
  neo4j_value_t argv[1] = { empty_map };
  queue_message(ios, NEO4J_SUCCESS_MESSAGE, argv, 1); // response to BEGIN
}

void queue_commit_success (neo4j_iostream_t *ios) {
  neo4j_map_entry_t ent = neo4j_map_entry("bookmark", neo4j_string("example-bookmark:1"));
  neo4j_value_t argv[1] = { neo4j_map(&ent,1) };
  queue_message(ios, NEO4J_SUCCESS_MESSAGE, argv, 1); // response to COMMIT
}

void queue_rollback_success(neo4j_iostream_t *ios) {
  neo4j_value_t empty_map = neo4j_map(NULL, 0);
  neo4j_value_t argv[1] = { empty_map };
  queue_message(ios, NEO4J_SUCCESS_MESSAGE, argv, 1); // response to ROLLBACK
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


START_TEST (test_transaction)
{
    queue_begin_success(server_ios); // BEGIN
    neo4j_transaction_t *tx = neo4j_begin_tx(connection, 10000, "w", "neo4j"); // sends BEGIN

    ck_assert_ptr_ne(tx, NULL);
    ck_assert_int_eq(neo4j_tx_failure(tx), 0);
    ck_assert(neo4j_tx_is_open(tx));
    ck_assert(!neo4j_tx_defunct(tx));
    ck_assert_str_eq(neo4j_tx_dbname(tx), "neo4j");
    const neo4j_value_t *argv;
    uint16_t argc;
    neo4j_message_type_t type = recv_message(server_ios, &mpool, &argv, &argc);
    ck_assert(type == NEO4J_BEGIN_MESSAGE);
    ck_assert_int_eq(argc, 1);
    ck_assert(neo4j_type(argv[0]) == NEO4J_MAP);
    char buf[128];
    ck_assert( neo4j_eq(neo4j_map_get(argv[0],"mode"),neo4j_string("w")));
    ck_assert( neo4j_eq(neo4j_map_get(argv[0],"tx_timeout"),neo4j_int(10000)) );
    ck_assert( neo4j_eq(neo4j_map_get(argv[0],"db"),neo4j_string("neo4j")) );
    queue_commit_success(server_ios);
    int result = neo4j_commit(tx);
    ck_assert_int_eq(result, 0);
    type = recv_message(server_ios, &mpool, &argv, &argc);
    ck_assert(type == NEO4J_COMMIT_MESSAGE);
    ck_assert(!neo4j_tx_is_open(tx)); // tx is closed
    ck_assert_int_eq( neo4j_rollback(tx), -1 ); // can't rollback closed tx
    neo4j_free_tx(tx);
    queue_begin_success(server_ios);
    queue_rollback_success(server_ios);
    tx = neo4j_begin_tx(connection, -1, NULL, "neo4j");
    ck_assert_ptr_ne(tx, NULL);
    ck_assert_str_eq( neo4j_tx_mode(tx),"w" );
    type = recv_message(server_ios, &mpool, &argv, &argc);
    ck_assert(type == NEO4J_BEGIN_MESSAGE);
    ck_assert( neo4j_is_null(neo4j_map_get(argv[0],"tx_timeout")) );
    ck_assert_int_eq( neo4j_rollback(tx), 0 );
    type = recv_message(server_ios, &mpool, &argv, &argc);
    ck_assert(type == NEO4J_ROLLBACK_MESSAGE);
    ck_assert_int_eq( neo4j_commit(tx), -1 ); // can't commit closed tx
    neo4j_free_tx(tx);
    queue_failure(server_ios); // test failure
    tx = neo4j_begin_tx(connection, 0, NULL, "neo4j");
    ck_assert_int_eq(tx->failure, NEO4J_TRANSACTION_FAILED);
    ck_assert_int_eq(neo4j_tx_failure(tx), NEO4J_TRANSACTION_FAILED);
    ck_assert_int_eq( tx->failed, 1 );
    ck_assert_str_eq( neo4j_tx_failure_code(tx), "Neo.ClientError.Sample");
    ck_assert_str_eq( neo4j_tx_failure_message(tx), "Sample error");

    // check run in tx - using the result_stream machinery
    connection->failed = false; // kludge and reuse
    queue_begin_success(server_ios);
    tx = neo4j_begin_tx(connection, 0, NULL,"neo4j");
    ck_assert_int_eq(tx->failed, 0);

    queue_run_success(server_ios);
    queue_record(server_ios); // PULL_ALL
    queue_record(server_ios); // PULL_ALL
    queue_stream_end_success_with_counts(server_ios); // PULL_ALL
    queue_commit_success(server_ios);

    neo4j_result_stream_t *tx_results = neo4j_run_in_tx(tx, "RETURN 1", neo4j_null);
    ck_assert_ptr_ne(tx_results, NULL);
    ck_assert_ptr_ne(neo4j_fetch_next(tx_results), NULL);
    ck_assert(neo4j_tx_is_open(tx));
    ck_assert_ptr_ne(neo4j_fetch_next(tx_results), NULL);
    ck_assert(neo4j_tx_is_open(tx));
    ck_assert_ptr_eq(neo4j_fetch_next(tx_results),NULL);
    ck_assert(neo4j_tx_is_open(tx));
    ck_assert_int_eq(errno, 0);
    ck_assert_int_eq(neo4j_check_failure(tx_results), 0);
    ck_assert_int_eq(neo4j_close_results(tx_results),0);
    ck_assert_int_eq(neo4j_commit(tx), 0);
    ck_assert_int_eq(tx->failed, 0);
    ck_assert(!neo4j_tx_is_open(tx));
    neo4j_free_tx(tx);
    assert(rb_is_empty(in_rb));
}
END_TEST

TCase* transaction_tcase(void) {
  TCase *tc = tcase_create("transaction");
  tcase_add_checked_fixture(tc, setup, teardown);
  tcase_add_test(tc, test_transaction);
  return tc;
}
