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
#ifndef NEO4J_THREAD_H
#define NEO4J_THREAD_H


unsigned long neo4j_current_thread_id(void);


#ifdef HAVE_PTHREADS

#include <pthread.h>

#define neo4j_mutex_t pthread_mutex_t
#define neo4j_mutex_init(n) pthread_mutex_init((n),NULL)
#define neo4j_mutex_lock pthread_mutex_lock
#define neo4j_mutex_unlock pthread_mutex_unlock
#define neo4j_mutex_destroy pthread_mutex_destroy

#define neo4j_once_t pthread_once_t
#define NEO4J_ONCE_INIT PTHREAD_ONCE_INIT
#define neo4j_thread_once(c,r) pthread_once((c),(r))

#else
#error "No threading support found"
#endif

#endif/*NEO4J_THREAD_H*/
