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
#include "network.h"
#include "logging.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

static void init_getaddrinfo_hints(struct addrinfo *hints);
static int unsupported_sock_error(int err);
static void set_socket_options(int fd, const neo4j_config_t *config,
        neo4j_logger_t *logger);
static int connect_with_timeout(int fd, const struct sockaddr *address,
        socklen_t address_len, time_t timeout, neo4j_logger_t *logger);
static int update_socket_flags(int fd, int flags_to_set, int flags_to_clear,
        neo4j_logger_t *logger);


int neo4j_connect_tcp_socket(const char *hostname, const char *servname,
        const neo4j_config_t *config, neo4j_logger_t *logger)
{
    REQUIRE(hostname != NULL, -1);

    struct addrinfo hints;
    struct addrinfo *candidate_addresses = NULL;
    int err = 0;

    init_getaddrinfo_hints(&hints);
    err = getaddrinfo(hostname, servname, &hints, &candidate_addresses);
    if (err)
    {
        errno = NEO4J_UNKNOWN_HOST;
        return -1;
    }

    int fd = -1;
    struct addrinfo *addr;
    for (addr = candidate_addresses; addr != NULL; addr = addr->ai_next)
    {
        fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (fd < 0)
        {
            if (!unsupported_sock_error(errno))
                continue;
            neo4j_log_error_errno(logger, "socket");
            freeaddrinfo(candidate_addresses);
            return -1;
        }

        set_socket_options(fd, config, logger);

        char hostnum[NI_MAXHOST];
        char servnum[NI_MAXSERV];
        err = getnameinfo(addr->ai_addr, addr->ai_addrlen,
                hostnum, sizeof(hostnum), servnum, sizeof(servnum),
                NI_NUMERICHOST | NI_NUMERICSERV);
        if (err)
        {
            neo4j_log_error(logger, "getnameinfo: %s", gai_strerror(err));
            freeaddrinfo(candidate_addresses);
            errno = NEO4J_UNEXPECTED_ERROR;
            return -1;
        }

        neo4j_log_debug(logger, "attempting connection to %s [%s]",
                hostnum, servnum);

        err = connect_with_timeout(fd, addr->ai_addr, addr->ai_addrlen,
                    config->connect_timeout, logger);
        if (err == 0)
        {
            break;
        }
        else if (err < 0)
        {
            return -1;
        }

        char ebuf[256];
        neo4j_log_info(logger, "connection to %s [%s] failed: %s",
                hostnum, servnum, neo4j_strerror(errno, ebuf, sizeof(ebuf)));

        close(fd);
        fd = -1;
    }

    freeaddrinfo(candidate_addresses);
    return fd;
}


void init_getaddrinfo_hints(struct addrinfo *hints)
{
    memset(hints, 0, sizeof(struct addrinfo));
    hints->ai_family = PF_UNSPEC;
    hints->ai_socktype = SOCK_STREAM;
    hints->ai_protocol = IPPROTO_TCP;
#ifdef HAVE_AI_ADDRCONFIG
    /* make calls to getaddrinfo send AAAA queries only if at least one
     * IPv6 interface is configured */
    hints->ai_flags |= AI_ADDRCONFIG;
#endif
}


inline int unsupported_sock_error(int err)
{
    return (err == EPFNOSUPPORT ||
            err == EAFNOSUPPORT ||
            err == EPROTONOSUPPORT ||
            err == ESOCKTNOSUPPORT ||
            err == ENOPROTOOPT);
}


void set_socket_options(int fd, const neo4j_config_t *config,
        neo4j_logger_t *logger)
{
    int option = 1;

#ifdef HAVE_SO_NOSIGPIPE
    if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &option, sizeof(int)))
    {
        neo4j_log_warn_errno(logger, "setsockopt");
        // continue
    }
#endif

    if ((option = config->so_sndbuf_size) > 0)
    {
        assert(option > 0);
        if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &option, sizeof(int)))
        {
            neo4j_log_warn_errno(logger, "setsockopt");
            // continue
        }
    }

    if ((option = config->so_rcvbuf_size) > 0)
    {
        assert(option > 0);
        if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &option, sizeof(int)))
        {
            neo4j_log_warn_errno(logger, "setsockopt");
            // continue
        }
    }
}


int connect_with_timeout(int fd, const struct sockaddr *address,
        socklen_t address_len, time_t timeout, neo4j_logger_t *logger)
{
    struct timeval tv, *tvp = NULL;

    if (timeout > 0)
    {
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        tvp = &tv;
    }

    if (update_socket_flags(fd, O_NONBLOCK, 0, logger))
    {
        return -1;
    }

    int err = connect(fd, address, address_len);

    if (err)
    {
        if (errno != EINPROGRESS)
        {
            return 1;
        }

        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(fd, &fdset);

        do
        {
            err = select(fd + 1, NULL, &fdset, NULL, tvp);
        } while (err < 0 && errno == EINTR);

        if (err < 0)
        {
            neo4j_log_error_errno(logger, "select");
            return -1;
        }

        if (err == 0)
        {
            errno = ETIMEDOUT;
            return 1;
        }

        int option_value;
        socklen_t option_len = sizeof(option_value);
        err = getsockopt(fd, SOL_SOCKET, SO_ERROR, &option_value, &option_len);
        if (err)
        {
            neo4j_log_error_errno(logger, "getsockopt");
            return -1;
        }

        if (option_value != 0)
        {
            errno = option_value;
            return 1;
        }
    }

    if (update_socket_flags(fd, 0, O_NONBLOCK, logger))
    {
        return -1;
    }

    return 0;
}


int update_socket_flags(int fd, int flags_to_set, int flags_to_clear,
        neo4j_logger_t *logger)
{
    int arg;
    if ((arg = fcntl(fd, F_GETFL, 0)) < 0)
    {
        neo4j_log_error_errno(logger, "fcntl");
        return -1;
    }

    arg |= flags_to_set;
    arg &= flags_to_clear;

    if (fcntl(fd, F_SETFL, arg) < 0)
    {
        neo4j_log_error_errno(logger, "fcntl");
        return -1;
    }

    return 0;
}
