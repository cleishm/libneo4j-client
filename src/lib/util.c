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
#include "util.h"
#include "neo4j-client.h"
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <sys/stat.h>


char *strcat_alloc(const char *s1, const char *s2)
{
    REQUIRE(s1 != NULL, NULL);
    if (s2 == NULL)
    {
        return strdup(s1);
    }

    size_t s1len = strlen(s1);
    size_t s2len = strlen(s2);
    size_t n = s1len + s2len + 1;

    char *s = malloc(n);
    if (s == NULL)
    {
        return NULL;
    }

    memcpy(s, s1, s1len);
    memcpy(s+s1len, s2, s2len);
    s[n-1] = '\0';
    return s;
}


ssize_t memcpy_iov_s(void *dst, const struct iovec *iov, int iovcnt,
        size_t dmax)
{
    REQUIRE(dst != NULL, -1);
    REQUIRE(iov != NULL, -1);
    REQUIRE(iovcnt >= 0, -1);
    REQUIRE(dmax <= SSIZE_MAX, -1);

    if (iovlen(iov, iovcnt) > dmax)
    {
        errno = EFAULT;
        return -1;
    }

    size_t n = 0;
    for (int i = 0; i < iovcnt; ++i)
    {
        memcpy((uint8_t *)dst + n, iov[i].iov_base, iov[i].iov_len);
        n += iov[i].iov_len;
    }

    return n;
}


int iov_skip(struct iovec *diov, int diovcnt,
        const struct iovec *siov, int siovcnt, size_t nbyte)
{
    REQUIRE(diov != NULL, -1);
    REQUIRE(siov != NULL, -1);
    REQUIRE(nbyte == 0 || siovcnt > 0, -1);

    // find the first source iovector that should be used
    int si = 0;
    while (si < siovcnt && nbyte >= siov[si].iov_len)
    {
        nbyte -= siov[si].iov_len;
        si++;
    }

    if (si == siovcnt && nbyte > 0)
    {
        return 0;
    }

    if (siovcnt - si > diovcnt)
    {
        errno = EFAULT;
        return -1;
    }

    // if necessary, find offset into first source iovector
    int di = 0;
    if (nbyte > 0)
    {
        diov[0].iov_base = ((uint8_t *)(siov[si].iov_base)) + nbyte;
        diov[0].iov_len = siov[si].iov_len - nbyte;
        di++;
        si++;
    }

    // copy remaining iovectors unchanged
    for (; si < siovcnt; di++, si++)
    {
        if (siov[si].iov_len == 0)
        {
            si++;
            continue;
        }
        assert(di < diovcnt);
        diov[di] = siov[si];
    }

    return di;
}


int iov_limit(struct iovec *diov, int diovcnt,
        const struct iovec *siov, int siovcnt, size_t nbyte)
{
    REQUIRE(diov != NULL, -1);
    REQUIRE(siov != NULL, -1);
    if (nbyte == 0)
    {
        return 0;
    }
    REQUIRE(siovcnt > 0, -1);

    // copy whole iovectors first
    int si = 0;
    int di = 0;
    while (si < siovcnt && di < diovcnt && nbyte >= siov[si].iov_len)
    {
        if (siov[si].iov_len == 0)
        {
            si++;
            continue;
        }
        diov[di] = siov[si];
        nbyte -= siov[si].iov_len;
        si++;
        di++;
    }

    // offset into remaining iovector
    if (si < siovcnt && nbyte > 0)
    {
        if (di == diovcnt)
        {
            errno = EFAULT;
            return -1;
        }
        assert(nbyte < siov[si].iov_len);
        diov[di].iov_base = siov[si].iov_base;
        diov[di].iov_len = nbyte;
        di++;
    }

    return di;
}


ssize_t neo4j_dirname(const char *path, char *buffer, size_t n)
{
    if (path == NULL)
    {
        path = "";
    }

    const char *end = path + strlen(path) - 1;
    for (; end > path && *end == '/'; --end)
        ;
    for (; end > path && *end != '/'; --end)
        ;

    if (end <= path)
    {
        path = end = (*path == '/')? "/" : ".";
    }

    for (; end > path && *end == '/'; --end)
        ;
    ++end;

    size_t len = end - path;
    if (buffer != NULL)
    {
        if ((len + 1) > n)
        {
            errno = ERANGE;
            return -1;
        }

        memcpy(buffer, path, len);
        buffer[len] = '\0';
    }
    return len;
}


ssize_t neo4j_basename(const char *path, char *buffer, size_t n)
{
    if (path == NULL)
    {
        path = "";
    }

    const char *end = path + strlen(path) - 1;
    for (; end > path && *end == '/'; --end)
        ;

    const char *p = end;
    for (; p > path && *p != '/'; --p)
        ;

    if (end == p)
    {
        p = end = (*p == '/')? "/" : ".";
    }
    else if (*p == '/')
    {
        ++p;
    }
    ++end;

    size_t len = end - p;
    if (buffer != NULL)
    {
        if ((len + 1) > n)
        {
            errno = ERANGE;
            return -1;
        }

        memcpy(buffer, p, len);
        buffer[len] = '\0';
    }
    return len;
}


int neo4j_mkdir_p(const char *path)
{
    REQUIRE(path != NULL, -1);

    char buf[PATH_MAX];
    size_t len = strlen(path);
    while (len > 0 && path[len-1] == '/')
    {
        --len;
    }
    if (len == 0)
    {
        return 0;
    }
    if (len >= PATH_MAX)
    {
        errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(buf, path, len);
    buf[len] = '\0';

    for (char *slash = buf; *slash != '\0';)
    {
        slash += strspn(slash, "/");
        slash += strcspn(slash, "/");

        char prev = *slash;
        *slash = '\0';

        struct stat sb;
        if (stat(buf, &sb))
        {
            if (errno != ENOENT || (mkdir(path, 0777) && errno != EEXIST))
            {
                return -1;
            }
        }
        else if (!S_ISDIR(sb.st_mode))
        {
            return -1;
        }

        *slash = prev;
    }

    return 0;
}
