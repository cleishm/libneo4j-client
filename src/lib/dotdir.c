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
#include "client_config.h"
#include "memory.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>


#define NEO4J_DOT_DIR ".neo4j"


static ssize_t homedir(char *buf, size_t n);


ssize_t neo4j_dot_dir(char *buf, size_t n, const char *append)
{
    char hbuf[PATH_MAX];
    ssize_t hlen = homedir(hbuf, sizeof(hbuf));
    if (hlen < 0)
    {
        if (errno == ERANGE)
        {
            errno = ENAMETOOLONG;
        }
        return -1;
    }

    size_t alen = 0;
    if (append != NULL)
    {
        for (; *append != '\0' && *append == '/'; ++append)
            ;
        alen = strlen(append);
    }

    size_t dlen = sizeof(NEO4J_DOT_DIR) - 1;
    size_t len = (size_t)hlen + 1 + dlen + ((alen > 0)? 1 + alen : 0);

    if (buf != NULL)
    {
        if ((len + 1) > n)
        {
            errno = ERANGE;
            return -1;
        }

        memcpy(buf, hbuf, hlen);
        buf += hlen;
        *(buf++) = '/';
        memcpy(buf, NEO4J_DOT_DIR, dlen);
        buf += dlen;
        if (alen > 0)
        {
            *(buf++) = '/';
            memcpy(buf, append, alen);
            buf += alen;
        }
        *buf = '\0';
    }

    return len;
}


ssize_t homedir(char *buf, size_t n)
{
    assert(buf != NULL);

    ssize_t result = -1;
    char *pwbuf = NULL;

    const char *hdir = getenv("HOME");
    if (hdir == NULL)
    {
        ssize_t pwbufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
        if (pwbufsize < 0)
        {
            return -1;
        }

        pwbuf = malloc(pwbufsize);
        if (pwbuf == NULL)
        {
            return -1;
        }

        struct passwd pwd;
        struct passwd *result = NULL;
        int err = getpwuid_r(geteuid(), &pwd, pwbuf, pwbufsize, &result);
        if (err != 0)
        {
            errno = err;
            goto cleanup;
        }
        if (result == NULL)
        {
            errno = EIDRM;
            goto cleanup;
        }
        hdir = pwd.pw_dir;
    }

    size_t hlen = strlen(hdir);
    for (; hlen > 0 && hdir[hlen-1] == '/'; --hlen)
        ;
    if ((hlen + 1) > n)
    {
        errno = ERANGE;
        return -1;
    }
    memcpy(buf, hdir, hlen + 1);

    result = hlen;

    int errsv;
cleanup:
    errsv = errno;
    if (pwbuf != NULL)
    {
        free(pwbuf);
    }
    errno = errsv;
    return result;
}
