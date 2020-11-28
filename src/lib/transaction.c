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
#include "transaction.h"
#include "connection.h"
#include "neo4j-client.h"
#include "memory.h"
#include "result_stream.h"
#include "values.h"
#include <assert.h>
#include <stddef.h>
#include <unistd.h>

neo4j_transaction_t *new_transaction(neo4j_config_t *config, int timeout, const char *mode);
void destroy_transaction(neo4j_transaction_t *tx);
int begin_callback(void *cdata, neo4j_message_type_t type, const neo4j_value_t *argv, uint16_t argc);
int commit_callback(void *cdata, neo4j_message_type_t type, const neo4j_value_t *argv, uint16_t argc);
int rollback_callback(void *cdata, neo4j_message_type_t type, const neo4j_value_t *argv, uint16_t argc);



// rough out the transaction based calls
// there will be bookkeeping to do - handling errors when server state
// is mismatch with the request:
// - when server is READY but run_in_tx is exec'd
// - when server is STREAMING but run is exec'd <- may be handled by the results->starting,
//   results->streaming flags
// - when server is TX_READY but run or send is exec'd
// - when server is TX_STREAMING but run or send is exec'd
// erroring when negotiated protocol is not 3+

// Note, BEGIN, COMMIT, ROLLBACK responses don't belong on a results stream. So maybe
// should have a transaction structure, analogous to the results structure, that
// stores info like success responses, failure responses, bookmarks.

// begin_tx - specify timeout and mode, but ignore bookmarks and metadata ATM
// must check tx->failed to see if tx is all right
neo4j_transaction_t *neo4j_begin_tx(neo4j_connection_t *connection,
                                      int tx_timeout, const char *tx_mode)
{
    REQUIRE(connection != NULL, NULL);

    neo4j_config_t *config = connection->config;
    neo4j_transaction_t *tx = new_transaction(config, tx_timeout, tx_mode);

    neo4j_session_transact(connection, "BEGIN", begin_callback, tx);
    return tx;
}

int begin_callback(void *cdata, neo4j_message_type_t type, const neo4j_value_t *argv, uint16_t argc)
{
  assert(cdata != NULL);
  assert(argc == 0 || argv != NULL);
  neo4j_transaction_t *tx = (neo4j_transaction_t *) cdata;

  if (type == NEO4J_FAILURE_MESSAGE)
    {
      // but should put failure message in log?
      neo4j_log_error_errno(tx->logger, "tx begin failed", NEO4J_TRANSACTION_FAILED);
      // get FAILURE argv and set tx failure info here
      tx->failed = 1;
      tx->failure = NEO4J_TRANSACTION_FAILED;
      tx->failure_code = neo4j_map_get(argv[0],"code");
      tx->failure_message = neo4j_map_get(argv[0],"message");
      return -1;
    }
  if (type == NEO4J_IGNORED_MESSAGE)
    {
      neo4j_log_trace(tx->logger, "tx begin ignored");
      return 0;
    }
  char description[128];
  snprintf(description, sizeof(description), "%s in %p (response to BEGIN)",
           neo4j_message_type_str(type), (void *)connection);

  if (type != NEO4J_SUCCESS_MESSAGE)
    {
      neo4j_log_error(tx->logger, "Unexpected %s", description);
      tx->failed = 1;
      tx->failure = EPROTO;
      return -1;
    }
  tx->is_open = 1;
  return 0;
}

// commit_tx
// must check tx->is_failed upon return
neo4j_transaction_t *neo4j_commit_tx(neo4j_transaction_t *tx)
{
    REQUIRE(tx != NULL, NULL);
    neo4j_session_transact(tx->connection, "COMMIT", commit_callback, tx);
    return tx;
}

int commit_callback(void *cdata, neo4j_message_type_t type, const neo4j_value_t *argv, uint16_t argc)
{
  assert(cdata != NULL);
  assert(argc == 0 || argv != NULL);
  neo4j_transaction_t *tx = (neo4j_transaction_t *) cdata;

  if (type == NEO4J_FAILURE_MESSAGE)
    {
      // but should put failure message in log?
      neo4j_log_error_errno(tx->logger, "tx commit failed", NEO4J_TRANSACTION_FAILED);
      // get FAILURE argv and set tx failure info here
      tx->failed = 1;
      tx->failure = NEO4J_TRANSACTION_FAILED;
      tx->failure_code = neo4j_map_get(argv[0],"code");
      tx->failure_message = neo4j_map_get(argv[0],"message");
      // check here if transaction timed out; if so, set tx->is_expired
      return -1;
    }
  if (type == NEO4J_IGNORED_MESSAGE)
    {
      neo4j_log_trace(tx->logger, "tx commit ignored");
      return 0;
    }
  char description[128];
  snprintf(description, sizeof(description), "%s in %p (response to COMMIT)",
           neo4j_message_type_str(type), (void *)tx->connection);

  if (type != NEO4J_SUCCESS_MESSAGE)
    {
      neo4j_log_error(tx->logger, "Unexpected %s", description);
      tx->failed = 1;
      tx->failure = EPROTO;
      return -1;
    }
  if (argc) {
    neo4j_value_t svr_extra = argv[0];
    neo4j_value_t bookmark = neo4j_map_get(svr_extra,"bookmark");
    if (bookmark != NULL) {
      tx->commit_bookmark = bookmark;
    }
  }
  tx->is_open = 0;
  return 0;
}

