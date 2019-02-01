/* vi:set ts=4 sw=4 expandtab:
 *
 * @configure_input@
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

#if __GNUC__ > 3
#define __neo4j_const __attribute__((const))
#define __neo4j_pure __attribute__((pure))
#define __neo4j_malloc __attribute__((malloc))
#define __neo4j_must_check __attribute__((warn_unused_result))
#define __neo4j_deprecated __attribute__((deprecated))
#else
#define __neo4j_pure /*pure*/
#define __neo4j_const /*const*/
#define __neo4j_malloc /*malloc*/
#define __neo4j_must_check /*must check*/
#define __neo4j_deprecated /*deprecated*/
#endif

#if (__STDC_VERSION__ >= 199901L)
#elif __GNUC__ >= 3
#define restrict __restrict
#elif _MSC_VER >= 1500
#define restrict __restrict
#else
#define restrict
#endif

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
 * Function type for callback when a passwords is required.
 *
 * Should copy the password into the supplied buffer, and return the
 * actual length of the password.
 *
 * @param [userdata] The user data for the callback.
 * @param [buf] The buffer to copy the password into.
 * @param [len] The length of the buffer.
 * @return The length of the password as copied into the buffer.
 */
typedef ssize_t (*neo4j_password_callback_t)(void *userdata,
        char *buf, size_t len);

/**
 * Function type for callback when username and/or password is required.
 *
 * Should update the `NULL` terminated strings in the `username` and/or
 * `password` buffers.
 *
 * @param [userdata] The user data for the callback.
 * @param [host] The host description (typically "<hostname>:<port>").
 * @param [username] A buffer of size `usize`, possibly containing a `NULL`
 *         terminated default username.
 * @param [usize] The size of the username buffer.
 * @param [password] A buffer of size `psize`, possibly containing a `NULL`
 *         terminated default password.
 * @param [psize] The size of the password buffer.
 * @return 0 on success, -1 on error (errno should be set).
 */
typedef int (*neo4j_basic_auth_callback_t)(void *userdata,
        const char *host, char *username, size_t usize,
        char *password, size_t psize);



/*
 * =====================================
 * version
 * =====================================
 */

/* Compile time version details.
 * For runtime version inspection, use libcypher_parser_version() instead.
 */
#define NEO4J_VERSION "@PACKAGE_VERSION@"
#define NEO4J_MAJOR_VERSION @PACKAGE_MAJOR_VERSION@
#define NEO4J_MINOR_VERSION @PACKAGE_MINOR_VERSION@
#define NEO4J_PATCH_VERSION @PACKAGE_PATCH_VERSION@
#define NEO4J_DEVELOPMENT_VERSION "@PACKAGE_DEVELOPMENT_VERSION@"

#define NEO4J_VERSION_NUMBER \
    ((NEO4J_MAJOR_VERSION << 20) | \
    (NEO4J_MINOR_VERSION << 12) | \
    (NEO4J_PATCH_VERSION << 4) | @PACKAGE_STATUS_VERSION@)

/**
 * The version string for libneo4j-client.
 */
__neo4j_const
const char *libneo4j_client_version(void);

/**
 * The default client ID string for libneo4j-client.
 */
__neo4j_const
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
 * @return 0 on success, or -1 on error (errno will be set).
 */
int neo4j_client_init(void);


/**
 * Cleanup after use of the neo4j client library.
 *
 * Whilst it is not necessary to call this function, it can be useful
 * for clearing any allocated memory when testing with tools such as valgrind.
 *
 * @return 0 on success, or -1 on error (errno will be set).
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

#define NEO4J_STD_LOGGER_DEFAULT 0
#define NEO4J_STD_LOGGER_NO_PREFIX (1<<0)

/**
 * Obtain a standard logger provider.
 *
 * The logger will output to the provided `FILE`.
 *
 * A bitmask of flags may be supplied, which may include:
 * - NEO4J_STD_LOGGER_NO_PREFIX - don't output a prefix on each logline
 *
 * If no flags are required, pass 0 or `NEO4J_STD_LOGGER_DEFAULT`.
 *
 * @param [stream] The stream to output to.
 * @param [level] The default level to log at.
 * @param [flags] A bitmask of flags for the standard logger output.
 * @return A `neo4j_logger_provider`, or `NULL` on error (errno will be set).
 */
__neo4j_must_check
struct neo4j_logger_provider *neo4j_std_logger_provider(FILE *stream,
        uint_fast8_t level, uint_fast32_t flags);

/**
 * Free a standard logger provider.
 *
 * Provider must have been obtained via neo4j_std_logger_provider().
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
__neo4j_const
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
            const struct iovec *iov, unsigned int iovcnt);

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
            const struct iovec *iov, unsigned int iovcnt);

    /**
     * Flush the output buffer of the iostream.
     *
     * For unbuffered streams, this is a no-op.
     *
     * @param [self] This stream.
     * @return 0 on success, or -1 on error (errno will be set).
     */
    int (*flush)(struct neo4j_iostream *self);

    /**
     * Close the stream.
     *
     * This function should close the stream and deallocate memory associated
     * with it.
     *
     * @param [self] This stream.
     * @return 0 on success, or -1 on error (errno will be set).
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
     * @return A new `neo4j_iostream`, or `NULL` on error (errno will be set).
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
#define NEO4J_SESSION_FAILED -19
#define NEO4J_SESSION_ENDED -20
#define NEO4J_UNCLOSED_RESULT_STREAM -21
#define NEO4J_STATEMENT_EVALUATION_FAILED -22
#define NEO4J_STATEMENT_PREVIOUS_FAILURE -23
#define NEO4J_TLS_NOT_SUPPORTED -24
#define NEO4J_TLS_VERIFICATION_FAILED -25
#define NEO4J_NO_SERVER_TLS_SUPPORT -26
#define NEO4J_SERVER_REQUIRES_SECURE_CONNECTION -27
#define NEO4J_INVALID_MAP_KEY_TYPE -28
#define NEO4J_INVALID_LABEL_TYPE -29
#define NEO4J_INVALID_PATH_NODE_TYPE -30
#define NEO4J_INVALID_PATH_RELATIONSHIP_TYPE -31
#define NEO4J_INVALID_PATH_SEQUENCE_LENGTH -32
#define NEO4J_INVALID_PATH_SEQUENCE_IDX_TYPE -33
#define NEO4J_INVALID_PATH_SEQUENCE_IDX_RANGE -34
#define NEO4J_NO_PLAN_AVAILABLE -35
#define NEO4J_AUTH_RATE_LIMIT -36
#define NEO4J_TLS_MALFORMED_CERTIFICATE -37
#define NEO4J_SESSION_RESET -38
#define NEO4J_SESSION_BUSY -39

/**
 * Print the error message corresponding to an error number.
 *
 * @param [stream] The stream to write to.
 * @param [errnum] The error number.
 * @param [message] `NULL`, or a pointer to a message string which will
 *         be prepend to the error message, separated by a colon and space.
 */
void neo4j_perror(FILE *stream, int errnum, const char *message);

/**
 * Look up the error message corresponding to an error number.
 *
 * @param [errnum] The error number.
 * @param [buf] A character buffer that may be used to hold the message.
 * @param [buflen] The length of the provided buffer.
 * @return A pointer to a character string containing the error message.
 */
__neo4j_must_check
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
 * a connection, for buffers, etc.
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
     * @return A pointer to the allocated memory, or `NULL` on error
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
     * @return A pointer to the allocated memory, or `NULL` on error
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
/** The neo4j bytes value type. */
extern const neo4j_type_t NEO4J_BYTES;
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
/** The neo4j identity value type. */
extern const neo4j_type_t NEO4J_IDENTITY;
extern const neo4j_type_t NEO4J_STRUCT;

union _neo4j_value_data
{
    /* These are all private fields and shouldn't be accessed directly. */
    uint64_t _int;
    uintptr_t _ptr;
    double _dbl;
};

