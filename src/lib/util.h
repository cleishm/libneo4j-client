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
#include <limits.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/param.h>


/**
 * Get the containing structure address.
 *
 * @internal
 */
#define container_of(ptr, type, member) \
        (type *)(void *)( (uint8_t *)(uintptr_t)(ptr) - offsetof(type,member) )


/**
 * Ensure the condition is true, or return the specified result value.
 *
 * @internal
 */
#define REQUIRE(cond, res) \
    if (!(cond)) { errno = EINVAL; return (res); }


/**
 * Check if the named value is null, and if so then update it to point to a
 * stack variable of the specified type.
 *
 * @internal
 */
#define ENSURE_NOT_NULL(type, name, val) \
    type _##name = (val); \
    if (name == NULL) \
    { \
        name = &_##name; \
    }


/**
 * Ignore the result from a function call (suppressing -Wunused-result).
 *
 * (See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66425#c18)
 *
 * @internal
 */
#define ignore_unused_result(func) if (func) { }


/**
 * Determine the minimum of two integers.
 *
 * @internal
 *
 * @param [a] The first integer.
 * @param [b] The second integer.
 * @return The smaller of the two integers.
 */
static inline int min(int a, int b)
{
    return (a <= b)? a : b;
}

/**
 * Determine the minimum of two unsigned integers.
 *
 * @internal
 *
 * @param [a] The first integer.
 * @param [b] The second integer.
 * @return The smaller of the two integers.
 */
static inline unsigned int minu(unsigned int a, unsigned int b)
{
    return (a <= b)? a : b;
}

/**
 * Determine the minimum of two size_t values.
 *
 * @internal
 *
 * @param [a] The first size_t value.
 * @param [b] The second size_t value.
 * @return The smaller of the two size_t values.
 */
static inline size_t minzu(size_t a, size_t b)
{
    return (a <= b)? a : b;
}

/**
 * The maximum of two integers.
 *
 * @internal
 *
 * @param [a] The first integer.
 * @param [b] The second integer.
 * @return The larger of the two integers.
 */
static inline int max(int a, int b)
{
    return (a >= b)? a : b;
}

/**
 * The maximum of two unsigned integers.
 *
 * @internal
 *
 * @param [a] The first integer.
 * @param [b] The second integer.
 * @return The larger of the two integers.
 */
static inline unsigned int maxu(unsigned int a, unsigned int b)
{
    return (a >= b)? a : b;
}

/**
 * Determine the maximum of two size_t values.
 *
 * @internal
 *
 * @param [a] The first size_t value.
 * @param [b] The second size_t value.
 * @return The larger of the two size_t values.
 */
static inline size_t maxzu(size_t a, size_t b)
{
    return (a >= b)? a : b;
}


#ifdef HAVE_ENDIAN_H
#include <endian.h>
#endif
#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif
#ifdef HAVE_LIBKERN_OSBYTEORDER_H
#include <libkern/OSByteOrder.h>
#endif


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
#    error "No htobe64 or alternative"
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
#    error "No be64toh or alternative"
#  endif
#endif


#ifndef HAVE_MEMSET_S
#define memset_s(s, smax, c, n) memset(s, c, n)
#endif


/**
 * Duplicate a region of memory.
 *
 * @internal
 *
 * @param [src] The memory area to copy from.
 * @param [n] The length of memory to duplicate (in bytes).
 * @return A pointer to the duplicated memory, or -1 on error
 *         (errno will be set).
 */
static inline void *memdup(const void *src, size_t n)
{
    void *dst = malloc(n);
    if (dst == NULL)
    {
        return NULL;
    }
    return memcpy(dst, src, n);
}


/**
 * Duplicate a string, if it's not null.
 *
 * @internal
 *
 * @param [dptr] A pointer to where the address of the duplicated string, or
 *         `NULL`, should be written.
 * @param [s] The string to duplicate, or `NULL`.
 * @return 0 on success, of -1 if an error occurs (errno will be set).
 */
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


/**
 * Replace a string pointer.
 *
 * If the existing pointer is not `NULL`, it will be passed to free(3).
 *
 * @internal
 *
 * @param [dptr] A pointer to the address of the existing string.
 * @param [s] The replacement string pointer.
 */
static inline void replace_strptr(char **dptr, char *s)
{
    if (*dptr != NULL)
    {
        free(*dptr);
    }
    *dptr = s;
}


/**
 * Replace a string pointer with a duplicate.
 *
 * If the existing pointer is not `NULL`, it will be passed to free(3).
 *
 * @internal
 *
 * @param [dptr] A pointer to the address of the existing string.
 * @param [s] The replacement string pointer, which will be duplicated unless
 *         it is `NULL`.
 * @param [n] The length of the replacement string.
 * @return 0 on success, -1 on error (errno will be set).
 */
