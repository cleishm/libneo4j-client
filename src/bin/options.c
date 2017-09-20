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
#include <assert.h>
#include <limits.h>


struct options
{
    const char *name;
    int (*set)(shell_state_t *state, struct cypher_input_position pos,
            const char *value);
    bool allow_null;
    int (*unset)(shell_state_t *state, struct cypher_input_position pos);
    const char *(*get)(shell_state_t *state, char *buf, size_t n);
    const char *description;
};

static int set_ascii(shell_state_t *state, struct cypher_input_position pos, const char *value);
static int unset_ascii(shell_state_t *state, struct cypher_input_position pos);
static const char *get_ascii(shell_state_t *state, char *buf, size_t n);

static int set_colorize(shell_state_t *state, struct cypher_input_position pos, const char *value);
static int unset_colorize(shell_state_t *state, struct cypher_input_position pos);
static const char *get_colorize(shell_state_t *state, char *buf, size_t n);

static int set_echo(shell_state_t *state, struct cypher_input_position pos, const char *value);
static int unset_echo(shell_state_t *state, struct cypher_input_position pos);
static const char *get_echo(shell_state_t *state, char *buf, size_t n);

static int set_insecure(shell_state_t *state, struct cypher_input_position pos, const char *value);
static int unset_insecure(shell_state_t *state, struct cypher_input_position pos);
static const char *get_insecure(shell_state_t *state, char *buf, size_t n);

static int set_inspect(shell_state_t *state, struct cypher_input_position pos, const char *value);
static int unset_inspect(shell_state_t *state, struct cypher_input_position pos);
static const char *get_inspect(shell_state_t *state, char *buf, size_t n);

static int set_output(shell_state_t *state, struct cypher_input_position pos, const char *value);
static const char *get_format(shell_state_t *state, char *buf, size_t n);

static int set_outfile(shell_state_t *state, struct cypher_input_position pos, const char *value);
static int unset_outfile(shell_state_t *state, struct cypher_input_position pos);
static const char *get_outfile(shell_state_t *state, char *buf, size_t n);

static int set_quotestrings(shell_state_t *state, struct cypher_input_position pos, const char *value);
static int unset_quotestrings(shell_state_t *state, struct cypher_input_position pos);
static const char *get_quotestrings(shell_state_t *state, char *buf, size_t n);

static int set_username(shell_state_t *state, struct cypher_input_position pos, const char *value);
static int unset_username(shell_state_t *state, struct cypher_input_position pos);
static const char *get_username(shell_state_t *state, char *buf, size_t n);

static int unset_width(shell_state_t *state, struct cypher_input_position pos);
static const char *get_width(shell_state_t *state, char *buf, size_t n);

static int set_rowlines(shell_state_t *state, struct cypher_input_position pos, const char *value);
static int unset_rowlines(shell_state_t *state, struct cypher_input_position pos);
static const char *get_rowlines(shell_state_t *state, char *buf, size_t n);

static int set_timing(shell_state_t *state, struct cypher_input_position pos, const char *value);
static int unset_timing(shell_state_t *state, struct cypher_input_position pos);
static const char *get_timing(shell_state_t *state, char *buf, size_t n);

static int set_wrap(shell_state_t *state, struct cypher_input_position pos, const char *value);
static int unset_wrap(shell_state_t *state, struct cypher_input_position pos);
static const char *get_wrap(shell_state_t *state, char *buf, size_t n);

