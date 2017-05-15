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
#include "metadata.h"
#include "memory.h"
#include "util.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>


static int parse_failure_message(struct neo4j_failure_details *details,
        const char *message, neo4j_mpool_t *mpool);
static struct neo4j_statement_execution_step *meta_execution_steps(
        neo4j_value_t map, const char *description, const char *key_name,
        neo4j_mpool_t *mpool, neo4j_logger_t *logger);
static int map_get_typed(neo4j_value_t *value, neo4j_value_t map,
        const char *path, const char *key, neo4j_type_t expected,
        bool allow_null, const char *description, neo4j_logger_t *logger);
static char *extract_string(neo4j_value_t map, const char *path,
        const char *key, neo4j_mpool_t *mpool, const char *description,
        neo4j_logger_t *logger);
static int extract_int(long long *i, neo4j_value_t map, const char *path,
        const char *key, const char *description, neo4j_logger_t *logger);
static long long extract_uint(neo4j_value_t map, const char *path,
        const char *key, const char *description, neo4j_logger_t *logger);
static int extract_double(double *d, neo4j_value_t map, const char *path,
        const char *key, const char *description, neo4j_logger_t *logger);
static int extract_string_list(const char * const **strings,
        unsigned int *nstrings, neo4j_value_t map, const char *path,
        const char *key, bool allow_null, neo4j_mpool_t *mpool,
        const char *description, neo4j_logger_t *logger);
static char *alloc_string(neo4j_value_t value, neo4j_mpool_t *mpool);


const neo4j_value_t *neo4j_validate_metadata(const neo4j_value_t *fields,
        uint16_t nfields, const char *description, neo4j_logger_t *logger)
{
    assert(description != NULL);

    if (nfields != 1)
    {
        neo4j_log_error(logger, "Invalid number of fields in %s", description);
        errno = EPROTO;
        return NULL;
    }
    assert(fields != NULL);

    neo4j_type_t field_type = neo4j_type(fields[0]);
    if (field_type != NEO4J_MAP)
    {
        neo4j_log_error(logger, "Invalid field in %s: got %s, expected MAP",
                description, neo4j_typestr(field_type));
        errno = EPROTO;
        return NULL;
    }

    return &(fields[0]);
}


void neo4j_metadata_log(neo4j_logger_t *logger, uint_fast8_t level,
        const char *msg, neo4j_value_t metadata)
{
    char detail[1024];
    size_t n = neo4j_ntostring(metadata, detail, sizeof(detail));
    if (n >= sizeof(detail))
    {
        // TODO: dynamically allocate `detail` if static size is insufficient
        detail[sizeof(detail)-1] = '\0';
        detail[sizeof(detail)-2] = '.';
        detail[sizeof(detail)-3] = '.';
        detail[sizeof(detail)-4] = '.';
    }
    neo4j_log(logger, level, "%s: %s", msg, detail);
}


int neo4j_meta_failure_details(struct neo4j_failure_details *details,
        const neo4j_value_t map, neo4j_mpool_t *mpool, const char *description,
        neo4j_logger_t *logger)
{
    size_t pdepth = neo4j_mpool_depth(*mpool);

    const char *code_string = extract_string(map, NULL, "code", mpool,
            description, logger);
    if (code_string == NULL)
    {
        goto failure;
    }
    const char *message_string = extract_string(map, NULL, "message", mpool,
            description, logger);
    if (message_string == NULL)
    {
        goto failure;
    }

    details->code = code_string;
    details->message = message_string;
    // The message contains a lot of detail that needs to be parsed out.
    // Hopefully neo4j will start to provide these as separate attributes in
    // the response, avoiding the need for parsing and the risk created
    // due to the dependence on the specific format of the string.
    // Ref: https://github.com/neo4j/neo4j/issues/7318
    if (parse_failure_message(details, message_string, mpool))
    {
        goto failure;
    }

    return 0;

    int errsv;
failure:
    errsv = errno;
    neo4j_mpool_drainto(mpool, pdepth);
    errno = errsv;
    return -1;
}