struct neo4j_value
{
    /* These are all private fields and shouldn't be accessed directly. */
    uint8_t _vt_off;
    uint8_t _type; /*TODO: combine with _vt_off? (both always have same value)*/
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
 * Check the type of a neo4j value.
 *
 * @param [value] The neo4j value.
 * @param [type] The neo4j type.
 * @return `true` if the node is of the specified type and `false` otherwise.
 */
__neo4j_const
bool neo4j_instanceof(neo4j_value_t value, neo4j_type_t type);

/**
 * Get a string description of the neo4j type.
 *
 * @param [t] The neo4j type.
 * @return A pointer to a `NULL` terminated string containing the type name.
 */
__neo4j_const
const char *neo4j_typestr(neo4j_type_t t);

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
 * Get a UTF-8 string representation of a neo4j value.
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
 * Print a UTF-8 string representation of a neo4j value to a stream.
 *
 * @param [value] The neo4j value.
 * @param [stream] The stream to print to.
 * @return The number of bytes written to the stream, or -1 on error
 *         (errno will be set).
 */
ssize_t neo4j_fprint(neo4j_value_t value, FILE *stream);

/**
 * Compare two neo4j values for equality.
 *
 * @param [value1] The first neo4j value.
 * @param [value2] The second neo4j value.
 * @return `true` if the two values are equivalent, `false` otherwise.
 */
__neo4j_pure
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
__neo4j_const
neo4j_value_t neo4j_bool(bool value);

/**
 * Return the native boolean value from a neo4j boolean.
 *
 * Note that the result is undefined if the value is not of type NEO4J_BOOL.
 *
 * @param [value] The neo4j value.
 * @return The native boolean true or false.
 */
__neo4j_const
bool neo4j_bool_value(neo4j_value_t value);


/**
 * Construct a neo4j value encoding an integer.
 *
 * @param [value] A signed integer. This must be in the range INT64_MIN to
 *         INT64_MAX, or it will be capped to the closest value.
 * @return A neo4j value encoding the integer.
 */
__neo4j_const
neo4j_value_t neo4j_int(long long value);

/**
 * Return the native integer value from a neo4j int.
 *
 * Note that the result is undefined if the value is not of type NEO4J_INT.
 *
 * @param [value] The neo4j value.
 * @return The native integer value.
 */
__neo4j_const
long long neo4j_int_value(neo4j_value_t value);


/**
 * Construct a neo4j value encoding a double.
 *
 * @param [value] A double precision floating point value.
 * @return A neo4j value encoding the float.
 */
__neo4j_const
neo4j_value_t neo4j_float(double value);

/**
 * Return the native double value from a neo4j float.
 *
 * Note that the result is undefined if the value is not of type NEO4J_FLOAT.
 *
 * @param [value] The neo4j value.
 * @return The native double value.
 */
__neo4j_const
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
__neo4j_const
neo4j_value_t neo4j_ustring(const char *u, unsigned int n);

/**
 * Return the length of a neo4j UTF-8 string.
 *
 * Note that the result is undefined if the value is not of type NEO4J_STRING.
 *
 * @param [value] The neo4j string.
 * @return The length of the string in bytes.
 */
__neo4j_const
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
__neo4j_const
const char *neo4j_ustring_value(neo4j_value_t value);

/**
 * Copy a neo4j string to a `NULL` terminated buffer.
 *
 * As much of the string will be copied to the buffer as possible, and
 * the result will be `NULL` terminated.
 *
 * Note that the result is undefined if the value is not of type NEO4J_STRING.
 *
 * @attention The content copied to the buffer may contain UTF-8 multi-byte
 *         characters.
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
 * Construct a neo4j value encoding a byte sequence.
 *
 * @param [u] A pointer to a byte sequence. The pointer must remain valid, and
 *         the content unchanged, for the lifetime of the neo4j value.
 * @param [n] The length of the byte sequence. This must be less than
 *         UINT32_MAX in length (and will be truncated otherwise).
 * @return A neo4j value encoding the string.
 */
__neo4j_const
neo4j_value_t neo4j_bytes(const char *u, unsigned int n);

/**
 * Return the length of a neo4j byte sequence.
 *
 * Note that the result is undefined if the value is not of type NEO4J_BYTES.
 *
 * @param [value] The neo4j byte sequence.
 * @return The length of the sequence.
 */
__neo4j_const
unsigned int neo4j_bytes_length(neo4j_value_t value);

/**
 * Return a pointer to a byte sequence.
 *
 * The pointer will be to a byte sequence. The length of the sequence can be
 * obtained using neo4j_bytes_length(value).
 *
 * Note that the result is undefined if the value is not of type NEO4J_BYTES.
 *
 * @param [value] The neo4j byte sequence.
 * @return A pointer to a byte sequence.
 */
__neo4j_const
const char *neo4j_bytes_value(neo4j_value_t value);


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
__neo4j_const
neo4j_value_t neo4j_list(const neo4j_value_t *items, unsigned int n);

/**
 * Return the length of a neo4j list (number of entries).
 *
 * Note that the result is undefined if the value is not of type NEO4J_LIST.
 *
 * @param [value] The neo4j list.
 * @return The number of entries.
 */
__neo4j_const
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
__neo4j_pure
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
__neo4j_pure
neo4j_value_t neo4j_map(const neo4j_map_entry_t *entries, unsigned int n);

/**
 * Return the size of a neo4j map (number of entries).
 *
 * Note that the result is undefined if the value is not of type NEO4J_MAP.
 *
 * @param [value] The neo4j map.
 * @return The number of entries.
 */
__neo4j_const
unsigned int neo4j_map_size(neo4j_value_t value);

/**
 * Return an entry from a neo4j map.
 *
 * Note that the result is undefined if the value is not of type NEO4J_MAP.
 *
 * @param [value] The neo4j map.
 * @param [index] The index of the entry to return.
 * @return The entry at the specified index, or `NULL` if the index
 *         is too large.
 */
__neo4j_pure
const neo4j_map_entry_t *neo4j_map_getentry(neo4j_value_t value,
        unsigned int index);

/**
 * @fn neo4j_value_t neo4j_map_get(neo4j_value_t value, const char *key);
 * @brief Return a value from a neo4j map.
 *
 * Note that the result is undefined if the value is not of type NEO4J_MAP.
 *
 * @param [value] The neo4j map.
 * @param [key] The null terminated string key for the entry.
 * @return The value stored under the specified key, or `NULL` if the key is
 *         not known.
 */
#define neo4j_map_get(value, key) neo4j_map_kget(value, neo4j_string(key))

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
__neo4j_pure
neo4j_value_t neo4j_map_kget(neo4j_value_t value, neo4j_value_t key);

/**
 * @fn neo4j_map_entry_t neo4j_map_entry(const char *key, neo4j_value_t value);
 * @brief Constrct a neo4j map entry.
 *
 * @param [key] The null terminated string key for the entry.
 * @param [value] The value for the entry.
 * @return A neo4j map entry.
 */
#define neo4j_map_entry(key, value) neo4j_map_kentry(neo4j_string(key), value)

/**
 * Constrct a neo4j map entry using a value key.
 *
 * The value key must be of type NEO4J_STRING.
 *
 * @param [key] The key for the entry.
 * @param [value] The value for the entry.
 * @return A neo4j map entry.
 */
__neo4j_pure
neo4j_map_entry_t neo4j_map_kentry(neo4j_value_t key, neo4j_value_t value);


/**
 * Return the label list of a neo4j node.
 *
 * Note that the result is undefined if the value is not of type NEO4J_NODE.
 *
 * @param [value] The neo4j node.
 * @return A neo4j value encoding the list of labels.
 */
__neo4j_pure
neo4j_value_t neo4j_node_labels(neo4j_value_t value);

/**
 * Return the property map of a neo4j node.
 *
 * Note that the result is undefined if the value is not of type NEO4J_NODE.
 *
 * @param [value] The neo4j node.
 * @return A neo4j value encoding the map of properties.
 */
__neo4j_pure
neo4j_value_t neo4j_node_properties(neo4j_value_t value);

/**
 * Return the identity of a neo4j node.
 *
 * @param [value] The neo4j node.
 * @return A neo4j value encoding the identity of the node.
 */
__neo4j_pure
neo4j_value_t neo4j_node_identity(neo4j_value_t value);


/**
 * Return the type of a neo4j relationship.
 *
 * Note that the result is undefined if the value is not of type
 * NEO4J_RELATIONSHIP.
 *
 * @param [value] The neo4j node.
 * @return A neo4j value encoding the type as a string.
 */
__neo4j_pure
neo4j_value_t neo4j_relationship_type(neo4j_value_t value);

/**
 * Return the property map of a neo4j relationship.
 *
 * Note that the result is undefined if the value is not of type
 * NEO4J_RELATIONSHIP.
 *
 * @param [value] The neo4j relationship.
 * @return A neo4j value encoding the map of properties.
 */
__neo4j_pure
neo4j_value_t neo4j_relationship_properties(neo4j_value_t value);

/**
 * Return the identity of a neo4j relationship.
 *
 * @param [value] The neo4j relationship.
 * @return A neo4j value encoding the Identity of the relationship.
 */
__neo4j_pure
neo4j_value_t neo4j_relationship_identity(neo4j_value_t value);

/**
 * Return the start node identity for a neo4j relationship.
 *
 * @param [value] The neo4j relationship.
 * @return A neo4j value encoding the Identity of the start node.
 */
__neo4j_pure
neo4j_value_t neo4j_relationship_start_node_identity(neo4j_value_t value);

/**
 * Return the end node identity for a neo4j relationship.
 *
 * @param [value] The neo4j relationship.
 * @return A neo4j value encoding the Identity of the end node.
 */
__neo4j_pure
neo4j_value_t neo4j_relationship_end_node_identity(neo4j_value_t value);


/**
 * Return the length of a neo4j path.
 *
 * The length of a path is defined by the number of relationships included in
 * it.
 *
 * Note that the result is undefined if the value is not of type NEO4J_PATH.
 *
 * @param [value] The neo4j path.
 * @return The length of the path.
 */
__neo4j_pure
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
__neo4j_pure
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
__neo4j_pure
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
 * The returned configuration must be later released using
 * neo4j_config_free().
 *
 * @return A pointer to a new neo4j client configuration, or `NULL` on error
 *         (errno will be set).
 */
__neo4j_must_check
neo4j_config_t *neo4j_new_config(void);

/**
 * Release a neo4j client configuration.
 *
 * @param [config] A pointer to a neo4j client configuration. This pointer will
 *         be invalid after the function returns.
 */
void neo4j_config_free(neo4j_config_t *config);

/**
 * Duplicate a neo4j client configuration.
 *
 * The returned configuration must be later released using
 * neo4j_config_free().
 *
 * @param [config] A pointer to a neo4j client configuration.
 * @return A duplicate configuration.
 */
__neo4j_must_check
neo4j_config_t *neo4j_config_dup(const neo4j_config_t *config);

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
 * Get the client ID in the neo4j client configuration.
 *
 * @param [config] The neo4j client configuration.
 * @return A pointer to the client ID, or `NULL` if one is not set.
 */
__neo4j_pure
const char *neo4j_config_get_client_id(const neo4j_config_t *config);

#define NEO4J_MAXUSERNAMELEN 1023

/**
 * Set the username in the neo4j client configuration.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [username] The username to authenticate with. The string will be
 *         duplicated, and thus may point to temporary memory.
 * @return 0 on success, or -1 on error (errno will be set).
 */
__neo4j_must_check
int neo4j_config_set_username(neo4j_config_t *config, const char *username);

/**
 * Get the username in the neo4j client configuration.
 *
 * The returned username will only be valid whilst the configuration is
 * unchanged.
 *
 * @param [config] The neo4j client configuration.
 * @return A pointer to the username, or `NULL` if one is not set.
 */
__neo4j_pure
const char *neo4j_config_get_username(const neo4j_config_t *config);

#define NEO4J_MAXPASSWORDLEN 1023

/**
 * Set the password in the neo4j client configuration.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [password] The password to authenticate with. The string will be
 *         duplicated, and thus may point to temporary memory.
 * @return 0 on success, or -1 on error (errno will be set).
 */
__neo4j_must_check
int neo4j_config_set_password(neo4j_config_t *config, const char *password);

/**
 * Set the basic authentication callback.
 *
 * If a username and/or password is required for basic authentication and
 * isn't available in the configuration or connection URI, then this callback
 * will be invoked to obtain the username and/or password.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [callback] The callback to be invoked.
 * @param [userdata] User data that will be supplied to the callback.
 * @return 0 on success, or -1 on error (errno will be set).
 */
int neo4j_config_set_basic_auth_callback(neo4j_config_t *config,
        neo4j_basic_auth_callback_t callback, void *userdata);

/**
 * Set the location of a TLS private key and certificate chain.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [path] The path to the PEM file containing the private key
 *         and certificate chain. The string will be
 *         duplicated, and thus may point to temporary memory.
 * @return 0 on success, or -1 on error (errno will be set).
 */
__neo4j_must_check
int neo4j_config_set_TLS_private_key(neo4j_config_t *config,
        const char *path);

/**
 * Obtain the path to the TLS private key and certificate chain.
 *
 * @param [config] The neo4j client configuration.
 * @return The path set in the config, or `NULL` if none.
 */
const char *neo4j_config_get_TLS_private_key(const neo4j_config_t *config);

/**
 * Set the password callback for the TLS private key file.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [callback] The callback to be invoked whenever a password for
 *         the certificate file is required.
 * @param [userdata] User data that will be supplied to the callback.
 * @return 0 on success, or -1 on error (errno will be set).
 */
__neo4j_must_check
int neo4j_config_set_TLS_private_key_password_callback(neo4j_config_t *config,
        neo4j_password_callback_t callback, void *userdata);

/**
 * Set the password for the TLS private key file.
 *
 * This is a simpler alternative to using
 * neo4j_config_set_TLS_private_key_password_callback().
 *
 * @param [config] The neo4j client configuration to update.
 * @param [password] The password for the certificate file. This string should
 *         remain allocated whilst the config is allocated _or if any
 *         connections opened with the config remain active_.
 * @return 0 on success, or -1 on error (errno will be set).
 */
__neo4j_must_check
int neo4j_config_set_TLS_private_key_password(neo4j_config_t *config,
        const char *password);

/**
 * Set the location of a file containing TLS certificate authorities (and CRLs).
 *
 * The file should contain the certificates of the trusted CAs and CRLs. The
 * file must be in base64 privacy enhanced mail (PEM) format.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [path] The path to the PEM file containing the trusted CAs and CRLs.
 *         The string will be duplicated, and thus may point to temporary memory.
 * @return 0 on success, or -1 on error (errno will be set).
 */
__neo4j_must_check
int neo4j_config_set_TLS_ca_file(neo4j_config_t *config, const char *path);

/**
 * Obtain the path to the TLS certificate authority file.
 *
 * @param [config] The neo4j client configuration.
 * @return The path set in the config, or `NULL` if none.
 */
const char *neo4j_config_get_TLS_ca_file(const neo4j_config_t *config);

/**
 * Set the location of a directory of TLS certificate authorities (and CRLs).
 *
 * The specified directory should contain the certificates of the trusted CAs
 * and CRLs, named by hash according to the `c_rehash` tool.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [path] The path to the directory of CAs and CRLs.  The string will be
 *         duplicated, and thus may point to temporary memory.
 * @return 0 on success, or -1 on error (errno will be set).
 */
__neo4j_must_check
int neo4j_config_set_TLS_ca_dir(neo4j_config_t *config, const char *path);

/**
 * Obtain the path to the TLS certificate authority directory.
 *
 * @param [config] The neo4j client configuration.
 * @return The path set in the config, or `NULL` if none.
 */
const char *neo4j_config_get_TLS_ca_dir(const neo4j_config_t *config);

/**
 * Enable or disable trusting of known hosts.
 *
 * When enabled, the neo4j client will check if a host has been previously
 * trusted and stored into the "known hosts" file, and that the host
 * fingerprint still matches the previously accepted value. This is enabled by
 * default.
 *
 * If verification fails, the callback set with
 * neo4j_config_set_unverified_host_callback() will be invoked.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [enable] `true` to enable trusting of known hosts, and `false` to
 *         disable this behaviour.
 * @return 0 on success, or -1 on error (errno will be set).
 */
__neo4j_must_check
int neo4j_config_set_trust_known_hosts(neo4j_config_t *config, bool enable);

/**
 * Check if trusting of known hosts is enabled.
 *
 * @param [config] The neo4j client configuration.
 * @return `true` if enabled and `false` otherwise.
 */
bool neo4j_config_get_trust_known_hosts(const neo4j_config_t *config);

/**
 * Set the location of the known hosts file for TLS certificates.
 *
 * The file, which will be created and maintained by neo4j client,
 * will be used for storing trust information when using "Trust On First Use".
 *
 * @param [config] The neo4j client configuration to update.
 * @param [path] The path to known hosts file. The string will be
 *         duplicated, and thus may point to temporary memory.
 * @return 0 on success, or -1 on error (errno will be set).
 */
__neo4j_must_check
int neo4j_config_set_known_hosts_file(neo4j_config_t *config, const char *path);

/**
 * Obtain the path to the known hosts file.
 *
 * @param [config] The neo4j client configuration.
 * @return The path set in the config, or `NULL` if none.
 */
const char *neo4j_config_get_known_hosts_file(const neo4j_config_t *config);


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
 *         trusted for future connections, or -1 on error (errno should be set).
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
 * @return 0 on success, or -1 on error (errno will be set).
 */
int neo4j_config_set_unverified_host_callback(neo4j_config_t *config,
        neo4j_unverified_host_callback_t callback, void *userdata);

/**
 * Set the I/O output buffer size.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [size] The I/O output buffer size.
 * @return 0 on success, or -1 on error (errno will be set).
 */
int neo4j_config_set_sndbuf_size(neo4j_config_t *config, size_t size);

/**
 * Get the size for the I/O output buffer.
 *
 * @param [config] The neo4j client configuration.
 * @return The sndbuf size.
 */
size_t neo4j_config_get_sndbuf_size(const neo4j_config_t *config);

/**
 * Set the I/O input buffer size.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [size] The I/O input buffer size.
 * @return 0 on success, or -1 on error (errno will be set).
 */
int neo4j_config_set_rcvbuf_size(neo4j_config_t *config, size_t size);

/**
 * Get the size for the I/O input buffer.
 *
 * @param [config] The neo4j client configuration.
 * @return The rcvbuf size.
 */
size_t neo4j_config_get_rcvbuf_size(const neo4j_config_t *config);

/**
 * Set a logger provider in the neo4j client configuration.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [logger_provider] The logger provider function.
 */
void neo4j_config_set_logger_provider(neo4j_config_t *config,
        struct neo4j_logger_provider *logger_provider);

/**
 * Set the socket send buffer size.
 *
 * This is only applicable to the standard connection factory.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [size] The socket send buffer size, or 0 to use the system default.
 * @return 0 on success, or -1 on error (errno will be set).
 */
int neo4j_config_set_so_sndbuf_size(neo4j_config_t *config, unsigned int size);

/**
 * Get the size for the socket send buffer.
 *
 * @param [config] The neo4j client configuration.
 * @return The so_sndbuf size.
 */
unsigned int neo4j_config_get_so_sndbuf_size(const neo4j_config_t *config);

/**
 * Set the socket receive buffer size.
 *
 * This is only applicable to the standard connection factory.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [size] The socket receive buffer size, or 0 to use the system default.
 * @return 0 on success, or -1 on error (errno will be set).
 */
int neo4j_config_set_so_rcvbuf_size(neo4j_config_t *config, unsigned int size);

/**
 * Get the size for the socket receive buffer.
 *
 * @param [config] The neo4j client configuration.
 * @return The so_rcvbuf size.
 */
unsigned int neo4j_config_get_so_rcvbuf_size(const neo4j_config_t *config);

/**
 * Set a connection factory.
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

/**
 * The standard memory allocator.
 *
 * This memory allocator delegates to the system malloc/free functions.
 */
extern struct neo4j_memory_allocator neo4j_std_memory_allocator;

/**
 * Set a memory allocator.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [allocator] The memory allocator.
 */
void neo4j_config_set_memory_allocator(neo4j_config_t *config,
        struct neo4j_memory_allocator *allocator);

/**
 * Get the memory allocator.
 *
 * @param [config] The neo4j client configuration.
 * @return The memory allocator.
 */
struct neo4j_memory_allocator *neo4j_config_get_memory_allocator(
        const neo4j_config_t *config);

/**
 * Set the maximum number of requests that can be pipelined to the
 * server.
 *
 * @attention Setting this value too high could result in deadlocking within
 * the client, as the client will block when trying to send statements
 * to a server with a full queue, instead of reading results that would drain
 * the queue.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [n] The new maximum.
 */
void neo4j_config_set_max_pipelined_requests(neo4j_config_t *config,
        unsigned int n);

/**
 * Get the maximum number of requests that can be pipelined to the server.
 *
 * @param [config] The neo4j client configuration.
 * @return The number of requests that can be pipelined.
 */
unsigned int neo4j_config_get_max_pipelined_requests(
        const neo4j_config_t *config);

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
 *         terminating character), or -1 on error (errno will be set).
 */