static struct options options[] =
    { { "ascii", set_ascii, true, unset_ascii, get_ascii,
          "render only 7-bit ASCII characters in result tables" },
      { "colorize", set_colorize, true, unset_colorize, get_colorize,
          "render ANSI colorized output" },
      { "echo", set_echo, true, unset_echo, get_echo,
          "echo commands and statements before rendering results" },
      { "format", set_format, false, NULL, get_format,
          "set the output format (`table` or `csv`)." },
      { "insecure", set_insecure, true, unset_insecure, get_insecure,
          "do not attempt to establish secure connections" },
      { "inspect", set_inspect, false, unset_inspect, get_inspect,
          "the number of rows to inspect when calculating column widths" },
      { "output", set_output, false, NULL, NULL, NULL },
      { "outfile", set_outfile, false, unset_outfile, get_outfile,
          "redirect output to a file" },
      { "quotestrings", set_quotestrings, true, unset_quotestrings, get_quotestrings,
          "quote strings in result tables" },
      { "username", set_username, false, unset_username, get_username,
          "the default username for connections" },
      { "rowlines", set_rowlines, true, unset_rowlines, get_rowlines,
          "render a line between each output row in result tables" },
      { "timing", set_timing, true, unset_timing, get_timing,
          "display timing information after each query" },
      { "width", set_width, false, unset_width, get_width,
          "the width to render tables (`auto` for terminal width)" },
      { "wrap", set_wrap, true, unset_wrap, get_wrap,
          "wrap field values in result tables" },
      { NULL, false, NULL } };


void options_display(shell_state_t *state, FILE *stream)
{
    struct options_colorization *colors = state->colorize->options;
    char buf[64];
    for (unsigned int i = 0; options[i].name != NULL; ++i)
    {
        if (options[i].get != NULL)
        {
            const char *name = options[i].name;
            const char *val = options[i].get(state, buf, sizeof(buf));
            assert(val != NULL);
            unsigned int end_offset = strlen(name) + strlen(val) + 3;
            fprintf(stream, " %s%s%s=%s%s%s %*s%s// %s%s\n",
                    colors->opt[0], name, colors->opt[1],
                    colors->val[0], val, colors->val[1],
                    (end_offset < 20)? 20 - end_offset : 0, "",
                    colors->dsc[0], options[i].description, colors->dsc[1]);
        }
    }
}


int option_set(shell_state_t *state, struct cypher_input_position pos,
        const char *name, const char *value)
{
    if (value != NULL && *value == '\0')
    {
        value = NULL;
    }

    for (unsigned int i = 0; options[i].name != NULL; ++i)
    {
        if (strcmp(options[i].name, name) == 0)
        {
            if (value == NULL && !options[i].allow_null)
            {
                print_error(state, pos, "Option '%s' requires a value", name);
                return -1;
            }
            return options[i].set(state, pos, value);
        }
    }

    if (value == NULL && strncmp(name, "no", 2) == 0)
    {
        for (unsigned int i = 0; options[i].name != NULL; ++i)
        {
            if (options[i].allow_null && strcmp(options[i].name, name + 2) == 0)
            {
                return options[i].unset(state, pos);
            }
        }
    }

    print_error(state, pos, "Unknown option '%s'", name);
    return -1;
}


int option_unset(shell_state_t *state, struct cypher_input_position pos,
        const char *name)
{
    for (unsigned int i = 0; options[i].name != NULL; ++i)
    {
        if (strcmp(options[i].name, name) == 0)
        {
            if (options[i].unset != NULL)
            {
                return options[i].unset(state, pos);
            }
            else
            {
                print_error(state, pos, "Cannot unset option '%s'", name);
                return -1;
            }
        }
    }

    print_error(state, pos, "Unknown option '%s'", name);
    return -1;
}


int set_ascii(shell_state_t *state, struct cypher_input_position pos,
        const char *value)
{
    if (value == NULL || strcmp(value, "on") == 0)
    {
        neo4j_config_set_render_ascii(state->config, true);
    }
    else if (strcmp(value, "off") == 0)
    {
        neo4j_config_set_render_ascii(state->config, false);
    }
    else
    {
        print_error(state, pos, "Must set ascii to 'on' or 'off'");
        return -1;
    }
    return 0;
}


int unset_ascii(shell_state_t *state, struct cypher_input_position pos)
{
    neo4j_config_set_render_ascii(state->config, false);
    return 0;
}


const char *get_ascii(shell_state_t *state, char *buf, size_t n)
{
    return neo4j_config_get_render_ascii(state->config)? "on" : "off";
}


