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
#include "messages.h"
#include "chunking_iostream.h"
#include "deserialization.h"
#include "serialization.h"
#include "util.h"
#include "values.h"

#define DECLARE_MESSAGE_TYPE(type_name, signature) \
    static const struct neo4j_message_type type_name##_MESSAGE = \
        { .name = #type_name, .struct_signature = signature }

DECLARE_MESSAGE_TYPE(INIT, 0x01);
DECLARE_MESSAGE_TYPE(RUN, 0x10);
DECLARE_MESSAGE_TYPE(DISCARD_ALL, 0X2F);
DECLARE_MESSAGE_TYPE(PULL_ALL, 0X3F);
DECLARE_MESSAGE_TYPE(ACK_FAILURE, 0X0E);
DECLARE_MESSAGE_TYPE(RESET, 0X0F);
DECLARE_MESSAGE_TYPE(RECORD, 0X71);
DECLARE_MESSAGE_TYPE(SUCCESS, 0X70);
DECLARE_MESSAGE_TYPE(FAILURE, 0X7F);
DECLARE_MESSAGE_TYPE(IGNORED, 0X7E);

static const neo4j_message_type_t neo4j_message_types[] =
    { &INIT_MESSAGE,
      &RUN_MESSAGE,
      &DISCARD_ALL_MESSAGE,
      &PULL_ALL_MESSAGE,
      &ACK_FAILURE_MESSAGE,
      &RESET_MESSAGE,
      &RECORD_MESSAGE,
      &SUCCESS_MESSAGE,
      &FAILURE_MESSAGE,
      &IGNORED_MESSAGE };
static const int _max_message_type =
    (sizeof(neo4j_message_types) / sizeof(neo4j_message_type_t));

const neo4j_message_type_t NEO4J_INIT_MESSAGE = &INIT_MESSAGE;
const neo4j_message_type_t NEO4J_RUN_MESSAGE = &RUN_MESSAGE;
const neo4j_message_type_t NEO4J_DISCARD_ALL_MESSAGE = &DISCARD_ALL_MESSAGE;
const neo4j_message_type_t NEO4J_PULL_ALL_MESSAGE = &PULL_ALL_MESSAGE;
const neo4j_message_type_t NEO4J_ACK_FAILURE_MESSAGE = &ACK_FAILURE_MESSAGE;
const neo4j_message_type_t NEO4J_RESET_MESSAGE = &RESET_MESSAGE;
const neo4j_message_type_t NEO4J_RECORD_MESSAGE = &RECORD_MESSAGE;
const neo4j_message_type_t NEO4J_SUCCESS_MESSAGE = &SUCCESS_MESSAGE;
const neo4j_message_type_t NEO4J_FAILURE_MESSAGE = &FAILURE_MESSAGE;
const neo4j_message_type_t NEO4J_IGNORED_MESSAGE = &IGNORED_MESSAGE;


neo4j_message_type_t neo4j_message_type_for_signature(uint8_t signature)
{
    for (int i = 0; i < _max_message_type; ++i)
    {
        if (signature == neo4j_message_types[i]->struct_signature)
        {
            return neo4j_message_types[i];
        }
    }
    return NULL;
}


int neo4j_message_send(neo4j_iostream_t *ios, neo4j_message_type_t type,
        const neo4j_value_t *argv, uint16_t argc, uint8_t *buffer,
        uint16_t bsize, uint16_t max_chunk)
{
    REQUIRE(ios != NULL, -1);
    REQUIRE(argc == 0 || argv != NULL, -1);

    struct neo4j_chunking_iostream chunking_ios;
    neo4j_iostream_t *cios = neo4j_chunking_iostream_init(&chunking_ios,
            ios, buffer, bsize, max_chunk);

    neo4j_value_t structure = neo4j_struct(type->struct_signature, argv, argc);
    if (neo4j_serialize(structure, cios))
    {
        return -1;
    }

    neo4j_ios_close(cios);
    return 0;
}


int neo4j_message_recv(neo4j_iostream_t *ios,
        neo4j_mpool_t *mpool, neo4j_message_type_t *type,
        const neo4j_value_t **argv, uint16_t *argc)
{
    REQUIRE(ios != NULL, -1);
    REQUIRE(mpool != NULL, -1);
    REQUIRE(type != NULL, -1);
    size_t pdepth = neo4j_mpool_depth(*mpool);

    struct neo4j_chunking_iostream chunking_ios;
    neo4j_iostream_t *cios = neo4j_chunking_iostream_init(&chunking_ios,
            ios, NULL, 0, UINT16_MAX);

    neo4j_value_t message;
    if (neo4j_deserialize(cios, mpool, &message))
    {
        goto failure;
    }

    if (neo4j_type(message) != NEO4J_STRUCT)
    {
        errno = EPROTO;
        goto failure;
    }

    neo4j_message_type_t message_type =
        neo4j_message_type_for_signature(neo4j_struct_signature(message));
    if (message_type == NULL)
    {
        errno = EPROTO;
        goto failure;
    }

    *type = message_type;
    if (argv != NULL)
    {
        *argv = neo4j_struct_fields(message);
    }
    if (argc != NULL)
    {
        *argc = neo4j_struct_size(message);
    }

    neo4j_ios_close(cios);
    return 0;

    int errsv;
failure:
    errsv = errno;
    neo4j_mpool_drainto(mpool, pdepth);
    errno = errsv;
    return -1;
}