ssize_t neo4j_dotdir(char *buffer, size_t n, const char *append);
ssize_t neo4j_dot_dir(char *buffer, size_t n, const char *append)
    __neo4j_deprecated;

/**
 * Return a path within the neo4j dot directory.
 *
 * The neo4j dot directory is typically ".neo4j" within the users home
 * directory. If append is `NULL`, then an absoulte path to the home
 * directory is placed into buffer.
 *
 * @param [append] The relative path to append to the dot directory, which
 *         may be `NULL`.
 * @return The resulting path as a null-terminated string (which must
 *         later be deallocated by calling free()) or `NULL`
 *         on error (errno will be set).
 */
char *neo4j_adotdir(const char *append);


/*
 * =====================================
 * connection
 * =====================================
 */

#define NEO4J_DEFAULT_TCP_PORT 7687

#define NEO4J_CONNECT_DEFAULT 0
#define NEO4J_INSECURE (1<<0)
#define NEO4J_NO_URI_CREDENTIALS (1<<1)
#define NEO4J_NO_URI_PASSWORD (1<<2)

/**
 * Establish a connection to a neo4j server.
 *
 * A bitmask of flags may be supplied, which may include:
 * - NEO4J_INSECURE - do not attempt to establish a secure connection. If a
 *   secure connection is required, then connect will fail with errno set to
 *   `NEO4J_SERVER_REQUIRES_SECURE_CONNECTION`.
 * - NEO4J_NO_URI_CREDENTIALS - do not use credentials provided in the
 *   server URI (use credentials from the configuration instead).
 * - NEO4J_NO_URI_PASSWORD - do not use any password provided in the
 *   server URI (obtain password from the configuration instead).
 *
 * If no flags are required, pass 0 or `NEO4J_CONNECT_DEFAULT`.
 *
 * @param [uri] A URI describing the server to connect to, which may also
 *         include authentication data (which will override any provided
 *         in the config).
 * @param [config] The neo4j client configuration to use for this connection,
 *         or `NULL` if the default configuration should be used.
 * @param [flags] A bitmask of flags to control connections.
 * @return A pointer to a `neo4j_connection_t` structure, or `NULL` on error
 *         (errno will be set).
 */
