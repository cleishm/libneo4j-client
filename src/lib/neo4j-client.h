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
/**
 * @file neo4j-client.h
 */
#ifndef NEO4J_CLIENT_H
#define NEO4J_CLIENT_H

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma GCC visibility push(default)


/**
 * Configuration for neo4j client.
 */
typedef struct neo4j_config neo4j_config_t;

/**
 * A connection to a neo4j server.
 */
typedef struct neo4j_connection neo4j_connection_t;

/**
 * A session within a connection.
 */
typedef struct neo4j_session neo4j_session_t;

/**
 * A stream of results from a job.
 */
typedef struct neo4j_result_stream neo4j_result_stream_t;

/**
 * A result from a job.
 */
typedef struct neo4j_result neo4j_result_t;

/**
 * A neo4j value.
 */
typedef struct neo4j_value neo4j_value_t;

/**
 * A neo4j value type.
 */
typedef uint8_t neo4j_type_t;

/**
 * Function type for callback when passwords are required.
 *
 * Should copy the password into the supplied buffer, and return the
 * actual length of the password.
 *
 * @param [userdata] The user data for the callback.
 * @param [buf] The buffer to copy the password into.
 * @param [len] The length of the buffer.
 * @return The length of the password as copied into the buffer.
 */
typedef size_t (*neo4j_password_callback_t)(void *userdata,
        char *buf, size_t len);


/*
 * =====================================
 * version
 * =====================================
 */

/**
 * The version string for libneo4j-client.
 */
const char *libneo4j_client_version(void);

/**
 * The default client ID string for libneo4j-client.
 */
const char *libneo4j_client_id(void);


/*
 * =====================================
 * init
 * =====================================
 */

/**
 * Initialize the neo4j client library.
 *
 * This function should be invoked once per application including the neo4j
 * client library.
 *
 * NOTE: when compiled with GCC, this method is automatically invoked when
 * the shared library is loaded.
 *
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_client_init(void);


/**
 * Cleanup after use of the neo4j client library.
 *
 * Whilst it is not necessary to call this function, it can be useful
 * for clearing any allocated memory when testing with tools such as valgrind.
 *
 * NOTE: when compiled with GCC, this method is automatically invoked when
 * the shared library is unloaded.
 *
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_client_cleanup(void);


/*
 * =====================================
 * logging
 * =====================================
 */

#define NEO4J_LOG_ERROR 0
#define NEO4J_LOG_WARN 1
#define NEO4J_LOG_INFO 2
#define NEO4J_LOG_DEBUG 3
#define NEO4J_LOG_TRACE 4

/**
 * A logger for neo4j client.
 */
struct neo4j_logger
{
    /**
     * Retain a reference to this logger.
     *
     * @param [self] This logger.
     * @return This logger.
     */
    struct neo4j_logger *(*retain)(struct neo4j_logger *self);
    /**
     * Release a reference to this logger.
     *
     * If all references have been released, the logger will be deallocated.
     *
     * @param [self] This logger.
     */
    void (*release)(struct neo4j_logger *self);
    /**
     * Write an entry to the log.
     *
     * @param [self] This logger.
     * @param [level] The log level for the entry.
     * @param [format] The printf-style message format.
     * @param [ap] The list of arguments for the format.
     */
    void (*log)(struct neo4j_logger *self, uint_fast8_t level,
        const char *format, va_list ap);
    /**
     * Determine if a logging level is enabled for this logger.
     *
     * @param [self] This logger.
     * @param [level] The level to check.
     * @return `true` if the level is enabled and `false` otherwise.
     */
    bool (*is_enabled)(struct neo4j_logger *self, uint_fast8_t level);
    /**
     * Change the logging level for this logger.
     *
     * @param [self] This logger.
     * @param [level] The level to set.
     */
    void (*set_level)(struct neo4j_logger *self, uint_fast8_t level);
};

/**
 * A provider for a neo4j logger.
 */
struct neo4j_logger_provider
{
    /**
     * Get a new logger for the provided name.
     *
     * @param [self] This provider.
     * @param [name] The name for the new logger.
     * @return A `neo4j_logger`, or `NULL` on error (errno will be set).
     */
    struct neo4j_logger *(*get_logger)(struct neo4j_logger_provider *self,
            const char *name);
};

#define NEO4J_STD_LOGGER_NO_PREFIX (1<<0)

/**
 * Obtain a standard logger provider.
 *
 * The logger will output to the provided `FILE`.
 *
 * A bitmask of flags may be supplied, which may include:
 * - NEO4J_STD_LOGGER_NO_PREFIX - don't output a prefix on each logline
 *
 * @param [stream] The stream to output to.
 * @param [level] The default level to log at.
 * @param [flags] A bitmask of flags for the standard logger output.
 * @return A `neo4j_logger_provider`, or `NULL` on error (errno will be set).
 */
struct neo4j_logger_provider *neo4j_std_logger_provider(FILE *stream,
        uint_fast8_t level, uint_fast32_t flags);

/**
 * Free a standard logger provider.
 *
 * Provider must have been obtained via `neo4j_std_logger_provider(...)`.
 *
 * @param [provider] The provider to free.
 */
void neo4j_std_logger_provider_free(struct neo4j_logger_provider *provider);

/**
 * The name for the logging level.
 *
 * @param [level] The logging level.
 * @return A `NULL` terminated ASCII string describing the logging level.
 */
const char *neo4j_log_level_str(uint_fast8_t level);


/*
 * =====================================
 * I/O
 * =====================================
 */

/**
 * An I/O stream for neo4j client.
 */
struct neo4j_iostream
{
    /**
     * Read bytes from a stream into the supplied buffer.
     *
     * @param [self] This stream.
     * @param [buf] A pointer to a memory buffer to read into.
     * @param [nbyte] The size of the memory buffer.
     * @return The bytes read, or -1 on error (errno will be set).
     */
    ssize_t (*read)(struct neo4j_iostream *self,
            void *buf, size_t nbyte);
    /**
     * Read bytes from a stream into the supplied I/O vector.
     *
     * @param [self] This stream.
     * @param [iov] A pointer to the I/O vector.
     * @param [iovcnt] The length of the I/O vector.
     * @return The bytes read, or -1 on error (errno will be set).
     */
    ssize_t (*readv)(struct neo4j_iostream *self,
            const struct iovec *iov, int iovcnt);

