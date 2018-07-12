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
#ifndef NEO4J_ATOMIC_H
#define NEO4J_ATOMIC_H

#ifdef HAVE_STDATOMIC_H

#include <stdatomic.h>

typedef struct
{
    atomic_bool value;
} neo4j_atomic_bool;

static inline void neo4j_atomic_bool_init(neo4j_atomic_bool *b, bool v)
{
    atomic_init(&(b->value), v);
}

static inline bool neo4j_atomic_bool_set(neo4j_atomic_bool *b, bool v)
{
    return atomic_exchange(&(b->value), v);
}

static inline bool neo4j_atomic_bool_get(neo4j_atomic_bool *b)
{
    return atomic_load(&(b->value));
}

#elif defined(__GNUC__)

typedef struct
{
    bool value;
} neo4j_atomic_bool;

static inline void neo4j_atomic_bool_init(neo4j_atomic_bool *b, bool v)
{
    b->value = v;
}

static inline bool neo4j_atomic_bool_set(neo4j_atomic_bool *b, bool v)
{
    return __sync_val_compare_and_swap(&(b->value), !v, v);
}

static inline bool neo4j_atomic_bool_get(neo4j_atomic_bool *b)
{
    __sync_synchronize();
    return b->value;
}

#else
#error Missing atomics implementation (stdatomic)
#endif

#endif/*NEO4J_ATOMIC_H*/
