TESTS = check_libneo4j-client
check_PROGRAMS = check_libneo4j-client
check_libneo4j_client_SOURCES = \
	${check_libneo4j_client_CHECKS} \
	check_libneo4j-client.c \
	check_libneo4j-client_suite.c \
	canned_result_stream.c \
	canned_result_stream.h \
	memiostream.c \
	memiostream.h \
	memstream.c \
	memstream.h \
	util.c \
	util.h

check_libneo4j_client_CHECKS = \
	check_buffering_iostream.c \
	check_chunking_iostream.c \
	check_config.c \
	check_connection.c \
	check_deserialization.c \
	check_dotdir.c \
	check_error_handling.c \
	check_logging.c \
	check_memory.c \
	check_fit_column_widths.c \
	check_render_plan.c \
	check_render_results.c \
	check_result_stream.c \
	check_ring_buffer.c \
	check_serialization.c \
	check_tofu.c \
	check_u8.c \
	check_uri.c \
	check_util.c \
	check_values.c

if WITH_TLS
if HAVE_OPENSSL
check_libneo4j_client_CHECKS += \
	check_openssl.c
endif
endif

check_libneo4j-client_suite.c: ${check_libneo4j_client_CHECKS}
	@(names=`for check in $^; do \
		basename $$check .c | sed -e 's/^check_//'; \
	done`; \
	echo "#include <check.h>"; \
	echo; \
	for name in $$names; do \
		echo "TCase* $${name}_tcase(void);"; \
	done; \
	echo; \
	echo "Suite *libneo4j_client_suite(void)"; \
	echo "{"; \
	echo "    Suite *s = suite_create(\"libneo4j-client\");"; \
	for name in $$names; do \
		echo "    suite_add_tcase(s, $${name}_tcase());"; \
	done; \
	echo "    return s;"; \
	echo "}") > $@

check_libneo4j_client_CFLAGS = @CHECK_CFLAGS@
check_libneo4j_client_LDFLAGS = -static
check_libneo4j_client_LDADD = ../src/libneo4j-client.la @CHECK_LIBS@

CLEANFILES = check_libneo4j-client_suite.c