__neo4j_must_check
neo4j_connection_t *neo4j_connect(const char *uri, neo4j_config_t *config,
        uint_fast32_t flags);

/**
 * Establish a connection to a neo4j server.
 *
 * A bitmask of flags may be supplied, which may include:
 * - NEO4J_INSECURE - do not attempt to establish a secure connection. If a
 *   secure connection is required, then connect will fail with errno set to
 *   `NEO4J_SERVER_REQUIRES_SECURE_CONNECTION`.
 *
 * If no flags are required, pass 0 or `NEO4J_CONNECT_DEFAULT`.
 *
 * @param [hostname] The hostname to connect to.
 * @param [port] The port to connect to.
 * @param [config] The neo4j client configuration to use for this connection,
 *         or `NULL` if the default configuration should be used.
 * @param [flags] A bitmask of flags to control connections.
 * @return A pointer to a `neo4j_connection_t` structure, or `NULL` on error
 *         (errno will be set).
 */
__neo4j_must_check
neo4j_connection_t *neo4j_tcp_connect(const char *hostname, unsigned int port,
        neo4j_config_t *config, uint_fast32_t flags);

/**
 * Close a connection to a neo4j server.
 *
 * @param [connection] The connection to close. This pointer will be invalid
 *         after the function returns.
 * @return 0 on success, or -1 on error (errno will be set).
 */