int parse_failure_message(struct neo4j_failure_details *details,
        const char *s, neo4j_mpool_t *mpool)
{
    details->description = s;
    details->line = 0;
    details->column = 0;
    details->offset = 0;
    details->context = NULL;
    details->context_offset = 0;

    // Assume that messages containing position information always include
    // it at the end of the first line as " (line xx, column yy (offset zz))\n"
    const char *position = strstr(s, " (line ");
    if (position == NULL)
    {
        return 0;
    }

    unsigned int line;
    unsigned int column;
    unsigned int offset;
    if (sscanf(position, " (line %u, column %u (offset: %u))",
                &line, &column, &offset) != 3)
    {
        return 0;
    }

    const char *pend = strchr(position, '\n');
    if (pend == NULL)
    {
        return 0;
    }

    const char *context = pend + 1;
    if (*context != '"')
    {
        return 0;
    }

    const char *cend = strchr(context, '\n');
    if (cend == NULL || (cend - context) <= 2 || *(cend-1) != '"')
    {
        return 0;
    }

    size_t coffset = strspn(cend+1, " ");
    if (coffset == 0 || *(cend+1+coffset) != '^')
    {
        return 0;
    }

    ++context;
    --cend;
    --coffset;

    char *description = alloc_string(neo4j_ustring(s, position - s), mpool);
    if (description == NULL)
    {
        return -1;
    }

    char *ctx = alloc_string(neo4j_ustring(context, cend - context), mpool);
    if (ctx == NULL)
    {
        return -1;
    }

    details->description = description;
    details->line = line;
    details->column = column;
    details->offset = offset;
    details->context = ctx;
    details->context_offset = coffset;
    return 0;
}


int neo4j_meta_fieldnames(const char * const **names, unsigned int *nnames,
        neo4j_value_t map, neo4j_mpool_t *mpool, const char *description,
        neo4j_logger_t *logger)
{
    return extract_string_list(names, nnames, map, NULL, "fields", false,
            mpool, description, logger);
}


int neo4j_meta_statement_type(neo4j_value_t map, const char *description,
        neo4j_logger_t *logger)
{
    assert(neo4j_type(map) == NEO4J_MAP);
    assert(description != NULL);

    neo4j_value_t stype;
    if (map_get_typed(&stype, map, NULL, "type", NEO4J_STRING,
            true, description, logger))
    {
        return -1;
    }

    if (neo4j_is_null(stype))
    {
        return NEO4J_CONTROL_STATEMENT;
    }
    if (neo4j_eq(neo4j_string("r"), stype))
    {
        return NEO4J_READ_ONLY_STATEMENT;
    }
    else if (neo4j_eq(neo4j_string("w"), stype))
    {
        return NEO4J_WRITE_ONLY_STATEMENT;
    }
    else if (neo4j_eq(neo4j_string("rw"), stype))
    {
        return NEO4J_READ_WRITE_STATEMENT;
    }
    else if (neo4j_eq(neo4j_string("s"), stype))
    {
        return NEO4J_SCHEMA_UPDATE_STATEMENT;
    }
    else
    {
        neo4j_log_error(logger,
                "Invalid metadata in %s: unrecognized 'type' value",
                description);
        errno = EPROTO;
        return -1;
    }
}


long long neo4j_meta_result_available_after(neo4j_value_t map,
            const char *description, neo4j_logger_t *logger)
{
    assert(neo4j_type(map) == NEO4J_MAP);
    assert(description != NULL);
    return extract_uint(map, NULL, "result_available_after",
                description, logger);
}


long long neo4j_meta_result_consumed_after(neo4j_value_t map,
        const char *description, neo4j_logger_t *logger)
{
    assert(neo4j_type(map) == NEO4J_MAP);
    assert(description != NULL);
    return extract_uint(map, NULL, "result_consumed_after",
                description, logger);
}


