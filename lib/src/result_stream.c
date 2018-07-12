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
#include "../../config.h"
#include "result_stream.h"
#include "connection.h"
#include "client_config.h"
#include "job.h"
#include "metadata.h"
#include "util.h"
#include <assert.h>
#include <stddef.h>


int neo4j_check_failure(neo4j_result_stream_t *results)
{
    REQUIRE(results != NULL, -1);
    return results->check_failure(results);
}


const char *neo4j_error_code(neo4j_result_stream_t *results)
{
    REQUIRE(results != NULL, NULL);
    return results->error_code(results);
}


const char *neo4j_error_message(neo4j_result_stream_t *results)
{
    REQUIRE(results != NULL, NULL);
    return results->error_message(results);
}


const struct neo4j_failure_details *neo4j_failure_details(
        neo4j_result_stream_t *results)
{
    REQUIRE(results != NULL, NULL);
    return results->failure_details(results);
}


unsigned int neo4j_nfields(neo4j_result_stream_t *results)
{
    REQUIRE(results != NULL, 0);
    return results->nfields(results);
}


const char *neo4j_fieldname(neo4j_result_stream_t *results,
        unsigned int index)
{
    REQUIRE(results != NULL, NULL);
    return results->fieldname(results, index);
}


neo4j_result_t *neo4j_fetch_next(neo4j_result_stream_t *results)
{
    REQUIRE(results != NULL, NULL);
    return results->fetch_next(results);
}


neo4j_result_t *neo4j_peek(neo4j_result_stream_t *results, unsigned int depth)
{
    REQUIRE(results != NULL, NULL);
    return results->peek(results, depth);
}


unsigned long long neo4j_result_count(neo4j_result_stream_t *results)
{
    REQUIRE(results != NULL, -1);
    return results->count(results);
}


unsigned long long neo4j_results_available_after(neo4j_result_stream_t *results)
{
    REQUIRE(results != NULL, -1);
    return results->available_after(results);
}


unsigned long long neo4j_results_consumed_after(neo4j_result_stream_t *results)
{
    REQUIRE(results != NULL, -1);
    return results->consumed_after(results);
}


int neo4j_statement_type(neo4j_result_stream_t *results)
{
    REQUIRE(results != NULL, -1);
    return results->statement_type(results);
}


struct neo4j_statement_plan *neo4j_statement_plan(
        neo4j_result_stream_t *results)
{
    REQUIRE(results != NULL, NULL);
    return results->statement_plan(results);
}


struct neo4j_update_counts neo4j_update_counts(neo4j_result_stream_t *results)
{
    if (results == NULL)
    {
        struct neo4j_update_counts counts;
        memset(&counts, 0, sizeof(counts));
        return counts;
    }
    return results->update_counts(results);
}


int neo4j_close_results(neo4j_result_stream_t *results)
{
    REQUIRE(results != NULL, -1);
    return results->close(results);
}


neo4j_value_t neo4j_result_field(const neo4j_result_t *result,
        unsigned int index)
{
    REQUIRE(result != NULL, neo4j_null);
    return result->field(result, index);
}


neo4j_result_t *neo4j_retain(neo4j_result_t *result)
{
    REQUIRE(result != NULL, NULL);
    return result->retain(result);
}


void neo4j_release(neo4j_result_t *result)
{
    assert(result != NULL);
    result->release(result);
}



typedef struct run_result_stream run_result_stream_t;

typedef struct result_record result_record_t;
struct result_record
{
    neo4j_result_t _result;

    unsigned int refcount;
    neo4j_mpool_t mpool;
    neo4j_value_t list;
    // TODO: add skip list for faster peeking
    result_record_t *next;
};


struct run_result_stream
{
    neo4j_result_stream_t _result_stream;

    neo4j_connection_t *connection;
    neo4j_job_t job;
    neo4j_logger_t *logger;
    neo4j_memory_allocator_t *allocator;
    neo4j_mpool_t mpool;
    neo4j_mpool_t record_mpool;
    unsigned int refcount;
    unsigned int starting;
    unsigned int streaming;
    int statement_type;
    struct neo4j_statement_plan *statement_plan;
    struct neo4j_update_counts update_counts;
    unsigned long long available_after;
    unsigned long long consumed_after;
    int failure;
    struct neo4j_failure_details failure_details;
    unsigned int nfields;
    const char *const *fields;
    result_record_t *records;
    result_record_t *records_tail;
    unsigned long long records_depth;
    result_record_t *last_fetched;
    unsigned long long nrecords;
    unsigned int awaiting_records;
};


