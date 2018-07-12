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


static ssize_t _neo4j_dirname(const char *path, char **buffer, size_t *n);
static ssize_t _neo4j_basename(const char *path, char **buffer, size_t *n);


ssize_t neo4j_dirname(const char *path, char *buffer, size_t n)
{
    return _neo4j_dirname(path, (buffer != NULL)? &buffer : NULL, &n);
}


char *neo4j_adirname(const char *path)
{
    char *buffer = NULL;
    size_t n = 0;
    if (_neo4j_dirname(path, &buffer, &n) < 0)
    {
        return NULL;
    }
    return buffer;
}


ssize_t _neo4j_dirname(const char *path, char **buffer, size_t *n)
{
    assert(n != NULL);

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
        if (*buffer == NULL)
        {
            *buffer = malloc(len + 1);
            if (*buffer == NULL)
            {
                return -1;
            }
        }
        else if ((len + 1) > *n)
        {
            errno = ERANGE;
            return -1;
        }

        memcpy(*buffer, path, len);
        (*buffer)[len] = '\0';
    }
    *n = len + 1;
    return len;
}


ssize_t neo4j_basename(const char *path, char *buffer, size_t n)
{
    return _neo4j_basename(path, (buffer != NULL)? &buffer : NULL, &n);
}


char *neo4j_abasename(const char *path)
{
    char *buffer = NULL;
    size_t n = 0;
    if (_neo4j_basename(path, &buffer, &n) < 0)
    {
        return NULL;
    }
    return buffer;
}


ssize_t _neo4j_basename(const char *path, char **buffer, size_t *n)
{
    assert(n != NULL);

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
        if (*buffer == NULL)
        {
            *buffer = malloc(len + 1);
            if (*buffer == NULL)
            {
                return -1;
            }
        }
        else if ((len + 1) > *n)
        {
            errno = ERANGE;
            return -1;
        }

        memcpy(*buffer, p, len);
        (*buffer)[len] = '\0';
    }
    *n = len + 1;
    return len;
}


int neo4j_mkdir_p(const char *path)
{
    REQUIRE(path != NULL, -1);

    size_t len = strlen(path);
    while (len > 0 && path[len-1] == '/')
    {
        --len;
    }
    if (len == 0)
    {
        return 0;
    }
    char *buf = strndup(path, len);
    if (buf == NULL)
    {
        return -1;
    }

    int result = -1;

    for (char *slash = buf; *slash != '\0';)
    {
        slash += strspn(slash, "/");
        slash += strcspn(slash, "/");

        char prev = *slash;
        *slash = '\0';

        struct stat sb;
        if (stat(buf, &sb))
        {
            if (errno != ENOENT || (mkdir(buf, 0777) && errno != EEXIST))
            {
                goto cleanup;
            }
        }
        else if (!S_ISDIR(sb.st_mode))
        {
            goto cleanup;
        }

        *slash = prev;
    }

    result = 0;

cleanup:
    free(buf);
    return result;
}


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


static inline unsigned char tolower_indep(unsigned char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return (c - 'A') + 'a';
    }
    return c;
}


int strcasecmp_indep(const char *s1, const char *s2)
{
    for (; (*s1 != '\0') && (*s2 != '\0'); s1++, s2++)
    {
        unsigned char c1 = tolower_indep(*s1);
        unsigned char c2 = tolower_indep(*s2);
        if (c1 != c2)
        {
            return (c1 < c2)? -1 : 1;
        }
    }
    return (*s2 != '\0')? -1 : ((*s1 != '\0')? 1 : 0);
}


int strncasecmp_indep(const char *s1, const char *s2, size_t n)
{
    for (; n > 0 && (*s1 != '\0') && (*s2 != '\0'); s1++, s2++, --n)
    {
        unsigned char c1 = tolower_indep(*s1);
        unsigned char c2 = tolower_indep(*s2);
        if (c1 != c2)
        {
            return (c1 < c2)? -1 : 1;
        }
    }
    return (n == 0)? 0 : ((*s2 != '\0')? -1 : ((*s1 != '\0')? 1 : 0));
}


size_t memcpy_from_iov(void *dst, size_t n,
        const struct iovec *iov, unsigned int iovcnt)
{
    REQUIRE(dst != NULL, -1);
    REQUIRE(iov != NULL, -1);

    size_t copied = 0;
    for (unsigned int i = 0; n > 0 && i < iovcnt; ++i)
    {
        size_t l = minzu(iov[i].iov_len, n);
        memcpy((uint8_t *)dst + copied, iov[i].iov_base, l);
        copied += l;
        n -= l;
    }
    return copied;
}


size_t memcpy_to_iov(const struct iovec *iov, unsigned int iovcnt,
        const void *src, size_t n)
{
    REQUIRE(iov != NULL, -1);
    REQUIRE(src != NULL, -1);

    const uint8_t *src_bytes = src;
    size_t copied = 0;
    for (; n > 0 && iovcnt > 0; ++iov, --iovcnt)
    {
        size_t l = minzu(iov[0].iov_len, n);
        memcpy(iov[0].iov_base, src_bytes, l);
        copied += l;
        src_bytes += l;
        n -= l;
    }
    return copied;
}


