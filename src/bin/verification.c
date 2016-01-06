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
#include "verification.h"
#include "state.h"
#include <ctype.h>
#include <errno.h>
#include <string.h>


static int unrecognized_host_verification(shell_state_t *state,
        const char *host, const char *fingerprint);
static int mismatched_host_verification(shell_state_t *state,
        const char *host, const char *fingerprint);
static int read_response(shell_state_t *state);


int host_verification(void *userdata, const char *host, const char *fingerprint,
        neo4j_unverified_host_reason_t reason)
{
    shell_state_t *state = (shell_state_t *)userdata;
    switch (reason)
    {
    case NEO4J_HOST_VERIFICATION_UNRECOGNIZED:
        return unrecognized_host_verification(state, host, fingerprint);
    case NEO4J_HOST_VERIFICATION_MISMATCH:
        return mismatched_host_verification(state, host, fingerprint);
    }
    return NEO4J_HOST_VERIFICATION_REJECT;
}


int unrecognized_host_verification(shell_state_t *state,
        const char *host, const char *fingerprint)
{
    fprintf(state->tty,
"The authenticity of host '%s' could not be established.\n"
"TLS certificate fingerprint is %s.\n"
"Would you like to trust this host (NO/yes/once)? ", host, fingerprint);
    fflush(state->tty);
    return read_response(state);
}


int mismatched_host_verification(shell_state_t *state,
        const char *host, const char *fingerprint)
{
    fprintf(state->tty,
"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
"@    WARNING: SERVER IDENTIFICATION HAS CHANGED!     @\n"
"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
"Someone could be eavesdropping on you right now (man-in-the-middle attack)!\n"
"It is also possible that the TLS certificate for '%s' has been changed.\n"
"The fingerprint of the TLS certificate sent by the server is %s.\n"
"Would you like to trust this new certificate (NO/yes/once)? ",
        host, fingerprint);
    fflush(state->tty);
    return read_response(state);
}


int read_response(shell_state_t *state)
{
    char buf[256];
    while (fgets(buf, sizeof(buf), state->tty) != NULL)
    {
        char *p = buf;
        for (; isspace(*p); ++p)
            ;
        size_t l = strlen(p);
        for (; l > 0 && isspace(p[l-1]); --l)
        {
            p[l-1] = '\0';
        }

        if (p[0] == '\0' || strcmp(p, "n") == 0 || strcmp(p, "N") == 0 ||
                strcmp(p, "no") == 0 || strcmp(p, "NO") == 0)
        {
            return NEO4J_HOST_VERIFICATION_REJECT;
        }
        if (strcmp(p, "y") == 0 || strcmp(p, "Y") == 0 ||
                strcmp(p, "yes") == 0 || strcmp(p, "YES") == 0)
        {
            return NEO4J_HOST_VERIFICATION_TRUST;
        }
        if (strcmp(p, "o") == 0 || strcmp(p, "O") == 0 ||
                strcmp(p, "once") == 0 || strcmp(p, "ONCE") == 0)
        {
            return NEO4J_HOST_VERIFICATION_ACCEPT_ONCE;
        }

        fprintf(state->tty, "Please type 'no', 'yes' or 'once': ");
        fflush(state->tty);
    }

    int err = ferror(state->tty);
    if (err == 0)
    {
        return NEO4J_HOST_VERIFICATION_REJECT;
    }
    errno = err;
    return -1;
}
