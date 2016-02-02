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
#include "uri.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define _lowalpha "abcdefghijklmnopqrstuvwxyz"
#define _upalpha "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define _alpha _lowalpha _upalpha
#define _digit "0123456789"
#define _alphanum _alpha _digit
#define _hex _digit "abcdefABCDEF"
#define _mark "-_.!~*'()"
#define _unreserved _alphanum _mark
#define _reserved ";/?:@&=+$,"

static char alpha[] = _alpha;
static char scheme_chars[] = _alphanum "+-.";
static char userinfo_chars[] = _unreserved "%" ";:&=+$,";
static char host_chars[] = _alphanum "-.";
static char ipv6_chars[] = _hex ":.%";
static char port_chars[] = _digit;
static char path_chars[] = _unreserved "%" ":@&=+$," ";" "/";
static char query_chars[] = _unreserved _reserved "%";
static char fragment_chars[] = _unreserved _reserved "%";


static inline void maybe_set(const char **endptr, const char *val)
{
    if (endptr != NULL)
    {
        *endptr = val;
    }
}


struct uri *parse_uri(const char *str, const char **endptr)
{
    if (str == NULL)
    {
        maybe_set(endptr, NULL);
        errno = EINVAL;
        return NULL;
    }

    const char *scheme_start = str;
    if (strchr(alpha, *scheme_start) == NULL)
    {
        maybe_set(endptr, scheme_start);
        errno = EINVAL;
        return NULL;
    }
    size_t scheme_len = strspn(scheme_start, scheme_chars);

    if (*(scheme_start + scheme_len) != ':')
    {
        maybe_set(endptr, scheme_start + scheme_len);
        errno = EINVAL;
        return NULL;
    }

    const char *hier_part = scheme_start + scheme_len + 1;
    if (*hier_part != '/')
    {
        maybe_set(endptr, hier_part);
        errno = EINVAL;
        return NULL;
    }

    if (*(hier_part + 1) != '/')
    {
        maybe_set(endptr, hier_part + 1);
        errno = EINVAL;
        return NULL;
    }

    const char *userinfo_start = hier_part + 2;
    size_t userinfo_len = strspn(userinfo_start, userinfo_chars);

    const char *hostname_start;
    if (*(userinfo_start + userinfo_len) != '@')
    {
        userinfo_len = 0;
        hostname_start = userinfo_start;
    }
    else
    {
        hostname_start = userinfo_start + userinfo_len + 1;
    }

    size_t hostname_len;
    const char *port_start;
    if (*hostname_start == '[')
    {
        hostname_start++;
        hostname_len = strspn(hostname_start, ipv6_chars);
        if (*(hostname_start + hostname_len) != ']')
        {
            maybe_set(endptr, hostname_start + hostname_len);
            errno = EINVAL;
            return NULL;
        }
        port_start = hostname_start + hostname_len + 1;
    }
    else
    {
        hostname_len = strspn(hostname_start, host_chars);
        if (strchr(":/?", *(hostname_start + hostname_len)) == NULL)
        {
            maybe_set(endptr, hostname_start + hostname_len);
            errno = EINVAL;
            return NULL;
        }
        port_start = hostname_start + hostname_len;
    }

    size_t port_len;
    if (*port_start != ':')
    {
        port_len = 0;
    }
    else
    {
        port_start++;
        port_len = strspn(port_start, port_chars);
        if (strchr("/?", *(port_start + port_len)) == NULL)
        {
            maybe_set(endptr, port_start + port_len);
            errno = EINVAL;
            return NULL;
        }
    }

    const char *path_start = port_start + port_len;
    size_t path_len = strspn(path_start, path_chars);
    if (strchr("?#", *(path_start + path_len)) == NULL)
    {
        maybe_set(endptr, path_start + path_len);
        errno = EINVAL;
        return NULL;
    }

    const char *query_start = path_start + path_len;
    size_t query_len;
    if (*query_start != '?')
    {
        query_len = 0;
    }
    else
    {
        query_start++;
        query_len = strspn(query_start, query_chars);
        if (strchr("#", *(query_start + query_len)) == NULL)
        {
            maybe_set(endptr, query_start + query_len);
            errno = EINVAL;
            return NULL;
        }
    }

    const char *fragment_start = query_start + query_len;
    size_t fragment_len;
    if (*fragment_start == '\0')
    {
        fragment_len = 0;
    }
    else
    {
        fragment_start++;
        fragment_len = strspn(fragment_start, fragment_chars);
        if (*(fragment_start + fragment_len) != '\0')
        {
            maybe_set(endptr, fragment_start + fragment_len);
            errno = EINVAL;
            return NULL;
        }
    }

    maybe_set(endptr, fragment_start + fragment_len);

    struct uri *uri = calloc(1, sizeof(struct uri));
    if (uri == NULL)
    {
        return NULL;
    }

    uri->scheme = strndup(scheme_start, scheme_len);
    if (uri->scheme == NULL)
    {
        goto cleanup;
    }
    if (userinfo_len > 0)
    {
        uri->userinfo = strndup(userinfo_start, userinfo_len);
        if (uri->userinfo == NULL)
        {
            goto cleanup;
        }
    }
    if (hostname_len > 0)
    {
        uri->hostname = strndup(hostname_start, hostname_len);
        if (uri->hostname == NULL)
        {
            goto cleanup;
        }
    }
    uri->port = -1;
    if (port_len > 0)
    {
        char *eptr;
        long port = strtol(port_start, &eptr, 10);
        if (eptr != (port_start + port_len) || port > UINT16_MAX)
        {
            maybe_set(endptr, eptr);
            errno = EINVAL;
            goto cleanup;
        }
        uri->port = (int)port;
    }
    uri->path = strndup(path_start, path_len);
    if (uri->path == NULL)
    {
        goto cleanup;
    }
    if (query_len > 0)
    {
        uri->query = strndup(query_start, query_len);
        if (uri->query == NULL)
        {
            goto cleanup;
        }
    }
    if (fragment_len > 0)
    {
        uri->fragment = strndup(fragment_start, fragment_len);
        if (uri->fragment == NULL)
        {
            goto cleanup;
        }
    }

    return uri;

    int errsv;
cleanup:
    errsv = errno;
    free_uri(uri);
    errno = errsv;
    return NULL;
}


void free_uri(struct uri *uri)
{
    if (uri->fragment)
    {
        free(uri->fragment);
    }
    if (uri->query)
    {
        free(uri->query);
    }
    if (uri->path)
    {
        free(uri->path);
    }
    if (uri->hostname)
    {
        free(uri->hostname);
    }
    if (uri->userinfo)
    {
        free(uri->userinfo);
    }
    if (uri->scheme)
    {
        free(uri->scheme);
    }
    free(uri);
}
