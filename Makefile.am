include aminclude.am

SUBDIRS = m4 src .
ACLOCAL_AMFLAGS = -I m4

man1_MANS = neo4j-client.1

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

EXTRA_DIST = \
	doxygen.cfg \
	LICENSE \
	neo4j-client.1.in \
	README.md

docker-check:
	$(MAKE) dist
	id=`docker create -i debian-dev \
		sh -c 'cd $(PACKAGE_TARNAME)-$(PACKAGE_VERSION) && ./configure --enable-silent-rules && make check'` && \
	trap "docker rm $$id" EXIT && \
	docker cp - $$id:/ < $(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.gz && \
	docker start -i $$id

docker-valgrind:
	$(MAKE) dist
	id=`docker create -i debian-dev \
		sh -c 'cd $(PACKAGE_TARNAME)-$(PACKAGE_VERSION) && ./configure --enable-silent-rules && make check && CK_FORK=no valgrind --suppressions=valgrind.suppressions --leak-check=full ./tests/check_libneo4j-client'` && \
	trap "docker rm $$id" EXIT && \
	docker cp - $$id:/ < $(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.gz && \
	docker cp valgrind.suppressions $$id:/$(PACKAGE_TARNAME)-$(PACKAGE_VERSION) && \
	docker start -i $$id

clean-local:
	rm -rf doc src/.doc
