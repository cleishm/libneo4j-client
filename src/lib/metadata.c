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
#include <assert.h>


static char *extract_string(neo4j_value_t value, neo4j_mpool_t *mpool);


const neo4j_value_t *neo4j_validate_metadata(const neo4j_value_t *fields,
        uint16_t nfields, const char *description, neo4j_logger_t *logger)
{
    assert(description != NULL);

    if (nfields != 1)
    {
        neo4j_log_error(logger, "invalid number of fields in %s", description);
        errno = EPROTO;
        return NULL;
    }
    assert(fields != NULL);

    neo4j_type_t field_type = neo4j_type(fields[0]);
    if (field_type != NEO4J_MAP)
    {
        neo4j_log_error(logger, "invalid field in %s: got %s, expected MAP",
                description, neo4j_type_str(field_type));
        errno = EPROTO;
        return NULL;
    }

    return &(fields[0]);
}


int neo4j_meta_failure_details(const char **code, const char **message, const
        neo4j_value_t map, neo4j_mpool_t *mpool, const char *description,
        neo4j_logger_t *logger)
{
    size_t pdepth = neo4j_mpool_depth(*mpool);

    neo4j_value_t code_val = neo4j_map_get(map, neo4j_string("code"));
    if (neo4j_is_null(code_val))
    {
        neo4j_log_error(logger, "invalid field in %s: no 'code' property",
                description);
        errno = EPROTO;
        goto failure;
    }
    if (neo4j_type(code_val) != NEO4J_STRING)
    {
        neo4j_log_error(logger,
                "invalid field in %s: 'code' property not a String",
                description);
        errno = EPROTO;
        goto failure;
    }

    const neo4j_value_t message_val =
        neo4j_map_get(map, neo4j_string("message"));
    if (neo4j_is_null(message_val))
    {
        neo4j_log_error(logger, "invalid field in %s: no 'message' property",
                description);
        errno = EPROTO;
        goto failure;
    }
    if (neo4j_type(message_val) != NEO4J_STRING)
    {
        neo4j_log_error(logger,
                "invalid field in %s: 'message' property not a String",
                description);
        errno = EPROTO;
        goto failure;
    }

    const char *code_string = extract_string(code_val, mpool);
    if (code_string == NULL)
    {
        goto failure;
    }
    const char *message_string = extract_string(message_val, mpool);
    if (message_string == NULL)
    {
        goto failure;
    }

    *code = code_string;
    *message = message_string;
    return 0;

    int errsv;
failure:
    errsv = errno;
    neo4j_mpool_drainto(mpool, pdepth);
    errno = errsv;
    return -1;
}


int neo4j_meta_fieldnames(const char * const **names, const neo4j_value_t map,
        neo4j_mpool_t *mpool, const char *description, neo4j_logger_t *logger)
{
    assert(neo4j_type(map) == NEO4J_MAP);
    assert(mpool != NULL);
    assert(description != NULL);
    size_t pdepth = neo4j_mpool_depth(*mpool);

    const neo4j_value_t mfields = neo4j_map_get(map, neo4j_string("fields"));
    if (neo4j_is_null(mfields))
    {
        neo4j_log_error(logger, "invalid field in %s: no 'fields' property",
                description);
        errno = EPROTO;
        goto failure;
    }
    if (neo4j_type(mfields) != NEO4J_LIST)
    {
        neo4j_log_error(logger,
                "invalid field in %s: 'fields' property not a List",
                description);
        errno = EPROTO;
        goto failure;
    }

    unsigned int n = neo4j_list_length(mfields);
    if (n == 0)
    {
        *names = NULL;
        return 0;
    }

    char **cfields = neo4j_mpool_calloc(mpool, n, sizeof(const char *));
    if (cfields == NULL)
    {
        goto failure;
    }

    for (unsigned int i = 0; i < n; ++i)
    {
        const neo4j_value_t fieldname = neo4j_list_get(mfields, i);
        if (neo4j_type(fieldname) != NEO4J_STRING)
        {
            neo4j_log_error(logger,
                    "invalid field in %s: fields[%d] not a String",
                    i, description);
            errno = EPROTO;
            goto failure;
        }
        cfields[i] = extract_string(fieldname, mpool);
        if (cfields[i] == NULL)
        {
            goto failure;
        }
    }

    // clang will incorrectly raise an error without this cast
    *names = (const char *const *)cfields;
    return n;

    int errsv;
failure:
    errsv = errno;
    neo4j_mpool_drainto(mpool, pdepth);
    errno = errsv;
    return -1;
}


char *extract_string(neo4j_value_t value, neo4j_mpool_t *mpool)
{
    assert(neo4j_type(value) == NEO4J_STRING);
    size_t nlength = neo4j_string_length(value);
    char *s = neo4j_mpool_alloc(mpool, nlength + 1);
    if (s == NULL)
    {
        return NULL;
    }
    neo4j_string_value(value, s, nlength + 1);
    return s;
}