size_t memcpy_from_iov_to_iov(const struct iovec *diov, unsigned int diovcnt,
        const struct iovec *siov, unsigned int siovcnt)
{
    REQUIRE(diov != NULL, -1);
    REQUIRE(siov != NULL, -1);

    size_t copied = 0;
    size_t doffset = 0;
    for (unsigned int si = 0; si < siovcnt; ++si)
    {
        uint8_t *src_bytes = siov[si].iov_base;
        size_t src_len = siov[si].iov_len;
        for (; diovcnt > 0; --diovcnt, ++diov, doffset = 0)
        {
            size_t l = minzu(diov[0].iov_len - doffset, src_len);
            memcpy((uint8_t *)diov[0].iov_base + doffset, src_bytes, l);
            copied += l;
            src_bytes += l;
            src_len -= l;
            if (src_len == 0)
            {
                doffset += l;
                break;
            }
        }
    }
    return copied;
}


unsigned int iov_skip(struct iovec *diov, const struct iovec *siov,
        unsigned int iovcnt, size_t nbyte)
{
    REQUIRE(diov != NULL, -1);
    REQUIRE(siov != NULL, -1);
    REQUIRE(nbyte == 0 || iovcnt > 0, -1);

    // find the first source iovector that should be used
    unsigned int si = 0;
    while (si < iovcnt && nbyte >= siov[si].iov_len)
    {
        nbyte -= siov[si].iov_len;
        si++;
    }

    if (si == iovcnt && nbyte > 0)
    {
        return 0;
    }

    // if necessary, find offset into first source iovector
    unsigned int di = 0;
    if (nbyte > 0)
    {
        diov[0].iov_base = ((uint8_t *)(siov[si].iov_base)) + nbyte;
        diov[0].iov_len = siov[si].iov_len - nbyte;
        di++;
        si++;
    }

    // copy remaining iovectors unchanged
    for (; si < iovcnt; di++, si++)
    {
        if (siov[si].iov_len == 0)
        {
            si++;
            continue;
        }
        diov[di] = siov[si];
    }

    return di;
}


unsigned int iov_limit(struct iovec *diov, const struct iovec *siov,
        unsigned int iovcnt, size_t nbyte)
{
    REQUIRE(diov != NULL, -1);
    REQUIRE(siov != NULL, -1);
    if (nbyte == 0)
    {
        return 0;
    }
    REQUIRE(iovcnt > 0, -1);

    // copy whole iovectors first
    unsigned int si = 0;
    unsigned int di = 0;
    while (si < iovcnt && nbyte >= siov[si].iov_len)
    {
        assert(di < iovcnt);
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
    if (si < iovcnt && nbyte > 0)
    {
        assert(nbyte < siov[si].iov_len);
        assert(di < iovcnt);
        diov[di].iov_base = siov[si].iov_base;
        diov[di].iov_len = nbyte;
        di++;
    }

    return di;
}


struct memcspn_index
{
    unsigned char table[UCHAR_MAX+1];
    unsigned char marker;
};


size_t memcspn(const void *s, size_t n, const unsigned char *reject,
        size_t rlen)
{
#if defined THREAD_LOCAL && !(defined NO_THREAD_LOCAL_MEMCSPN)
    static THREAD_LOCAL struct memcspn_index index = { .marker = UCHAR_MAX };
#else
    struct memcspn_index index = { .marker = UCHAR_MAX };
#endif

    if (index.marker == UCHAR_MAX)
    {
        memset(index.table, 0, sizeof(index.table));
        index.marker = 1;
    }
    else
    {
        index.marker++;
    }

    const unsigned char *c = reject + rlen;
    while (c-- > reject)
    {
        index.table[*c] = index.marker;
    }

    const unsigned char *end = (const unsigned char *)s + n;
    for (c = s; c < end; c++)
    {
        if (index.table[*c] == index.marker)
        {
            return c - (const unsigned char *)s;
        }
    }
    return n;
}


static inline bool identifier_char(unsigned char c)
{
    return ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '_');

}


size_t memspn_ident(const void *s, size_t n)
{
    const unsigned char *c = (const unsigned char *)s;
    for (; c < (const unsigned char *)s + n; c++)
    {
        if (!identifier_char(*c))
        {
            return c - (const unsigned char *)s;
        }
    }
    return n;
}


bool hostname_matches(const char *hostname, const char *pattern)
{
    if (strncasecmp_indep(pattern, "xn--", 4) == 0)
    {
        // no wildcards in internationalized domain names
        return (strcasecmp_indep(hostname, pattern) == 0);
    }

    const char *wildcard = strchr(pattern, '*');
    if (wildcard == NULL)
    {
        // no wildcard
        return (strcasecmp_indep(hostname, pattern) == 0);
    }

    const char *pattern_tail = strchr(pattern, '.');
    if (pattern_tail == NULL || pattern_tail < wildcard)
    {
        // wildcard is not in the first label (or there is only one label)
        return (strcasecmp_indep(hostname, pattern) == 0);
    }

    const char *host_tail = strchr(hostname, '.');
    if (host_tail == NULL || strcasecmp_indep(host_tail, pattern_tail) != 0)
    {
        // does not match after the wildcard label
        return false;
    }

    if ((host_tail - hostname) < (pattern_tail - pattern))
    {
        // the wildcard can't match anything
        return false;
    }

    if (strncasecmp(hostname, pattern, wildcard - pattern) != 0)
    {
        // does not match before the wildcard
        return false;
    }

    size_t len = pattern_tail - (wildcard + 1);
    if (strncasecmp(host_tail - len, wildcard + 1, len) != 0)
    {
        // does not match before the wildcard
        return false;
    }

    return true;
}


int describe_host(char *buf, size_t cap, const char *hostname,
        unsigned int port)
{
    int r = snprintf(buf, cap, "%s:%u", hostname, port);
    if (r < 0)
    {
        return r;
    }
    if ((size_t)r >= cap)
    {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}
