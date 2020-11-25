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
#include "serialization.h"
#include "deserialization.h"
#include "memory.h"
#include "metadata.h"
#include "network.h"
#include "util.h"
#include <assert.h>
#include <stddef.h>
#include <unistd.h>

neo4j_transaction_t *new_transaction(neo4j_config_t *config, int timeout, const char *mode);
void destroy_transaction(neo4j_transaction_t *tx);

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
neo4j_transaction_t *neo4j_begin_tx(neo4j_connection_t *connection,
                                      int tx_timeout, const char *tx_mode)
{
    REQUIRE(connection != NULL, NULL);

    neo4j_config_t *config = connection->config;
    neo4j_transaction_t *tx = new_transaction(config, timeout, mode);

    if (neo4j_session_transact(connection, "BEGIN", begin_callback, tx))
    {
      // handle fail
      neo4j_log_debug_errno(tx->logger, "begin transaction failed");
      goto failure;
    }
    int errsv; // ?
 failure:
    errsrv = errno; // ?
    // destroy_transaction(tx); not here
    set_tx_failure(tx,errsrv);
    errno = errsrv;
}

int begin_callback(void *cdata, neo4j_message_type_t type, const neo4j_value_t *argv, uint16_t argc)
{
  assert(cdata != NULL);
  assert(argc == 0 || argv != NULL);
  neo4j_transaction_t *tx = (neo4j_transaction_t *) cdata;

  if (type == NEO4J_FAILURE_MESSAGE)
    {
      // get FAILURE argv and set tx failure info here
      return ...
        }

}

neo4j_transaction_t *neo4j_run_in_tx(neo4j_connection_t *connection, void *cdata,
  const char *statement, neo4j_value_t params)
{
  neo4j_transaction_t *tx = (neo4j_transaction_t *) cdata;
}

neo4j_transaction_t *neo4j_commit_tx(neo4j_connection_t *connection, void *cdata)
{
  neo4j_transaction_t *tx = (neo4j_transaction_t *) cdata;

}

neo4j_transaction_t *neo4j_rollback_tx(neo4j_connection_t *connection, void *cdata)
{
  neo4j_transaction_t *tx = (neo4j_transaction_t *) cdata;
}

// constructor

neo4j_transaction_t *new_transaction(neo4j_config_t *config, int timeout, const char *mode) {
  neo4j_transaction_t *tx = neo4j_calloc(config->allocator,
                                         NULL, 1, sizeof(neo4j_transaction_t));
  tx->logger = neo4j_get_logger(config, "transactions");
  tx->connection = connection;
  tx->timeout = (timeout == 0 ? DEFAULT_TX_TIMEOUT : timeout);
  tx->mode = mode;
  neo4j_map_entry_t ent[2] = { neo4j_map_entry("tx_timeout",neo4j_int(tx_timeout)),
                               neo4j_map_entry("mode",neo4j_string(tx_mode)) };
  tx->extra = &(neo4j_map(ent,2));

  return tx;
}

// tx setters
void set_tx_failure(neo4j_transaction_t *tx, int error) {
  assert(tx != NULL);
  assert(error != 0);
  assert( tx != NULL );
  assert( err != 0 );
  tx->failure = error;
  // begin_callback should have set failure info using response argv from server

}

// tx getters
void tx_bookmarks(neo4j_transaction_t *tx) {
}

// destructor

void destroy_transaction(neo4j_transaction_t *tx) {
  return;
}
