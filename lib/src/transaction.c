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
#include "util.h"
#include "values.h"
#include <assert.h>
#include <stddef.h>
#include <unistd.h>
#include <stdio.h>

neo4j_transaction_t *new_transaction(neo4j_config_t *config, neo4j_connection_t *connection, int timeout, const char *mode, const char *dbname);
int begin_callback(void *cdata, neo4j_message_type_t type, const neo4j_value_t *argv, uint16_t argc);
int commit_callback(void *cdata, neo4j_message_type_t type, const neo4j_value_t *argv, uint16_t argc);
int rollback_callback(void *cdata, neo4j_message_type_t type, const neo4j_value_t *argv, uint16_t argc);
int tx_failure(neo4j_transaction_t *tx);
int tx_expired(neo4j_transaction_t *tx);
int tx_commit(neo4j_transaction_t *tx);
int tx_rollback(neo4j_transaction_t *tx);
neo4j_result_stream_t *tx_run(neo4j_transaction_t *tx, const char *statement, neo4j_value_t params);


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
// must check neo4j_tx_failure(tx)
neo4j_transaction_t *neo4j_begin_tx(neo4j_connection_t *connection,
        int tx_timeout, const char *tx_mode, const char *dbname)
{
    REQUIRE(connection != NULL, NULL);

    neo4j_config_t *config = connection->config;
    if (connection->version < 3)
      {
        errno = NEO4J_FEATURE_UNAVAILABLE;
        char ebuf[256];
        neo4j_log_error(connection->logger,
                "Cannot create transaction on %p: %s\n", (void *)connection,
                        neo4j_strerror(errno, ebuf, sizeof(ebuf)));
        return NULL;
      }
    neo4j_transaction_t *tx = new_transaction(config, connection, tx_timeout, tx_mode, dbname);
    fprintf(stderr,"Dude I'm here\n");
    if (neo4j_session_transact(connection, "BEGIN", begin_callback, tx))
      {
        neo4j_log_error_errno(tx->logger, "tx begin failed");
        tx->failed = 1;
        tx->failure = errno;
      }
    return tx;
}

int begin_callback(void *cdata, neo4j_message_type_t type, const neo4j_value_t *argv, uint16_t argc)
{
  assert(cdata != NULL);
  assert(argc == 0 || argv != NULL);
  neo4j_transaction_t *tx = (neo4j_transaction_t *) cdata;

  if (type == NEO4J_FAILURE_MESSAGE)
    {
      // get FAILURE argv and set tx failure info here
      tx->failed = 1;
      tx->failure = NEO4J_TRANSACTION_FAILED;
      tx->failure_code = neo4j_map_get(argv[0],"code");
      tx->failure_message = neo4j_map_get(argv[0],"message");
      errno = tx->failure;
      neo4j_log_error_errno(tx->logger, "tx begin failed");

      return -1;
    }
  if (type == NEO4J_IGNORED_MESSAGE)
    {
      neo4j_log_trace(tx->logger, "tx begin ignored");
      return 0;
    }
  char description[128];
  snprintf(description, sizeof(description), "%s in %p (response to BEGIN)",
           neo4j_message_type_str(type), (void *)tx->connection);

  if (type != NEO4J_SUCCESS_MESSAGE)
    {
      neo4j_log_error(tx->logger, "Unexpected %s", description);
      tx->failed = 1;
      tx->failure = EPROTO;
      errno = tx->failure;
      return -1;
    }
  tx->is_open = 1;
  return 0;
}

// commit_tx
int tx_commit(neo4j_transaction_t *tx)
{
    REQUIRE(tx != NULL, -1);
    if (tx->is_open == 0) {
      neo4j_log_debug(tx->logger, "can't commit a closed tx");
      return -1;
    }
    if (tx->is_expired == 1) {
      neo4j_log_debug(tx->logger, "tx is expired");
      return -1;
    }
    if (neo4j_session_transact(tx->connection, "COMMIT", commit_callback, tx))
      {
        neo4j_log_error_errno(tx->logger, "tx commit failed");
        tx->failed = 1;
        tx->failure = errno;
      }
    return -tx->failed;
}