int neo4j_close(neo4j_connection_t *connection);


/**
 * Get the hostname for a connection.
 *
 * @param [connection] The neo4j connection.
 * @return A pointer to a hostname string, which will remain valid only whilst
 *         the connection remains open.
 */
__neo4j_pure
const char *neo4j_connection_hostname(const neo4j_connection_t *connection);


/**
 * Get the port for a connection.
 *
 * @param [connection] The neo4j connection.
 * @return The port of the connection.
 */
__neo4j_pure
unsigned int neo4j_connection_port(const neo4j_connection_t *connection);


/**
 * Get the username for a connection.
 *
 * @param [connection] The neo4j connection.
 * @return A pointer to a username string, which will remain valid only whilst
 *         the connection remains open, or `NULL` if no username was associated
 *         with the connection.
 */
__neo4j_pure
const char *neo4j_connection_username(const neo4j_connection_t *connection);


/**
 * Check if a given connection uses TLS.
 *
 * @param [connection] The neo4j connection.
 * @return `true` if the connection was established over TLS, and `false`
 *         otherwise.
 */
__neo4j_pure
bool neo4j_connection_is_secure(const neo4j_connection_t *connection);


/*
 * =====================================
 * session
 * =====================================
 */

/**
 * Reset a session.
 *
 * Invoking this function causes all server-held state for the connection to be
 * cleared, including rolling back any open transactions, and causes any
 * existing result stream to be terminated.
 *
 * @param [connection] The connection to reset.
 * @return 0 on sucess, or -1 on error (errno will be set).
 */
int neo4j_reset(neo4j_connection_t *connection);

/**
 * Check if the server indicated that credentials have expired.
 *
 * @param [connection] The connection.
 * @return `true` if the server indicated that credentials have expired,
 *         and `false` otherwise.
 */
__neo4j_pure
bool neo4j_credentials_expired(const neo4j_connection_t *connection);

/**
 * Get the server ID string.
 *
 * @param [connection] The connection.
 * @return The server ID string, or `NULL` if none was available.
 */
__neo4j_pure
const char *neo4j_server_id(const neo4j_connection_t *connection);


/*
 * =====================================
 * job
 * =====================================
 */

/**
 * Evaluate a statement.
 *
 * @attention The statement and the params must remain valid until the returned
 * result stream is closed.
 *
 * @param [connection] The connection.
 * @param [statement] The statement to be evaluated. This must be a `NULL`
 *         terminated string and may contain UTF-8 multi-byte characters.
 * @param [params] The parameters for the statement, which must be a value of
 *         type NEO4J_MAP or #neo4j_null.
 * @return A `neo4j_result_stream_t`, or `NULL` on error (errno will be set).
 */
__neo4j_must_check
neo4j_result_stream_t *neo4j_run(neo4j_connection_t *connection,
        const char *statement, neo4j_value_t params);

/**
 * Evaluate a statement, ignoring any results.
 *
 * The `neo4j_result_stream_t` returned from this function will not
 * provide any results. It can be used to check for evaluation errors using
 * neo4j_check_failure().
 *
 * @param [connection] The connection.
 * @param [statement] The statement to be evaluated. This must be a `NULL`
 *         terminated string and may contain UTF-8 multi-byte characters.
 * @param [params] The parameters for the statement, which must be a value of
 *         type NEO4J_MAP or #neo4j_null.
 * @return A `neo4j_result_stream_t`, or `NULL` on error (errno will be set).
 */
__neo4j_must_check
neo4j_result_stream_t *neo4j_send(neo4j_connection_t *connection,
        const char *statement, neo4j_value_t params);


/*
 * =====================================
 * result stream
 * =====================================
 */

/**
 * Check if a results stream has failed.
 *
 * Note: if the error is `NEO4J_STATEMENT_EVALUATION_FAILED`, then additional
 * error information will be available via neo4j_error_message().
 *
 * @param [results] The result stream.
 * @return 0 if no failure has occurred, and an error number otherwise.
 */
int neo4j_check_failure(neo4j_result_stream_t *results);

/**
 * Get the number of fields in a result stream.
 *
 * @param [results] The result stream.
 * @return The number of fields in the result, or 0 if no fields were available
 *         or on error (errno will be set).
 */
unsigned int neo4j_nfields(neo4j_result_stream_t *results);

/**
 * Get the name of a field in a result stream.
 *
 * @attention Note that the returned pointer is only valid whilst the result
 * stream has not been closed.
 *
 * @param [results] The result stream.
 * @param [index] The field index to get the name of.
 * @return The name of the field, or `NULL` on error (errno will be set).
 *         If returned, the name will be a `NULL` terminated string and may
 *         contain UTF-8 multi-byte characters.
 */
const char *neo4j_fieldname(neo4j_result_stream_t *results,
        unsigned int index);

/**
 * Fetch the next record from the result stream.
 *
 * @attention The pointer to the result will only remain valid until the
 * next call to neo4j_fetch_next() or until the result stream is closed. To
 * hold the result longer, use neo4j_retain() and neo4j_release().
 *
 * @param [results] The result stream.
 * @return The next result, or `NULL` if the stream is exahusted or an
 *         error has occurred (errno will be set).
 */
__neo4j_must_check
neo4j_result_t *neo4j_fetch_next(neo4j_result_stream_t *results);

/**
 * Peek at a record in the result stream.
 *
 * @attention The pointer to the result will only remain valid until it is
 * retreived via neo4j_fetch_next() or until the result stream is closed. To
 * hold the result longer, use neo4j_retain() and neo4j_release().
 *
 * @attention All results up to the specified depth will be retrieved and
 * held in memory. Avoid using this method with large depths.
 *
 * @param [results] The result stream.
 * @param [depth] The depth to peek into the remaining records in the stream.
 * @return The result at the specified depth, or `NULL` if the stream is
 *         exahusted or an error has occurred (errno will be set).
 */
neo4j_result_t *neo4j_peek(neo4j_result_stream_t *results, unsigned int depth);

/**
 * Close a result stream.
 *
 * Closes the result stream and releases all memory held by it, including
 * results and values obtained from it.
 *
 * @attention After this function is invoked, all `neo4j_result_t` objects
 * fetched from this stream, and any values obtained from them, will be invalid
 * and _must not be accessed_. Doing so will result in undetermined and
 * unstable behaviour. This is true even if this function returns an error.
 *
 * @param [results] The result stream. The pointer will be invalid after the
 *         function returns.
 * @return 0 on success, or -1 on error (errno will be set).
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
 * When neo4j_check_failure() returns `NEO4J_STATEMENT_EVALUATION_FAILED`,
 * then this function can be used to get the error code sent from neo4j.
 *
 * @attention Note that the returned pointer is only valid whilst the result
 * stream has not been closed.
 *
 * @param [results] The result stream.
 * @return A `NULL` terminated string reprenting the error code, or `NULL`
 *         if the stream has not failed or the failure was not
 *         `NEO4J_STATEMENT_EVALUATION_FAILED`.
 */
const char *neo4j_error_code(neo4j_result_stream_t *results);