    /**
     * Write bytes to a stream from the supplied buffer.
     *
     * @param [self] This stream.
     * @param [buf] A pointer to a memory buffer to read from.
     * @param [nbyte] The size of the memory buffer.
     * @return The bytes written, or -1 on error (errno will be set).
     */
    ssize_t (*write)(struct neo4j_iostream *self,
            const void *buf, size_t nbyte);
    /**
     * Write bytes to a stream ifrom the supplied I/O vector.
     *
     * @param [self] This stream.
     * @param [iov] A pointer to the I/O vector.
     * @param [iovcnt] The length of the I/O vector.
     * @return The bytes written, or -1 on error (errno will be set).
     */
    ssize_t (*writev)(struct neo4j_iostream *self,
            const struct iovec *iov, int iovcnt);

    /**
     * Close the stream.
     *
     * This function should close the stream and deallocate memory associated
     * with it.
     *
     * @param [self] This stream.
     * @return 0 on success, or -1 if an error occurs (errno will be set).
     */
    int (*close)(struct neo4j_iostream *self);
};

/**
 * A factory for establishing communications with neo4j.
 */
struct neo4j_connection_factory
{
    /**
     * Establish a TCP connection.
     *
     * @param [self] This factory.
     * @param [hostname] The hostname to connect to.
     * @param [port] The TCP port number to connect to.
     * @param [config] The client configuration.
     * @param [flags] A bitmask of flags to control connections.
     * @param [logger] A logger that may be used for status logging.
     * @return A new neo4j_iostream, or `NULL` if an error occurs
     *         (errno will be set).
     */
    struct neo4j_iostream *(*tcp_connect)(struct neo4j_connection_factory *self,
            const char *hostname, unsigned int port,
            neo4j_config_t *config, uint_fast32_t flags,
            struct neo4j_logger *logger);
};


/*
 * =====================================
 * error handling
 * =====================================
 */

#define NEO4J_UNEXPECTED_ERROR -10
#define NEO4J_INVALID_URI -11
#define NEO4J_UNKNOWN_URI_SCHEME -12
#define NEO4J_UNKNOWN_HOST -13
#define NEO4J_PROTOCOL_NEGOTIATION_FAILED -14
#define NEO4J_INVALID_CREDENTIALS -15
#define NEO4J_CONNECTION_CLOSED -16
#define NEO4J_TOO_MANY_SESSIONS -17
#define NEO4J_SESSION_ACTIVE -18
#define NEO4J_SESSION_FAILED -19
#define NEO4J_SESSION_ENDED -20
#define NEO4J_UNCLOSED_RESULT_STREAM -21
#define NEO4J_STATEMENT_EVALUATION_FAILED -22
#define NEO4J_STATEMENT_PREVIOUS_FAILURE -23
#define NEO4J_TLS_NOT_SUPPORTED -24
#define NEO4J_TLS_VERIFICATION_FAILED -25
#define NEO4J_INVALID_MAP_KEY_TYPE -26
#define NEO4J_INVALID_LABEL_TYPE -27
#define NEO4J_INVALID_PATH_NODE_TYPE -28
#define NEO4J_INVALID_PATH_RELATIONSHIP_TYPE -29
#define NEO4J_INVALID_PATH_SEQUENCE_LENGTH -30
#define NEO4J_INVALID_PATH_SEQUENCE_IDX_TYPE -31
#define NEO4J_INVALID_PATH_SEQUENCE_IDX_RANGE -32

/**
 * Look up the error message corresponding to an error number.
 *
 * @param [errnum] The error number.
 * @param [buf] A character buffer that may be used to hold the message.
 * @param [buflen] The length of the provided buffer.
 * @return A pointer to a character string containing the error message.
 */
const char *neo4j_strerror(int errnum, char *buf, size_t buflen);


/*
 * =====================================
 * memory
 * =====================================
 */

/**
 * A memory allocator for neo4j client.
 *
 * This will be used to allocate regions of memory as required by
 * a session, for buffers, etc.
 */
struct neo4j_memory_allocator
{
    /**
     * Allocate memory from this allocator.
     *
     * @param [self] This allocator.
     * @param [context] An opaque 'context' for the allocation, which an
     *         allocator may use to try an optimize storage as memory allocated
     *         with the same context is likely (but not guaranteed) to be all
     *         deallocated at the same time. Context may be `NULL`, in which
     *         case it does not offer any guidance on deallocation.
     * @param [size] The amount of memory (in bytes) to allocate.
     * @return A pointer to the allocated memory, or `NULL` if an error occurs
     *         (errno will be set).
     */
    void *(*alloc)(struct neo4j_memory_allocator *self, void *context,
            size_t size);
    /**
     * Allocate memory for consecutive objects from this allocator.
     *
     * Allocates contiguous space for multiple objects of the specified size,
     * and fills the space with bytes of value zero.
     *
     * @param [self] This allocator.
     * @param [context] An opaque 'context' for the allocation, which an
     *         allocator may use to try an optimize storage as memory allocated
     *         with the same context is likely (but not guaranteed) to be all
     *         deallocated at the same time. Context may be `NULL`, in which
     *         case it does not offer any guidance on deallocation.
     * @param [count] The number of objects to allocate.
     * @param [size] The size (in bytes) of each object.
     * @return A pointer to the allocated memory, or `NULL` if an error occurs
     *         (errno will be set).
     */
    void *(*calloc)(struct neo4j_memory_allocator *self, void *context,
            size_t count, size_t size);
    /**
     * Return memory to this allocator.
     *
     * @param [self] This allocator.
     * @param [ptr] A pointer to the memory being returned.
     */
    void (*free)(struct neo4j_memory_allocator *self, void *ptr);
    /**
     * Return multiple memory regions to this allocator.
     *
     * @param [self] This allocator.
     * @param [ptrs] An array of pointers to memory for returning.
     * @param [n] The length of the pointer array.
     */
    void (*vfree)(struct neo4j_memory_allocator *self,
            void **ptrs, size_t n);
};


