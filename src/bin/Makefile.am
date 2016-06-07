bin_PROGRAMS = neo4j-client

neo4j_client_SOURCES = neo4j-client.c \
	batch.c \
	batch.h \
	batch_parser.c \
	batch_parser.leg \
	batch_parser.h \
	evaluate.c \
	evaluate.h \
	interactive.c \
	interactive.h \
	render.c \
	render.h \
	state.c \
	state.h \
	util.c \
	util.h \
	verification.c \
	verification.h

neo4j_client_CPPFLAGS = \
	-I${top_builddir}/src/lib \
	$(LIBEDIT_CFLAGS) $(CYPHER_PARSER_CFLAGS)
neo4j_client_LDADD = \
	${top_builddir}/src/lib/libneo4j-client.la \
	$(LIBEDIT_LIBS) $(CYPHER_PARSER_LIBS)

batch_parser.c: batch_parser_leg.c

if HAVE_LEG
%_leg.c: %.leg
	$(LEG) -o $@ $<
else
%_leg.c: %.leg
	$(error Cannot compile .leg source file, as the leg compiler was not found by the configure script: please install http://piumarta.com/software/peg/ and re-run the configure script)
endif

EXTRA_DIST = batch_parser_leg.c
MAINTAINERCLEANFILES = batch_parser_leg.c
