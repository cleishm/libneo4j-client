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

#define DEFAULT_TX_TIMEOUT (60)


// tx accessors
int neo4j_check_tx_failure(neo4j_transaction_t *tx);
const char *neo4j_tx_error_code(neo4j_transaction_t *tx);
const char *neo4j_tx_error_message(neo4j_transaction_t *tx);

struct neo4j_transaction
{
  int (*check_failure)(neo4j_transaction_t *self);
  const char *(*error_code)(neo4j_transaction_t *self);
  const char *(*error_message)(neo4j_transaction_t *self);

  // begin, run, commit, rollback from methods within this structure?
  const char** bookmarks;
  neo4j_result_stream_t *tx_results;
  neo4j_connection_t *connection;
  neo4j_logger_t *logger;
  neo4j_value_t *metadata;
  unsigned int is_open;
  unsigned int is_expired;
  neo4j_value_t *extra;
  int timeout;
  const char *mode;
  int failure;
}

#endif/*NEO4J_TRANSACTION_H*/
