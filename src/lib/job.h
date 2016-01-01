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
#ifndef NEO4J_JOB_H
#define NEO4J_JOB_H

#include "neo4j-client.h"

typedef struct neo4j_job neo4j_job_t;
struct neo4j_job
{
    void (*notify_session_ending)(neo4j_job_t *self);
    neo4j_job_t *next;
};


/**
 * Notify a job that the session it is attached to in ending.
 *
 * @param [job] The job to notify.
 */
static inline void neo4j_job_notify_session_ending(neo4j_job_t *job)
{
    job->notify_session_ending(job);
}

#endif/*NEO4J_JOB_H*/
