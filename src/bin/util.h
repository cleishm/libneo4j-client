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
#ifndef NEO4J_UTIL_H
#define NEO4J_UTIL_H

#include <arpa/inet.h>
#include <cypher-parser.h>
#include <netdb.h>
#include <stdlib.h>


#ifndef MAXSERVNAMELEN
#  ifdef NI_MAXSERV
#    define MAXSERVNAMELEN NI_MAXSERV
#  else
#    define MAXSERVNAMELEN 32
#  endif
#endif

#ifndef MAXHOSTNAMELEN
#  ifdef NI_MAXHOST
#    define MAXHOSTNAMELEN NI_MAXHOST
#  else
#    define MAXHOSTNAMELEN 1025
#  endif
#endif

#define NEO4J_MAXHOSTLEN (MAXHOSTNAMELEN + 1 + MAXSERVNAMELEN)

char *strncpy_alloc(char **dest, size_t *cap, const char *s, size_t n);

/**
 * Ignore the result from a function call (suppressing -Wunused-result).
 *
 * (See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66425#c18)
 *
 * @internal
 */
#define ignore_unused_result(func) if (func) { }

#endif/*NEO4J_UTIL_H*/
