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
#ifndef NEO4J_SERIALIZATION_H
#define NEO4J_SERIALIZATION_H

#include "neo4j-client.h"
#include "iostream.h"

/**
 * Serialize a neo4j value to a stream.
 *
 * @internal
 *
 * @param [value] A neo4j value to be serialized.
 * @param [stream] The iostream to write to.
 * @return 0 on success, -1 on failure (errno will be set).
 */
__neo4j_must_check
int neo4j_serialize(neo4j_value_t v, struct neo4j_iostream *stream);

int neo4j_null_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream);
int neo4j_bool_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream);
int neo4j_int_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream);
int neo4j_float_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream);
int neo4j_string_serialize(const neo4j_value_t *value,
        neo4j_iostream_t *stream);
int neo4j_bytes_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream);
int neo4j_list_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream);
int neo4j_map_serialize(const neo4j_value_t *value, neo4j_iostream_t *stream);
int neo4j_struct_serialize(const neo4j_value_t *value,
        neo4j_iostream_t *stream);

#endif/*NEO4J_SERIALIZATION_H*/
