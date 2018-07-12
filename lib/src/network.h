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
#ifndef NEO4J_NETWORK_H
#define NEO4J_NETWORK_H

#include "neo4j-client.h"

/**
 * Connect a TCP socket.
 *
 * @internal
 *
 * @param [hostname] The hostname to connect to.
 * @param [servname] The name of the TCP service to connect to.
 * @param [config] The client configuration.
 * @param [logger] A logger to write diagnostics and errors to.
 * @return 0 on success, or -1 on failure (errno will be set).
 */
__neo4j_must_check
int neo4j_connect_tcp_socket(const char *hostname, const char *servname,
        const neo4j_config_t *config, struct neo4j_logger *logger);

#endif/*NEO4J_NETWORK_H*/
