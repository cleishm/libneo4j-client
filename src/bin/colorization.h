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


struct error_colorization
{
    const char *typ[2];
    const char *pos[2];
    const char *msg[2];
    const char *ctx[2];
    const char *ptr[2];
};


struct help_colorization
{
    const char *cmd[2];
    const char *arg[2];
    const char *dsc[2];
};


struct status_colorization
{
    const char *url[2];
    const char *wrn[2];
};


struct options_colorization
{
    const char *opt[2];
    const char *val[2];
    const char *dsc[2];
};


struct exports_colorization
{
    const char *key[2];
    const char *val[2];
};


struct shell_colorization
{
    struct error_colorization *error;
    struct help_colorization *help;
    struct status_colorization *status;
    struct options_colorization *options;
    struct exports_colorization *exports;
};


extern const struct shell_colorization *no_shell_colorization;
extern const struct shell_colorization *ansi_shell_colorization;


#endif/*NEO4J_COLORIZATION_H*/
