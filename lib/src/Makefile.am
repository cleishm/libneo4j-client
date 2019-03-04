lib_LTLIBRARIES = libneo4j-client.la

include_HEADERS = neo4j-client.h
libneo4j_client_la_SOURCES = \
	atomic.h \
	error_handling.c \
	buffering_iostream.c \
	buffering_iostream.h \
	chunking_iostream.c \
	chunking_iostream.h \
	client_config.c \
	client_config.h \
	connection.c \
	connection.h \
	deserialization.c \
	deserialization.h \
	dotdir.c \
	init.c \
	iostream.c \
	iostream.h \
	job.h \
	logging.c \
	logging.h \
	memory.c \
	memory.h \
	messages.c \
	messages.h \
	metadata.c \
	metadata.h \
	network.c \
	network.h \
	print.c \
	print.h \
	posix_iostream.c \
	posix_iostream.h \
	render.c \
	render.h \
	render_plan.c \
	render_results.c \
	result_stream.c \
	result_stream.h \
	ring_buffer.c \
	ring_buffer.h \
	serialization.c \
	serialization.h \
	thread.c \
	thread.h \
	tofu.c \
	tofu.h \
	u8.c \
	uri.c \
	uri.h \
	util.c \
	util.h \
	values.c \
	values.h

if WITH_TLS
if HAVE_OPENSSL
libneo4j_client_la_SOURCES += \
	openssl.c \
	openssl.h \
	openssl_iostream.c \
	openssl_iostream.h
endif
endif

libneo4j_client_la_CFLAGS = $(PTHREAD_CFLAGS) $(OPENSSL_CFLAGS) -Wpedantic -Wvla
libneo4j_client_la_LIBADD = $(PTHREAD_LIBS) $(OPENSSL_LIBS) $(LIBM)
# Updating the version-info:
# 1. If the library source code has changed at all since the last update, then
#    increment revision (‘c:r:a’ becomes ‘c:r+1:a’).
# 2. If any interfaces have been added, removed, or changed since the last
#    update, increment current, and set revision to 0.
# 3. If any interfaces have been added since the last public release, then
#    increment age.
# 4. If any interfaces have been removed or changed since the last public
#    release, then set age to 0.
libneo4j_client_la_LDFLAGS = -version-info 14:0:3
