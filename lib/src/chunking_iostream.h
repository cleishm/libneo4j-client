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
#ifndef NEO4J_CHUNKING_IOSTREAM_H
#define NEO4J_CHUNKING_IOSTREAM_H

#include "neo4j-client.h"
#include "iostream.h"
#include <assert.h>
#include <stddef.h>

/**
 * Create a chunking iostream.
 *
 * @internal
 *
 * @param [delegate] The underlying stream to read/write chunks from.
 * @param [snd_min_chunk] The minimal chunk size.
 * @param [snd_max_chunk] The maximum chunk size.
 * @return A newly created iostream.
 */
__neo4j_must_check
neo4j_iostream_t *neo4j_chunking_iostream(neo4j_iostream_t *delegate,
        uint16_t snd_min_chunk, uint16_t snd_max_chunk);


struct neo4j_chunking_iostream
{
    neo4j_iostream_t _iostream;
    neo4j_iostream_t *delegate;
    uint16_t snd_max_chunk;
    uint8_t *snd_buffer;
    uint16_t snd_buffer_size;
    uint16_t snd_buffer_used;
    bool data_sent;
    int rcv_chunk_remaining;
    int rcv_errno;
};


/**
 * Initialize a `struct neo4j_chunking_iostream`.
 *
 * @internal
 *
 * @param [ios] The chunking stream to initialize.
 * @param [delegate] The underlying stream to read/write chunks from.
 * @param [buffer] A buffer to use for data held until a minimal chunk size is
 *         reached.
 * @param [bsize] The size of `buffer` (and the minimal chunk size).
 * @param [max_chunk] The maximum chunk size.
 * @return A pointer to a `neo4j_iostream_t` based on the chunking iostream.
 */
neo4j_iostream_t *neo4j_chunking_iostream_init(
        struct neo4j_chunking_iostream *ios, neo4j_iostream_t *delegate,
        uint8_t *buffer, uint16_t bsize, uint16_t max_chunk);

#endif/*NEO4J_CHUNKING_IOSTREAM_H*/