// rollback_tx
// must check tx->failed after call
neo4j_transaction_t *neo4j_rollback_tx(neo4j_transaction_t *tx)
{
  REQUIRE(tx != NULL, NULL);
  neo4j_session_transact(tx->connection, "ROLLBACK", rollback_callback, tx);
  return tx;
}

int rollback_callback(void *cdata, neo4j_message_type_t type, const neo4j_value_t *argv, uint16_t argc)
{
  assert(cdata != NULL);
  assert(argc == 0 || argv != NULL);
  neo4j_transaction_t *tx = (neo4j_transaction_t *) cdata;
  if (type == NEO4J_FAILURE_MESSAGE)
    {
      // but should put failure message in log?
      neo4j_log_error_errno(tx->logger, "tx rollback failed", NEO4J_TRANSACTION_FAILED);
      // get FAILURE argv and set tx failure info here
      tx->failed = 1;
      tx->failure = NEO4J_TRANSACTION_FAILED;
      tx->failure_code = neo4j_map_get(argv[0],"code");
      tx->failure_message = neo4j_map_get(argv[0],"message");
      // check here if transaction timed out; if so, set tx->is_expired
      return -1;
    }
  if (type == NEO4J_IGNORED_MESSAGE)
    {
      neo4j_log_trace(tx->logger, "tx rollback ignored");
      return 0;
    }
  char description[128];
  snprintf(description, sizeof(description), "%s in %p (response to ROLLBACK)",
           neo4j_message_type_str(type), (void *)tx->connection);

  if (type != NEO4J_SUCCESS_MESSAGE)
    {
      neo4j_log_error(tx->logger, "Unexpected %s", description);
      tx->failed = 1;
      tx->failure = EPROTO;
      return -1;
    }

  // Bolt 3.0 spec sez SUCCESS argv "may contain metadata relating to the outcome". Pfft.
  tx->is_open = 0;
  return 0;
}

// run_in_tx
// returns a result stream, namely, tx->results
// if it borks...

neo4j_result_stream_t *neo4j_run_in_tx(neo4j_transaction_t *tx,
                                     const char *statement, neo4j_value_t params)
{
  REQUIRE(tx != NULL, NULL);
  neo4j_transaction_t *tx = (neo4j_transaction_t *) cdata;
  tx->results = neo4j_run( tx->connection, statement, params );
  return tx->results
}

// constructor

neo4j_transaction_t *new_transaction(neo4j_config_t *config, int timeout, const char *mode) {
  neo4j_transaction_t *tx = neo4j_calloc(config->allocator,
                                         NULL, 1, sizeof(neo4j_transaction_t));
  tx->allocator = config->allocator;
  tx->logger = neo4j_get_logger(config, "transactions");
  tx->connection = connection;
  tx->timeout = (timeout == 0 ? DEFAULT_TX_TIMEOUT : timeout);
  tx->mode = ( mode == NULL ? "w" : mode );
  neo4j_map_entry_t ent[2] = { neo4j_map_entry("tx_timeout",neo4j_int(tx->timeout)),
                               neo4j_map_entry("mode",neo4j_string(tx->mode)) };
  tx->extra = &(neo4j_map(ent,2));
  tx->is_open = 0;
  tx->is_expired = 0;
  tx->failed = 0;
  tx->failure_code = neo4j_null;
  tx->failure_message = neo4j_null;
  tx->bookmarks = &neo4j_null;
  tx->num_bookmarks = 0;
  return tx;
}

// tx getters
void tx_bookmarks(neo4j_transaction_t *tx) {
}

// destructor

void destroy_transaction(neo4j_transaction_t *tx) {
  // free bookmarks array space
  // free tx space
  if (tx->num_bookmarks > 0) {
    neo4j_free(tx->allocator, (void *)tx->bookmarks);
  }
  neo4j_free(tx->allocator, tx);
}
