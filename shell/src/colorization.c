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
#include "colorization.h"


#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_COLOR_BOLD "\x1b[1m"
#define ANSI_COLOR_BRIGHT "\x1b[38;5;15m"
#define ANSI_COLOR_RED "\x1b[31;1m"
#define ANSI_COLOR_GREEN "\x1b[38;5;2m"
#define ANSI_COLOR_BLUE "\x1b[38;5;4m"


static struct error_colorization _no_error_colorization =
    { .typ = { "", "" },
      .pos = { "", "" },
      .msg = { "", "" },
      .ctx = { "", "" },
      .ptr = { "", "" } };

static struct error_colorization _ansi_error_colorization =
    { .typ = { ANSI_COLOR_RED, ANSI_COLOR_RESET },
      .pos = { ANSI_COLOR_BOLD, ANSI_COLOR_RESET },
      .msg = { ANSI_COLOR_BOLD, ANSI_COLOR_RESET },
      .ctx = { "", "" },
      .ptr = { ANSI_COLOR_RED, ANSI_COLOR_RESET } };

static struct help_colorization _no_help_colorization =
    { .cmd = { "", "" },
      .arg = { "", "" },
      .dsc = { "", "" } };

static struct help_colorization _ansi_help_colorization =
    { .cmd = { ANSI_COLOR_BRIGHT, ANSI_COLOR_RESET },
      .arg = { ANSI_COLOR_GREEN, ANSI_COLOR_RESET },
      .dsc = { ANSI_COLOR_BLUE, ANSI_COLOR_RESET } };

static struct status_colorization _no_status_colorization =
    { .url = { "", "" },
      .wrn = { "", "" } };

static struct status_colorization _ansi_status_colorization =
    { .url = { ANSI_COLOR_BOLD, ANSI_COLOR_RESET },
      .wrn = { ANSI_COLOR_RED, ANSI_COLOR_RESET } };

static struct options_colorization _no_options_colorization =
    { .opt = { "", "" },
      .val = { "", "" },
      .dsc = { "", "" } };

static struct options_colorization _ansi_options_colorization =
    { .opt = { ANSI_COLOR_BRIGHT, ANSI_COLOR_RESET },
      .val = { ANSI_COLOR_GREEN, ANSI_COLOR_RESET },
      .dsc = { ANSI_COLOR_BLUE, ANSI_COLOR_RESET } };

static struct exports_colorization _no_exports_colorization =
    { .key = { "", "" },
      .val = { "", "" } };

static struct exports_colorization _ansi_exports_colorization =
    { .key = { ANSI_COLOR_BRIGHT, ANSI_COLOR_RESET },
      .val = { ANSI_COLOR_GREEN, ANSI_COLOR_RESET } };


static struct shell_colorization _no_shell_colorization =
    { .error = &_no_error_colorization,
      .help = &_no_help_colorization,
      .status = &_no_status_colorization,
      .options = &_no_options_colorization,
      .exports = &_no_exports_colorization };

static struct shell_colorization _ansi_shell_colorization =
    { .error = &_ansi_error_colorization,
      .status = &_ansi_status_colorization,
      .help = &_ansi_help_colorization,
      .options = &_ansi_options_colorization,
      .exports = &_ansi_exports_colorization };


const struct shell_colorization *no_shell_colorization =
        &_no_shell_colorization;
const struct shell_colorization *ansi_shell_colorization =
        &_ansi_shell_colorization;
