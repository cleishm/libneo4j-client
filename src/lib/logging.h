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
#ifndef NEO4J_LOGGING_H
#define NEO4J_LOGGING_H

#include "neo4j-client.h"
#include "client_config.h"

#if __GNUC__ > 3
#define __neo4j_format(string_index, first) \
        __attribute__((format (printf, string_index, first)))
#else
#define __neo4j_format(string_index, first) /*format*/
#endif

typedef struct neo4j_logger neo4j_logger_t;

/**
 * Get a logger for the specified name.
 *
 * @internal
 *
 * @param [config] The client configuration.
 * @param [logname] The name for the logger.
 * @return A logger for the specified name.
 */
static inline neo4j_logger_t *neo4j_get_logger(const neo4j_config_t *config,
        const char *logname)
{
    if (config->logger_provider == NULL)
    {
        return NULL;
    }

    return config->logger_provider->get_logger(
            config->logger_provider, logname);
}

/**
 * Retain a reference for a logger.
 *
 * @internal
 *
 * @param [logger] The logger that will be held.
 * @return The same logger instance.
 */
static inline neo4j_logger_t *neo4j_logger_retain(neo4j_logger_t *logger)
{
    if (logger == NULL)
    {
        return NULL;
    }
    return logger->retain(logger);
}

/**
 * Release a reference to a logger.
 *
 * @internal
 *
 * @param [logger] The logger to be released. May be deallocated if all
 *         references have been released.
 */
static inline void neo4j_logger_release(neo4j_logger_t *logger)
{
    if (logger == NULL)
    {
        return;
    }
    logger->release(logger);
}


/**
 * Check if the logger is enabled for the specified level
 *
 * @internal
 *
 * @param [logger] The logger.
 * @param [level] The level to check.
 * @return `true` if enabled, `false` otherwise.
 */
static inline bool neo4j_log_is_enabled(neo4j_logger_t *logger,
        uint_fast8_t level)
{
    if (logger == NULL)
    {
        return false;
    }
    return logger->is_enabled(logger, level);
}

/**
 * Write an entry to a logger.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [level] The level to log at.
 * @param [format] The printf-style format for the log message.
 * @param [ap] A vector of arguments to the format.
 */
static inline void neo4j_vlog(neo4j_logger_t *logger, uint_fast8_t level,
        const char *format, va_list ap)
{
    if (logger == NULL)
    {
        return;
    }
    logger->log(logger, level, format, ap);
}

/**
 * Write an entry to a logger.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [level] The level to log at.
 * @param [format] The printf-style format for the log message.
 * @param [ap] A vector of arguments to the format.
 */
static inline void neo4j_log(neo4j_logger_t *logger, uint_fast8_t level,
        const char *format, ...) __neo4j_format(3, 4);
static inline void neo4j_log(neo4j_logger_t *logger, uint_fast8_t level,
        const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    neo4j_vlog(logger, level, format, ap);
    va_end(ap);
}

/**
 * Write a log entry containing an error message string.
 *
 * Writes the message with the format `"some message: errno message"`.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [level] The level to log at.
 * @param [message] The message for the start of the log line.
 */
void neo4j_log_errno(neo4j_logger_t *logger, uint_fast8_t level,
        const char *message);


/**
 * Write a trace entry to a logger.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [format] The printf-style format for the log message.
 * @param [ap] A vector of arguments to the format.
 */
static inline void neo4j_vlog_trace(neo4j_logger_t *logger,
        const char *format, va_list ap)
{
    neo4j_vlog(logger, NEO4J_LOG_TRACE, format, ap);
}

/**
 * Write a trace entry to a logger.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [format] The printf-style format for the log message.
 * @param [...] Arguments to the format.
 */
static inline void neo4j_log_trace(neo4j_logger_t *logger,
        const char *format, ...) __neo4j_format(2, 3);
static inline void neo4j_log_trace(neo4j_logger_t *logger,
        const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    neo4j_vlog_trace(logger, format, ap);
    va_end(ap);
}

/**
 * Write a trace entry containing an error message string.
 *
 * Writes the message with the format `"some message: errno message"`.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [message] The message for the start of the log line.
 */
static inline void neo4j_log_trace_errno(neo4j_logger_t *logger,
        const char *message)
{
    neo4j_log_errno(logger, NEO4J_LOG_TRACE, message);
}


/**
 * Write a debug entry to a logger.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [format] The printf-style format for the log message.
 * @param [ap] A vector of arguments to the format.
 */