static run_result_stream_t *run_rs_open(neo4j_connection_t *connection);
static int run_rs_check_failure(neo4j_result_stream_t *self);
static const char *run_rs_error_code(neo4j_result_stream_t *self);
static const char *run_rs_error_message(neo4j_result_stream_t *self);
const struct neo4j_failure_details *run_rs_failure_details(
        neo4j_result_stream_t *results);
static unsigned int run_rs_nfields(neo4j_result_stream_t *results);
static const char *run_rs_fieldname(neo4j_result_stream_t *self,
        unsigned int index);
static neo4j_result_t *run_rs_fetch_next(neo4j_result_stream_t *self);
static neo4j_result_t *run_rs_peek(neo4j_result_stream_t *self,
        unsigned int depth);
static unsigned long long run_rs_count(neo4j_result_stream_t *self);
static unsigned long long run_rs_available_after(neo4j_result_stream_t *self);
static unsigned long long run_rs_consumed_after(neo4j_result_stream_t *self);
static int run_rs_statement_type(neo4j_result_stream_t *self);
static struct neo4j_statement_plan *run_rs_statement_plan(
        neo4j_result_stream_t *self);
static struct neo4j_update_counts run_rs_update_counts(
        neo4j_result_stream_t *self);
static int run_rs_close(neo4j_result_stream_t *self);

static neo4j_value_t run_result_field(const neo4j_result_t *self,
        unsigned int index);
static neo4j_result_t *run_result_retain(neo4j_result_t *self);
static void run_result_release(neo4j_result_t *self);

static void abort_job(neo4j_job_t *job, int err);
static int run_callback(void *cdata, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc);
static int pull_all_callback(void *cdata, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc);
static int discard_all_callback(void *cdata, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc);
static int stream_end(run_result_stream_t *results, neo4j_message_type_t type,
        const char *src_message_type, const neo4j_value_t *argv, uint16_t argc);
static int await(run_result_stream_t *results, const unsigned int *condition);
static int append_result(run_result_stream_t *results,
        const neo4j_value_t *argv, uint16_t argc);
void result_record_release(result_record_t *record);
static int set_eval_failure(run_result_stream_t *results,
        const char *src_message_type, const neo4j_value_t *argv, uint16_t argc);
static void set_failure(run_result_stream_t *results, int error);


neo4j_result_stream_t *neo4j_run(neo4j_connection_t *connection,
        const char *statement, neo4j_value_t params)
{
    REQUIRE(connection != NULL, NULL);
    REQUIRE(statement != NULL, NULL);
    REQUIRE(neo4j_type(params) == NEO4J_MAP || neo4j_is_null(params), NULL);

    run_result_stream_t *results = run_rs_open(connection);
    if (results == NULL)
    {
        return NULL;
    }

    if (neo4j_session_run(connection, &(results->mpool), statement, params,
            run_callback, results))
    {
        neo4j_log_debug_errno(results->logger, "neo4j_session_run failed");
        goto failure;
    }
    (results->refcount)++;

    if (neo4j_session_pull_all(results->connection, &(results->record_mpool),
            pull_all_callback, results))
    {
        neo4j_log_debug_errno(results->logger, "neo4j_session_pull_all failed");
        goto failure;
    }
    (results->refcount)++;

    results->starting = true;
    results->streaming = true;
    return &(results->_result_stream);

    int errsv;
failure:
    errsv = errno;
    run_rs_close(&(results->_result_stream));
    errno = errsv;
    return NULL;
}