/**
 * Return the error message sent from neo4j.
 *
 * When neo4j_check_failure() returns `NEO4J_STATEMENT_EVALUATION_FAILED`,
 * then this function can be used to get the detailed error message sent
 * from neo4j.
 *
 * @attention Note that the returned pointer is only valid whilst the result
 * stream has not been closed.
 *
 * @param [results] The result stream.
 * @return The error message, or `NULL` if the stream has not failed or the
 *         failure was not `NEO4J_STATEMENT_EVALUATION_FAILED`. If returned,
 *         the message will be a `NULL` terminated string and may contain UTF-8
 *         mutli-byte characters.
 */
const char *neo4j_error_message(neo4j_result_stream_t *results);

/**
 * Failure details.
 */
struct neo4j_failure_details
{
    /** The failure code. */
    const char *code;
    /**
     * The complete failure message.
     *
     * @attention This may contain UTF-8 multi-byte characters.
     */
    const char *message;
    /**
     * The human readable description of the failure.
     *
     * @attention This may contain UTF-8 multi-byte characters.
     */
    const char *description;
    /**
     * The line of statement text that the failure relates to.
     *
     * Will be 0 if the failure was not related to a line of statement text.
     */
    unsigned int line;
    /**
     * The column of statement text that the failure relates to.
     *
     * Will be 0 if the failure was not related to a line of statement text.
     */
    unsigned int column;
    /**
     * The character offset into the statement text that the failure relates to.
     *
     * Will be 0 if the failure is related to the first character of the
     * statement text, or if the failure was not related to the statement text.
     */
    unsigned int offset;
    /**
     * A string providing context around where the failure occurred.
     *
     * @attention This may contain UTF-8 multi-byte characters.
     *
     * Will be `NULL` if the failure was not related to the statement text.
     */
    const char *context;
    /**
     * The offset into the context where the failure occurred.
     *
     * Will be 0 if the failure was not related to a line of statement text.
     */
    unsigned int context_offset;
};

/**
 * Return the details of a statement evaluation failure.
 *
 * When neo4j_check_failure() returns `NEO4J_STATEMENT_EVALUATION_FAILED`,
 * then this function can be used to get the details of the failure.
 *
 * @attention Note that the returned pointer is only valid whilst the result
 * stream has not been closed.
 *
 * @param [results] The result stream.
 * @return A pointer to the failure details, or `NULL` if no failure details
 *         were available.
 */
const struct neo4j_failure_details *neo4j_failure_details(
        neo4j_result_stream_t *results);

/**
 * Return the number of records received in a result stream.
 *
 * This value will continue to increase until all results have been fetched.
 *
 * @param [results] The result stream.
 * @return The number of results.
 */
unsigned long long neo4j_result_count(
        neo4j_result_stream_t *results);

/**
 * Return the reported time until the first record was available.
 *
 * @param [results] The result stream.
 * @return The time, in milliseconds, or 0 if it was not available.
 */
unsigned long long neo4j_results_available_after(
        neo4j_result_stream_t *results);

/**
 * Return the reported time until all records were consumed.
 *
 * @attention As the consumption time is only available at the end of the result
 * stream, invoking this function will will result in any unfetched results
 * being pulled from the server and held in memory. It is usually better to
 * exhaust the stream using neo4j_fetch_next() before invoking this
 * method.
 *
 * @param [results] The result stream.
 * @return The time, in milliseconds, or 0 if it was not available.
 */
unsigned long long neo4j_results_consumed_after(neo4j_result_stream_t *results);


#define NEO4J_READ_ONLY_STATEMENT 0
#define NEO4J_WRITE_ONLY_STATEMENT 1
#define NEO4J_READ_WRITE_STATEMENT 2
#define NEO4J_SCHEMA_UPDATE_STATEMENT 3
#define NEO4J_CONTROL_STATEMENT 4

/**
 * Return the statement type for the result stream.
 *
 * The returned value will be one of the following:
 * - NEO4J_READ_ONLY_STATEMENT
 * - NEO4J_WRITE_ONLY_STATEMENT
 * - NEO4J_READ_WRITE_STATEMENT
 * - NEO4J_SCHEMA_UPDATE_STATEMENT
 * - NEO4J_CONTROL_STATEMENT
 *
 * @attention As the statement type is only available at the end of the result
 * stream, invoking this function will will result in any unfetched results
 * being pulled from the server and held in memory. It is usually better to
 * exhaust the stream using neo4j_fetch_next() before invoking this
 * method.
 *
 * @param [results] The result stream.
 * @return The statement type, or -1 on error (errno will be set).
 */
int neo4j_statement_type(neo4j_result_stream_t *results);

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
 * exhaust the stream using neo4j_fetch_next() before invoking this
 * method.
 *
 * @param [results] The result stream.
 * @return The update counts. If an error has occurred, all the counts will be
 *         zero.
 */
struct neo4j_update_counts neo4j_update_counts(neo4j_result_stream_t *results);


struct neo4j_statement_execution_step;

/**
 * The plan (or profile) for an evaluated statement.
 *
 * Plans and profiles differ only in that execution steps do not contain row
 * and db-hit data.
 */
struct neo4j_statement_plan
{
    /** The version of the compiler that produced the plan/profile. */
    const char *version;
    /** The planner that was used to produce the plan/profile. */
    const char *planner;
    /** The runtime that was or would be used for evaluating the statement. */
    const char *runtime;
    /** `true` if profile data is included in the execution steps. */
    bool is_profile;
    /** The output execution step. */
    struct neo4j_statement_execution_step *output_step;
};

/**
 * An execution step in a plan (or profile) for an evaluated statement.
 */
struct neo4j_statement_execution_step
{
    /** The name of the operator type applied in this execution step. */
    const char *operator_type;
    /** An array of identifier names available in this step. */
    const char * const *identifiers;
    /** The number of identifiers. */
    unsigned int nidentifiers;
    /** The estimated number of rows to be handled by this step. */
    double estimated_rows;
    /** The number of rows handled by this step (for profiled plans only). */
    unsigned long long rows;
    /** The number of db_hits (for profiled plans only). */
    unsigned long long db_hits;
    /** The number of page cache hits (for profiled plans only). */
    unsigned long long page_cache_hits;
    /** The number of page cache misses (for profiled plans only). */
    unsigned long long page_cache_misses;
    /** An array containing the sources for this step. */
    struct neo4j_statement_execution_step **sources;
    /** The number of sources. */
    unsigned int nsources;

    /**
     * A NEO4J_MAP, containing all the arguments for this step as provided by
     * the server.
     */
    neo4j_value_t arguments;
};


/**
 * Return the statement plan for the result stream.
 *
 * The returned statement plan, if not `NULL`, must be later released using
 * neo4j_statement_plan_release().
 *
 * If the was no plan (or profile) in the server response, the result of this
 * function will be `NULL` and errno will be set to NEO4J_NO_PLAN_AVAILABLE.
 * Note that errno will not be modified when a plan is returned, so error
 * checking MUST evaluate the return value first.
 *
 * @param [results] The result stream.
 * @return The statement plan/profile, or `NULL` if a plan/profile was not
 *         available or on error (errno will be set).
 */
__neo4j_must_check
struct neo4j_statement_plan *neo4j_statement_plan(
        neo4j_result_stream_t *results);

/**
 * Release a statement plan.
 *
 * The pointer will be invalid and should not be used after this function
 * is called.
 *
 * @param [plan] A statment plan.
 */
void neo4j_statement_plan_release(struct neo4j_statement_plan *plan);


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
 * @return The field from the result, or #neo4j_null if index is out of bounds.
 */
neo4j_value_t neo4j_result_field(const neo4j_result_t *result,
        unsigned int index);

