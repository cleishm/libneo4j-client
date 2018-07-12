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


static ssize_t _neo4j_dotdir(char **buf, size_t *n, const char *append);
static ssize_t homedir(char **buf, size_t *n);


// deprecated name
ssize_t neo4j_dot_dir(char *buf, size_t n, const char *append)
{
    return neo4j_dotdir(buf, n, append);
}


ssize_t neo4j_dotdir(char *buf, size_t n, const char *append)
{
    return _neo4j_dotdir((buf != NULL)? &buf : NULL, &n, append);
}


char *neo4j_adotdir(const char *append)
{
    char *buf = NULL;
    size_t n = 0;
    if (_neo4j_dotdir(&buf, &n, append) < 0)
    {
        return NULL;
    }
    return buf;
}


ssize_t _neo4j_dotdir(char **buf, size_t *n, const char *append)
{
    bool allocate = (buf != NULL && *buf == NULL);

    ssize_t hlen = homedir(buf, n);
    if (hlen < 0)
    {
        errno = ERANGE;
        goto failure;
    }
    assert(buf == NULL || (*buf != NULL && *n > 0));

    size_t dlen = sizeof(NEO4J_DOT_DIR) - 1;
    assert(dlen < SIZE_MAX);
    if ((!allocate && (*n - hlen - 1) < (1 + dlen)) ||
        (allocate && (SIZE_MAX - hlen - 1) < (1 + dlen)))
    {
        errno = ERANGE;
        goto failure;
    }

    size_t len = (size_t)hlen + 1 + dlen;

    size_t alen = 0;
    if (append != NULL)
    {
        for (; *append != '\0' && *append == '/'; ++append)
            ;
        alen = strlen(append);
    }
    if (alen > 0)
    {
        if (alen >= SIZE_MAX || (SIZE_MAX - len - 1) < (1 + alen))
        {
            errno = ERANGE;
            goto failure;
        }
        len += 1 + alen;
    }

    if (buf != NULL)
    {
        if (*n < (len + 1))
        {
            if (!allocate)
            {
                errno = ERANGE;
                goto failure;
            }
            assert(buf != NULL);
            char *rbuf = realloc(*buf, len + 1);
            if (rbuf == NULL)
            {
                goto failure;
            }
            *buf = rbuf;
            *n = len + 1;
        }

        assert(len < *n);
        char *p = *buf + hlen;
        *(p++) = '/';
        memcpy(p, NEO4J_DOT_DIR, dlen);
        p += dlen;
        if (alen > 0)
        {
            *(p++) = '/';
            memcpy(p, append, alen);
            p += alen;
        }
        *p = '\0';
    }

    return len;

    int errsv;
failure:
    errsv = errno;
    if (allocate)
    {
        free(*buf);
        *buf = NULL;
    }
    errno = errsv;
    return -1;
}


ssize_t homedir(char **buf, size_t *n)
{
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
    if (buf != NULL)
    {
        if (*buf == NULL)
        {
            *buf = malloc(hlen + 1);
            if (*buf == NULL)
            {
                goto cleanup;
            }
            *n = hlen + 1;
        }
        else if ((hlen + 1) > *n)
        {
            errno = ERANGE;
            goto cleanup;
        }

        memcpy(*buf, hdir, hlen + 1);
    }

    result = hlen;

    int errsv;
cleanup:
    errsv = errno;
    free(pwbuf);
    errno = errsv;
    return result;
}