int commit_callback(void *cdata, neo4j_message_type_t type, const neo4j_value_t *argv, uint16_t argc)
{
  assert(cdata != NULL);
  assert(argc == 0 || argv != NULL);
  neo4j_transaction_t *tx = (neo4j_transaction_t *) cdata;

  if (type == NEO4J_FAILURE_MESSAGE)
    {
      // get FAILURE argv and set tx failure info here
      tx->failed = 1;
      tx->failure = NEO4J_TRANSACTION_FAILED;
      tx->failure_code = neo4j_map_get(argv[0],"code");
      tx->failure_message = neo4j_map_get(argv[0],"message");
      // check here if transaction timed out; if so, set tx->is_expired
      errno = tx->failure;
      neo4j_log_error_errno(tx->logger, "tx commit failed");
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
      tx->failed = -1;
      tx->failure = EPROTO;
      errno = tx->failure;
      return -1;
    }
  if (argc) {
    neo4j_value_t svr_extra = argv[0];
    neo4j_value_t bookmark = neo4j_map_get(svr_extra,"bookmark");
    if (!neo4j_is_null(bookmark))
      {
        tx->commit_bookmark = bookmark;
      }
  }
  tx->is_open = 0;
  return 0;
}

// rollback_tx
// must check tx->failed after call
int tx_rollback(neo4j_transaction_t *tx)
{
  REQUIRE(tx != NULL, -1);
  if (tx->is_open == 0) {
    neo4j_log_debug(tx->logger, "can't roll back a closed tx");
    return -1;
  }
  if (tx->is_expired == 1) {
    neo4j_log_debug(tx->logger, "tx is expired");
    return -1;
  }
  if (neo4j_session_transact(tx->connection, "ROLLBACK", rollback_callback, tx))
    {
      neo4j_log_error_errno(tx->logger, "tx rollback failed");
      tx->failed = 1;
      tx->failure = errno;
    }
  return -tx->failed;
}

int rollback_callback(void *cdata, neo4j_message_type_t type, const neo4j_value_t *argv, uint16_t argc)
{
  assert(cdata != NULL);
  assert(argc == 0 || argv != NULL);
  neo4j_transaction_t *tx = (neo4j_transaction_t *) cdata;
  if (type == NEO4J_FAILURE_MESSAGE)
    {
      // get FAILURE argv and set tx failure info here
      tx->failed = 1;
      tx->failure = NEO4J_TRANSACTION_FAILED;
      tx->failure_code = neo4j_map_get(argv[0],"code");
      tx->failure_message = neo4j_map_get(argv[0],"message");
      // check here if transaction timed out; if so, set tx->is_expired
      errno = tx->failure;
      neo4j_log_error_errno(tx->logger, "tx rollback failed");
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
      errno = tx->failure;
      return -1;
    }

  // Bolt 3.0 spec sez SUCCESS argv "may contain metadata relating to the outcome". Pfft.
  tx->is_open = 0;
  return 0;
}

// run_in_tx
// returns a result stream, namely, tx->results
// returns NULL and expires tx if tx has timed out

neo4j_result_stream_t *tx_run(neo4j_transaction_t *tx,
                              const char *statement, neo4j_value_t params)
{
  REQUIRE(tx != NULL, NULL);
  // short circuit dbname if version isn't high enough
  if (tx->connection->version < 4 || neo4j_tx_dbname(tx) == NULL)
    {
      tx->results = neo4j_run( tx->connection, statement, params );
    }
  else
    {
      tx->results = neo4j_run_in_db( tx->connection, statement, params, neo4j_tx_dbname(tx) );
    }
  if (tx->results == NULL)
    {
      tx->failed = 1;
      tx->failure = errno;
      return NULL;
    }
  if (neo4j_check_failure(tx->results))
    {
      if (strcmp(neo4j_error_code(tx->results),
                 "Neo.ClientError.Transaction.TransactionTimedOut") == 0) {
        tx->failed = 1;
        tx->is_expired = 1;
        return NULL;
      }
    }
  return tx->results;
}

int tx_expired(neo4j_transaction_t *tx)
{
  if (tx->is_expired == 1) { // if set elsewhere (in tx_run)
    return 1;
  }
  if (tx->failed == 0) {
    tx->is_expired = 0;
  }
  else {
    if (strcmp(neo4j_tx_failure_code(tx),
               "Neo.ClientError.Transaction.TransactionTimedOut") == 0)
      {
        tx->is_expired = 1;
      }
    else
      {
        tx->is_expired = 0;
      }
  }
  return tx->is_expired;
}