/*
 * =====================================
 * values
 * =====================================
 */

/** The neo4j null value type. */
extern const neo4j_type_t NEO4J_NULL;
/** The neo4j boolean value type. */
extern const neo4j_type_t NEO4J_BOOL;
/** The neo4j integer value type. */
extern const neo4j_type_t NEO4J_INT;
/** The neo4j float value type. */
extern const neo4j_type_t NEO4J_FLOAT;
/** The neo4j string value type. */
extern const neo4j_type_t NEO4J_STRING;
/** The neo4j list value type. */
extern const neo4j_type_t NEO4J_LIST;
/** The neo4j map value type. */
extern const neo4j_type_t NEO4J_MAP;
/** The neo4j node value type. */
extern const neo4j_type_t NEO4J_NODE;
/** The neo4j relationship value type. */
extern const neo4j_type_t NEO4J_RELATIONSHIP;
/** The neo4j path value type. */
extern const neo4j_type_t NEO4J_PATH;
extern const neo4j_type_t NEO4J_STRUCT;

union _neo4j_value_data
{
    uint64_t _int;
    uintptr_t _ptr;
    double _dbl;
};

struct neo4j_value
{
    uint8_t _vt_off;
    uint8_t _type; // TODO: combine with _vt_off? (both always have same value)
    uint16_t _pad1;
    uint32_t _pad2;
    union _neo4j_value_data _vdata;
};


/**
 * An entry in a neo4j map.
 */
typedef struct neo4j_map_entry neo4j_map_entry_t;
struct neo4j_map_entry
{
    neo4j_value_t key;
    neo4j_value_t value;
};


/**
 * @fn neo4j_type_t neo4j_type(neo4j_value_t value)
 * @brief Get the type of a neo4j value.
 *
 * @param [value] The neo4j value.
 * @return The type of the value.
 */
#define neo4j_type(v) ((v)._type)

/**
 * Get a string description of the neo4j type.
 *
 * @param [t] The neo4j type.
 * @return A pointer to a `NULL` terminated string containing the type name.
 */
const char *neo4j_type_str(const neo4j_type_t t);

/**
 * Get a string representation of a neo4j value.
 *
 * Writes as much of the representation as possible into the buffer,
 * ensuring it is always `NULL` terminated.
 *
 * @param [value] The neo4j value.
 * @param [strbuf] A buffer to write the string representation into.
 * @param [n] The length of the buffer.
 * @return A pointer to the provided buffer.
 */
char *neo4j_tostring(neo4j_value_t value, char *strbuf, size_t n);

/**
 * Get a string representation of a neo4j value.
 *
 * Writes as much of the representation as possible into the buffer,
 * ensuring it is always `NULL` terminated.
 *
 * @param [value] The neo4j value.
 * @param [strbuf] A buffer to write the string representation into.
 * @param [n] The length of the buffer.
 * @return The number of bytes that would have been written into the buffer
 *         had the buffer been large enough.
 */
size_t neo4j_ntostring(neo4j_value_t value, char *strbuf, size_t n);

/**
 * Compare two neo4j values for equality.
 *
 * @param [value1] The first neo4j value.
 * @param [value2] The second neo4j value.
 * @return `true` if the two values are equivalent, `false` otherwise.
 */
bool neo4j_eq(neo4j_value_t value1, neo4j_value_t value2);

/**
 * @fn bool neo4j_is_null(neo4j_value_t value);
 * @brief Check if a neo4j value is the null value.
 *
 * @param [value] The neo4j value.
 * @return `true` if the value is the null value.
 */
#define neo4j_is_null(v) (neo4j_type(v) == NEO4J_NULL)


/**
 * The neo4j null value.
 */
extern const neo4j_value_t neo4j_null;


/**
 * Construct a neo4j value encoding a boolean.
 *
 * @param [value] A boolean value.
 * @return A neo4j value encoding the boolean.
 */
neo4j_value_t neo4j_bool(bool value);

/**
 * Return the native boolean value from a neo4j boolean.
 *
 * Note that the result is undefined if the value is not of type NEO4J_BOOL.
 *
 * @param [value] The neo4j value
 * @return The native boolean true or false
 */
bool neo4j_bool_value(neo4j_value_t value);


/**
 * Construct a neo4j value encoding an integer.
 *
 * @param [value] A signed integer. This must be in the range INT64_MIN to
 *         INT64_MAX, or it will be capped to the closest value.
 * @return A neo4j value encoding the integer.
 */
neo4j_value_t neo4j_int(long long value);

/**
 * Return the native integer value from a neo4j int.
 *
 * Note that the result is undefined if the value is not of type NEO4J_INT.
 *
 * @param [value] The neo4j value
 * @return The native integer value
 */
long long neo4j_int_value(neo4j_value_t value);


/**
 * Construct a neo4j value encoding a double.
 *
 * @param [value] A double precision floating point value.
 * @return A neo4j value encoding the double.
 */
neo4j_value_t neo4j_float(double value);

/**
 * Return the native double value from a neo4j float.
 *
 * Note that the result is undefined if the value is not of type NEO4J_FLOAT.
 *
 * @param [value] The neo4j value
 * @return The native double value
 */
double neo4j_float_value(neo4j_value_t value);


/**
 * @fn neo4j_value_t neo4j_string(const char *s)
 * @brief Construct a neo4j value encoding a string.
 *
 * @param [s] A pointer to a `NULL` terminated ASCII string. The pointer
 *         must remain valid, and the content unchanged, for the lifetime of
 *         the neo4j value.
 * @return A neo4j value encoding the string.
 */
#define neo4j_string(s) (neo4j_ustring((s), strlen(s)))

/**
 * Construct a neo4j value encoding a string.
 *
 * @param [u] A pointer to a UTF-8 string. The pointer must remain valid, and
 *         the content unchanged, for the lifetime of the neo4j value.
 * @param [n] The length of the UTF-8 string. This must be less than
 *         UINT32_MAX in length (and will be truncated otherwise).
 * @return A neo4j value encoding the string.
 */