neo4j_result_stream_t *neo4j_send(neo4j_connection_t *connection,
        const char *statement, neo4j_value_t params)
{
    REQUIRE(connection != NULL, NULL);
    REQUIRE(statement != NULL, NULL);
    REQUIRE(neo4j_type(params) == NEO4J_MAP || neo4j_is_null(params), NULL);

    run_result_stream_t *results = run_rs_open(connection);
    if (results == NULL)
    {
        return NULL;
    }

    if (neo4j_session_run(connection, &(results->mpool), statement, params,
            run_callback, results))
    {
        neo4j_log_debug_errno(results->logger, "neo4j_connection_run failed");
        goto failure;
    }
    (results->refcount)++;

    if (neo4j_session_discard_all(results->connection, &(results->mpool),
            discard_all_callback, results))
    {
        neo4j_log_debug_errno(results->logger,
                "neo4j_connection_discard_all failed");
        goto failure;
    }
    (results->refcount)++;

    results->starting = true;
    results->streaming = true;
    return &(results->_result_stream);

    int errsv;
failure:
    errsv = errno;
    run_rs_close(&(results->_result_stream));
    errno = errsv;
    return NULL;
}


run_result_stream_t *run_rs_open(neo4j_connection_t *connection)
{
    assert(connection != NULL);
    neo4j_config_t *config = connection->config;

    run_result_stream_t *results = neo4j_calloc(config->allocator,
            NULL, 1, sizeof(run_result_stream_t));

    results->connection = connection;
    results->logger = neo4j_get_logger(config, "results");
    results->allocator = config->allocator;
    results->mpool = neo4j_std_mpool(config);
    results->record_mpool = neo4j_std_mpool(config);
    results->statement_type = -1;
    results->refcount = 1;

    results->job.abort = abort_job;
    if (neo4j_attach_job(connection, &(results->job)))
    {
        neo4j_log_debug_errno(results->logger,
                "failed to attach job to connection");
        goto failure;
    }

    neo4j_result_stream_t *result_stream = &(results->_result_stream);
    result_stream->check_failure = run_rs_check_failure;
    result_stream->error_code = run_rs_error_code;
    result_stream->error_message = run_rs_error_message;
    result_stream->failure_details = run_rs_failure_details;
    result_stream->nfields = run_rs_nfields;
    result_stream->fieldname = run_rs_fieldname;
    result_stream->fetch_next = run_rs_fetch_next;
    result_stream->peek = run_rs_peek;
    result_stream->count = run_rs_count;
    result_stream->available_after = run_rs_available_after;
    result_stream->consumed_after = run_rs_consumed_after;
    result_stream->statement_type = run_rs_statement_type;
    result_stream->statement_plan = run_rs_statement_plan;
    result_stream->update_counts = run_rs_update_counts;
    result_stream->close = run_rs_close;
    return results;

    int errsv;
failure:
    errsv = errno;
    run_rs_close(&(results->_result_stream));
    errno = errsv;
    return NULL;
}


int run_rs_check_failure(neo4j_result_stream_t *self)
{
    run_result_stream_t *results = container_of(self,
            run_result_stream_t, _result_stream);
    REQUIRE(results != NULL, -1);

    if (results->failure != 0 || await(results, &(results->starting)))
    {
        assert(results->failure != 0);
        // continue
    }
    return results->failure;
}


const char *run_rs_error_code(neo4j_result_stream_t *self)
{
    run_result_stream_t *results = container_of(self,
            run_result_stream_t, _result_stream);
    REQUIRE(results != NULL, NULL);
    return results->failure_details.code;
}


const char *run_rs_error_message(neo4j_result_stream_t *self)
{
    run_result_stream_t *results = container_of(self,
            run_result_stream_t, _result_stream);
    REQUIRE(results != NULL, NULL);
    return results->failure_details.message;
}


const struct neo4j_failure_details *run_rs_failure_details(
        neo4j_result_stream_t *self)
{
    run_result_stream_t *results = container_of(self,
            run_result_stream_t, _result_stream);
    REQUIRE(results != NULL, NULL);
    return &(results->failure_details);
}


unsigned int run_rs_nfields(neo4j_result_stream_t *self)
{
    run_result_stream_t *results = container_of(self,
            run_result_stream_t, _result_stream);
    REQUIRE(results != NULL, 0);

    if (results->failure != 0 || await(results, &(results->starting)))
    {
        assert(results->failure != 0);
        errno = results->failure;
        return 0;
    }
    return results->nfields;
}


