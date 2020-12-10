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
#ifndef NEO4J_TOFU_H
#define NEO4J_TOFU_H

#include "neo4j-client.h"

/**
 * Check if the host is trusted according to the TOFU process.
 *
 * @internal
 *
 * @param [hostname] The hostname.
 * @param [port] The port on the host.
 * @param [fingerprint] The fingerprint of the host.
 * @param [config] The client config.
 * @param [flags] A bitmask of flags to control connections.
 * @return 0 if the host is trusted; 1 if the host is not trusted and
 *         no unverified host callback was made; 2 if the host is not
 *         trusted and a verified host callback was made; and -1
 *         if an error occurs (errno will be set).
 */
__neo4j_must_check
int neo4j_check_known_hosts(const char * restrict hostname, int port,
        const char * restrict fingerprint, const neo4j_config_t *config,
        uint_fast8_t flags);

#endif/*NEO4J_TOFU_H*/