neo4j_value_t neo4j_ustring(const char *u, unsigned int n);

/**
 * Return the length of a neo4j UTF-8 string.
 *
 * Note that the result is undefined if the value is not of type NEO4J_STRING.
 *
 * @param [value] The neo4j string.
 * @return The length of the string in bytes.
 */
unsigned int neo4j_string_length(neo4j_value_t value);

/**
 * Return a pointer to a UTF-8 string.
 *
 * The pointer will be to a UTF-8 string, and will NOT be `NULL` terminated.
 * The length of the string, in bytes, can be obtained using
 * neo4j_ustring_length(value).
 *
 * Note that the result is undefined if the value is not of type NEO4J_STRING.
 *
 * @param [value] The neo4j string.
 * @return A pointer to a UTF-8 string, which will not be terminated.
 */
const char *neo4j_ustring_value(neo4j_value_t value);

/**
 * Copy a neo4j string to a `NULL` terminated buffer.
 *
 * As much of the string will be copied to the buffer as possible, and
 * the result will be `NULL` terminated.
 *
 * Note that the result is undefined if the value is not of type NEO4J_STRING.
 *
 * @param [value] The neo4j string.
 * @param [buffer] A pointer to a buffer for storing the string. The pointer
 *         must remain valid, and the content unchanged, for the lifetime of
 *         the neo4j value.
 * @param [length] The length of the buffer.
 * @return A pointer to the supplied buffer.
 */
char *neo4j_string_value(neo4j_value_t value, char *buffer, size_t length);


/**
 * Construct a neo4j value encoding a list.
 *
 * @param [items] An array of neo4j values. The pointer to the items must
 *         remain valid, and the content unchanged, for the lifetime of the
 *         neo4j value.
 * @param [n] The length of the array of items. This must be less than
 *         UINT32_MAX (or the list will be truncated).
 * @return A neo4j value encoding the list.
 */
neo4j_value_t neo4j_list(const neo4j_value_t *items, unsigned int n);

/**
 * Return the length of a neo4j list (number of entries).
 *
 * Note that the result is undefined if the value is not of type NEO4J_LIST.
 *
 * @param [value] The neo4j list.
 * @return The number of entries.
 */
unsigned int neo4j_list_length(neo4j_value_t value);

/**
 * Return an element from a neo4j list.
 *
 * Note that the result is undefined if the value is not of type NEO4J_LIST.
 *
 * @param [value] The neo4j list.
 * @param [index] The index of the element to return.
 * @return A pointer to a `neo4j_value_t` element, or `NULL` if the index is
 *         beyond the end of the list.
 */
neo4j_value_t neo4j_list_get(neo4j_value_t value, unsigned int index);


/**
 * Construct a neo4j value encoding a map.
 *
 * @param [entries] An array of neo4j map entries. This pointer must remain
 *         valid, and the content unchanged, for the lifetime of the neo4j
 *         value.
 * @param [n] The length of the array of entries. This must be less than
 *         UINT32_MAX (or the list of entries will be truncated).
 * @return A neo4j value encoding the map.
 */
neo4j_value_t neo4j_map(const neo4j_map_entry_t *entries, unsigned int n);

/**
 * Return the size of a neo4j map (number of entries).
 *
 * Note that the result is undefined if the value is not of type NEO4J_MAP.
 *
 * @param [value] The neo4j map.
 * @return The number of entries.
 */
unsigned int neo4j_map_size(neo4j_value_t value);

/**
 * Return an entry from a neo4j map.
 *
 * Note that the result is undefined if the value is not of type NEO4J_MAP.
 *
 * @param [value] The neo4j map.
 * @param [index] The index of the entry to return.
 * @return The entry at the specified index, or `NULL` if the index is too large.
 */
const neo4j_map_entry_t *neo4j_map_getentry(neo4j_value_t value,
        unsigned int index);

/**
 * Return a value from a neo4j map.
 *
 * Note that the result is undefined if the value is not of type NEO4J_MAP.
 *
 * @param [value] The neo4j map.
 * @param [key] The map key.
 * @return The value stored under the specified key, or `NULL` if the key is
 *         not known.
 */
neo4j_value_t neo4j_map_get(neo4j_value_t value, neo4j_value_t key);

/**
 * Constrct a neo4j map entry.
 *
 * @param [key] The key for the entry.
 * @param [value] The value for the entry.
 * @return A neo4j map entry.
 */
neo4j_map_entry_t neo4j_map_entry(neo4j_value_t key, neo4j_value_t value);


/**
 * Return the label list from a neo4j node.
 *
 * Note that the result is undefined if the value is not of type NEO4J_NODE.
 *
 * @param [value] The neo4j node.
 * @return A neo4j value encoding the list of labels.
 */
neo4j_value_t neo4j_node_labels(neo4j_value_t value);

/**
 * Return the property map from a neo4j node.
 *
 * Note that the result is undefined if the value is not of type NEO4J_NODE.
 *
 * @param [value] The neo4j node.
 * @return A neo4j value encoding the map of properties.
 */
neo4j_value_t neo4j_node_properties(neo4j_value_t value);


/**
 * Return the type from a neo4j relationship.
 *
 * Note that the result is undefined if the value is not of type
 * NEO4J_RELATIONSHIP.
 *
 * @param [value] The neo4j node.
 * @return A neo4j value encoding the type as a String.
 */
neo4j_value_t neo4j_relationship_type(neo4j_value_t value);

/**
 * Return the property map from a neo4j relationship.
 *
 * Note that the result is undefined if the value is not of type
 * NEO4J_RELATIONSHIP.
 *
 * @param [value] The neo4j relationship.
 * @return A neo4j value encoding the map of properties.
 */
neo4j_value_t neo4j_relationship_properties(neo4j_value_t value);

/**
 * Return the length of a neo4j path.
 *
 * The length of a path is defined by the number of relationships included in
 * it.
 *
 * Note that the result is undefined if the value is not of type NEO4J_PATH.
 *
 * @param [value] The neo4j path.
 * @return The length of the path
 */
unsigned int neo4j_path_length(neo4j_value_t value);

