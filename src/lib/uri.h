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
#ifndef NEO4J_URI_H
#define NEO4J_URI_H

struct uri
{
    char *scheme;
    char *userinfo;
    char *hostname;
    int port;
    char *path;
    char *query;
    char *fragment;
};

/**
 * Parse a URI.
 *
 * @param [str] A pointer to a `NULL` terminated string containing the URI.
 * @param [endptr] A pointer to a char pointer, which will be updated to the
 *         last valid character of the input. If this points to the `NULL`
 *         character, then the entire input was valid.
 * @return A newly allocated `struct uri`.
 */
struct uri *parse_uri(const char *str, const char **endptr);

/**
 * Deallocate a `struct uri`.
 *
 * @param [uri] The `struct uri` to deallocate.
 */
void free_uri(struct uri *uri);

#endif/*NEO4J_URI_H*/