int neo4j_meta_update_counts(struct neo4j_update_counts *counts,
        neo4j_value_t map, const char *description,
        neo4j_logger_t *logger)
{
    assert(counts != NULL);
    assert(neo4j_type(map) == NEO4J_MAP);
    assert(description != NULL);

    neo4j_value_t stats;
    if (map_get_typed(&stats, map, NULL, "stats", NEO4J_MAP, true,
            description, logger))
    {
        return -1;
    }
    if (neo4j_is_null(stats))
    {
        memset(counts, 0, sizeof(struct neo4j_update_counts));
        return 0;
    }

    static const char * const field_names[] = {
        "nodes-created",
        "nodes-deleted",
        "relationships-created",
        "relationships-deleted",
        "properties-set",
        "labels-added",
        "labels-removed",
        "indexes-added",
        "indexes-removed",
        "constraints-added",
        "constraints-removed",
        NULL
    };
    unsigned long long * const count_fields[] = {
        &(counts->nodes_created),
        &(counts->nodes_deleted),
        &(counts->relationships_created),
        &(counts->relationships_deleted),
        &(counts->properties_set),
        &(counts->labels_added),
        &(counts->labels_removed),
        &(counts->indexes_added),
        &(counts->indexes_removed),
        &(counts->constraints_added),
        &(counts->constraints_removed),
        NULL
    };

    assert(sizeof(field_names) == sizeof(count_fields));

    for (int i = 0; field_names[i] != NULL; ++i)
    {
        neo4j_value_t val;
        if (map_get_typed(&val, stats, "stats", field_names[i],
                NEO4J_INT, true, description, logger))
        {
            return -1;
        }
        if (neo4j_is_null(val))
        {
            continue;
        }

        int64_t count = neo4j_int_value(val);
        if (count < 0)
        {
            neo4j_log_error(logger,
                    "Invalid field in %s: 'stats.%s' value out of range",
                    description, field_names[i]);
            errno = EPROTO;
            return -1;
        }

        assert(count_fields[i] != NULL);
        *(count_fields[i]) = (unsigned long long)count;
    }

    return 0;
}


struct ref_counted_statement_plan
{
    struct neo4j_statement_plan _plan;

    unsigned int refcount;
    neo4j_mpool_t mpool;
};


struct neo4j_statement_plan *neo4j_meta_plan(neo4j_value_t map,
        const char *description, const neo4j_config_t *config,
        neo4j_logger_t *logger)
{
    assert(neo4j_type(map) == NEO4J_MAP);
    assert(description != NULL);
    assert(config != NULL);

    bool is_profile = true;

    neo4j_value_t plan_map;
    if (map_get_typed(&plan_map, map, NULL, "profile", NEO4J_MAP,
            true, description, logger))
    {
        return NULL;
    }
    if (neo4j_is_null(plan_map))
    {
        is_profile = false;
        if (map_get_typed(&plan_map, map, NULL, "plan", NEO4J_MAP, true,
                description, logger))
        {
            return NULL;
        }
        if (neo4j_is_null(plan_map))
        {
            errno = NEO4J_NO_PLAN_AVAILABLE;
            return NULL;
        }
    }
    const char *key_name = is_profile? "profile" : "plan";

    neo4j_mpool_t mpool = neo4j_std_mpool(config);

    struct ref_counted_statement_plan *rc_plan = neo4j_mpool_calloc(&mpool, 1,
            sizeof(struct ref_counted_statement_plan));
    if (rc_plan == NULL)
    {
        return NULL;
    }

    struct neo4j_statement_plan *plan = &(rc_plan->_plan);

    plan->output_step =
        meta_execution_steps(plan_map, description, key_name, &mpool, logger);
    if (plan->output_step == NULL)
    {
        goto failure;
    }

    neo4j_value_t final_args = plan->output_step->arguments;
    assert(neo4j_type(final_args) == NEO4J_MAP);

    char path[32];
    snprintf(path, sizeof(path), "%s.args", key_name);
    plan->version = extract_string(final_args, path, "version",
            &mpool, description, logger);
    if (plan->version == NULL)
    {
        goto failure;
    }
    plan->planner = extract_string(final_args, path, "planner",
            &mpool, description, logger);
    if (plan->planner == NULL)
    {
        goto failure;
    }
    plan->runtime = extract_string(final_args, path, "runtime",
            &mpool, description, logger);
    if (plan->runtime == NULL)
    {
        goto failure;
    }

    plan->is_profile = is_profile;

    rc_plan->refcount = 1;
    rc_plan->mpool = mpool;
    return plan;

    int errsv;
failure:
    errsv = errno;
    neo4j_mpool_drain(&mpool);
    errno = errsv;
    return NULL;
}


struct neo4j_statement_plan *neo4j_statement_plan_retain(
        struct neo4j_statement_plan *plan)
{
    if (plan == NULL)
    {
        return NULL;
    }
    struct ref_counted_statement_plan *rc_plan = container_of(plan,
            struct ref_counted_statement_plan, _plan);
    ++(rc_plan->refcount);
    return plan;
}


