TESTS = check_libneo4j-client
check_PROGRAMS = check_libneo4j-client
check_libneo4j_client_SOURCES = \
	${check_libneo4j_client_CHECKS} \
	check_libneo4j-client.c \
	check_libneo4j-client_suite.c \
	buffered_iostream.c \
	buffered_iostream.h \
	canned_result_stream.c \
	canned_result_stream.h \
	memstream.c \
	memstream.h \
	util.c \
	util.h

check_libneo4j_client_CHECKS = \
	check_chunking_iostream.c \
	check_cli_parser.c \
	check_config.c \
	check_connection.c \
	check_deserialization.c \
	check_dotdir.c \
	check_error_handling.c \
	check_job.c \
	check_logging.c \
	check_memory.c \
	check_render.c \
	check_ring_buffer.c \
	check_serialization.c \
	check_session.c \
	check_tofu.c \
	check_uri.c \
	check_util.c \
	check_values.c

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
check_libneo4j_client_LDADD = \
	$(top_builddir)/src/lib/libneo4j-client.la @CHECK_LIBS@

MAINTAINERCLEANFILES = check_libneo4j-client_suite.c
