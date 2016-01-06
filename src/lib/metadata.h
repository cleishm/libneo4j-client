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
#ifndef NEO4J_METADATA_H
#define NEO4J_METADATA_H

#include "neo4j-client.h"
#include "logging.h"


/**
 * Validate metadata received in a server message.
 *
 * Validates that the message contained a single field, and that the field was
 * of type MAP.
 *
 * @param [fields] The fields of the server message.
 * @param [nfields] The number of fields.
 * @param [description] A description of the message, for use when logging
 *         errors.
 * @param [logger] A logger to emit error messages to.
 * @return A neo4j map value, or NULL if an error occurs (errno will be set).
 */
const neo4j_value_t *neo4j_validate_metadata(const neo4j_value_t *fields,
        uint16_t nfields, const char *description, neo4j_logger_t *logger);

/**
 * Validate failure metadata received in a server message.
 *
 * Ensures that the metadata map contains a "code" and a "message" entry,
 * both of which must be of type String, and returns null terminated strings
 * of each in the provided pointers.
 *
 * @param [code] A pointer that will be updated to reference a null terminated
 *         string containing the failure code.
 * @param [message] A pointer that will be updated to reference a null
 *         terminated string containing the failure message.
 * @param [map] The metadata map.
 * @param [mpool] A memory pool to allocate strings in.
 * @param [description] A description of the message, for use when logging
 *         errors.
 * @param [logger] A logger to emit error messages to.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_meta_failure_details(const char **code, const char **message, const
        neo4j_value_t map, neo4j_mpool_t *mpool, const char *description,
        neo4j_logger_t *logger);

/**
 * Extract fields from a neo4j map.
 *
 * Validates that the map contains a "fields" entry, of type List, containing
 * all String values. These strings are extracted into a set of null
 * terminated strings.
 *
 * @param [names] A pointer that will be updated to reference an array of
 *         field name strings, or `NULL` if there were zero field names.
 * @param [map] The metadata map.
 * @param [mpool] A memory pool to allocate strings in.
 * @param [description] A description of the message, for use when logging
 *         errors.
 * @param [logger] A logger to emit error messages to.
 * @return The number of fields, or -1 if an error occurs (errno will be set).
 */
int neo4j_meta_fieldnames(const char * const **names, const neo4j_value_t map,
        neo4j_mpool_t *mpool, const char *description, neo4j_logger_t *logger);


#endif/*NEO4J_METADATA_H*/
