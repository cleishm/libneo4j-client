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
#ifndef NEO4J_MESSAGES_H
#define NEO4J_MESSAGES_H

#include "neo4j-client.h"
#include "iostream.h"
#include "memory.h"
#include <stdint.h>

typedef const struct neo4j_message_type *neo4j_message_type_t;
struct neo4j_message_type
{
    const char *name;
    uint8_t struct_signature;
};

extern const neo4j_message_type_t NEO4J_INIT_MESSAGE;
extern const neo4j_message_type_t NEO4J_RUN_MESSAGE;
extern const neo4j_message_type_t NEO4J_DISCARD_ALL_MESSAGE;
extern const neo4j_message_type_t NEO4J_PULL_ALL_MESSAGE;
extern const neo4j_message_type_t NEO4J_ACK_FAILURE_MESSAGE;
extern const neo4j_message_type_t NEO4J_RESET_MESSAGE;
extern const neo4j_message_type_t NEO4J_RECORD_MESSAGE;
extern const neo4j_message_type_t NEO4J_SUCCESS_MESSAGE;
extern const neo4j_message_type_t NEO4J_FAILURE_MESSAGE;
extern const neo4j_message_type_t NEO4J_IGNORED_MESSAGE;

neo4j_message_type_t neo4j_message_type_for_signature(uint8_t signature);

static inline const char *neo4j_message_type_str(neo4j_message_type_t type)
{
    return type->name;
}


/**
 * Send a message on an iostream.
 *
 * This call may block until network buffers have sufficient space.
 *
 * @internal
 *
 * @param [ios] The iostream to send over.
 * @param [type] The message type.
 * @param [argv] The vector of argument values to send with the message.
 * @param [argc] The length of the argument vector.
 * @param [buffer] A buffer to use for data held until a minimal chunk size is
 *         reached.
 * @param [bsize] The size of `buffer` (and the minimal chunk size).
 * @param [max_chunk] The maximum chunk size.
 * @return 0 on success, -1 on failure (errno will be set).
 */
__neo4j_must_check
int neo4j_message_send(neo4j_iostream_t *ios, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc, uint8_t *buffer,
        uint16_t bsize, uint16_t max_chunk);

/**
 * Receive a message on a connection.
 *
 * This call may block until data is available from the network.
 *
 * @internal
 *
 * @param [ios] The iostream to receive from.
 * @param [mpool] A memory pool to allocate values and buffer spaces in.
 * @param [type] A pointer to a message type, which will be updated.
 * @param [argv] A pointer to an argument vector, which will be updated
 *         to point to the received message arguments.
 * @param [argc] A pointer to a `uin16_t`, which will be updated with the
 *         length of the received argument vector.
 * @return 0 on success, -1 on failure (errno will be set).
 */
__neo4j_must_check
int neo4j_message_recv(neo4j_iostream_t *ios,
        neo4j_mpool_t *mpool, neo4j_message_type_t *type,
        const neo4j_value_t **argv, uint16_t *argc);

#endif/*NEO4J_MESSAGES_H*/