static inline void neo4j_vlog_debug(neo4j_logger_t *logger,
        const char *format, va_list ap)
{
    neo4j_vlog(logger, NEO4J_LOG_DEBUG, format, ap);
}

/**
 * Write a debug entry to a logger.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [format] The printf-style format for the log message.
 * @param [...] Arguments to the format.
 */
static inline void neo4j_log_debug(neo4j_logger_t *logger,
        const char *format, ...) __neo4j_format(2, 3);
static inline void neo4j_log_debug(neo4j_logger_t *logger,
        const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    neo4j_vlog_debug(logger, format, ap);
    va_end(ap);
}

/**
 * Write a debug entry containing an error message string.
 *
 * Writes the message with the format `"some message: errno message"`.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [message] The message for the start of the log line.
 */
static inline void neo4j_log_debug_errno(neo4j_logger_t *logger,
        const char *message)
{
    neo4j_log_errno(logger, NEO4J_LOG_DEBUG, message);
}


/**
 * Write an info entry to a logger.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [format] The printf-style format for the log message.
 * @param [ap] A vector of arguments to the format.
 */
static inline void neo4j_vlog_info(neo4j_logger_t *logger,
        const char *format, va_list ap)
{
    neo4j_vlog(logger, NEO4J_LOG_INFO, format, ap);
}

/**
 * Write an info entry to a logger.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [format] The printf-style format for the log message.
 * @param [...] Arguments to the format.
 */
static inline void neo4j_log_info(neo4j_logger_t *logger,
        const char *format, ...) __neo4j_format(2, 3);
static inline void neo4j_log_info(neo4j_logger_t *logger,
        const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    neo4j_vlog_info(logger, format, ap);
    va_end(ap);
}

/**
 * Write an info entry containing an error message string.
 *
 * Writes the message with the format `"some message: errno message"`.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [message] The message for the start of the log line.
 */
static inline void neo4j_log_info_errno(neo4j_logger_t *logger,
        const char *message)
{
    neo4j_log_errno(logger, NEO4J_LOG_INFO, message);
}


/**
 * Write a warn entry to a logger.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [format] The printf-style format for the log message.
 * @param [ap] A vector of arguments to the format.
 */
static inline void neo4j_vlog_warn(neo4j_logger_t *logger,
        const char *format, va_list ap)
{
    neo4j_vlog(logger, NEO4J_LOG_WARN, format, ap);
}

/**
 * Write a warn entry to a logger.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [format] The printf-style format for the log message.
 * @param [...] Arguments to the format.
 */
static inline void neo4j_log_warn(neo4j_logger_t *logger,
        const char *format, ...) __neo4j_format(2, 3);
static inline void neo4j_log_warn(neo4j_logger_t *logger,
        const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    neo4j_vlog_warn(logger, format, ap);
    va_end(ap);
}

/**
 * Write a warn entry containing an error message string.
 *
 * Writes the message with the format `"some message: errno message"`.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [message] The message for the start of the log line.
 */
static inline void neo4j_log_warn_errno(neo4j_logger_t *logger,
        const char *message)
{
    neo4j_log_errno(logger, NEO4J_LOG_WARN, message);
}


/**
 * Write an error entry to a logger.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [format] The printf-style format for the log message.
 * @param [ap] A vector of arguments to the format.
 */
static inline void neo4j_vlog_error(neo4j_logger_t *logger,
        const char *format, va_list ap)
{
    neo4j_vlog(logger, NEO4J_LOG_ERROR, format, ap);
}

/**
 * Write an error entry to a logger.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [format] The printf-style format for the log message.
 * @param [...] Arguments to the format.
 */
static inline void neo4j_log_error(neo4j_logger_t *logger,
        const char *format, ...) __neo4j_format(2, 3);
static inline void neo4j_log_error(neo4j_logger_t *logger,
        const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    neo4j_vlog_error(logger, format, ap);
    va_end(ap);
}

/**
 * Write an error entry containing an error message string.
 *
 * Writes the message with the format `"some message: errno message"`.
 *
 * @internal
 *
 * @param [logger] The logger to write to.
 * @param [message] The message for the start of the log line.
 */
static inline void neo4j_log_error_errno(neo4j_logger_t *logger,
        const char *message)
{
    neo4j_log_errno(logger, NEO4J_LOG_ERROR, message);
}

#endif/*NEO4J_LOGGING_H*/