/**
 * Retain a result.
 *
 * This retains the result and all values contained within it, preventing
 * them from being deallocated on the next call to neo4j_fetch_next()
 * or when the result stream is closed via neo4j_close_results(). Once
 * retained, the result _must_ be explicitly released via
 * neo4j_release().
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

#define NEO4J_RENDER_DEFAULT 0
#define NEO4J_RENDER_SHOW_NULLS (1<<0)
#define NEO4J_RENDER_QUOTE_STRINGS (1<<1)
#define NEO4J_RENDER_ASCII (1<<2)
#define NEO4J_RENDER_ASCII_ART (1<<3)
#define NEO4J_RENDER_ROWLINES (1<<4)
#define NEO4J_RENDER_WRAP_VALUES (1<<5)
#define NEO4J_RENDER_NO_WRAP_MARKERS (1<<6)
#define NEO4J_RENDER_ANSI_COLOR (1<<7)

/**
 * Enable or disable rendering NEO4J_NULL values.
 *
 * If set to `true`, then NEO4J_NULL values will be rendered using the
 * string 'null'. Otherwise, they will be blank.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [enable] `true` to enable rendering of NEO4J_NULL values, and
 *         `false` to disable this behaviour.
 */
void neo4j_config_set_render_nulls(neo4j_config_t *config, bool enable);

/**
 * Check if rendering of NEO4J_NULL values is enabled.
 *
 * @param [config] The neo4j client configuration.
 * @return `true` if rendering of NEO4J_NULL values is enabled, and `false`
 *         otherwise.
 */
bool neo4j_config_get_render_nulls(const neo4j_config_t *config);

/**
 * Enable or disable quoting of NEO4J_STRING values.
 *
 * If set to `true`, then NEO4J_STRING values will be rendered with
 * surrounding quotes.
 *
 * @note This only applies when rendering to a table. In CSV output, strings
 * are always quoted.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [enable] `true` to enable rendering of NEO4J_STRING values with
 *         quotes, and `false` to disable this behaviour.
 */
void neo4j_config_set_render_quoted_strings(neo4j_config_t *config,
        bool enable);

/**
 * Check if quoting of NEO4J_STRING values is enabled.
 *
 * @note This only applies when rendering to a table. In CSV output, strings
 * are always quoted.
 *
 * @param [config] The neo4j client configuration.
 * @return `true` if quoting of NEO4J_STRING values is enabled, and `false`
 *         otherwise.
 */
bool neo4j_config_get_render_quoted_strings(const neo4j_config_t *config);

/**
 * Enable or disable rendering in ASCII-only.
 *
 * If set to `true`, then render output will only use ASCII characters and
 * any non-ASCII characters in values will be escaped. Otherwise, UTF-8
 * characters will be used, including unicode border drawing characters.
 *
 * @note This does not effect CSV output.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [enable] `true` to enable rendering in only ASCII characters,
 *         and `false` to disable this behaviour.
 */
void neo4j_config_set_render_ascii(neo4j_config_t *config, bool enable);

/**
 * Check if ASCII-only rendering is enabled.
 *
 * @note This does not effect CSV output.
 *
 * @param [config] The neo4j client configuration.
 * @return `true` if ASCII-only rendering is enabled, and `false`
 *         otherwise.
 */
bool neo4j_config_get_render_ascii(const neo4j_config_t *config);

/**
 * Enable or disable rendering of rowlines in result tables.
 *
 * If set to `true`, then render output will separate each table row
 * with a rowline.
 *
 * @note This only applies when rendering results to a table.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [enable] `true` to enable rowline rendering, and `false` to disable
 *         this behaviour.
 */
void neo4j_config_set_render_rowlines(neo4j_config_t *config, bool enable);

/**
 * Check if rendering of rowlines is enabled.
 *
 * @note This only applies when rendering results to a table.
 *
 * @param [config] The neo4j client configuration.
 * @return `true` if rowline rendering is enabled, and `false`
 *         otherwise.
 */
bool neo4j_config_get_render_rowlines(const neo4j_config_t *config);

/**
 * Enable or disable wrapping of values in result tables.
 *
 * If set to `true`, then values will be wrapped when rendering tables.
 * Otherwise, they will be truncated.
 *
 * @note This only applies when rendering results to a table.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [enable] `true` to enable value wrapping, and `false` to disable this
 *         behaviour.
 */
void neo4j_config_set_render_wrapped_values(neo4j_config_t *config,
        bool enable);

/**
 * Check if wrapping of values in result tables is enabled.
 *
 * @note This only applies when rendering results to a table.
 *
 * @param [config] The neo4j client configuration.
 * @return `true` if wrapping of values is enabled, and `false` otherwise.
 */
bool neo4j_config_get_render_wrapped_values(const neo4j_config_t *config);

/**
 * Enable or disable the rendering of wrap markers when wrapping or truncating.
 *
 * If set to `true`, then values that are wrapped or truncated will be
 * rendered with a wrap marker. The default value for this is `true`.
 *
 * @note This only applies when rendering results to a table.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [enable] `true` to display wrap markers, and `false` to disable this
 *         behaviour.
 */
void neo4j_config_set_render_wrap_markers(neo4j_config_t *config, bool enable);

/**
 * Check if wrap markers will be rendered when wrapping or truncating.
 *
 * @note This only applies when rendering results to a table.
 *
 * @param [config] The neo4j client configuration.
 * @return `true` if wrap markers are enabled, and `false` otherwise.
 */
bool neo4j_config_get_render_wrap_markers(const neo4j_config_t *config);

/**
 * Set the number of results to inspect when determining column widths.
 *
 * If set to 0, no inspection will occur.
 *
 * @note This only applies when rendering results to a table.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [rows] The number of results to inspect.
 */
void neo4j_config_set_render_inspect_rows(neo4j_config_t *config,
        unsigned int rows);

/**
 * Set the number of results to inspect when determining column widths.
 *
 * @note This only applies when rendering results to a table.
 *
 * @param [config] The neo4j client configuration.
 * @return The number of results that will be inspected to determine column
 *         widths.
 */
unsigned int neo4j_config_get_render_inspect_rows(
        const neo4j_config_t *config);


struct neo4j_results_table_colors
{
    const char * const border[2];
    const char * const header[2];
    const char * const cells[2];
};

/** Results table colorization rules for uncolorized table output. */
extern const struct neo4j_results_table_colors *
        neo4j_results_table_no_colors;
/** Results table colorization rules for ANSI terminal output. */
extern const struct neo4j_results_table_colors *
        neo4j_results_table_ansi_colors;

/**
 * Set the colorization rules for rendering of results tables.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [colors] Colorization rules for result tables. The pointer must
 *         remain valid until the config (and any duplicates) have been
 *         released.
 */
void neo4j_config_set_results_table_colors(neo4j_config_t *config,
        const struct neo4j_results_table_colors *colors);

/**
 * Get the colorization rules for rendering of results tables.
 *
 * @param [config] The neo4j client configuration to update.
 * @return The colorization rules for result table rendering.
 */
const struct neo4j_results_table_colors *neo4j_config_get_results_table_colors(
        const neo4j_config_t *config);


struct neo4j_plan_table_colors
{
    const char * const border[2];
    const char * const header[2];
    const char * const cells[2];
    const char * const graph[2];
};

/** Plan table colorization rules for uncolorized plan table output. */
extern const struct neo4j_plan_table_colors *neo4j_plan_table_no_colors;
/** Plan table colorization rules for ANSI terminal output. */
extern const struct neo4j_plan_table_colors *neo4j_plan_table_ansi_colors;

/**
 * Set the colorization rules for rendering of plan tables.
 *
 * @param [config] The neo4j client configuration to update.
 * @param [colors] Colorization rules for plan tables.  The pointer must
 *         remain valid until the config (and any duplicates) have been
 *         released.
 */
void neo4j_config_set_plan_table_colors(neo4j_config_t *config,
        const struct neo4j_plan_table_colors *colors);

/**
 * Get the colorization rules for rendering of plan tables.
 *
 * @param [config] The neo4j client configuration to update.
 * @return The colorization rules for plan table rendering.
 */
const struct neo4j_plan_table_colors *neo4j_config_get_plan_table_colorization(
        const neo4j_config_t *config);



#define NEO4J_RENDER_MAX_WIDTH 4095