const char *run_rs_fieldname(neo4j_result_stream_t *self,
        unsigned int index)
{
    run_result_stream_t *results = container_of(self,
            run_result_stream_t, _result_stream);
    REQUIRE(results != NULL, NULL);

    if (results->failure != 0 || await(results, &(results->starting)))
    {
        assert(results->failure != 0);
        errno = results->failure;
        return NULL;
    }
    assert(results->fields != NULL);
    if (index >= results->nfields)
    {
        errno = EINVAL;
        return NULL;
    }
    assert(results->fields != NULL);
    return results->fields[index];
}


neo4j_result_t *run_rs_fetch_next(neo4j_result_stream_t *self)
{
    run_result_stream_t *results = container_of(self,
            run_result_stream_t, _result_stream);
    REQUIRE(results != NULL, NULL);

    if (results->last_fetched != NULL)
    {
        result_record_release(results->last_fetched);
        results->last_fetched = NULL;
    }

    if (results->records == NULL)
    {
        if (!results->streaming)
        {
            errno = results->failure;
            return NULL;
        }
        assert(results->failure == 0);
        results->awaiting_records = 1;
        if (await(results, &(results->awaiting_records)))
        {
            errno = results->failure;
            return NULL;
        }
        if (results->records == NULL)
        {
            assert(!results->streaming);
            errno = results->failure;
            return NULL;
        }
    }

    result_record_t *record = results->records;
    results->records = record->next;
    --(results->records_depth);
    if (results->records == NULL)
    {
        assert(results->records_depth == 0);
        results->records_tail = NULL;
    }
    record->next = NULL;

    results->last_fetched = record;
    return &(record->_result);
}


neo4j_result_t *run_rs_peek(neo4j_result_stream_t *self, unsigned int depth)
{
    run_result_stream_t *results = container_of(self,
            run_result_stream_t, _result_stream);
    REQUIRE(results != NULL, NULL);

    if (results->records_depth <= depth)
    {
        if (!results->streaming)
        {
            errno = results->failure;
            return NULL;
        }

        assert(results->failure == 0);
        results->awaiting_records = depth - results->records_depth + 1;
        if (await(results, &(results->awaiting_records)))
        {
            errno = results->failure;
            return NULL;
        }

        if (results->records_depth <= depth)
        {
            assert(!results->streaming);
            errno = results->failure;
            return NULL;
        }
    }

    result_record_t *record = results->records;
    assert(record != NULL);
    for (unsigned int i = depth; i > 0; --i)
    {
        record = record->next;
        assert(record != NULL);
    }
    return &(record->_result);
}


unsigned long long run_rs_count(neo4j_result_stream_t *self)
{
    run_result_stream_t *results = container_of(self,
            run_result_stream_t, _result_stream);
    REQUIRE(results != NULL, 0);
    return results->nrecords;
}


unsigned long long run_rs_available_after(neo4j_result_stream_t *self)
{
    run_result_stream_t *results = container_of(self,
            run_result_stream_t, _result_stream);
    REQUIRE(results != NULL, 0);

    if (results->failure != 0 || await(results, &(results->starting)))
    {
        assert(results->failure != 0);
        errno = results->failure;
        return 0;
    }

    return results->available_after;
}


unsigned long long run_rs_consumed_after(neo4j_result_stream_t *self)
{
    run_result_stream_t *results = container_of(self,
            run_result_stream_t, _result_stream);
    REQUIRE(results != NULL, 0);

    if (results->failure != 0 || await(results, &(results->streaming)))
    {
        assert(results->failure != 0);
        errno = results->failure;
        return 0;
    }

    return results->consumed_after;
}


int run_rs_statement_type(neo4j_result_stream_t *self)
{
    run_result_stream_t *results = container_of(self,
            run_result_stream_t, _result_stream);
    REQUIRE(results != NULL, -1);

    if (results->failure != 0 || await(results, &(results->streaming)))
    {
        assert(results->failure != 0);
        errno = results->failure;
        return -1;
    }

    return results->statement_type;
}


struct neo4j_statement_plan *run_rs_statement_plan(neo4j_result_stream_t *self)
{
    run_result_stream_t *results = container_of(self,
            run_result_stream_t, _result_stream);
    REQUIRE(results != NULL, NULL);

    if (results->failure != 0 || await(results, &(results->streaming)))
    {
        assert(results->failure != 0);
        errno = results->failure;
        return NULL;
    }

