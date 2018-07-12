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
#include "../src/logging.h"
#include <check.h>


struct log_event
{
    char name[1024];
    uint8_t level;
    char format[1024];
};

struct test_logger_provider
{
    struct neo4j_logger_provider logger_provider;
    struct log_event *event;
};

struct test_logger
{
    neo4j_logger_t logger;
    char name[1024];
    struct log_event *event;
};


static neo4j_logger_t *get_test_logger(
        struct neo4j_logger_provider *provider, const char *name);
static void record_event(neo4j_logger_t *logger, uint8_t level,
        const char *format, va_list ap);
static void release_logger(neo4j_logger_t *logger);


static neo4j_config_t *config;
static struct log_event event;
static struct neo4j_logger_provider test_logger_provider =
        { .get_logger = get_test_logger };


static void setup(void)
{
    config = neo4j_new_config();
    neo4j_config_set_logger_provider(config, &test_logger_provider);
}


static void teardown(void)
{
    neo4j_config_free(config);
}


neo4j_logger_t *get_test_logger(struct neo4j_logger_provider *provider,
         const char *name)
{
    struct test_logger *logger = (struct test_logger *)calloc(1,
            sizeof(struct test_logger));
    if (logger == NULL)
    {
        return NULL;
    }
    logger->logger.log = record_event;
    logger->logger.release = release_logger;
    logger->event = &event;

    strncpy(logger->name, name, sizeof(logger->name));
    logger->name[sizeof(logger->name) - 1] = '\0';
    return (neo4j_logger_t *)logger;
}


void record_event(neo4j_logger_t *logger, uint8_t level,
        const char *format, va_list ap)
{
    struct test_logger *tlogger = (struct test_logger *)logger;
    struct log_event *event = tlogger->event;
    strcpy(event->name, tlogger->name);
    event->level = level;
    strncpy(event->format, format, sizeof(event->format));
    event->format[sizeof(event->format) - 1] = '\0';
}


void release_logger(neo4j_logger_t *logger)
{
    free(logger);
}


START_TEST (test_logging_handles_null_logger)
{
    ck_assert(neo4j_logger_retain(NULL) == NULL);
    neo4j_logger_release(NULL);

    neo4j_log_debug(NULL, "msg");
    neo4j_log_info(NULL, "msg");
    neo4j_log_warn(NULL, "msg");
    neo4j_log_error(NULL, "msg");
}
END_TEST


START_TEST (test_logging_logs_event)
{
    neo4j_logger_t *logger = neo4j_get_logger(config, "LOGNAME");
    ck_assert(logger != NULL);

    neo4j_log_debug(logger, "a log message");
    ck_assert_str_eq(event.name, "LOGNAME");
    ck_assert_int_eq(event.level, NEO4J_LOG_DEBUG);
    ck_assert_str_eq(event.format, "a log message");

    neo4j_logger_release(logger);

    logger = neo4j_get_logger(config, "OTHER");
    ck_assert(logger != NULL);

    neo4j_log_warn(logger, "another message");
    ck_assert_str_eq(event.name, "OTHER");
    ck_assert_int_eq(event.level, NEO4J_LOG_WARN);
    ck_assert_str_eq(event.format, "another message");

    neo4j_logger_release(logger);
}
END_TEST


START_TEST (std_logger_provider_returns_same_logger_for_name)
{
    struct neo4j_logger_provider *provider =
        neo4j_std_logger_provider(stderr, NEO4J_LOG_DEBUG, 0);
    ck_assert(provider != NULL);

    neo4j_logger_t *logger1 = provider->get_logger(provider, "LOGNAME");
    neo4j_logger_t *logger2 = provider->get_logger(provider, "LOGNAME");
    ck_assert_ptr_eq(logger1, logger2);

    neo4j_logger_release(logger1);
    neo4j_logger_t *logger3 = provider->get_logger(provider, "LOGNAME");
    ck_assert_ptr_eq(logger2, logger3);

    neo4j_logger_release(logger2);

    neo4j_logger_t *logger4 = provider->get_logger(provider, "OTHER");
    ck_assert_ptr_ne(logger3, logger4);

    neo4j_logger_release(logger3);
    neo4j_logger_release(logger4);
    neo4j_std_logger_provider_free(provider);
}
END_TEST


TCase* logging_tcase(void)
{
    TCase *tc = tcase_create("logging");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_logging_handles_null_logger);
    tcase_add_test(tc, test_logging_logs_event);
    tcase_add_test(tc, std_logger_provider_returns_same_logger_for_name);
    return tc;
}
