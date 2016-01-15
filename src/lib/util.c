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

static bool identifier_chars[UCHAR_MAX+1] = {
        false, // 0x00
        false, // 0x01
        false, // 0x02
        false, // 0x03
        false, // 0x04
        false, // 0x05
        false, // 0x06
        false, // 0x07
        false, // 0x08
        false, // 0x09
        false, // 0x0a
        false, // 0x0b
        false, // 0x0c
        false, // 0x0d
        false, // 0x0e
        false, // 0x0f
        false, // 0x10
        false, // 0x11
        false, // 0x12
        false, // 0x13
        false, // 0x14
        false, // 0x15
        false, // 0x16
        false, // 0x17
        false, // 0x18
        false, // 0x19
        false, // 0x1a
        false, // 0x1b
        false, // 0x1c
        false, // 0x1d
        false, // 0x1e
        false, // 0x1f
        false, // 0x20
        false, // 0x21
        false, // 0x22
        false, // 0x23
        false, // 0x24
        false, // 0x25
        false, // 0x26
        false, // 0x27
        false, // 0x28
        false, // 0x29
        false, // 0x2a
        false, // 0x2b
        false, // 0x2c
        false, // 0x2d
        false, // 0x2e
        false, // 0x2f
        true,  // 0x30 '0'
        true,  // 0x31 '1'
        true,  // 0x32 '2'
        true,  // 0x33 '3'
        true,  // 0x34 '4'
        true,  // 0x35 '5'
        true,  // 0x36 '6'
        true,  // 0x37 '7'
        true,  // 0x38 '8'
        true,  // 0x39 '9'
        false, // 0x3a
        false, // 0x3b
        false, // 0x3c
        false, // 0x3d
        false, // 0x3e
        false, // 0x3f
        false, // 0x40
        true,  // 0x41 'A'
        true,  // 0x42 'B'
        true,  // 0x43 'C'
        true,  // 0x44 'D'
        true,  // 0x45 'E'
        true,  // 0x46 'F'
        true,  // 0x47 'G'
        true,  // 0x48 'H'
        true,  // 0x49 'I'
        true,  // 0x4a 'J'
        true,  // 0x4b 'K'
        true,  // 0x4c 'L'
        true,  // 0x4d 'M'
        true,  // 0x4e 'N'
        true,  // 0x4f 'O'
        true,  // 0x50 'P'
        true,  // 0x51 'Q'
        true,  // 0x52 'R'
        true,  // 0x53 'S'
        true,  // 0x54 'T'
        true,  // 0x55 'U'
        true,  // 0x56 'V'
        true,  // 0x57 'W'
        true,  // 0x58 'X'
        true,  // 0x59 'Y'
        true,  // 0x5a 'Z'
        false, // 0x5b
        false, // 0x5c
        false, // 0x5d
        false, // 0x5e
        true,  // 0x5f '_'
        false, // 0x60
        true,  // 0x61 'a'
        true,  // 0x62 'b'
        true,  // 0x63 'c'
        true,  // 0x64 'd'
        true,  // 0x65 'e'
        true,  // 0x66 'f'
        true,  // 0x67 'g'
        true,  // 0x68 'h'
        true,  // 0x69 'i'
        true,  // 0x6a 'j'
        true,  // 0x6b 'k'
        true,  // 0x6c 'l'
        true,  // 0x6d 'm'
        true,  // 0x6e 'n'
        true,  // 0x6f 'o'
        true,  // 0x70 'p'
        true,  // 0x71 'q'
        true,  // 0x72 'r'
        true,  // 0x73 's'
        true,  // 0x74 't'
        true,  // 0x75 'u'
        true,  // 0x76 'v'
        true,  // 0x77 'w'
        true,  // 0x78 'x'
        true,  // 0x79 'y'
        true,  // 0x7a 'z'
        false, // 0x7b
        false, // 0x7c
        false, // 0x7d
        false, // 0x7e
        false, // 0x7f
        false, // 0x80
        false, // 0x81
        false, // 0x82
        false, // 0x83
        false, // 0x84
        false, // 0x85
        false, // 0x86
        false, // 0x87
        false, // 0x88
        false, // 0x89
        false, // 0x8a
        false, // 0x8b
        false, // 0x8c
        false, // 0x8d
        false, // 0x8e
        false, // 0x8f
        false, // 0x90
        false, // 0x91
        false, // 0x92
        false, // 0x93
        false, // 0x94
        false, // 0x95
        false, // 0x96
        false, // 0x97
        false, // 0x98
        false, // 0x99
        false, // 0x9a
        false, // 0x9b
        false, // 0x9c
        false, // 0x9d
        false, // 0x9e
        false, // 0x9f
        false, // 0xa0
        false, // 0xa1
        false, // 0xa2
        false, // 0xa3
        false, // 0xa4
        false, // 0xa5
        false, // 0xa6
        false, // 0xa7
        false, // 0xa8
        false, // 0xa9
        false, // 0xaa
        false, // 0xab
        false, // 0xac
        false, // 0xad
        false, // 0xae
        false, // 0xaf
        false, // 0xb1
        false, // 0xb2
        false, // 0xb3
        false, // 0xb4
        false, // 0xb5
        false, // 0xb6
        false, // 0xb7
        false, // 0xb8
        false, // 0xb9
        false, // 0xba
        false, // 0xbb
        false, // 0xbc
        false, // 0xbd
        false, // 0xbe
        false, // 0xbf
        false, // 0xc0
        false, // 0xc1
        false, // 0xc2
        false, // 0xc3
        false, // 0xc4
        false, // 0xc5
        false, // 0xc6
        false, // 0xc7
        false, // 0xc8
        false, // 0xc9
        false, // 0xca
        false, // 0xcb
        false, // 0xcc
        false, // 0xcd
        false, // 0xce
        false, // 0xcf
        false, // 0xd0
        false, // 0xd1
        false, // 0xd2
        false, // 0xd3
        false, // 0xd4
        false, // 0xd5
        false, // 0xd6
        false, // 0xd7
        false, // 0xd8
        false, // 0xd9
        false, // 0xda
        false, // 0xdb
        false, // 0xdc
        false, // 0xdd
        false, // 0xde
        false, // 0xdf
        false, // 0xe0
        false, // 0xe1
        false, // 0xe2
        false, // 0xe3
        false, // 0xe4
        false, // 0xe5
        false, // 0xe6
        false, // 0xe7
        false, // 0xe8
        false, // 0xe9
        false, // 0xea
        false, // 0xeb
        false, // 0xec
        false, // 0xed
        false, // 0xee
        false, // 0xef
        false, // 0xf0
        false, // 0xf1
        false, // 0xf2
        false, // 0xf3
        false, // 0xf4
        false, // 0xf5
        false, // 0xf6
        false, // 0xf7
        false, // 0xf8
        false, // 0xf9
        false, // 0xfa
        false, // 0xfb
        false, // 0xfc
        false, // 0xfd
        false, // 0xfe
        false // 0xff
    };


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


struct memcspn_index
{
    unsigned char table[UCHAR_MAX+1];
    unsigned char marker;
};


size_t memcspn(const void *s, size_t n, const unsigned char *reject,
        size_t rlen)
{
#ifdef THREAD_LOCAL
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


size_t memspn_ident(const void *s, size_t n)
{
    const unsigned char *c = (const unsigned char *)s;
    for (; c < (const unsigned char *)s + n; c++)
    {
        if (!identifier_chars[*c])
        {
            return c - (const unsigned char *)s;
        }
    }
    return n;
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
            if (errno != ENOENT || (mkdir(buf, 0777) && errno != EEXIST))
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