void neo4j_statement_plan_release(struct neo4j_statement_plan *plan)
{
    if (plan == NULL)
    {
        return;
    }
    struct ref_counted_statement_plan *rc_plan = container_of(plan,
            struct ref_counted_statement_plan, _plan);
    if (--(rc_plan->refcount) > 0)
    {
        return;
    }
    neo4j_mpool_t mpool = rc_plan->mpool;
    neo4j_mpool_drain(&mpool);
}


struct neo4j_statement_execution_step *meta_execution_steps(
        neo4j_value_t map, const char *description, const char *path,
        neo4j_mpool_t *mpool, neo4j_logger_t *logger)
{
    size_t pdepth = neo4j_mpool_depth(*mpool);

    struct neo4j_statement_execution_step *step = neo4j_mpool_calloc(mpool, 1,
            sizeof(struct neo4j_statement_execution_step));
    if (step == NULL)
    {
        return NULL;
    }

    if (map_get_typed(&(step->arguments), map, path, "args", NEO4J_MAP, false,
            description, logger))
    {
        goto failure;
    }

    step->operator_type = extract_string(map, path, "operatorType",
            mpool, description, logger);
    if (step->operator_type == NULL)
    {
        goto failure;
    }

    if (extract_string_list(&(step->identifiers), &(step->nidentifiers),
                map, path, "identifiers", true, mpool, description, logger))
    {
        goto failure;
    }

    char subpath[256];
    snprintf(subpath, sizeof(subpath), "%s.%s", path, "args");

    if (extract_double(&(step->estimated_rows), step->arguments, subpath,
                "EstimatedRows", description, logger))
    {
        goto failure;
    }

    long long rows;
    if ((rows = extract_uint(map, path, "rows", description, logger)) < 0)
    {
        goto failure;
    }
    step->rows = (unsigned long long)rows;

    long long db_hits;
    if ((db_hits = extract_uint(map, path, "dbHits", description, logger)) < 0)
    {
        goto failure;
    }
    step->db_hits = (unsigned long long)db_hits;

    long long page_cache_hits;
    if ((page_cache_hits = extract_uint(map, path, "pageCacheHits",
                description, logger)) < 0)
    {
        goto failure;
    }
    step->page_cache_hits = (unsigned long long)page_cache_hits;

    long long page_cache_misses;
    if ((page_cache_misses = extract_uint(map, path, "pageCacheMisses",
                description, logger)) < 0)
    {
        goto failure;
    }
    step->page_cache_misses = (unsigned long long)page_cache_misses;

    neo4j_value_t children;
    if (map_get_typed(&children, map, path, "children", NEO4J_LIST,
            true, description, logger))
    {
        goto failure;
    }
    if (neo4j_is_null(children))
    {
        step->sources = NULL;
        step->nsources = 0;
    }
    else
    {
        step->nsources = neo4j_list_length(children);
        step->sources = neo4j_mpool_calloc(mpool, step->nsources,
                sizeof(struct neo4j_statement_execution_step *));
        if (step->sources == NULL)
        {
            goto failure;
        }

        for (unsigned int i = 0; i < step->nsources; ++i)
        {
            snprintf(subpath, sizeof(subpath), "%s.children[%u]", path, i);

            neo4j_value_t child = neo4j_list_get(children, i);
            if (neo4j_type(child) != NEO4J_MAP)
            {
                neo4j_log_error(logger,
                        "Invalid field in %s: %s is %s, expected Map",
                        description, subpath,
                        neo4j_typestr(neo4j_type(child)));
                errno = EPROTO;
                goto failure;
            }
            // TODO: consider limiting depth of recursion
            step->sources[i] = meta_execution_steps(
                    child, description, subpath, mpool, logger);
            if (step->sources[i] == NULL)
            {
                goto failure;
            }
        }
    }

    return step;

    int errsv;
failure:
    errsv = errno;
    neo4j_mpool_drainto(mpool, pdepth);
    errno = errsv;
    return NULL;
}