static inline int replace_strptr_ndup(char **dptr, const char *s, size_t n)
{
    char *dup;
    if (s != NULL)
    {
        dup = strndup(s, n);
        if (dup == NULL)
        {
            return -1;
        }
    }
    else
    {
        dup = NULL;
    }
    replace_strptr(dptr, dup);
    return 0;
}


/**
 * Replace a string pointer with a duplicate.
 *
 * If the existing pointer is not `NULL`, it will be passed to free(3).
 *
 * @internal
 *
 * @param [dptr] A pointer to the address of the existing string.
 * @param [s] The replacement string pointer, which must be null terminated
 *         and will be duplicated unless it is `NULL`.
 * @return 0 on success, -1 on error (errno will be set).
 */
static inline int replace_strptr_dup(char **dptr, const char *s)
{
    char *dup;
    if (s != NULL)
    {
        dup = strdup(s);
        if (dup == NULL)
        {
            return -1;
        }
    }
    else
    {
        dup = NULL;
    }
    replace_strptr(dptr, dup);
    return 0;
}


/**
 * Allocate a new string containing the concatenation of two strings.
 *
 * The new string will be allocated using malloc(3).
 *
 * @internal
 *
 * @param [s1] The string for the start of the concatenated string.
 * @param [s2] The string for the end of the concatenated string.
 * @return The newly allocated string containing the concatenation. This must
 *         be deallocated using free(3).
 */
char *strcat_alloc(const char *s1, const char *s2);


/**
 * Locale-independent case-insensitive string comparison.
 *
 * @internal
 *
 * @param [s1] The first string to compare.
 * @param [s2] The second string to compare.
 * @return An integer greater than, equal to, or less than 0, according as the
 *         string `s1` is greater than, equal to, or less than the string `s2`.
 */
int strcasecmp_indep(const char *s1, const char *s2);


/**
 * Locale-independent case-insensitive string comparison.
 *
 * @internal
 *
 * @param [s1] The first string to compare.
 * @param [s2] The second string to compare.
 * @param [n] The maximum number of characters to compare.
 * @return An integer greater than, equal to, or less than 0, according as the
 *         string `s1` is greater than, equal to, or less than the string `s2`.
 */
int strncasecmp_indep(const char *s1, const char *s2, size_t n);


/**
 * @fn bool contains_null(void *ptrs[], int n)
 * @brief Check if an array of pointers contains any `NULL` pointer.
 *
 * @internal
 *
 * @param [ptrs] The array of pointers to check.
 * @param [n] The length of the pointer array.
 * @return `true` if the array contains any `NULL` pointer, and false if
 *         none are `NULL`.
 */
#define contains_null(ptr, n) _contains_null((void **)(ptr), (n))
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


#ifndef IOV_MAX
#  ifdef UIO_MAXIOV
#    define IOV_MAX UIO_MAXIOV
#  else
#    define IOV_MAX 1024
#  endif
#endif

#ifndef IOV_STACK_MAX
#define IOV_STACK_MAX 8
#endif

