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
 * Log the contents of a metadata map.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [level] The level to log at.
 * @param [msg] A message to prefix in the log ('%s: ').
 * @param [metadata] A neo4j map value.
 */
void neo4j_metadata_log(neo4j_logger_t *logger, uint_fast8_t level,
        const char *msg, neo4j_value_t metadata);

/**
 * Validate failure metadata received in a server message.
 *
 * Ensures that the metadata map contains a "code" and a "message" entry,
 * both of which must be of type String, and returns null terminated strings
 * of each in the provided pointers.
 *
 * @internal
 *
 * @param [details] A pointer to a failure details struct that will be
 *         populated after successful return.
 * @param [map] The metadata map.
 * @param [mpool] A memory pool to allocate strings in.
 * @param [description] A description of the message, for use when logging
 *         errors.
 * @param [logger] A logger to emit error messages to.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
__neo4j_must_check
int neo4j_meta_failure_details(struct neo4j_failure_details *details,
        neo4j_value_t map, neo4j_mpool_t *mpool, const char *description,
        neo4j_logger_t *logger);

/**
 * Extract fields from a metadata map.
 *
 * Validates that the map contains a "fields" entry, of type List, containing
 * all String values. These strings are extracted into a set of null
 * terminated strings.
 *
 * @internal
 *
 * @param [names] A pointer that will be updated to reference an array of
 *         field name strings, or `NULL` if there were zero field names.
 * @param [nnames] A pointer to an unsigned int that will be updated with
 *         the total number of strings in the names array.
 * @param [map] The metadata map.
 * @param [mpool] A memory pool to allocate strings in.
 * @param [description] A description of the message from which the metadata
 *         came, for use when logging errors.
 * @param [logger] A logger to emit error messages to.
 * @return The number of fields, or -1 if an error occurs (errno will be set).
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
__neo4j_must_check
int neo4j_meta_fieldnames(const char * const **names, unsigned int *nnames,
        neo4j_value_t map, neo4j_mpool_t *mpool, const char *description,
        neo4j_logger_t *logger);

/**
 * Extract a timing value from a metadata map.
 *
 * Checks for a "result_available_after" entry, of type Integer.
 *
 * @internal
 *
 * @param [map] The metadata map.
 * @param [description] A description of the message from which the metadata
 *         came, for use when logging errors.
 * @param [logger] A logger to emit error messages to.
 * @return The value, or -1 if an error occurs (errno will be set).
 */
long long neo4j_meta_result_available_after(neo4j_value_t map,
        const char *description, neo4j_logger_t *logger);

/**
 * Extract a timing value from a metadata map.
 *
 * Checks for a "result_consumed_after" entry, of type Integer.
 *
 * @internal
 *
 * @param [map] The metadata map.
 * @param [description] A description of the message from which the metadata
 *         came, for use when logging errors.
 * @param [logger] A logger to emit error messages to.
 * @return The value, or -1 if an error occurs (errno will be set).
 */
long long neo4j_meta_result_consumed_after(neo4j_value_t map,
        const char *description, neo4j_logger_t *logger);

/**
 * Extract statement type from a metadata map.
 *
 * Checks for a "type" entry, of type String. If found, and is of
 * a known type, the matching statement type is returned.
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
 * Extract update counts from a metadata map.
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

/**
 * Extract a statement plan from a metadata map.
 *
 * Checks for a "plan" or "profile" entry, and extracts the plan as provided
 * by the server. The returned plan must be later released using
 * `neo4j_statment_plan_release(...)`.
 *
 * If there is no plan in the metadata, a `NULL` value will be returned
 * and errno will be set to NEO4J_NO_PLAN_AVAILABLE. Note that errno will not
 * be modified when a plan is returned, so error checking MUST evaluate the
 * return value first.
 *
 * @internal
 *
 * @param [map] The metadata map.
 * @param [description] A description of the message from which the metadata
 *         came, for use when logging errors.
 * @param [config] The client configuration.
 * @param [logger] A logger to emit error messages to.
 * @return A pointer to the plan, or `NULL` if the plan is not available or
 *         if an error occurs (errno will be set).
 */
struct neo4j_statement_plan *neo4j_meta_plan(neo4j_value_t map,
        const char *description, const neo4j_config_t *config,
        neo4j_logger_t *logger);

/**
 * Retain a statement plan.
 *
 * @param [plan] The plan to retain.
 * @return The plan.
 */
struct neo4j_statement_plan *neo4j_statement_plan_retain(
        struct neo4j_statement_plan *plan);


#endif/*NEO4J_METADATA_H*/