int map_get_typed(neo4j_value_t *value, neo4j_value_t map, const char *path,
        const char *key, neo4j_type_t expected, bool allow_null,
        const char *description, neo4j_logger_t *logger)
{
    neo4j_value_t val = neo4j_map_get(map, key);
    if (neo4j_is_null(val))
    {
        if (allow_null)
        {
            *value = neo4j_null;
            return 0;
        }
        neo4j_log_error(logger, "Invalid metadata in %s: no '%s%s%s' property",
                description, (path != NULL)? path : "",
                (path != NULL)? "." : "", key);
        errno = EPROTO;
        return -1;
    }
    if (neo4j_type(val) != expected)
    {
        neo4j_log_error(logger,
                "Invalid field in %s: '%s%s%s' is %s, expected %s",
                description, (path != NULL)? path : "",
                (path != NULL)? "." : "", key, neo4j_typestr(neo4j_type(val)),
                neo4j_typestr(expected));
        errno = EPROTO;
        return -1;
    }
    *value = val;
    return 0;
}


char *extract_string(neo4j_value_t map, const char *path, const char *key,
        neo4j_mpool_t *mpool, const char *description, neo4j_logger_t *logger)
{
    neo4j_value_t val;
    if (map_get_typed(&val, map, path, key, NEO4J_STRING, false,
            description, logger))
    {
        return NULL;
    }
    return alloc_string(val, mpool);
}


int extract_int(long long *i, neo4j_value_t map, const char *path,
        const char *key, const char *description, neo4j_logger_t *logger)
{
    neo4j_value_t val;
    if (map_get_typed(&val, map, path, key, NEO4J_INT, true,
            description, logger))
    {
        return -1;
    }
    if (neo4j_is_null(val))
    {
        *i = 0;
        return 0;
    }
    *i = neo4j_int_value(val);
    return 0;
}


long long extract_uint(neo4j_value_t map, const char *path,
        const char *key, const char *description, neo4j_logger_t *logger)
{
    long long val;
    if (extract_int(&val, map, path, key, description, logger))
    {
        return -1;
    }
    return (val > 0)? val : 0;
}


int extract_double(double *d, neo4j_value_t map, const char *path,
        const char *key, const char *description, neo4j_logger_t *logger)
{
    neo4j_value_t val;
    if (map_get_typed(&val, map, path, key, NEO4J_FLOAT, true,
            description, logger))
    {
        return -1;
    }
    if (neo4j_is_null(val))
    {
        *d = 0;
        return 0;
    }
    *d = neo4j_float_value(val);
    return 0;
}


int extract_string_list(const char * const **strings, unsigned int *nstrings,
        neo4j_value_t map, const char *path, const char *key, bool allow_null,
        neo4j_mpool_t *mpool, const char *description, neo4j_logger_t *logger)
{
    neo4j_value_t listv;
    if (map_get_typed(&listv, map, path, key, NEO4J_LIST, allow_null,
            description, logger))
    {
        return -1;
    }
    if (neo4j_is_null(listv))
    {
        assert(!allow_null);
        *strings = NULL;
        *nstrings = 0;
        return 0;
    }

    unsigned int n = neo4j_list_length(listv);
    if (n == 0)
    {
        *strings = NULL;
        *nstrings = 0;
        return 0;
    }

    size_t pdepth = neo4j_mpool_depth(*mpool);
    char **cstrings = neo4j_mpool_calloc(mpool, n, sizeof(const char *));
    if (cstrings == NULL)
    {
        return -1;
    }

    for (unsigned int i = 0; i < n; ++i)
    {
        neo4j_value_t sv = neo4j_list_get(listv, i);
        if (neo4j_type(sv) != NEO4J_STRING)
        {
            neo4j_log_error(logger,
                    "Invalid field in %s: %s%s%s[%d] is %s, expected String",
                    description, (path != NULL)? path : "",
                    (path != NULL)? "." : "", key, i,
                    neo4j_typestr(neo4j_type(sv)));
            errno = EPROTO;
            goto failure;
        }
        cstrings[i] = alloc_string(sv, mpool);
        if (cstrings[i] == NULL)
        {
            goto failure;
        }
    }

    // clang will incorrectly raise an error without this cast
    *strings = (const char *const *)cstrings;
    *nstrings = n;
    return 0;

    int errsv;
failure:
    errsv = errno;
    neo4j_mpool_drainto(mpool, pdepth);
    errno = errsv;
    return -1;
}


char *alloc_string(neo4j_value_t value, neo4j_mpool_t *mpool)
{
    assert(neo4j_type(value) == NEO4J_STRING);
    size_t nlength = neo4j_string_length(value);
    char *s = neo4j_mpool_alloc(mpool, nlength + 1);
    if (s == NULL)
    {
        return NULL;
    }
    return neo4j_string_value(value, s, nlength + 1);
}
