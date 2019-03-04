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
#include "authentication.h"
#include "batch.h"
#include "connect.h"
#include "evaluate.h"
#include "interactive.h"
#include "render.h"
#include "state.h"
#include "verification.h"
#include <ctype.h>
#include <cypher-parser.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <neo4j-client.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifndef _PATH_TTY
#  define _PATH_TTY "/dev/tty"
#endif


#define NEO4J_HISTORY_FILE "client-history"
#define NEO4J_USERNAME_ENV "NEO4J_USERNAME"
#define NEO4J_PASSWORD_ENV "NEO4J_PASSWORD"


const char *shortopts = "e:hi:o:p:Pu:v";

#define HISTFILE_OPT 1000
#define CA_FILE_OPT 1001
#define CA_DIRECTORY_OPT 1002
#define INSECURE_OPT 1003
#define NON_INTERACTIVE_OPT 1004
#define KNOWN_HOSTS_OPT 1005
#define NO_KNOWN_HOSTS_OPT 1006
#define NOHIST_OPT 1007
#define VERSION_OPT 1008
#define PIPELINE_MAX_OPT 1009
#define SOURCE_MAX_DEPTH_OPT 1010
#define COLORIZE_OPT 1011
#define NO_COLORIZE_OPT 1012
#define EXPORT_OPT 1013

static struct option longopts[] =
    { { "help", no_argument, NULL, 'h' },
      { "history-file", required_argument, NULL, HISTFILE_OPT },
      { "no-history", no_argument, NULL, NOHIST_OPT },
      { "ca-file", required_argument, NULL, CA_FILE_OPT },
      { "ca-directory", required_argument, NULL, CA_DIRECTORY_OPT },
      { "colorize", no_argument, NULL, COLORIZE_OPT },
      { "colourise", no_argument, NULL, COLORIZE_OPT },
      { "no-colorize", no_argument, NULL, NO_COLORIZE_OPT },
      { "no-colourise", no_argument, NULL, NO_COLORIZE_OPT },
      { "insecure", no_argument, NULL, INSECURE_OPT },
      { "non-interactive", no_argument, NULL, NON_INTERACTIVE_OPT },
      { "username", required_argument, NULL, 'u' },
      { "password", required_argument, NULL, 'p' },
      { "known-hosts", required_argument, NULL, KNOWN_HOSTS_OPT },
      { "no-known-hosts", no_argument, NULL, NO_KNOWN_HOSTS_OPT },
      { "pipeline-max", required_argument, NULL, PIPELINE_MAX_OPT },
      { "source", required_argument, NULL, 'i' },
      { "source-max-depth", required_argument, NULL, SOURCE_MAX_DEPTH_OPT },
      { "eval", required_argument, NULL, 'e' },
      { "export", required_argument, NULL, EXPORT_OPT },
      { "output", required_argument, NULL, 'o' },
      { "verbose", no_argument, NULL, 'v' },
      { "version", no_argument, NULL, VERSION_OPT },
      { NULL, 0, NULL, 0 } };

static void usage(FILE *s, const char *prog_name)
{
    fprintf(s,
"usage: %1$s [OPTIONS] [URL]\n"
"       %1$s [OPTIONS] [host [port]]\n"
"options:\n"
" --help, -h          Output this usage information.\n"
" --history=file      Use the specified file for saving history.\n"
" --no-history        Do not save history.\n"
" --colorize          Colorize output using ANSI escape sequences.\n"
" --no-colorize       Disable colorization even when outputting to a TTY.\n"
" --ca-file=cert.pem  Specify a file containing trusted certificates.\n"
" --ca-directory=dir  Specify a directory containing trusted certificates.\n"
" --insecure          Do not attempt to establish a secure connection.\n"
" --non-interactive   Use non-interactive mode and do not prompt for\n"
"                     host verification or credentials when connecting\n"
"                     (default when no TTY is connected to the process).\n"
" --username=name, -u name\n"
"                     Connect using the specified username.\n"
" --password=pass, -p pass\n"
"                     Connect using the specified password. This is only\n"
"                     valid when a URL or host is also specified.\n"
" -P                  Prompt for a password, even in non-interactive mode.\n"
" --known-hosts=file  Set the path to the known-hosts file.\n"
" --no-known-hosts    Do not do host checking via known-hosts (use only TLS\n"
"                     certificate verification).\n"
" --output file, -o file\n"
"                     Redirect output to the specified file. Must be\n"
"                     specified in conjunction with --source/-i, and may be\n"
"                     specified multiple times.\n"
" --source file, -i file\n"
"                     Evaluate statements from the specified input file.\n"
"                     May be specified multiple times.\n"
" --eval script, -e script\n"
"                     Evaluate the argument string. May be specified multiple\n"
"                     times.\n"
" --export name=val   Export a parameter, which will be available in all\n"
"                     queries.\n"
" --verbose, -v       Increase logging verbosity.\n"
" --version           Output the version of neo4j-client and dependencies.\n"
"\n"
"If URL is supplied then a connection is first made to the specified Neo4j\n"
"graph database.\n"
"\n"
"If the shell is run connected to a TTY, then an interactive command prompt\n"
"is shown. Use `:exit` to quit. If the shell is not connected to a TTY, then\n"
"directives are read from stdin.\n",
        prog_name);
}

