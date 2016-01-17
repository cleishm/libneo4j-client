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
#include "logging.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>


void neo4j_log_errno(neo4j_logger_t *logger, uint_fast8_t level,
        const char *message)
{
    char ebuf[256];
    neo4j_log(logger, level, "%s: %s", message,
            neo4j_strerror(errno, ebuf, sizeof(ebuf)));
}


static struct neo4j_logger *std_provider_get_logger(
        struct neo4j_logger_provider *self, const char *name);
static struct neo4j_std_logger *find_std_logger(
        struct neo4j_std_logger *logger_list, const char *name);
static struct neo4j_std_logger *new_std_logger(FILE *stream, uint_fast8_t level,
        uint_fast32_t flags, const char *name);
static struct neo4j_logger *std_logger_retain(struct neo4j_logger *self);
static void std_logger_release(struct neo4j_logger *self);
static void std_logger_list_add(struct neo4j_std_logger *head,
        struct neo4j_std_logger *logger);
static void std_logger_list_remove(struct neo4j_std_logger *logger);
static void std_logger_log(struct neo4j_logger *self, uint_fast8_t level,
        const char *format, va_list ap);
static bool std_logger_is_enabled(struct neo4j_logger *self,
        uint_fast8_t level);
static void std_logger_set_level(struct neo4j_logger *self,
        uint_fast8_t level);


struct neo4j_std_logger
{
    struct neo4j_logger _logger;
    unsigned int refcount;
    FILE *stream;
    volatile uint_fast8_t level;
    uint_fast32_t flags;
    char *name;
    struct neo4j_std_logger *prev;
    struct neo4j_std_logger *next;
};


struct neo4j_std_logger_provider
{
    struct neo4j_logger_provider _provider;
    FILE *stream;
    uint_fast8_t level;
    uint_fast32_t flags;
    struct neo4j_std_logger loggers;
};


struct neo4j_logger_provider *neo4j_std_logger_provider(FILE *stream,
        uint_fast8_t level, uint_fast32_t flags)
{
    struct neo4j_std_logger_provider *std_provider = calloc(1,
            sizeof(struct neo4j_std_logger_provider));
    if (std_provider == NULL)
    {
        return NULL;
    }

    std_provider->stream = stream;
    std_provider->level = level;
    std_provider->flags = flags;

    struct neo4j_logger_provider *provider = &(std_provider->_provider);
    provider->get_logger = std_provider_get_logger;
    return provider;
}


void neo4j_std_logger_provider_free(struct neo4j_logger_provider *provider)
{
    struct neo4j_std_logger_provider *p = container_of(provider,
        struct neo4j_std_logger_provider, _provider);
    assert(p->loggers.prev == NULL);
    if (p->loggers.next != NULL)
    {
        p->loggers.next->prev = NULL;
    }
    p->stream = NULL;
    free(p);
}


struct neo4j_logger *std_provider_get_logger(struct neo4j_logger_provider *self,
        const char *name)
{
    struct neo4j_std_logger_provider *p = container_of(self,
        struct neo4j_std_logger_provider, _provider);

    struct neo4j_std_logger *stdlogger = find_std_logger(p->loggers.next, name);
    if (stdlogger == NULL)
    {
        stdlogger = new_std_logger(p->stream, p->level, p->flags, name);
        if (stdlogger == NULL)
        {
            return NULL;
        }
        std_logger_list_add(&(p->loggers), stdlogger);
    }

    return &(stdlogger->_logger);
}


struct neo4j_std_logger *find_std_logger(struct neo4j_std_logger *logger_list,
        const char *name)
{
    struct neo4j_std_logger *logger = logger_list;
    while (logger != NULL)
    {
        assert(logger->name != NULL);
        if (strcmp(logger->name, name) == 0)
        {
            ++(logger->refcount);
            return logger;
        }
        logger = logger->next;
    }
    return NULL;
}


struct neo4j_std_logger *new_std_logger(FILE *stream, uint_fast8_t level,
        uint_fast32_t flags, const char *name)
{
    struct neo4j_std_logger *stdlogger = calloc(1,
            sizeof(struct neo4j_std_logger));
    if (stdlogger == NULL)
    {
        return NULL;
    }

    neo4j_logger_t *logger = &(stdlogger->_logger);
    logger->retain = std_logger_retain;
    logger->release = std_logger_release;
    logger->log = std_logger_log;
    logger->is_enabled = std_logger_is_enabled;
    logger->set_level = std_logger_set_level;

    stdlogger->refcount = 1;
    stdlogger->stream = stream;
    stdlogger->level = level;
    stdlogger->flags = flags;
    stdlogger->name = strdup(name);
    if (stdlogger->name == NULL)
    {
        int errsv = errno;
        free(stdlogger);
        errno = errsv;
        return NULL;
    }
    return stdlogger;
}


struct neo4j_logger *std_logger_retain(struct neo4j_logger *self)
{
    struct neo4j_std_logger *logger = container_of(self,
            struct neo4j_std_logger, _logger);
    ++(logger->refcount);
    return self;
}


void std_logger_release(struct neo4j_logger *self)
{
    struct neo4j_std_logger *logger = container_of(self,
            struct neo4j_std_logger, _logger);
    if (--(logger->refcount) == 0)
    {
        std_logger_list_remove(logger);
        free(logger->name);
        free(logger);
    }
}


void std_logger_list_add(struct neo4j_std_logger *head,
        struct neo4j_std_logger *logger)
{
    logger->next = head->next;
    logger->prev = head;
    if (head->next != NULL)
    {
        head->next->prev = logger;
    }
    head->next = logger;
}


void std_logger_list_remove(struct neo4j_std_logger *logger)
{
    if (logger->prev != NULL)
    {
        logger->prev->next = logger->next;
    }
    if (logger->next != NULL)
    {
        logger->next->prev = logger->prev;
    }
}


void std_logger_log(struct neo4j_logger *self, uint_fast8_t level,
        const char *format, va_list ap)
{
    struct neo4j_std_logger *logger = container_of(self,
            struct neo4j_std_logger, _logger);
    if (level > logger->level)
    {
        return;
    }

    const char *levelname = neo4j_log_level_str(level);

    flockfile(logger->stream);
    if ((logger->flags & NEO4J_STD_LOGGER_NO_PREFIX) == 0)
    {
        fprintf(logger->stream, "%-5s [%s]: ", levelname, logger->name);
    }
    vfprintf(logger->stream, format, ap);
    putc('\n', logger->stream);
    funlockfile(logger->stream);
}


bool std_logger_is_enabled(struct neo4j_logger *self, uint_fast8_t level)
{
    struct neo4j_std_logger *logger = container_of(self,
            struct neo4j_std_logger, _logger);
    return level <= logger->level;
}


void std_logger_set_level(struct neo4j_logger *self, uint_fast8_t level)
{
    struct neo4j_std_logger *logger = container_of(self,
            struct neo4j_std_logger, _logger);
    // atomic write but not synchronized - in a multithreaded application,
    // raising the level may not immediately stop some log entries being
    // written, which is ok
    logger->level = level;
}


const char *neo4j_log_level_str(uint_fast8_t level)
{
    switch (level)
    {
    case NEO4J_LOG_ERROR:
        return "ERROR";
    case NEO4J_LOG_WARN:
        return "WARN";
    case NEO4J_LOG_INFO:
        return "INFO";
    case NEO4J_LOG_DEBUG:
        return "DEBUG";
    default:
        return "TRACE";
    }
}