// constructor

neo4j_transaction_t *new_transaction(neo4j_config_t *config, neo4j_connection_t *connection, int timeout, const char *mode, const char *dbname) {
  neo4j_transaction_t *tx = neo4j_calloc(config->allocator,
                                         NULL, 1, sizeof(neo4j_transaction_t));
  tx->allocator = config->allocator;
  tx->logger = neo4j_get_logger(config, "transactions");
  tx->connection = connection;
  tx->mpool = neo4j_std_mpool(config);

  tx->check_expired = tx_expired;
  tx->commit = tx_commit;
  tx->rollback = tx_rollback;
  tx->run = tx_run;

  tx->timeout = timeout;
  tx->mode = ( mode == NULL ? "w" : mode );
  tx->dbname = dbname;
  tx->is_open = 0;
  tx->is_expired = 0;
  tx->failed = 0;
  tx->failure = 0;
  tx->failure_code = neo4j_null;
  tx->failure_message = neo4j_null;
  tx->bookmarks = NULL;
  tx->num_bookmarks = 0;
  return tx;
}

// methods

int neo4j_commit(neo4j_transaction_t *tx)
{
  REQUIRE(tx != NULL, -1);
  return tx->commit(tx);
}

int neo4j_rollback(neo4j_transaction_t *tx)
{
  REQUIRE(tx != NULL, -1);
  return tx->rollback(tx);
}

neo4j_result_stream_t *neo4j_run_in_tx(neo4j_transaction_t *tx, const char *statement,
                                       neo4j_value_t params)
{
  REQUIRE(tx != NULL, NULL);
  REQUIRE(statement != NULL, NULL);
  REQUIRE(neo4j_type(params) == NEO4J_MAP || neo4j_is_null(params), NULL);
  if (neo4j_tx_expired(tx) == 1) {
    errno = NEO4J_TRANSACTION_DEFUNCT;
    neo4j_log_error(tx->logger,
                    "Attempt to run query in defunct transaction on %p\n",
                    (void *)tx->connection);
    tx->results = NULL;
    return NULL;
  }
  return tx->run(tx, statement, params);
}

// getters

int neo4j_tx_is_open(neo4j_transaction_t *tx)
{
  REQUIRE(tx != NULL, -1);
  return tx->is_open;
}

int neo4j_tx_expired(neo4j_transaction_t *tx)
{
  REQUIRE(tx != NULL, -1);
  return tx->check_expired(tx);
}

int neo4j_tx_failure(neo4j_transaction_t *tx)
{
  REQUIRE(tx != NULL, -1);
  return tx->failure;
}

int neo4j_tx_timeout(neo4j_transaction_t *tx)
{
  REQUIRE(tx != NULL, -1);
  return tx->timeout;
}

const char *neo4j_tx_mode(neo4j_transaction_t *tx)
{
  REQUIRE(tx != NULL, NULL);
  return tx->mode;
}

const char *neo4j_tx_dbname(neo4j_transaction_t *tx)
{
  REQUIRE(tx != NULL, NULL);
  return tx->dbname;
}

const char *neo4j_tx_failure_code(neo4j_transaction_t *tx)
{
  REQUIRE(tx != NULL, NULL);
  if (neo4j_is_null(tx->failure_code)) {
    return NULL;
  }
  char buf[128];
  return neo4j_string_value(tx->failure_code, buf, 128);
}

const char *neo4j_tx_failure_message(neo4j_transaction_t *tx)
{
  REQUIRE(tx != NULL, NULL);
  if (neo4j_is_null(tx->failure_message)) {
    return NULL;
  }
  char buf[128];
  return neo4j_string_value(tx->failure_message, buf, 128);
}

const char *neo4j_tx_commit_bookmark(neo4j_transaction_t *tx)
{
  REQUIRE(tx != NULL, NULL);
  if (neo4j_is_null(tx->commit_bookmark)) {
    return NULL;
  }
  char buf[128];
  return neo4j_string_value(tx->commit_bookmark, buf, 128);
}

// destructor

void neo4j_free_tx(neo4j_transaction_t *tx)
{
  // free bookmarks array space
  // free tx space
  if (tx->num_bookmarks > 0) {
    neo4j_free(tx->allocator, (void *)tx->bookmarks);
  }
  neo4j_free(tx->allocator, tx);
}