static shell_state_t state;

struct io_handler
{
    char *arg;
    bool is_output;
    int (*handle)(shell_state_t *state, struct cypher_input_position pos,
            const char *arg);
};

#define NEO4J_MAX_IO_ARGS 128

static void interrupt_handler(int signal);
static int add_io_handler(struct io_handler *io_handlers,
        unsigned int *nio_handlers,
        const char *arg, bool is_output,
        int (*handler)(shell_state_t *state, struct cypher_input_position pos,
            const char *arg));


int main(int argc, char *argv[])
{
    setlocale(LC_ALL,"");

    FILE *tty = fopen(_PATH_TTY, "r+");
    if (tty == NULL)
    {
        switch (errno)
        {
        case ENOENT:
        case ENXIO:
            // ignore
            break;
        default:
            perror("Can't open " _PATH_TTY);
            exit(EXIT_FAILURE);
        }
    }

    char prog_name[256];
    if (neo4j_basename(argv[0], prog_name, sizeof(prog_name)) < 0)
    {
        perror("Unexpected error");
        exit(EXIT_FAILURE);
    }

    uint8_t log_level = NEO4J_LOG_WARN;
    struct neo4j_logger_provider *provider = NULL;
    struct io_handler io_handlers[NEO4J_MAX_IO_ARGS];
    unsigned int nio_handlers = 0;
    char *histfile = NULL;

    neo4j_client_init();

    int result = EXIT_FAILURE;

    if (shell_state_init(&state, prog_name, stdin, stdout, stderr, tty))
    {
        neo4j_perror(stderr, errno, "Unexpected error");
        goto cleanup;
    }

    state.interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
    if ((histfile = neo4j_adotdir(NEO4J_HISTORY_FILE)) == NULL)
    {
        neo4j_perror(state.err, errno, "Unexpected error");
        goto cleanup;
    }
    state.histfile = histfile;

    if (isatty(STDERR_FILENO))
    {
        state.colorize = ansi_shell_colorization;
    }

    if (isatty(STDOUT_FILENO))
    {
        neo4j_config_set_results_table_colors(state.config,
                neo4j_results_table_ansi_colors);
        neo4j_config_set_plan_table_colors(state.config,
                neo4j_plan_table_ansi_colors);
    }

    const char *s;
    if ((s = getenv(NEO4J_USERNAME_ENV)) != NULL &&
            neo4j_config_set_username(state.config, s))
    {
        neo4j_perror(state.err, errno, "Unexpected error");
        goto cleanup;
    }
    bool password_set = false;
    if ((s = getenv(NEO4J_PASSWORD_ENV)) != NULL)
    {
        if (neo4j_config_set_password(state.config, s))
        {
            neo4j_perror(state.err, errno, "Unexpected error");
            goto cleanup;
        }
        password_set = true;
    }

    int c;
    while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) >= 0)
    {
        switch (c)
        {
        case 'h':
            usage(state.out, prog_name);
            result = EXIT_SUCCESS;
            goto cleanup;
        case 'v':
            ++log_level;
            break;
        case HISTFILE_OPT:
            state.histfile = (optarg[0] != '\0')? optarg : NULL;
            break;
        case CA_FILE_OPT:
            if (neo4j_config_set_TLS_ca_file(state.config, optarg))
            {
                neo4j_perror(state.err, errno, "Unexpected error");
                goto cleanup;
            }
            break;
        case CA_DIRECTORY_OPT:
            if (neo4j_config_set_TLS_ca_dir(state.config, optarg))
            {
                neo4j_perror(state.err, errno, "Unexpected error");
                goto cleanup;
            }
            break;
        case COLORIZE_OPT:
            state.colorize = ansi_shell_colorization;
            neo4j_config_set_results_table_colors(state.config,
                    neo4j_results_table_ansi_colors);
            neo4j_config_set_plan_table_colors(state.config,
                    neo4j_plan_table_ansi_colors);
            break;
        case NO_COLORIZE_OPT:
            state.colorize = no_shell_colorization;
            neo4j_config_set_results_table_colors(state.config,
                    neo4j_results_table_no_colors);
            neo4j_config_set_plan_table_colors(state.config,
                    neo4j_plan_table_no_colors);
            break;
        case INSECURE_OPT:
            state.connect_flags |= NEO4J_INSECURE;
            break;
        case NON_INTERACTIVE_OPT:
            state.interactive = false;
            if (tty != NULL)
            {
                fclose(tty);
                tty = NULL;
            }
            break;
        case 'u':
            if (neo4j_config_set_username(state.config, optarg))
            {
                neo4j_perror(state.err, errno, "Unexpected error");
                goto cleanup;
            }
            break;
        case 'p':
            if (neo4j_config_set_password(state.config, optarg))
            {
                neo4j_perror(state.err, errno, "Unexpected error");
                goto cleanup;
            }
            password_set = true;
            break;
        case 'P':
            state.password_prompt = true;
            break;
        case KNOWN_HOSTS_OPT:
            if (neo4j_config_set_known_hosts_file(state.config, optarg))
            {
                neo4j_perror(state.err, errno, "Unexpected error");
                goto cleanup;
            }
            break;
        case NO_KNOWN_HOSTS_OPT:
            if (neo4j_config_set_trust_known_hosts(state.config, false))
            {
                neo4j_perror(state.err, errno, "Unexpected error");
                goto cleanup;
            }
            break;
        case NOHIST_OPT:
            state.histfile = NULL;
            break;
        case PIPELINE_MAX_OPT:
            {
                int arg = atoi(optarg);
                if (arg < 1)
                {
                    fprintf(state.err, "Invalid pipeline-max '%s'\n", optarg);
                    goto cleanup;
                }
                state.pipeline_max = arg;
                neo4j_config_set_max_pipelined_requests(state.config, arg * 2);
            }
            break;
        case 'i':
            state.interactive = false;
            if (add_io_handler(io_handlers, &nio_handlers,
                        optarg, false, source))
            {
                goto cleanup;
            }
            break;
        case SOURCE_MAX_DEPTH_OPT:
            {
                int arg = atoi(optarg);
                if (arg < 1)
                {
                    fprintf(state.err, "Invalid source-max-depth '%s'\n",
                            optarg);
                    goto cleanup;
                }
                state.source_max_depth = arg;
            }
            break;
        case 'e':
            state.interactive = false;
            if (add_io_handler(io_handlers, &nio_handlers,
                        optarg, false, eval))
            {
                goto cleanup;
            }
            break;
        case 'o':
            if (add_io_handler(io_handlers, &nio_handlers,
                        optarg, true, redirect_output))
            {
                goto cleanup;
            }
            break;
        case EXPORT_OPT:
            {
                char *export = strdup(optarg);
                if (export == NULL)
                {
                    neo4j_perror(state.err, errno, "Unexpected error");
                    goto cleanup;
                }
                const char *eq = strchr(export, '=');
                if (eq == NULL)
                {
                    free(export);
                    fprintf(state.err, "Invalid source-max-depth '%s'\n",
                            optarg);
                    goto cleanup;
                }
                size_t elen = eq - export;
                for (; elen > 0 && isspace(export[elen-1]); --elen)
                    ;
                neo4j_value_t name = neo4j_ustring(export, elen);
                neo4j_value_t value = neo4j_string(eq + 1);
                if (shell_state_add_export(&state, name, value, export))
                {
                    free(export);
                    neo4j_perror(state.err, errno, "Unexpected error");
                    goto cleanup;
                }
            }
            break;
        case VERSION_OPT:
            fprintf(state.out, "neo4j-client: %s\n", PACKAGE_VERSION);
            fprintf(state.out, "libneo4j-client: %s\n",
                    libneo4j_client_version());
            fprintf(state.out, "libcypher-parser: %s\n",
                    libcypher_parser_version());
            result = EXIT_SUCCESS;
            goto cleanup;
        default:
            usage(state.err, prog_name);
            goto cleanup;
        }
    }

    if (nio_handlers > 0 && io_handlers[nio_handlers-1].is_output)
    {
        fprintf(stderr,
                "--output/-o must be followed by --source/-i or --eval/-e\n");
        goto cleanup;
    }

    argc -= optind;
    argv += optind;

    if (argc > 2)
    {
        usage(state.err, prog_name);
        goto cleanup;
    }

    if (argc == 0 && password_set)
    {
        fprintf(stderr, "--password/-p can only be used when a URL or "
                "host to connect to is also supplied.\n");
        goto cleanup;
    }

    uint8_t logger_flags = 0;
    if (log_level < NEO4J_LOG_DEBUG)
    {
        logger_flags = NEO4J_STD_LOGGER_NO_PREFIX;
    }
    provider = neo4j_std_logger_provider(state.err, log_level, logger_flags);
    if (provider == NULL)
    {
        neo4j_perror(state.err, errno, "Unexpected error");
        goto cleanup;
    }

    neo4j_config_set_logger_provider(state.config, provider);

    if (tty != NULL)
    {
        neo4j_config_set_unverified_host_callback(state.config,
                host_verification, &state);

        if (state.interactive)
        {
            state.password_prompt = true;
        }
    }
    else if (state.password_prompt)
    {
        fprintf(state.err,
                "Cannot prompt for a password in non-interactive mode\n");
        goto cleanup;
    }

    if (argc >= 1 && db_connect(&state, cypher_input_position_zero,
                argv[0], (argc > 1)? argv[1] : NULL))
    {
        goto cleanup;
    }

    // remove any password from the config
    ignore_unused_result(neo4j_config_set_password(state.config, NULL));

    if (signal(SIGINT, interrupt_handler) == SIG_ERR)
    {
        perror("Unexpected error");
        goto cleanup;
    }

    if (state.interactive)
    {
        state.render = render_results_table;
        neo4j_config_set_render_nulls(state.config, true);
        state.show_timing = true;
        state.infile = "<interactive>";
        state.source_depth = 1;
        if (interact(&state))
        {
            goto cleanup;
        }
    }
    else if (nio_handlers > 0)
    {
        state.render = render_results_csv;
        for (unsigned int i = 0; i < nio_handlers; ++i)
        {
            if (io_handlers[i].handle(&state, cypher_input_position_zero,
                    io_handlers[i].arg))
            {
                goto cleanup;
            }
        }
    }
    else
    {
        state.render = render_results_csv;
        state.infile = "<stdin>";
        state.source_depth = 1;
        if (batch(&state, cypher_input_position_zero, state.in))
        {
            goto cleanup;
        }
    }

    result = EXIT_SUCCESS;

cleanup:
    free(histfile);
    shell_state_destroy(&state);
    if (provider != NULL)
    {
        neo4j_std_logger_provider_free(provider);
    }
    for (unsigned int i = 0; i < nio_handlers; ++i)
    {
        free(io_handlers[i].arg);
    }
    if (tty != NULL)
    {
        fclose(tty);
    }
    neo4j_client_cleanup();
    return result;
}


int add_io_handler(struct io_handler *io_handlers, unsigned int *nio_handlers,
        const char *arg, bool is_output,
        int (*handler)(shell_state_t *state, struct cypher_input_position pos,
            const char *arg))
{
    if (*nio_handlers >= NEO4J_MAX_IO_ARGS)
    {
        fprintf(state.err,
                "Too many --source, --eval and/or --output args\n");
        return -1;
    }
    io_handlers[*nio_handlers].arg = strdup(arg);
    io_handlers[*nio_handlers].is_output = is_output;
    io_handlers[*nio_handlers].handle = handler;
    ++(*nio_handlers);
    return 0;
}


void interrupt_handler(int signal)
{
    if (state.connection != NULL && neo4j_reset(state.connection))
    {
        neo4j_perror(stderr, errno, "Reset failed");
    }
}
