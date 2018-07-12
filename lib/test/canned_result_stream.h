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
#ifndef CANNED_RESULT_STREAM_H
#define CANNED_RESULT_STREAM_H

#include "../src/neo4j-client.h"
#include "../src/result_stream.h"

neo4j_result_stream_t *neo4j_canned_result_stream(
        const char * const *fieldnames, unsigned int nfields,
        const neo4j_value_t *records, size_t nrecords);

void neo4j_crs_set_error(neo4j_result_stream_t *results, const char *msg);

#endif/*CANNED_RESULT_STREAM_H*/
