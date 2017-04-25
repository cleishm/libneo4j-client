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
#include "../../config.h"
#include "options.h"
#include "render.h"


struct options
{
    const char *name;
    int (*set)(shell_state_t *state, const char *value);
    bool allow_null;
    int (*unset)(shell_state_t *state);
    const char *(*get)(shell_state_t *state, char *buf, size_t n);
    const char *description;
};

static int set_ascii(shell_state_t *state, const char *value);
static int unset_ascii(shell_state_t *state);
static const char *get_ascii(shell_state_t *state, char *buf, size_t n);

static int set_echo(shell_state_t *state, const char *value);
static int unset_echo(shell_state_t *state);
static const char *get_echo(shell_state_t *state, char *buf, size_t n);

static int set_insecure(shell_state_t *state, const char *value);
static int unset_insecure(shell_state_t *state);
static const char *get_insecure(shell_state_t *state, char *buf, size_t n);

static int set_output(shell_state_t *state, const char *value);
static const char *get_format(shell_state_t *state, char *buf, size_t n);

static int set_outfile(shell_state_t *state, const char *value);
static int unset_outfile(shell_state_t *state);
static const char *get_outfile(shell_state_t *state, char *buf, size_t n);

static int set_username(shell_state_t *state, const char *value);
static int unset_username(shell_state_t *state);
static const char *get_username(shell_state_t *state, char *buf, size_t n);

static int unset_width(shell_state_t *state);
static const char * get_width(shell_state_t *state, char *buf, size_t n);

static struct options options[] =
    { { "ascii", set_ascii, true, unset_ascii, get_ascii,
          "render only 7-bit ASCII characters in result tables" },
      { "echo", set_echo, true, unset_echo, get_echo,
          "echo commands and statements before rendering results" },
      { "insecure", set_insecure, true, unset_insecure, get_insecure,
          "do not attempt to establish secure connections" },
      { "format", set_format, false, NULL, get_format,
          "set the output format (`table` or `csv`)." },
      { "output", set_output, false, NULL, NULL, NULL },
      { "outfile", set_outfile, false, unset_outfile, get_outfile,
          "redirect output to a file" },
      { "username", set_username, false, unset_username, get_username,
          "the default username for connections" },
      { "width", set_width, false, unset_width, get_width,
          "the width to render tables (`auto` for term width)" },
      { NULL, false, NULL } };


void options_display(shell_state_t *state, FILE *stream)
{
    char buf[64];
    for (unsigned int i = 0; options[i].name != NULL; ++i)
    {
        if (options[i].get != NULL)
        {
            const char *name = options[i].name;
            const char *val = options[i].get(state, buf, sizeof(buf));
            unsigned int end_offset = strlen(name) + strlen(val) + 3;
            fprintf(stream, " %s=%s %*s// %s\n", name, val,
                    (end_offset < 20)? 20 - end_offset : 0, "",
                    options[i].description);
        }
    }
}


int option_set(shell_state_t *state, const char *name,
        const char *value)
{
    for (unsigned int i = 0; options[i].name != NULL; ++i)
    {
        if (strcmp(options[i].name, name) == 0)
        {
            if (value != NULL && *value == '\0')
            {
                value = NULL;
            }
            if (value == NULL && !options[i].allow_null)
            {
                fprintf(state->err, "Option '%s' requires a value\n",
                        name);
                return -1;
            }
            return options[i].set(state, value);
        }
    }

    fprintf(state->err, "Unknown option '%s'\n", name);
    return -1;
}


int option_unset(shell_state_t *state, const char *name)
{
    for (unsigned int i = 0; options[i].name != NULL; ++i)
    {
        if (strcmp(options[i].name, name) == 0)
        {
            if (options[i].unset != NULL)
            {
                return options[i].unset(state);
            }
            else
            {
                fprintf(state->err, "Cannot unset option '%s'\n", name);
                return -1;
            }
        }
    }

    fprintf(state->err, "Unknown option '%s'\n", name);
    return -1;
}


int set_ascii(shell_state_t *state, const char *value)
{
    if (value == NULL || strcmp(value, "on") == 0)
    {
        state->render_flags |= NEO4J_RENDER_ASCII;
    }
    else if (strcmp(value, "off") == 0)
    {
        state->render_flags &= ~NEO4J_RENDER_ASCII;
    }
    else
    {
        fprintf(state->err, "Must set ascii to 'on' or 'off'\n");
        return -1;
    }
    return 0;
}


