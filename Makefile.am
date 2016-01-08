include aminclude.am

SUBDIRS = m4 src .
ACLOCAL_AMFLAGS = -I m4

if HAVE_CHECK
SUBDIRS += tests
endif

doc: doxygen-doc

doc/libneo4j-client.tag: src/.doc/neo4j-client.h
src/.doc/neo4j-client.h: src/lib/neo4j-client.h
	@mkdir -p src/.doc
	(cat "$<" && sed -ne "s/^.*@fn \(.*\)$$/\1;/p" < "$<") >> "$@"

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = neo4j-client.pc

docker-check:
	$(MAKE) dist
	id=`docker create -i debian-dev \
		sh -c 'cd $(PACKAGE_TARNAME)-$(PACKAGE_VERSION) && ./configure && make check'` && \
	docker cp - $$id:/ < $(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.gz && \
	docker start -i $$id

clean-local:
	rm -rf doc src/.doc