/**
 * Return the node at a given distance into the path.
 *
 * Note that the result is undefined if the value is not of type NEO4J_PATH.
 *
 * @param [value] The neo4j path.
 * @param [hops] The number of hops (distance).
 * @return A neo4j value enconding the node.
 */
neo4j_value_t neo4j_path_get_node(neo4j_value_t value, unsigned int hops);

/**
 * Return the relationship for the given hop in the path.
 *
 * Note that the result is undefined if the value is not of type NEO4J_PATH.
 *
 * @param [value] The neo4j path.
 * @param [hops] The number of hops (distance).
 * @param [forward] `NULL`, or a pointer to a boolean which will be set to
 *         `true` if the relationship was traversed in its natural direction
 *         and `false` if it was traversed backward.
 * @return A neo4j value enconding the relationship.
 */
neo4j_value_t neo4j_path_get_relationship(neo4j_value_t value,
        unsigned int hops, bool *forward);


/*
 * =====================================
 * config
 * =====================================
 */

/**
 * Generate a new neo4j client configuration.
 *
 * The lifecycle of the neo4j client configuration is managed by
 * reference counting.
 *
 * @return A pointer to a new neo4j client configuration, or `NULL` if an error
 *         occurs (errno will be set).
 */
neo4j_config_t *neo4j_new_config(void);

/**
 * Release a reference to the neo4j client configuration.
 *
 * @param [config] A pointer to a neo4j client configuration. This pointer will
 *         be invalid after the function returns.
 * */
void neo4j_config_free(neo4j_config_t *config);

/**
 * Set the client ID.
 *
 * The client ID will be used when identifying the client to neo4j.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [client_id] The client ID string. This string should remain allocated
 *         whilst the config is allocated _or if any connections opened with
 *         the config remain active_.
 */
void neo4j_config_set_client_id(neo4j_config_t *config, const char *client_id);

/**
 * Set a logger provider in the neo4j client configuration.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [logger_provider] The logger provider function.
 */
void neo4j_config_set_logger_provider(neo4j_config_t *config,
        struct neo4j_logger_provider *logger_provider);

/**
 * Set a connection factory in the neo4j client configuration.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [factory] The connection factory.
 */
void neo4j_config_set_connection_factory(neo4j_config_t *config,
        struct neo4j_connection_factory *factory);

/**
 * The standard connection factory.
 */
extern struct neo4j_connection_factory neo4j_std_connection_factory;

/*
 * The standard memory allocator.
 *
 * This memory allocator delegates to the system malloc/free functions.
 */
extern struct neo4j_memory_allocator neo4j_std_memory_allocator;

/**
 * Set a memory allocator in the neo4j client configuration.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [allocator] The memory allocator.
 */
void neo4j_config_set_memory_allocator(neo4j_config_t *config,
        struct neo4j_memory_allocator *allocator);

/**
 * Set the username in the neo4j client configuration.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [username] The username to authenticate with. The string will be
 *         duplicated, and thus may point to temporary memory.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_config_set_username(neo4j_config_t *config, const char *username);

/**
 * Set the password in the neo4j client configuration.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [password] The password to authenticate with. The string will be
 *         duplicated, and thus may point to temporary memory.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_config_set_password(neo4j_config_t *config, const char *password);

/**
 * Set the location of a TLS private key and certificate chain.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [path] The path to the PEM file containing the private key
 *         and certificate chain. This string should remain allocated whilst
 *         the config is allocated _or if any connections opened with the
 *         config remain active_.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_config_set_TLS_private_key(neo4j_config_t *config,
        const char *path);

/**
 * Set the password callback for the TLS private key file.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [callback] The callback to be invoked whenever a password for
 *         the certificate file is required.
 * @param [userdata] User data that will be supplied to the callback.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_config_set_TLS_private_key_password_callback(neo4j_config_t *config,
        neo4j_password_callback_t callback, void *userdata);

/**
 * Set the password for the TLS private key file.
 *
 * This is a simpler alternative to using
 * `neo4j_config_set_TLS_private_key_password_callback`.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [password] The password for the certificate file. This string should
 *         remain allocated whilst the config is allocated _or if any
 *         connections opened with the config remain active_.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_config_set_TLS_private_key_password(neo4j_config_t *config,
        const char *password);

/**
 * Set the location of a TLS certificate authority file.
 *
 * The file, in PEM format, should contain any needed root certificates that
 * may be needed to authenticate that returned by a peer.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [path] The path to the PEM file containing the root certificates.
 *         This string should remain allocated whilst the config is allocated
 *         _or if any connections opened with the config remain active_.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_config_set_TLS_ca_file(neo4j_config_t *config, const char *path);

/**
 * Set the location of a directory of TLS certificates.
 *
 * The specified directory should contain certificate files named by hash
 * according to the `c_rehash` tool.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [path] The path to the directory of certificates. This string should
 *         remain allocated whilst the config is allocated _or if any
 *         connections opened with the config remain active_.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_config_set_TLS_ca_dir(neo4j_config_t *config, const char *path);

/**
 * Enable or disable trusting of known hosts.
 *
 * When enabled, the neo4j client will check if a host has been previously
 * trusted and stored into the "known hosts" file, and that the host
 * fingerprint still matches the previously accepted value. This is enabled by
 * default.
 *
 * If verification fails, the callback set with
 * `neo4j_config_set_unverified_host_callback` will be invoked.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [enable] `true` to enable trusting of known hosts, and `false` to
 *         disable this behaviour.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_config_set_trust_known_hosts(neo4j_config_t *config, bool enable);

/**
 * Set the location of the known hosts file for TLS certificates.
 *
 * The file, which will be created and maintained by neo4j client,
 * will be used for storing trust information when using "Trust On First Use".
 *
 * @param [config] The neo4j client configuration to update.
 * @param [path] The path to known hosts file. This string should
 *         remain allocated whilst the config is allocated _or if any
 *         connections opened with the config remain active_.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_config_set_known_hosts_file(neo4j_config_t *config,
        const char *path);

typedef enum
{
    NEO4J_HOST_VERIFICATION_UNRECOGNIZED,
    NEO4J_HOST_VERIFICATION_MISMATCH
} neo4j_unverified_host_reason_t;

#define NEO4J_HOST_VERIFICATION_REJECT 0
#define NEO4J_HOST_VERIFICATION_ACCEPT_ONCE 1
#define NEO4J_HOST_VERIFICATION_TRUST 2

/**
 * Function type for callback when host verification has failed.
 *
 * @param [userdata] The user data for the callback.
 * @param [host] The host description (typically "<hostname>:<port>").
 * @param [fingerprint] The fingerprint for the host.
 * @param [reason] The reason for the verification failure, which will be
 *         either `NEO4J_HOST_VERIFICATION_UNRECOGNIZED` or
 *         `NEO4J_HOST_VERIFICATION_MISMATCH`.
 * @return `NEO4J_HOST_VERIFICATION_REJECT` if the host should be rejected,
 *         `NEO4J_HOST_VERIFICATION_ACCEPT_ONCE` if the host should be accepted
 *         for just the one connection, `NEO4J_HOST_VERIFICATION_TRUST` if the
 *         fingerprint should be stored in the "known hosts" file and thus
 *         trusted for future connections, or -1 if an error occurs (errno
 *         should be set).
 */
