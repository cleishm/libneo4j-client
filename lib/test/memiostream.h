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
#ifndef MEMIOSTREAM_H
#define MEMIOSTREAM_H

#include "../src/neo4j-client.h"
#include "../src/iostream.h"
#include "../src/ring_buffer.h"

neo4j_iostream_t *neo4j_memiostream(ring_buffer_t *inbuffer,
        ring_buffer_t *outbuffer);

static inline neo4j_iostream_t *neo4j_loopback_iostream(ring_buffer_t *buffer)
{
    return neo4j_memiostream(buffer, buffer);
}

#endif/*MEMIOSTREAM_H*/
