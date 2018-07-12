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
#ifndef NEO4J_RESULT_STREAM_H
#define NEO4J_RESULT_STREAM_H

#include "neo4j-client.h"


struct neo4j_result_stream
{
    /**
     * Check if a results stream has failed.
     *
     * @param [self] This result stream.
     * @return 0 if no failure has occurred, and an error number otherwise.
     */
    int (*check_failure)(neo4j_result_stream_t *self);

    /**
     * Return the error code sent from Neo4j.
     *
     * When `neo4j_check_failure` returns `NEO4J_STATEMENT_EVALUATION_FAILED`,
     * then this function can be used to get the error code sent from Neo4j.
     *
     * @param [self] This result stream.
     * @return A `NULL` terminated string reprenting the error code, or NULL
     *         if the stream has not failed or the failure was not
     *         `NEO4J_STATEMENT_EVALUATION_FAILED`.
     */
    const char *(*error_code)(neo4j_result_stream_t *self);

    /**
     * Return the error message sent from Neo4j.
     *
     * When `neo4j_check_failure` returns `NEO4J_STATEMENT_EVALUATION_FAILED`,
     * then this function can be used to get the detailed error message sent
     * from Neo4j.
     *
     * @param [self] This result stream.
     * @return A `NULL` terminated string containing the error message, or NULL
     *         if the stream has not failed or the failure was not
     *         `NEO4J_STATEMENT_EVALUATION_FAILED`.
     */
    const char *(*error_message)(neo4j_result_stream_t *self);

    /*
     * Return the details of a failure.
     *
     * When neo4j_check_failure() returns `NEO4J_STATEMENT_EVALUATION_FAILED`,
     * then this function can be used to get the details of the failure.
     *
     * @param [self] This result stream.
     * @return A pointer to the failure details, or `NULL` if no failure
     *         details were available.
     */
    const struct neo4j_failure_details *(*failure_details)(
            neo4j_result_stream_t *self);

    /**
     * Get the number of fields in a result stream.
     *
     * @param [self] This result stream.
     * @return The number of fields in the result, or -1 on failure
     *         (errno will be set).
     */
    unsigned int (*nfields)(neo4j_result_stream_t *self);

    /**
     * Get the name of a field in a result stream.
     *
     * @param [self] This result stream.
     * @param [index] The field index to get the name of.
     * @return The name of the field, as a NULL terminated string,
     *         or NULL if an error occurs (errno will be set).
     */
    const char *(*fieldname)(neo4j_result_stream_t *self, unsigned int index);

    /**
     * Fetch the next record from the result stream.
     *
     * @param [self] This result stream.
     * @return The next result, or NULL if the stream is exahusted or an
     *         error has occurred (errno will be set).
     */
    neo4j_result_t *(*fetch_next)(neo4j_result_stream_t *self);

    /**
     * Peek at a record in the result stream.
     *
     * @param [self] This result stream.
     * @param [depth] The depth to peek into the remaining records.
     * @return The result at the specified depth, or `NULL` if the stream is
     *         exahusted or an error has occurred (errno will be set).
     */
    neo4j_result_t *(*peek)(neo4j_result_stream_t *self, unsigned int depth);

    /**
     * Return the number of records received.
     *
     * This value will continue to increase until all results have been fetched.
     *
     * @param [self] This result stream.
     * @return The number of result records.
     */
    unsigned long long (*count)(neo4j_result_stream_t *results);

    /**
     * Return the reported time until the first record was available.
     *
     * @param [self] This result stream.
     * @return The time, in milliseconds.
     */
    unsigned long long (*available_after)(neo4j_result_stream_t *self);

    /**
     * Return the reported time until all records were consumed.
     *
     * @param [self] This result stream.
     * @return The time, in milliseconds.
     */
    unsigned long long (*consumed_after)(neo4j_result_stream_t *self);

    /**
     * Return the update counts for the result stream.
     *
     * @attention As the update counts are only available at the end of the
     * result stream, invoking this function will will result in any unfetched
     * results being pulled from the server and held in memory. It is usually
     * better to exhaust the stream using `neo4j_fetch_next(...)` before
     * invoking this method.
     *
     * @param [self] The result stream.
     * @return The update counts.
     */
    struct neo4j_update_counts (*update_counts)(neo4j_result_stream_t *self);

    /**
     * Return the statement type for the result stream.
     *
     * The returned value will either be -1, if an error occurs, or one of the
     * following values:
     * - NEO4J_READ_ONLY_STATEMENT
     * - NEO4J_WRITE_ONLY_STATEMENT
     * - NEO4J_READ_WRITE_STATEMENT
     * - NEO4J_SCHEMA_WRITE_STATEMENT
     *
     * @attention As the statement type is only available at the end of the
     * result stream, invoking this function will will result in any unfetched
     * results being pulled from the server and held in memory. It is usually
     * better to exhaust the stream using `neo4j_fetch_next(...)` before
     * invoking this method.
     *
     * @param [self] The result stream.
     * @return The statement type, or -1 if an error occurs (errno will be set).
     */
    int (*statement_type)(neo4j_result_stream_t *self);

    /**
     * Return the statement plan for the result stream.
     *
     * The returned statement plan, if not `NULL`, must be later released using
     * `neo4j_statement_plan_release(...)`.
     *
     * @param [self] The result stream.
     * @return The statement plan/profile, or `NULL` if none was provided.
     */
    struct neo4j_statement_plan *(*statement_plan)(neo4j_result_stream_t *self);

    /**
     * Close a result stream.
     *
     * Closes the result stream and releases all memory held by it, including
     * results and values obtained from it.
     *
     * NOTE: After this function is invoked, all `neo4j_result_t` objects
     * fetched from this stream, and any values obtained from them, will be
     * invalid and _must not be accessed_. Doing so will result in undetermined
     * and unstable behaviour. This is true even if this function returns an
     * error.
     *
     * @param [self] This result stream. The pointer will be invalid after the
     *         function returns.
     * @return 0 on success, or -1 on failure (errno will be set).
     */
    int (*close)(neo4j_result_stream_t *self);
};


struct neo4j_result
{
    /**
     * Get a field from a result.
     *
     * @param [self] This result.
     * @param [index] The field index to get.
     * @return The field from the result, or `neo4j_null` if index
     *         is out of bounds.
     */
    neo4j_value_t (*field)(const neo4j_result_t *self, unsigned int index);

    /**
     * Retain a result.
     *
     * This retains the result and all values contained within it, preventing
     * them from being deallocated on the next call to `neo4j_fetch_next(...)`
     * or when the result stream is closed via `neo4j_close_results(...)`. Once
     * retained, the result _must_ later be explicitly released via
     * `neo4j_release(...)`.
     *
     * @param [self] This result.
     * @return The result.
     */
    neo4j_result_t *(*retain)(neo4j_result_t *self);

    /**
     * Release a result.
     *
     * @param [self] This result.
     */
    void (*release)(neo4j_result_t *self);
};


#endif/*NEO4J_RESULT_STREAM_H*/