int set_colorize(shell_state_t *state, struct cypher_input_position pos,
        const char *value)
{
    if (value == NULL || strcmp(value, "on") == 0)
    {
        state->colorize = ansi_shell_colorization;
        neo4j_config_set_results_table_colors(state->config,
                neo4j_results_table_ansi_colors);
        neo4j_config_set_plan_table_colors(state->config,
                neo4j_plan_table_ansi_colors);
    }
    else if (strcmp(value, "off") == 0)
    {
        state->colorize = no_shell_colorization;
        neo4j_config_set_results_table_colors(state->config,
                neo4j_results_table_no_colors);
        neo4j_config_set_plan_table_colors(state->config,
                neo4j_plan_table_no_colors);
    }
    else
    {
        print_error(state, pos, "Must set color to 'on' or 'off'");
        return -1;
    }
    return 0;
}


int unset_colorize(shell_state_t *state, struct cypher_input_position pos)
{
    state->colorize = no_shell_colorization;
    neo4j_config_set_results_table_colors(state->config,
            neo4j_results_table_no_colors);
    neo4j_config_set_plan_table_colors(state->config,
            neo4j_plan_table_no_colors);
    return 0;
}


const char *get_colorize(shell_state_t *state, char *buf, size_t n)
{
    return (neo4j_config_get_results_table_colors(state->config) ==
            neo4j_results_table_ansi_colors)? "on" : "off";
}


int set_echo(shell_state_t *state, struct cypher_input_position pos,
        const char *value)
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
        print_error(state, pos, "Must set echo to 'on' or 'off'");
        return -1;
    }
    return 0;
}


int unset_echo(shell_state_t *state, struct cypher_input_position pos)
{
    state->echo = false;
    return 0;
}


const char *get_echo(shell_state_t *state, char *buf, size_t n)
{
    return (state->echo)? "on" : "off";
}


int set_insecure(shell_state_t *state, struct cypher_input_position pos,
        const char *value)
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
        print_error(state, pos, "Must set insecure to 'yes' or 'no'");
        return -1;
    }
    return 0;
}


int unset_insecure(shell_state_t *state, struct cypher_input_position pos)
{
    state->connect_flags &= ~NEO4J_INSECURE;
    return 0;
}


const char *get_insecure(shell_state_t *state, char *buf, size_t n)
{
    return (state->connect_flags & NEO4J_INSECURE)? "yes" : "no";
}


int set_inspect(shell_state_t *state, struct cypher_input_position pos,
        const char *value)
{
    char *endptr;
    unsigned long rows = strtoul(value, &endptr, 10);
    if (*value == '\0' || *endptr != '\0')
    {
        print_error(state, pos, "Invalid value '%s' for inspect", value);
        return -1;
    }
    if (rows > UINT_MAX)
    {
        print_error(state, pos,
                "Value for :inspect (%ld) out of range [0,%d]",
                rows, UINT_MAX);
        return -1;
    }

    neo4j_config_set_render_inspect_rows(state->config, rows);
    return 0;
}


int unset_inspect(shell_state_t *state, struct cypher_input_position pos)
{
    neo4j_config_set_render_inspect_rows(state->config, 0);
    return 0;
}


const char *get_inspect(shell_state_t *state, char *buf, size_t n)
{
    unsigned int rows = neo4j_config_get_render_inspect_rows(state->config);
    snprintf(buf, n, "%u", rows);
    return buf;
}


int set_format(shell_state_t *state, struct cypher_input_position pos,
        const char *value)
{
    renderer_t renderer = find_renderer(value);
    if (renderer == NULL)
    {
        print_error(state, pos, "Unknown output format '%s'", value);
        return -1;
    }
    state->render = renderer;
    return 0;
}


int set_output(shell_state_t *state, struct cypher_input_position pos,
        const char *value)
{
    print_warning(state, pos, "`:set output=value` is deprecated. "
            "Use `:set format=value` instead.");
    return set_format(state, pos, value);
}


const char *get_format(shell_state_t *state, char *buf, size_t n)
{
    const char *name = renderer_name(state->render);
    return (name != NULL)? name : "unknown";
}


int set_outfile(shell_state_t *state, struct cypher_input_position pos,
        const char *value)
{
    return redirect_output(state, pos, value);
}


