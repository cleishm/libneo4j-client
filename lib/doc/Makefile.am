include $(top_srcdir)/build-aux/automake/doxygen.am

if DX_COND_doc
all-local:
	$(MAKE) doxygen-doc
endif

libneo4j-client.tag: .deps/neo4j-client.h
.deps/neo4j-client.h: ../src/neo4j-client.h
	@mkdir -p .deps
	(cat "$<" && sed -ne "s/^.*@fn \(.*\)$$/\1;/p" < "$<") > "$@"

EXTRA_DIST = doxygen.cfg

clean-local:
	rm -rf .deps html latex libneo4j-client.tag libneo4j-client.pdf
