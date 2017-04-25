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
#ifndef NEO4J_COLORIZATION_H
#define NEO4J_COLORIZATION_H


#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_COLOR_BOLD "\x1b[1m"
#define ANSI_COLOR_RED "\x1b[31m"


struct error_colorization
{
    const char *typ[2];
    const char *pos[2];
    const char *msg[2];
    const char *ctx[2];
};


extern const struct error_colorization *no_error_colorization;
extern const struct error_colorization *ansi_error_colorization;


#endif/*NEO4J_COLORIZATION_H*/
