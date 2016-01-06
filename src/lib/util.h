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
#ifndef NEO4J_UTIL_H
#define NEO4J_UTIL_H

#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>

#ifdef HAVE_ENDIAN_H
#include <endian.h>
#endif
#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif
#ifdef HAVE_LIBKERN_OSBYTEORDER_H
#include <libkern/OSByteOrder.h>
#endif


#define REQUIRE(cond, res) \
    if (!(cond)) { errno = EINVAL; return res; }


#define ENSURE_NOT_NULL(type, name, val) \
    type _##name = (val); \
    if (name == NULL) \
    { \
        name = &_##name; \
    }


static inline int strdup_null(char **dptr, const char *s)
{
    if (s == NULL)
    {
        *dptr = NULL;
        return 0;
    }
    char *dup = strdup(s);
    if (dup == NULL)
    {
        return -1;
    }
    *dptr = dup;
    return 0;
}


static inline void replace_strptr(char **dptr, char *s)
{
    if (*dptr != NULL)
    {
        free(*dptr);
    }
    *dptr = s;
}


static inline int replace_strptr_ndup(char **dptr, const char *s, size_t n)
{
    char *dup = (s != NULL)? strndup(s, n) : NULL;
    if (s != NULL && dup == NULL)
    {
        return -1;
    }
    replace_strptr(dptr, dup);
    return 0;
}


static inline int replace_strptr_dup(char **dptr, const char *s)
{
    char *dup = (s != NULL)? strdup(s) : NULL;
    if (s != NULL && dup == NULL)
    {
        return -1;
    }
    replace_strptr(dptr, dup);
    return 0;
}


char *strcat_alloc(const char *s1, const char *s2);


#define contains_null(ptr, n) _contains_null((void **)ptr, n)
static inline bool _contains_null(void *ptrs[], int n)
{
    for (int i = 0; i < n; ++i)
    {
        if (ptrs[i] == NULL)
        {
            return true;
        }
    }
    return false;
}


static inline int min(int a, int b)
{
    return (a <= b)? a : b;
}


static inline unsigned int minu(unsigned int a, unsigned int b)
{
    return (a <= b)? a : b;
}


static inline size_t minzu(size_t a, size_t b)
{
    return (a <= b)? a : b;
}


static inline int max(int a, int b)
{
    return (a >= b)? a : b;
}


static inline unsigned int maxu(unsigned int a, unsigned int b)
{
    return (a >= b)? a : b;
}


static inline size_t iovlen(const struct iovec *iov, int iovcnt)
{
    size_t total = 0;
    for (int i = 0; i < iovcnt; ++i)
    {
        total += iov[i].iov_len;
    }
    return total;
}


ssize_t memcpy_iov_s(void *dst, const struct iovec *iov, int iovcnt,
        size_t dmax);

int iov_skip(struct iovec *diov, int diovcnt,
        const struct iovec *siov, int siovcnt, size_t nbyte);

int iov_limit(struct iovec *diov, int diovcnt,
        const struct iovec *siov, int siovcnt, size_t nbyte);


#ifndef HAVE_HTOBE64
#  ifdef HAVE_HTONLL
#    define htobe64(l) htonll(l)
#  elif HAVE_OSSWAPHOSTTOBIGINT64
#    define htobe64(l) OSSwapHostToBigInt64(l)
#  elif WORDS_BIGENDIAN
#    define htobe64(l) (l)
#  elif HAVE_BSWAP_64
#    define htobe64(l) bswap_64(l)
#  else
#    error "No htobe64 or altnerative"
#  endif
#endif

#ifndef HAVE_BE64TOH
#  ifdef HAVE_NTOHLL
#    define be64toh(l) ntohll(l)
#  elif HAVE_OSSWAPBIGTOHOSTINT64
#    define be64toh(l) OSSwapBigToHostInt64(l)
#  elif WORDS_BIGENDIAN
#    define be64toh(l) (l)
#  elif HAVE_BSWAP_64
#    define be64toh(l) bswap_64(l)
#  else
#    error "No be64toh or altnerative"
#  endif
#endif

#endif/*NEO4J_UTIL_H*/