typedef int (*neo4j_unverified_host_callback_t)(void *userdata,
        const char *host, const char *fingerprint,
        neo4j_unverified_host_reason_t reason);

/**
 * Set the unverified host callback.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [callback] The callback to be invoked whenever a host verification
 *         fails.
 * @param [userdata] User data that will be supplied to the callback.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_config_set_unverified_host_callback(neo4j_config_t *config,
        neo4j_unverified_host_callback_t callback, void *userdata);


/**
 * Return a path within the neo4j dot directory.
 *
 * The neo4j dot directory is typically ".neo4j" within the users home
 * directory. If append is `NULL`, then an absoulte path to the home
 * directory is placed into buffer.
 *
 * @param [buffer] The buffer in which to place the path, which will be
 *         null terminated. If the buffer is `NULL`, then the function
 *         will still return the length of the path it would have placed
 *         into the buffer.
 * @param [n] The size of the buffer. If the path is too large to place
 *         into the buffer (including the terminating '\0' character),
 *         an `ERANGE` error will result.
 * @param [append] The relative path to append to the dot directory, which
 *         may be `NULL`.
 * @return The length of the resulting path (not including the null
 *         terminating character), or -1 if an error occurs (errno will be set).
 */
ssize_t neo4j_dot_dir(char *buffer, size_t n, const char *append);

/**
 * Obtain the parent directory of a specified path.
 *
 * Any trailing '/' characters are not counted as part of the directory name.
 * If `path` is `NULL`, the empty string, or contains no '/' characters, the
 * path "." is placed into the result buffer.
 *
 * @param [path] The path.
 * @param [buffer] A buffer to place the parent directory path into, or `NULL`.
 * @param [n] The length of the buffer.
 * @return The length of the parent directory path, or -1 if an error
 *         occurs (errno will be set).
 */
ssize_t neo4j_dirname(const char *path, char *buffer, size_t n);

/**
 * Obtain the basename of a specified path.
 *
 * @param [path] The path.
 * @param [buffer] A buffer to place the base name into, or `NULL`.
 * @param [n] The length of the buffer.
 * @return The length of the base name, or -1 if an error occurs (errno will be
 *         set).
 */
ssize_t neo4j_basename(const char *path, char *buffer, size_t n);

/**
 * Create a directory and any required parent directories.
 *
 * Directories are created with default permissions as per the users umask.
 *
 * @param [path] The path of the directory to create.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_mkdir_p(const char *path);


/*
 * =====================================
 * connection
 * =====================================
 */

#define NEO4J_DEFAULT_TCP_PORT 7687

#define NEO4J_INSECURE (1<<0)

/**
 * Establish a connection to a neo4j server.
 *
 * A bitmask of flags may be supplied, which may include:
 * - NEO4J_INSECURE - do not attempt to establish a secure connection
 *
 * @param [uri] A URI describing the server to connect to, which may also
 *         include authentication data (which will override any provided
 *         in the config).
 * @param [config] The neo4j client configuration to use for this connection.
 * @param [flags] A bitmask of flags to control connections.
 * @return A pointer to a `neo4j_connection_t` structure, or `NULL` on error
 *         (errno will be set).
 */
neo4j_connection_t *neo4j_connect(const char *uri, neo4j_config_t *config,
        uint_fast32_t flags);

/**
 * Establish a connection to a neo4j server.
 *
 * @param [hostname] The hostname to connect to.
 * @param [port] The port to connect to.
 * @param [config] The neo4j client configuration to use for this connection.
 * @param [flags] A bitmask of flags to control connections.
 * @return A pointer to a `neo4j_connection_t` structure, or `NULL` on error
 *         (errno will be set).
 */
neo4j_connection_t *neo4j_tcp_connect(const char *hostname, unsigned int port,
        neo4j_config_t *config, uint_fast32_t flags);

/**
 * Close a connection to a neo4j server.
 *
 * @param [connection] The connection to close. This pointer will be invalid
 *         after the function returns, except when an error occurs and errno is
 *         set to NEO4J_SESSION_ACTIVE.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_close(neo4j_connection_t *connection);


/*
 * =====================================
 * session
 * =====================================
 */

/**
 * Create a new session for the given connection
 *
 * @param [connection] The connection over which to establish the session.
 * @return A pointer to a `neo4j_session_t` structure, or `NULL` on error
 *         (errno will be set).
 * */
neo4j_session_t *neo4j_new_session(neo4j_connection_t *connection);

/**
 * End a session with a neo4j server.
 *
 * @param [session] The session to end. The pointer will be invalid after the
 *         function returns.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_end_session(neo4j_session_t *session);


/*
 * =====================================
 * job
 * =====================================
 */

/**
 * Evaluate a statement.
 *
 * @param [session] The session to evaluate the statement in.
 * @param [statement] The statement to be evaluated.
 * @param [params] The parameters for the statement.
 * @param [n] The number of parameters.
 * @return A `neo4j_result_stream_t`, or `NULL` if an error occurs (errno
 *         will be set).
 */
