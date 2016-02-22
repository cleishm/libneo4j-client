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
#ifndef NEO4J_BUFFERING_IOSTREAM_H
#define NEO4J_BUFFERING_IOSTREAM_H

#include "neo4j-client.h"
#include "iostream.h"

/**
 * Create a buffering iostream.
 *
 * This iostream buffers reads and writes to the delegate iostream. At least
 * one of the read or the write buffer sizes must be > 0.
 *
 * @internal
 *
 * @param [delegate] The iostream that will be buffered.
 * @param [close] If `true` the delegate iostream will also be closed when this
 *         stream is closed.
 * @param [rcvbuf_size] The size of the read buffer.
 * @param [sndbuf_size] The size of the write buffer.
 * @return The newly created buffering iostream.
 */
__neo4j_must_check
neo4j_iostream_t *neo4j_buffering_iostream(neo4j_iostream_t *delegate,
        bool close, size_t rcvbuf_size, size_t sndbuf_size);

#endif/*NEO4J_BUFFERING_IOSTREAM_H*/
