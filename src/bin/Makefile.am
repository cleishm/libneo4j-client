bin_PROGRAMS = neo4j-client

neo4j_client_SOURCES = neo4j-client.c \
	batch.c \
	batch.h \
	evaluate.c \
	evaluate.h \
	interactive.c \
	interactive.h \
	render.c \
	render.h \
	state.c \
	state.h \
	verification.c \
	verification.h
neo4j_client_CPPFLAGS = -I${top_builddir}/src/lib
neo4j_client_LDADD = ${top_builddir}/src/lib/libneo4j-client.la ${LIBEDIT_LIBS}