/**
 * Render a result stream as a table.
 *
 * A bitmask of flags may be supplied, which may include:
 * - NEO4J_RENDER_SHOW_NULLS - output 'null' when rendering NULL values, rather
 * than an empty cell.
 * - NEO4J_RENDER_QUOTE_STRINGS - wrap strings in quotes.
 * - NEO4J_RENDER_ASCII - use only ASCII characters when rendering.
 * - NEO4J_RENDER_ROWLINES - render a line between each output row.
 * - NEO4J_RENDER_WRAP_VALUES - wrap oversized values over multiple lines.
 * - NEO4J_RENDER_NO_WRAP_MARKERS - don't indicate wrapping of values (should
 * be used with NEO4J_RENDER_ROWLINES).
 * - NEO4J_RENDER_ANSI_COLOR - use ANSI escape codes for colorization.
 *
 * If no flags are required, pass 0 or `NEO4J_RENDER_DEFAULT`.
 *
 * @attention The output will be written to the stream using UTF-8 encoding.
 *
 * @param [stream] The stream to render to.
 * @param [results] The results stream to render.
 * @param [width] The width of the table to render.
 * @param [flags] A bitmask of flags to control rendering.
 * @return 0 on success, or -1 on error (errno will be set).
 */
__neo4j_must_check
int neo4j_render_table(FILE *stream, neo4j_result_stream_t *results,
        unsigned int width, uint_fast32_t flags);

/**
 * Render a result stream as a table.
 *
 * @attention The output will be written to the stream using UTF-8 encoding.
 *
 * @param [config] A neo4j client configuration.
 * @param [stream] The stream to render to.
 * @param [results] The results stream to render.
 * @param [width] The width of the table to render.
 * @return 0 on success, or -1 on error (errno will be set).
 */
__neo4j_must_check
int neo4j_render_results_table(const neo4j_config_t *config, FILE *stream,
        neo4j_result_stream_t *results, unsigned int width);

/**
 * Render a result stream as comma separated value.
 *
 * A bitmask of flags may be supplied, which may include:
 * - NEO4J_RENDER_SHOW_NULL - output 'null' when rendering NULL values, rather
 * than an empty cell.
 *
 * If no flags are required, pass 0 or `NEO4J_RENDER_DEFAULT`.
 *
 * @attention The output will be written to the stream using UTF-8 encoding.
 *
 * @param [stream] The stream to render to.
 * @param [results] The results stream to render.
 * @param [flags] A bitmask of flags to control rendering.
 * @return 0 on success, or -1 on error (errno will be set).
 */
__neo4j_must_check
int neo4j_render_csv(FILE *stream, neo4j_result_stream_t *results,
        uint_fast32_t flags);

/**
 * Render a result stream as comma separated value.
 *
 * @attention The output will be written to the stream using UTF-8 encoding.
 *
 * @param [config] A neo4j client configuration.
 * @param [stream] The stream to render to.
 * @param [results] The results stream to render.
 * @return 0 on success, or -1 on error (errno will be set).
 */
__neo4j_must_check
int neo4j_render_results_csv(const neo4j_config_t *config, FILE *stream,
        neo4j_result_stream_t *results);
__neo4j_must_check
int neo4j_render_ccsv(const neo4j_config_t *config, FILE *stream,
        neo4j_result_stream_t *results) __neo4j_deprecated;

/**
 * Render a statement plan as a table.
 *
 * A bitmask of flags may be supplied, which may include:
 * - NEO4J_RENDER_ASCII - use only ASCII characters when rendering.
 * - NEO4J_RENDER_ANSI_COLOR - use ANSI escape codes for colorization.
 *
 * If no flags are required, pass 0 or `NEO4J_RENDER_DEFAULT`.
 *
 * @attention The output will be written to the stream using UTF-8 encoding.
 *
 * @param [stream] The stream to render to.
 * @param [plan] The statement plan to render.
 * @param [width] The width of the table to render.
 * @param [flags] A bitmask of flags to control rendering.
 * @return 0 on success, or -1 on error (errno will be set).
 */
__neo4j_must_check
int neo4j_render_plan_table(FILE *stream, struct neo4j_statement_plan *plan,
        unsigned int width, uint_fast32_t flags);

/**
 * Render a statement plan as a table.
 *
 * @attention The output will be written to the stream using UTF-8 encoding.
 *
 * @param [config] A neo4j client configuration.
 * @param [stream] The stream to render to.
 * @param [plan] The statement plan to render.
 * @param [width] The width of the table to render.
 * @return 0 on success, or -1 on error (errno will be set).
 */
__neo4j_must_check
int neo4j_render_plan_ctable(const neo4j_config_t *config, FILE *stream,
        struct neo4j_statement_plan *plan, unsigned int width);


/*
 * =====================================
 * utility methods
 * =====================================
 */

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
 * @return The length of the parent directory path, or -1 on error
 *         (errno will be set).
 */
ssize_t neo4j_dirname(const char *path, char *buffer, size_t n);

/**
 * Obtain the parent directory of a specified path.
 *
 * Any trailing '/' characters are not counted as part of the directory name.
 * If `path` is `NULL`, the empty string, or contains no '/' characters, the
 * path "." is placed into the result buffer.
 *
 * @param [path] The path.
 * @return The resulting path as a null-terminated string (which must
 *         later be deallocated by calling free()) or `NULL` on error
 *         (errno will be set).
 */
char *neo4j_adirname(const char *path);

/**
 * Obtain the basename of a specified path.
 *
 * @param [path] The path.
 * @param [buffer] A buffer to place the base name into, or `NULL`.
 * @param [n] The length of the buffer.
 * @return The length of the base name, or -1 on error (errno will be set).
 */
ssize_t neo4j_basename(const char *path, char *buffer, size_t n);

/**
 * Obtain the basename of a specified path.
 *
 * @param [path] The path.
 * @return The resulting path as a null-terminated string (which must
 *         later be deallocated by calling free()) or `NULL` on error
 *         (errno will be set).
 */
char *neo4j_abasename(const char *path);

/**
 * Create a directory and any required parent directories.
 *
 * Directories are created with default permissions as per the users umask.
 *
 * @param [path] The path of the directory to create.
 * @return 0 on success, or -1 on error (errno will be set).
 */
__neo4j_must_check
int neo4j_mkdir_p(const char *path);

/**
 * Return the number of bytes in a UTF-8 character.
 *
 * @param [s] The sequence of bytes containing the character.
 * @param [n] The maximum number of bytes to inspect.
 * @return The length, in bytes, of the UTF-8 character, or -1 if a
 *         decoding error occurs (errno will be set).
 */
int neo4j_u8clen(const char *s, size_t n);

/**
 * Return the column width of a UTF-8 character.
 *
 * @param [s] The sequence of bytes containing the character.
 * @param [n] The maximum number of bytes to inspect.
 * @return The width, in columns, of the UTF-8 character, or -1 if the
 *         character is unprintable or cannot be decoded.
 */
int neo4j_u8cwidth(const char *s, size_t n);

/**
 * Return the Unicode codepoint of a UTF-8 character.
 *
 * @param [s] The sequence of bytes containing the character.
 * @param [n] A ponter to a `size_t` containing the maximum number of bytes
 *        to inspect. On successful return, this will be updated to contain
 *        the number of bytes consumed by the character.
 * @return The codepoint, or -1 if a decoding error occurs (errno will be set).
 */
int neo4j_u8codepoint(const char *s, size_t *n);

/**
 * Return the column width of a Unicode codepoint.
 *
 * @param [cp] The codepoint value.
 * @return The width, in columns, of the Unicode codepoint, or -1 if the
 *         codepoint is unprintable.
 */
int neo4j_u8cpwidth(int cp);


/**
 * Return the column width of a UTF-8 string.
 *
 * @param [s] The UTF-8 encoded string.
 * @param [n] The maximum number of bytes to inspect.
 * @return The width, in columns, of the UTF-8 string.
 */
int neo4j_u8cswidth(const char *s, size_t n);


#pragma GCC visibility pop

#ifdef __cplusplus
}
#endif

#endif/*NEO4J_CLIENT_H*/
