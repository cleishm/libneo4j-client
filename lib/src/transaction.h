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
#ifndef NEO4J_TRANSACTION_H
#define NEO4J_TRANSACTION_H

#include "neo4j-client.h"
#include "atomic.h"
#include "result_stream.h"
#include "client_config.h"
#include "iostream.h"
#include "job.h"
#include "logging.h"
#include "memory.h"
#include "messages.h"
#include "uri.h"

// api level

neo4j_transaction_t *neo4j_begin_tx(neo4j_connection_t *connection, int tx_timeout, const char *tx_mode);
int neo4j_commit(neo4j_transaction_t *tx);
int neo4j_rollback(neo4j_transaction_t *tx);
neo4j_result_stream_t *neo4j_run_in_tx(neo4j_transaction_t *tx, const char *statement, neo4j_value_t params);
int neo4j_tx_is_open(neo4j_transaction_t *tx);
int neo4j_tx_expired(neo4j_transaction_t *tx);
int neo4j_tx_failure(neo4j_transaction_t *tx);
int neo4j_tx_timeout(neo4j_transaction_t *tx);
const char *neo4j_tx_mode(neo4j_transaction_t *tx);
const char *neo4j_tx_failure_code(neo4j_transaction_t *tx);
const char *neo4j_tx_failure_message(neo4j_transaction_t *tx);
const char *neo4j_tx_commit_bookmark(neo4j_transaction_t *tx);
void neo4j_free_tx(neo4j_transaction_t *tx);

struct neo4j_transaction
{
  int (*check_expired)(neo4j_transaction_t *self);
  const char *(*error_code)(neo4j_transaction_t *self);
  const char *(*error_message)(neo4j_transaction_t *self);
  int (*commit)(neo4j_transaction_t *self);
  int (*rollback)(neo4j_transaction_t *self);
  neo4j_result_stream_t *(*run)(neo4j_transaction_t *self, const char *statement, neo4j_value_t params);
  neo4j_value_t *bookmarks; // a C array of neo4j_strings
  int num_bookmarks; // len of bookmarks array
  neo4j_value_t metadata; // a neo4j_map of transaction metadata (values are neo4j_strings)
  neo4j_value_t commit_bookmark; // a neo4j_string returned on successful commit
  neo4j_result_stream_t *results; // results of RUN within this transaction
  neo4j_memory_allocator_t *allocator;
  neo4j_connection_t *connection;
  neo4j_logger_t *logger;
  neo4j_mpool_t mpool;

  unsigned int is_open; // this tx has begun and is active
  unsigned int is_expired; // this tx has expired and is defunct
  unsigned int failed; // this tx failed before successful commit or rollback
  int failure; // errno
  neo4j_value_t extra; // client-side message arguments
  int timeout; // client-side timeout in ms requested
  const char *mode; // client-side mode string : "w" write, "r" read
  neo4j_value_t failure_code; // server-side failure code (neo4j_string)
  neo4j_value_t failure_message; // server-side failure message (neo4j_string)
};

#endif/*NEO4J_TRANSACTION_H*/