neo4j_result_stream_t *neo4j_run(neo4j_session_t *session,
        const char *statement, const neo4j_map_entry_t *params, unsigned int n);

/**
 * Evaluate a statement, ignoring any results.
 *
 * The `neo4j_result_stream_t` returned from this function will not
 * provide any results. It can be used to check for evaluation errors using
 * `neo4j_check_failure`.
 *
 * @param [session] The session to evaluate the statement in.
 * @param [statement] The statement to be evaluated.
 * @param [params] The parameters for the statement.
 * @param [n] The number of parameters.
 * @return A `neo4j_result_stream_t`, or `NULL` if an error occurs (errno
 *         will be set).
 */
neo4j_result_stream_t *neo4j_send(neo4j_session_t *session,
        const char *statement, const neo4j_map_entry_t *params, unsigned int n);


/*
 * =====================================
 * result stream
 * =====================================
 */

/**
 * Check if a results stream has failed.
 *
 * Note: if the error is `NEO4J_STATEMENT_EVALUATION_FAILED`, then additional
 * error information will be available via `neo4j_error_message(...)`.
 *
 * @param [results] The result stream.
 * @return 0 if no failure has occurred, and an error number otherwise.
 */
int neo4j_check_failure(neo4j_result_stream_t *results);

/**
 * Get the number of fields in a result stream.
 *
 * @param [results] The result stream.
 * @return The number of fields in the result, or -1 if an error occurs
 *         (errno will be set).
 */
unsigned int neo4j_nfields(neo4j_result_stream_t *results);

/**
 * Get the name of a field in a result stream.
 *
 * @param [results] The result stream.
 * @param [index] The field index to get the name of.
 * @return The name of the field, as a NULL terminated string,
 *         or NULL if an error occurs (errno will be set).
 */
const char *neo4j_fieldname(neo4j_result_stream_t *results,
        unsigned int index);

/**
 * Fetch the next record from the result stream.
 *
 * @param [results] The result stream.
 * @return The next result, or NULL if the stream is exahusted or an
 *         error has occurred (errno will be set).
 */
neo4j_result_t *neo4j_fetch_next(neo4j_result_stream_t *results);

/**
 * Close a result stream.
 *
 * Closes the result stream and releases all memory held by it, including
 * results and values obtained from it.
 *
 * NOTE: After this function is invoked, all `neo4j_result_t` objects fetched
 * from this stream, and any values obtained from them, will be invalid and
 * _must not be accessed_. Doing so will result in undetermined and unstable
 * behaviour. This is true even if this function returns an error.
 *
 * @param [results] The result stream. The pointer will be invalid after the
 *         function returns.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_close_results(neo4j_result_stream_t *results);


/*
 * =====================================
 * result metadata
 * =====================================
 */

/**
 * Return the error code sent from neo4j.
 *
 * When `neo4j_check_failure` returns `NEO4J_STATEMENT_EVALUATION_FAILED`,
 * then this function can be used to get the error code sent from neo4j.
 *
 * @param [results] The result stream.
 * @return A `NULL` terminated string reprenting the error code, or NULL
 *         if the stream has not failed or the failure was not
 *         `NEO4J_STATEMENT_EVALUATION_FAILED`.
 */
const char *neo4j_error_code(neo4j_result_stream_t *results);

/**
 * Return the error message sent from neo4j.
 *
 * When `neo4j_check_failure` returns `NEO4J_STATEMENT_EVALUATION_FAILED`,
 * then this function can be used to get the detailed error message sent
 * from neo4j.
 *
 * @param [results] The result stream.
 * @return A `NULL` terminated string containing the error message, or NULL
 *         if the stream has not failed or the failure was not
 *         `NEO4J_STATEMENT_EVALUATION_FAILED`.
 */
const char *neo4j_error_message(neo4j_result_stream_t *results);

/**
 * Update counts.
 *
 * These are a count of all the updates that occurred as a result of
 * the statement sent to neo4j.
 */
struct neo4j_update_counts
{
    /** Nodes created. */
    unsigned long long nodes_created;
    /** Nodes deleted. */
    unsigned long long nodes_deleted;
    /** Relationships created. */
    unsigned long long relationships_created;
    /** Relationships deleted. */
    unsigned long long relationships_deleted;
    /** Properties set. */
    unsigned long long properties_set;
    /** Labels added. */
    unsigned long long labels_added;
    /** Labels removed. */
    unsigned long long labels_removed;
    /** Indexes added. */
    unsigned long long indexes_added;
    /** Indexes removed. */
    unsigned long long indexes_removed;
    /** Constraints added. */
    unsigned long long constraints_added;
    /** Constraints removed. */
    unsigned long long constraints_removed;
};

/**
 * Return the update counts for the result stream.
 *
 * @attention As the update counts are only available at the end of the result
 * stream, invoking this function will will result in any unfetched results
 * being pulled from the server and held in memory. It is usually better to
 * exhaust the stream using `neo4j_fetch_next(...)` before invoking this
 * method.
 *
 * @param [results] The result stream.
 * @return The update counts. If an error has occurred, all the counts will be
 *         zero.
 */
struct neo4j_update_counts neo4j_update_counts(neo4j_result_stream_t *results);


/*
 * =====================================
 * result
 * =====================================
 */

/**
 * Get a field from a result.
 *
 * @param [result] A result.
 * @param [index] The field index to get.
 * @return The field from the result, or `neo4j_null` if index is out of bounds.
 */
neo4j_value_t neo4j_result_field(const neo4j_result_t *result,
        unsigned int index);

/**
 * Retain a result.
 *
 * This retains the result and all values contained within it, preventing
 * them from being deallocated on the next call to `neo4j_fetch_next(...)`
 * or when the result stream is closed via `neo4j_close_results(...)`. Once
 * retained, the result _must_ later be explicitly released via
 * `neo4j_release(...)`.
 *
 * @param [result] A result.
 * @return The result.
 */
neo4j_result_t *neo4j_retain(neo4j_result_t *result);

/**
 * Release a result.
 *
 * @param [result] A previously retained result.
 */
void neo4j_release(neo4j_result_t *result);


