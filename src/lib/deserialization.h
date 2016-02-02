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
#ifndef NEO4J_DESERIALIZATION_H
#define NEO4J_DESERIALIZATION_H

#include "neo4j-client.h"
#include "iostream.h"
#include "memory.h"

/**
 * Read a neo4j value from a stream.
 *
 * @internal
 *
 * @param [stream] The iostream to read from.
 * @param [mpool] The memory pool to allocate value space in.
 * @param [value] A pointer to a neo4j value, which will be updated.
 * @return 0 on success, -1 on failure (errno will be set).
 */
__neo4j_must_check
int neo4j_deserialize(neo4j_iostream_t *stream, neo4j_mpool_t *mpool,
        neo4j_value_t *value);

#endif/*NEO4J_DESERIALIZATION_H*/
