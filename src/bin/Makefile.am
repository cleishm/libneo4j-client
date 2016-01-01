bin_PROGRAMS = neo4j-client

neo4j_client_SOURCES = neo4j-client.c \
	batch.c \
	batch.h \
	commands.c \
	commands.h \
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
neo4j_client_CPPFLAGS = -I${top_builddir}/src/lib
neo4j_client_LDADD = ${LIBEDIT_LIBS} -L${top_builddir}/src/lib -lneo4j-client