int unset_ascii(shell_state_t *state)
{
    state->render_flags &= ~NEO4J_RENDER_ASCII;
    return 0;
}


const char *get_ascii(shell_state_t *state, char *buf, size_t n)
{
    return (state->render_flags & NEO4J_RENDER_ASCII)? "on" : "off";
}


int set_echo(shell_state_t *state, const char *value)
{
    if (value == NULL || strcmp(value, "on") == 0)
    {
        state->echo = true;
    }
    else if (strcmp(value, "off") == 0)
    {
        state->echo = false;
    }
    else
    {
        fprintf(state->err, "Must set echo to 'on' or 'off'\n");
        return -1;
    }
    return 0;
}


int unset_echo(shell_state_t *state)
{
    state->echo = false;
    return 0;
}


const char *get_echo(shell_state_t *state, char *buf, size_t n)
{
    return (state->echo)? "on" : "off";
}


int set_insecure(shell_state_t *state, const char *value)
{
    if (value == NULL || strcmp(value, "yes") == 0)
    {
        state->connect_flags |= NEO4J_INSECURE;
    }
    else if (strcmp(value, "no") == 0)
    {
        state->connect_flags &= ~NEO4J_INSECURE;
    }
    else
    {
        fprintf(state->err, "Must set insecure to 'yes' or 'no'\n");
        return -1;
    }
    return 0;
}


int unset_insecure(shell_state_t *state)
{
    state->connect_flags &= ~NEO4J_INSECURE;
    return 0;
}


const char *get_insecure(shell_state_t *state, char *buf, size_t n)
{
    return (state->connect_flags & NEO4J_INSECURE)? "yes" : "no";
}


int set_format(shell_state_t *state, const char *value)
{
    renderer_t renderer = find_renderer(value);
    if (renderer == NULL)
    {
        fprintf(state->err, "Unknown output format '%s'\n", value);
        return -1;
    }
    state->render = renderer;
    return 0;
}


int set_output(shell_state_t *state, const char *value)
{
    fprintf(state->err,
            "WARNING: `:set output=value` is deprecated. "
            "Use `:set format=value` instead.\n");
    return set_format(state, value);
}


const char *get_format(shell_state_t *state, char *buf, size_t n)
{
    const char *name = renderer_name(state->render);
    return (name != NULL)? name : "unknown";
}


int set_outfile(shell_state_t *state, const char *value)
{
    return redirect_output(state, value);
}


int unset_outfile(shell_state_t *state)
{
    return set_outfile(state, NULL);
}


const char *get_outfile(shell_state_t *state, char *buf, size_t n)
{
    if (state->outfile == NULL)
    {
        return "";
    }
    snprintf(buf, n, "\"%s\"", state->outfile);
    return buf;
}


int set_username(shell_state_t *state, const char *value)
{
    return neo4j_config_set_username(state->config,
            (*value != '\0')? value : NULL);
}


int unset_username(shell_state_t *state)
{
    return neo4j_config_set_username(state->config, NULL);
}


const char *get_username(shell_state_t *state, char *buf, size_t n)
{
    const char *username = neo4j_config_get_username(state->config);
    if (username == NULL)
    {
        return "";
    }
    snprintf(buf, n, "\"%s\"", username);
    return buf;
}


int set_width(shell_state_t *state, const char *value)
{
    if (strcmp(value, "auto") == 0)
    {
        state->width = 0;
        return 0;
    }

    long width = strtol(value, NULL, 10);
    if (width < 2 || width >= NEO4J_RENDER_MAX_WIDTH)
    {
        fprintf(state->err, "Width value (%ld) out of range [1,%d)\n",
                width, NEO4J_RENDER_MAX_WIDTH);
        return -1;
    }

    state->width = (unsigned int)width;
    return 0;
}


int unset_width(shell_state_t *state)
{
    state->width = 0;
    return 0;
}


const char *get_width(shell_state_t *state, char *buf, size_t n)
{
    if (state->width == 0)
    {
        return "auto";
    }
    snprintf(buf, n, "%d", state->width);
    return buf;
}