    if (results->statement_plan == NULL)
    {
        errno = NEO4J_NO_PLAN_AVAILABLE;
        return NULL;
    }
    return neo4j_statement_plan_retain(results->statement_plan);
}


struct neo4j_update_counts run_rs_update_counts(neo4j_result_stream_t *self)
{
    run_result_stream_t *results = container_of(self,
            run_result_stream_t, _result_stream);
    if (results == NULL)
    {
        errno = EINVAL;
        goto failure;
    }

    if (results->failure != 0 || await(results, &(results->streaming)))
    {
        assert(results->failure != 0);
        errno = results->failure;
        goto failure;
    }

    return results->update_counts;

    struct neo4j_update_counts counts;
failure:
    memset(&counts, 0, sizeof(counts));
    return counts;
}


int run_rs_close(neo4j_result_stream_t *self)
{
    run_result_stream_t *results = container_of(self,
            run_result_stream_t, _result_stream);
    REQUIRE(results != NULL, -1);

    results->streaming = false;
    assert(results->refcount > 0);
    --(results->refcount);
    int err = await(results, &(results->refcount));
    // even if await fails, queued messages should still be drained
    assert(results->refcount == 0);

    if (results->connection != NULL)
    {
        neo4j_detach_job(results->connection, (neo4j_job_t *)&(results->job));
        results->connection = NULL;
    }

    if (results->last_fetched != NULL)
    {
        result_record_release(results->last_fetched);
        results->last_fetched = NULL;
    }
    while (results->records != NULL)
    {
        result_record_t *next = results->records->next;
        result_record_release(results->records);
        results->records = next;
    }

    neo4j_statement_plan_release(results->statement_plan);
    results->statement_plan = NULL;
    neo4j_logger_release(results->logger);
    results->logger = NULL;
    neo4j_mpool_drain(&(results->record_mpool));
    neo4j_mpool_drain(&(results->mpool));
    neo4j_free(results->allocator, results);
    return err;
}


neo4j_value_t run_result_field(const neo4j_result_t *self,
        unsigned int index)
{
    const result_record_t *record = container_of(self,
            result_record_t, _result);
    REQUIRE(record != NULL, neo4j_null);
    return neo4j_list_get(record->list, index);
}


neo4j_result_t *run_result_retain(neo4j_result_t *self)
{
    result_record_t *record = container_of(self,
            result_record_t, _result);
    REQUIRE(record != NULL, NULL);
    (record->refcount)++;
    return self;
}


void run_result_release(neo4j_result_t *self)
{
    result_record_t *record = container_of(self,
            result_record_t, _result);
    result_record_release(record);
}


void abort_job(neo4j_job_t *job, int err)
{
    run_result_stream_t *results = container_of(job,
            run_result_stream_t, job);
    if (results == NULL || results->connection == NULL)
    {
        return;
    }

    job->next = NULL;
    results->connection = NULL;
    if (results->streaming && results->failure == 0)
    {
        set_failure(results, err);
    }
}


int run_callback(void *cdata, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc)
{
    assert(cdata != NULL);
    assert(argc == 0 || argv != NULL);
    run_result_stream_t *results = (run_result_stream_t *)cdata;
    neo4j_logger_t *logger = results->logger;
    neo4j_connection_t *connection = results->connection;

    results->starting = false;
    --(results->refcount);

    if (type == NULL || connection == NULL)
    {
        return 0;
    }

    if (type == NEO4J_FAILURE_MESSAGE)
    {
        return set_eval_failure(results, "RUN", argv, argc);
    }
    if (type == NEO4J_IGNORED_MESSAGE)
    {
        if (results->failure == 0)
        {
            set_failure(results, NEO4J_STATEMENT_PREVIOUS_FAILURE);
        }
        return 0;
    }

    char description[128];
    snprintf(description, sizeof(description), "%s in %p (response to RUN)",
            neo4j_message_type_str(type), (void *)connection);

    if (type != NEO4J_SUCCESS_MESSAGE)
    {
        neo4j_log_error(logger, "Unexpected %s", description);
        set_failure(results, errno = EPROTO);
        return -1;
    }

    const neo4j_value_t *metadata = neo4j_validate_metadata(argv, argc,
            description, logger);
    if (metadata == NULL)
    {
        set_failure(results, errno);
        return -1;
    }

    if (neo4j_log_is_enabled(connection->logger, NEO4J_LOG_TRACE))
    {
        neo4j_metadata_log(logger, NEO4J_LOG_TRACE, description, *metadata);
    }

    if (neo4j_meta_fieldnames(&(results->fields), &(results->nfields),
                *metadata, &(results->mpool), description, logger))
    {
        set_failure(results, errno);
        return -1;
    }

    long long available_after =
            neo4j_meta_result_available_after(*metadata, description, logger);
    if (available_after < 0)
    {
        set_failure(results, errno);
        return -1;
    }
    results->available_after = available_after;

    return 0;
}


