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
 * @internal
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
 * @internal
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
__neo4j_must_check
int neo4j_meta_failure_details(const char **code, const char **message,
        neo4j_value_t map, neo4j_mpool_t *mpool, const char *description,
        neo4j_logger_t *logger);

/**
 * Extract fields from a neo4j map.
 *
 * Validates that the map contains a "fields" entry, of type List, containing
 * all String values. These strings are extracted into a set of null
 * terminated strings.
 *
 * @internal
 *
 * @param [names] A pointer that will be updated to reference an array of
 *         field name strings, or `NULL` if there were zero field names.
 * @param [map] The metadata map.
 * @param [mpool] A memory pool to allocate strings in.
 * @param [description] A description of the message from which the metadata
 *         came, for use when logging errors.
 * @param [logger] A logger to emit error messages to.
 * @return The number of fields, or -1 if an error occurs (errno will be set).
 */
int neo4j_meta_fieldnames(const char * const **names, neo4j_value_t map,
        neo4j_mpool_t *mpool, const char *description, neo4j_logger_t *logger);

/**
 * Extract statement type from a neo4j map.
 *
 * Checks for a "type" entry, of type String. If found, the matching
 * statement type is returned. If it is not found, or if the value is not
 * a known statement type.
 *
 * @internal
 *
 * @param [map] The metadata map.
 * @param [description] A description of the message from which the metadata
 *         came, for use when logging errors.
 * @param [logger] A logger to emit error messages to.
 * @return A statement type on success, or -1 if an error occurs
 *         (errno will be set).
 */
int neo4j_meta_statement_type(neo4j_value_t map, const char *description,
        neo4j_logger_t *logger);

/**
 * Extract update counts from a neo4j map.
 *
 * Checks for a "stats" entry, of type Map, containing another Map of
 * Integer values. These integers, if found, are used to populate the
 * supplied update counts structure.
 *
 * @internal
 *
 * @param [counts] A pointer to a update counts structure that will be
 *         populated with values found in the map.
 * @param [map] The metadata map.
 * @param [description] A description of the message from which the metadata
 *         came, for use when logging errors.
 * @param [logger] A logger to emit error messages to.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
__neo4j_must_check
int neo4j_meta_update_counts(struct neo4j_update_counts *counts,
        neo4j_value_t map, const char *description,
        neo4j_logger_t *logger);


#endif/*NEO4J_METADATA_H*/
