bin_PROGRAMS = neo4j-client

neo4j_client_SOURCES = neo4j-client.c \
	authentication.c \
	authentication.h \
	batch.c \
	batch.h \
	colorization.c \
	colorization.h \
	commands.c \
	commands.h \
	connect.c \
	connect.h \
	evaluate.c \
	evaluate.h \
	interactive.c \
	interactive.h \
	options.c \
	options.h \
	readpass.c \
	readpass.h \
	render.c \
	render.h \
	state.c \
	state.h \
	util.c \
	util.h \
	verification.c \
	verification.h

neo4j_client_CPPFLAGS = \
	-I$(top_srcdir)/lib/src \
	$(LIBEDIT_CFLAGS) $(CYPHER_PARSER_CFLAGS)
neo4j_client_LDADD = \
	$(top_builddir)/lib/src/libneo4j-client.la \
	$(LIBEDIT_LIBS) $(CYPHER_PARSER_LIBS)