int pull_all_callback(void *cdata, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc)
{
    assert(cdata != NULL);
    assert(argc == 0 || argv != NULL);
    run_result_stream_t *results = (run_result_stream_t *)cdata;

    if (type == NEO4J_RECORD_MESSAGE)
    {
        if (append_result(results, argv, argc))
        {
            neo4j_log_trace_errno(results->logger, "append_result failed");
            set_failure(results, errno);
            return -1;
        }
        return 1;
    }

    --(results->refcount);
    results->streaming = false;

    // not a record, so keep this memory along with the result stream
    if (neo4j_mpool_merge(&(results->mpool), &(results->record_mpool)) < 0)
    {
        neo4j_log_trace_errno(results->logger, "neo4j_mpool_merge failed");
        set_failure(results, errno);
        return -1;
    }

    return stream_end(results, type, "PULL_ALL", argv, argc);
}


int discard_all_callback(void *cdata, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc)
{
    assert(cdata != NULL);
    assert(argc == 0 || argv != NULL);
    run_result_stream_t *results = (run_result_stream_t *)cdata;

    --(results->refcount);
    results->streaming = false;

    return stream_end(results, type, "DISCARD_ALL", argv, argc);
}


int stream_end(run_result_stream_t *results, neo4j_message_type_t type,
        const char *src_message_type, const neo4j_value_t *argv, uint16_t argc)
{
    neo4j_logger_t *logger = results->logger;
    neo4j_connection_t *connection = results->connection;

    if (connection == NULL || type == NULL)
    {
        return 0;
    }

    neo4j_config_t *config = connection->config;

    if (type == NEO4J_IGNORED_MESSAGE)
    {
        if (results->failure == 0)
        {
            neo4j_log_error(logger,
                    "Unexpected IGNORED message received in %p"
                    " (in response to %s, yet no failure occurred)",
                    (void *)connection, src_message_type);
            set_failure(results, errno = EPROTO);
            return -1;
        }
        return 0;
    }

    assert(results->failure == 0);

    if (type == NEO4J_FAILURE_MESSAGE)
    {
        return set_eval_failure(results, src_message_type, argv, argc);
    }
    if (type != NEO4J_SUCCESS_MESSAGE)
    {
        neo4j_log_error(logger,
                "Unexpected %s message received in %p"
                " (in response to %s)", neo4j_message_type_str(type),
                (void *)connection, src_message_type);
        set_failure(results, errno = EPROTO);
        return -1;
    }

    char description[128];
    snprintf(description, sizeof(description), "SUCCESS in %p (response to %s)",
            (void *)connection, src_message_type);

    const neo4j_value_t *metadata = neo4j_validate_metadata(argv, argc,
            description, logger);
    if (metadata == NULL)
    {
        set_failure(results, errno);
        return -1;
    }

    if (neo4j_log_is_enabled(logger, NEO4J_LOG_TRACE))
    {
        neo4j_metadata_log(logger, NEO4J_LOG_TRACE, description, *metadata);
    }

    long long consumed_after =
            neo4j_meta_result_consumed_after(*metadata, description, logger);
    if (consumed_after < 0)
    {
        set_failure(results, errno);
        return -1;
    }
    results->consumed_after = consumed_after;

    results->statement_type =
        neo4j_meta_statement_type(*metadata, description, logger);
    if (results->statement_type < 0)
    {
        set_failure(results, errno);
        return -1;
    }

    results->statement_plan = neo4j_meta_plan(*metadata, description,
            config, logger);
    if (results->statement_plan == NULL && errno != NEO4J_NO_PLAN_AVAILABLE)
    {
        set_failure(results, errno);
        return -1;
    }

    if (neo4j_meta_update_counts(&(results->update_counts), *metadata,
                description, logger))
    {
        set_failure(results, errno);
        return -1;
    }

    return 0;
}


