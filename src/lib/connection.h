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
#ifndef NEO4J_CONNECTION_H
#define NEO4J_CONNECTION_H

#include "neo4j-client.h"
#include "client_config.h"
#include "iostream.h"
#include "memory.h"
#include "session.h"
#include "uri.h"

struct neo4j_connection
{
    neo4j_config_t *config;
    neo4j_logger_t *logger;

    neo4j_iostream_t *iostream;
    uint32_t version;
    bool insecure;

    uint8_t *snd_buffer;
    struct neo4j_request *request_queue;

    neo4j_session_t *session;
};


/**
 * Send a message on a connection.
 *
 * This call may block until network buffers have sufficient space.
 *
 * @param [connection] The connection to send over.
 * @param [type] The message type.
 * @param [argv] The vector of argument values to send with the message.
 * @param [argc] The length of the argument vector.
 * @return 0 on success, -1 on failure (errno will be set).
 */
int neo4j_connection_send(neo4j_connection_t *connection,
        neo4j_message_type_t type, const neo4j_value_t *argv, uint16_t argc);

/**
 * Receive a message on a connection.
 *
 * This call may block until data is available from the network.
 *
 * @param [connection] The connection to receive from.
 * @param [mpool] A memory pool to allocate values and buffer spaces in.
 * @param [type] A pointer to a message type, which will be updated.
 * @param [argv] A pointer to an argument vector, which will be updated
 *         to point to the received message arguments.
 * @param [argc] A pointer to a `uin16_t`, which will be updated with the
 *         length of the received argument vector.
 * @return 0 on success, -1 on failure (errno will be set).
 */
int neo4j_connection_recv(neo4j_connection_t *connection, neo4j_mpool_t *mpool,
        neo4j_message_type_t *type, const neo4j_value_t **argv, uint16_t *argc);

/**
 * Attach a session to a connection.
 *
 * @param [connection] The connection to attach to.
 * @param [session] The session to attach.
 * @return 0 on success, -1 on failure (errno will be set).
 */
int neo4j_attach_session(neo4j_connection_t *connection,
        neo4j_session_t *session);

/**
 * Detach a session from a connection.
 *
 * @param [connection] The connection to detach from.
 * @param [session] The session to detach.
 * @param [reusable] `true` if the connection may be reused, `false otherwise.
 * @return 0 on success, -1 on failure (errno will be set).
 */
int neo4j_detach_session(neo4j_connection_t *connection,
        neo4j_session_t *session, bool reusable);

#endif/*NEO4J_CONNECTION_H*/