int unset_outfile(shell_state_t *state, struct cypher_input_position pos)
{
    return set_outfile(state, pos, NULL);
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


int set_quotestrings(shell_state_t *state, struct cypher_input_position pos,
        const char *value)
{
    if (value == NULL || strcmp(value, "yes") == 0)
    {
        neo4j_config_set_render_quoted_strings(state->config, true);
    }
    else if (strcmp(value, "no") == 0)
    {
        neo4j_config_set_render_quoted_strings(state->config, false);
    }
    else
    {
        print_error(state, pos, "Must set quotestrings to 'yes' or 'no'");
        return -1;
    }
    return 0;
}


int unset_quotestrings(shell_state_t *state, struct cypher_input_position pos)
{
    neo4j_config_set_render_quoted_strings(state->config, false);
    return 0;
}


const char *get_quotestrings(shell_state_t *state, char *buf, size_t n)
{
    return neo4j_config_get_render_quoted_strings(state->config)? "yes" : "no";
}


int set_username(shell_state_t *state, struct cypher_input_position pos,
        const char *value)
{
    return neo4j_config_set_username(state->config,
            (*value != '\0')? value : NULL);
}


int unset_username(shell_state_t *state, struct cypher_input_position pos)
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


int set_width(shell_state_t *state, struct cypher_input_position pos,
        const char *value)
{
    if (strcmp(value, "auto") == 0)
    {
        state->width = 0;
        return 0;
    }

    long width = strtol(value, NULL, 10);
    if (width < 2 || width >= NEO4J_RENDER_MAX_WIDTH)
    {
        print_error(state, pos, "Width value (%ld) out of range [2,%d)",
                width, NEO4J_RENDER_MAX_WIDTH);
        return -1;
    }

    state->width = (unsigned int)width;
    return 0;
}


int unset_width(shell_state_t *state, struct cypher_input_position pos)
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


int set_rowlines(shell_state_t *state, struct cypher_input_position pos,
        const char *value)
{
    if (value == NULL || strcmp(value, "yes") == 0)
    {
        neo4j_config_set_render_rowlines(state->config, true);
    }
    else if (strcmp(value, "no") == 0)
    {
        neo4j_config_set_render_rowlines(state->config, false);
    }
    else
    {
        print_error(state, pos, "Must set rowlines to 'yes' or 'no'");
        return -1;
    }
    return 0;
}


int unset_rowlines(shell_state_t *state, struct cypher_input_position pos)
{
    neo4j_config_set_render_rowlines(state->config, false);
    return 0;
}


const char *get_rowlines(shell_state_t *state, char *buf, size_t n)
{
    return neo4j_config_get_render_rowlines(state->config)? "yes" : "no";
}


int set_timing(shell_state_t *state, struct cypher_input_position pos,
        const char *value)
{
    if (value == NULL || strcmp(value, "yes") == 0)
    {
        state->show_timing = true;
    }
    else if (strcmp(value, "no") == 0)
    {
        state->show_timing = false;
    }
    else
    {
        print_error(state, pos, "Must set timing to 'yes' or 'no'");
        return -1;
    }
    return 0;
}


int unset_timing(shell_state_t *state, struct cypher_input_position pos)
{
    state->show_timing = false;
    return 0;
}


const char *get_timing(shell_state_t *state, char *buf, size_t n)
{
    return (state->show_timing)? "yes" : "no";
}


int set_wrap(shell_state_t *state, struct cypher_input_position pos, const char *value)
{
    if (value == NULL || strcmp(value, "yes") == 0)
    {
        neo4j_config_set_render_wrapped_values(state->config, true);
    }
    else if (strcmp(value, "no") == 0)
    {
        neo4j_config_set_render_wrapped_values(state->config, false);
    }
    else
    {
        print_error(state, pos, "Must set wrap to 'yes' or 'no'");
        return -1;
    }
    return 0;
}


int unset_wrap(shell_state_t *state, struct cypher_input_position pos)
{
    neo4j_config_set_render_wrapped_values(state->config, false);
    return 0;
}


const char *get_wrap(shell_state_t *state, char *buf, size_t n)
{
    return neo4j_config_get_render_wrapped_values(state->config)? "yes" : "no";
}