#if defined THREAD_LOCAL && !(defined NO_THREAD_LOCAL_IOV)
struct _local_iovec
{
    struct iovec iov[IOV_MAX];
};
#define ALLOC_IOVEC(name, size) \
    assert((size) <= IOV_MAX); \
    static THREAD_LOCAL struct _local_iovec _##name; \
    struct iovec *name = (_##name).iov
#define FREE_IOVEC(name) do { } while(0)
#else
#define ALLOC_IOVEC(name, size) \
    struct iovec _##name[IOV_STACK_MAX]; \
    struct iovec *name = _##name; \
    do { if ((size) > IOV_STACK_MAX) { \
        name = malloc((size) * sizeof(struct iovec)); \
    } } while (0)
#define FREE_IOVEC(name) do { if (name != _##name) { free(name); } } while(0)
#endif


/**
 * Obtain the total length of an I/O vector.
 *
 * @internal
 *
 * @param [iov] The I/O vector.
 * @param [iovcnt] The length of the vector.
 * @return The total size of all buffers in the vector.
 */
static inline size_t iovlen(const struct iovec *iov, unsigned int iovcnt)
{
    size_t total = 0;
    for (unsigned int i = 0; i < iovcnt; ++i)
    {
        total += iov[i].iov_len;
    }
    return total;
}


/**
 * Copy from an I/O vector to a buffer.
 *
 * @internal
 *
 * @param [dst] The destination buffer.
 * @param [n] The size of the destination buffer.
 * @param [iov] The vector of buffers to copy from.
 * @param [iovcnt] The length of the vector.
 * @return The number of bytes copied to the destination buffer.
 */
size_t memcpy_from_iov(void *dst, size_t n,
        const struct iovec *iov, unsigned int iovcnt);

/**
 * Copy from a buffer to an I/O vector.
 *
 * @internal
 *
 * @param [iov] The vector of buffers to copy from.
 * @param [iovcnt] The length of the vector.
 * @param [src] The source buffer.
 * @param [n] The size of the source buffer.
 * @return The number of bytes copied to the I/O vector.
 */
size_t memcpy_to_iov(const struct iovec *iov, unsigned int iovcnt,
        const void *src, size_t n);

/**
 * Copy from an I/O vector to an I/O vector.
 *
 * @internal
 *
 * @param [diov] The destination I/O vector.
 * @param [diovcnt] The size of the destination I/O vector.
 * @param [siov] The source I/O vector.
 * @param [siovcnt] The size of the source I/O vector.
 * @return The number of bytes copied into the destination vector.
 */
size_t memcpy_from_iov_to_iov(const struct iovec *diov,
        unsigned int diovcnt, const struct iovec *siov, unsigned int siovcnt);

/**
 * Copy an I/O vector, skipping a given number of preceeding bytes.
 *
 * The source and destination vector may refer to the same memory, in
 * which case the modification is done in place.
 *
 * @internal
 *
 * @param [diov] The destination I/O vector.
 * @param [siov] The source I/O vector.
 * @param [iovcnt] The size of the vectors.
 * @param [nbyte] The number of bytes to skip.
 * @return The size of the output vector.
 */
unsigned int iov_skip(struct iovec *diov, const struct iovec *siov,
        unsigned int iovcnt, size_t nbyte);

/**
 * Copy an I/O vector, limiting to a given number of bytes.
 *
 * @internal
 *
 * @param [diov] The destination I/O vector.
 * @param [siov] The source I/O vector.
 * @param [iovcnt] The size of the vectors.
 * @param [nbyte] The number of bytes to limit to.
 * @return The size of the output vector.
 */
unsigned int iov_limit(struct iovec *diov, const struct iovec *siov,
        unsigned int siovcnt, size_t nbyte);


/**
 * Span the complement of a set of characters.
 *
 * Span the initial part of a memory region, as long as the characters from
 * `reject` do no occur. In other words, it returns the distance into the
 * memory region of the first character in `reject`, else the total length
 * of the memory region.
 *
 * @internal
 *
 * @param [s] The memory region to span.
 * @param [n] The size of the memory region.
 * @param [reject] An array of characters to reject.
 * @param [rlen] The number of characters in the reject array.
 * @return The offset of the first character in `reject`, or `n`.
 */
size_t memcspn(const void *s, size_t n, const unsigned char *reject,
        size_t rlen);

/**
 * Span identifier characters.
 *
 * Equivalent to `memcspn`, with reject containing characters that are not
 * valid in an identifier [a-zA-Z0-9_].
 *
 * @internal
 *
 * @param [s] The memory region to span.
 * @param [n] The size of the memory region.
 * @return The offset of the first non-identifier character, or `n`.
 */
size_t memspn_ident(const void *s, size_t n);


#ifndef MAXSERVNAMELEN
#  ifdef NI_MAXSERV
#    define MAXSERVNAMELEN NI_MAXSERV
#  else
#    define MAXSERVNAMELEN 32
#  endif
#endif

#ifndef MAXHOSTNAMELEN
#  ifdef NI_MAXHOST
#    define MAXHOSTNAMELEN NI_MAXHOST
#  else
#    define MAXHOSTNAMELEN 1025
#  endif
#endif

#define NEO4J_MAXHOSTLEN (MAXHOSTNAMELEN + 1 + MAXSERVNAMELEN)


/**
 * Check if a hostname matches a pattern.
 *
 * The pattern may be a complete DNS name, or may contain a wildcard
 * (as described in https://tools.ietf.org/html/rfc6125#section-6.4.3).
 *
 * @internal
 *
 * @param [hostname] The hostname to check.
 * @param [pattern] The pattern to check against.
 * @return `true` if the hostname matches the pattern, and `false` otherwise.
 */
bool hostname_matches(const char *hostname, const char *pattern);


/**
 * Describe a hostname and port.
 *
 * @internal
 *
 * @param [buf] The buffer to write the description into.
 * @param [cap] The capacity of the buffer.
 * @param [hostname] The hostname.
 * @param [port] The port.
 * @return 0 on success, of -1 if an error occurs (errno will be set).
 */
int describe_host(char *buf, size_t cap, const char *hostname,
        unsigned int port);


#endif/*NEO4J_UTIL_H*/