int await(run_result_stream_t *results, const unsigned int *condition)
{
    if (*condition > 0 && neo4j_session_sync(results->connection, condition))
    {
        set_failure(results, errno);
        return -1;
    }
    return 0;
}


int append_result(run_result_stream_t *results,
        const neo4j_value_t *argv, uint16_t argc)
{
    assert(results != NULL);
    neo4j_connection_t *connection = results->connection;

    if (argc != 1)
    {
        neo4j_log_error(results->logger,
                "Invalid number of fields in RECORD message received in %p",
                (void *)connection);
        errno = EPROTO;
        return -1;
    }

    assert(argv != NULL);

    neo4j_type_t arg_type = neo4j_type(argv[0]);
    if (arg_type != NEO4J_LIST)
    {
        neo4j_log_error(results->logger,
                "Invalid field in RECORD message received in %p"
                " (got %s, expected List)", (void *)connection,
                neo4j_typestr(arg_type));
        errno = EPROTO;
        return -1;
    }

    (results->nrecords)++;

    if (!results->streaming)
    {
        // discard memory for the record
        neo4j_mpool_drain(&(results->record_mpool));
        return 0;
    }

    assert(connection != NULL);
    neo4j_config_t *config = connection->config;

    result_record_t *record = neo4j_mpool_calloc(&(results->record_mpool),
            1, sizeof(result_record_t));
    if (record == NULL)
    {
        return -1;
    }

    record->refcount = 1;

    // save memory for the record with the record
    record->mpool = results->record_mpool;
    results->record_mpool = neo4j_std_mpool(config);

    record->list = argv[0];
    record->next = NULL;

    neo4j_result_t *result = &(record->_result);
    result->field = run_result_field;
    result->retain = run_result_retain;
    result->release = run_result_release;

    if (results->records == NULL)
    {
        assert(results->records_tail == NULL);
        assert(results->records_depth == 0);
        results->records = record;
        results->records_tail = record;
    }
    else
    {
        results->records_tail->next = record;
        results->records_tail = record;
    }
    ++(results->records_depth);

    if (results->awaiting_records > 0)
    {
        --(results->awaiting_records);
    }

    return 0;
}


void result_record_release(result_record_t *record)
{
    assert(record->refcount > 0);
    if (--(record->refcount) == 0)
    {
        // record was allocated in its own pool, so draining the pool
        // deallocates the record - so we have to copy the pool out first
        // or it'll be deallocated whist still draining
        neo4j_mpool_t mpool = record->mpool;
        neo4j_mpool_drain(&mpool);
    }
}


int set_eval_failure(run_result_stream_t *results, const char *src_message_type,
        const neo4j_value_t *argv, uint16_t argc)
{
    assert(results != NULL);

    if (results->failure != 0)
    {
        return 0;
    }

    set_failure(results, NEO4J_STATEMENT_EVALUATION_FAILED);

    char description[128];
    snprintf(description, sizeof(description), "FAILURE in %p (response to %s)",
            (void *)(results->connection), src_message_type);

    const neo4j_value_t *metadata = neo4j_validate_metadata(argv, argc,
            description, results->logger);
    if (metadata == NULL)
    {
        set_failure(results, errno);
        return -1;
    }

    if (neo4j_log_is_enabled(results->logger, NEO4J_LOG_TRACE))
    {
        neo4j_metadata_log(results->logger, NEO4J_LOG_TRACE, description,
                *metadata);
    }

    if (neo4j_meta_failure_details(&(results->failure_details), *metadata,
                &(results->mpool), description, results->logger))
    {
        set_failure(results, errno);
        return -1;
    }

    return 0;
}


void set_failure(run_result_stream_t *results, int error)
{
    assert(results != NULL);
    assert(error != 0);
    results->failure = error;
    results->streaming = false;
    results->awaiting_records = 0;
    memset(&(results->failure_details), 0, sizeof(results->failure_details));
}