/*
 * =====================================
 * render results
 * =====================================
 */

#define NEO4J_RENDER_MAX_WIDTH 4096

#define NEO4J_RENDER_SHOW_NULLS (1<<0)
#define NEO4J_RENDER_QUOTE_STRINGS (1<<1)

/**
 * Render a result stream as a table.
 *
 * Flags can be specified, as a bitmask, to control rendering. This rendering
 * method respects the flags `NEO4J_RENDER_SHOW_NULL` and
 * `NEO4J_RENDER_QUOTE_STRINGS`.
 *
 * @param [stream] The stream to render to.
 * @param [results] The results stream to render.
 * @param [width] The width of the table to render.
 * @param [flags] A bitmask of flags to control rendering.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_render_table(FILE *stream, neo4j_result_stream_t *results,
        unsigned int width, uint_fast32_t flags);

/**
 * Render a result stream as comma separated value.
 *
 * Flags can be specified, as a bitmask, to control rendering. This rendering
 * method respects the flag `NEO4J_RENDER_SHOW_NULL`.
 *
 * @param [stream] The stream to render to.
 * @param [results] The results stream to render.
 * @param [flags] A bitmask of flags to control rendering.
 * @return 0 on success, or -1 if an error occurs (errno will be set).
 */
int neo4j_render_csv(FILE *stream, neo4j_result_stream_t *results,
        uint_fast32_t flags);


/*
 * =====================================
 * command line interface
 * =====================================
 */

/**
 * @fn ssize_t neo4j_cli_parse(const char *s, const char **start, size_t *length, bool *complete);
 * @brief Parse a command or statement from a string.
 *
 * @param [s] The `NULL` terminated string to parse.
 * @param [start] Either `NULL`, or a pointer that will be set to the start of
 *         the command or statement within `s`.
 * @param [length] Either `NULL` or a pointer to a `size_t` that will be set to
 *         the length of the command or statement within `s`.
 * @param [complete] A pointer to a boolean that will be set to `true` if
 *         the parsed command or statement was read completely, or `false`
 *         otherwise.
 * @return The number of bytes consumed from the input string, 0 if no
 *         command or statement was found, and -1 if an error occurs
 *         (errno will be set).
 */
#define neo4j_cli_parse(s,b,l,c) (neo4j_cli_uparse((s),strlen(s),(b),(l),(c)))

/**
 * Parse a command or statement from a string.
 *
 * @param [s] The string to parse.
 * @param [n] The size of the string.
 * @param [start] Either `NULL`, or a pointer that will be set to the start of
 *         the command or statement within `s`.
 * @param [length] Either `NULL` or a pointer to a `size_t` that will be set to
 *         the length of the command or statement within `s`.
 * @param [complete] A pointer to a boolean that will be set to `true` if
 *         the parsed command or statement was read completely, or `false`
 *         otherwise.
 * @return The number of bytes consumed from the input string, 0 if no
 *         command or statement was found, and -1 if an error occurs
 *         (errno will be set).
 */
ssize_t neo4j_cli_uparse(const char *s, size_t n,
        const char **start, size_t *length, bool *complete);

/**
 * Parse a command or statement from a `FILE *` stream.
 *
 * @param [stream] The stream to parse
 * @param [buf] A pointer to a `const char *`, that must either be NULL or
 *         point to a malloced buffer. The buffer will be modified as needed
 *         by this function, as if via `realloc()`.
 * @param [bufcap] A pointer to a `size_t` that specifies the capacity of the
 *         buffer supplied via `*buf`. The value will be updated if the buffer
 *         is reallocated.
 * @param [start] Either `NULL`, or a pointer that will be set to the start of
 *         the command or statement within `buf`.
 * @param [length] Either `NULL` or a pointer to a `size_t` that will be set to
 *         the length of the command or statement within `buf`.
 * @param [complete] A pointer to a boolean that will be set to `true` if
 *         the parsed command or statement was read completely, or `false`
 *         otherwise.
 * @return The number of bytes consumed from the stream, 0 if no
 *         command or statement was found, and -1 if an error occurs
 *         (errno will be set).
 */
ssize_t neo4j_cli_fparse(FILE *stream,
        char ** restrict buf, size_t * restrict bufcap,
        char ** restrict start, size_t * restrict length, bool *complete);


/**
 * @fn ssize_t neo4j_cli_arg_parse(const char *s, const char **start, size_t *length, bool *complete);
 * @brief Parse an argument from a string.
 *
 * Parses a single argument from a string, which may be quoted.
 *
 * @param [s] The `NULL` terminated string to parse.
 * @param [start] Either `NULL`, or a pointer that will be set to the start of
 *         the argument within `s`.
 * @param [length] Either `NULL` or a pointer to a `size_t` that will be set to
 *         the length of the argument within `s`.
 * @param [complete] A pointer to a boolean that will be set to `true` if
 *         the parsed argument was read completely, or `false` otherwise.
 * @return The number of bytes consumed from the input string, 0 if no
 *         command or statement was found, and -1 if an error occurs
 *         (errno will be set).
 */
#define neo4j_cli_arg_parse(s,b,l,c) \
    (neo4j_cli_arg_uparse((s),strlen(s),(b),(l),(c)))

/**
 * Parse an argument from a string.
 *
 * Parses a single argument from a string, which may be quoted.
 *
 * @param [s] The string to parse.
 * @param [n] The size of the string.
 * @param [start] Either `NULL`, or a pointer that will be set to the start of
 *         the argument within `s`.
 * @param [length] Either `NULL` or a pointer to a `size_t` that will be set to
 *         the length of the argument within `s`.
 * @param [complete] A pointer to a boolean that will be set to `true` if
 *         the parsed argument was read completely, or `false` otherwise.
 * @return The number of bytes consumed from the input string, 0 if no
 *         command or statement was found, and -1 if an error occurs
 *         (errno will be set).
 */
ssize_t neo4j_cli_arg_uparse(const char *s, size_t n,
        const char **start, size_t *length, bool *complete);


#pragma GCC visibility pop

#ifdef __cplusplus
}
#endif

#endif/*NEO4J_CLIENT_H*/
