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
	docker run --rm -v $(abs_builddir):/$(PACKAGE) -ti debian-unstable-dev \
		sh -c 'tar xzmf /$(PACKAGE)/$(DIST_ARCHIVES) && cd $(distdir) && ./configure --enable-silent-rules && make check'

docker-valgrind:
	$(MAKE) dist
	docker run --rm -v $(abs_builddir):/$(PACKAGE) -ti debian-unstable-dev \
		sh -c 'tar xzmf /$(PACKAGE)/$(DIST_ARCHIVES) && cd $(distdir) && ./configure --enable-silent-rules --disable-debug && make check && CK_FORK=no valgrind --suppressions="/$(PACKAGE)/valgrind.suppressions" --leak-check=full --show-leak-kinds=all ./tests/check_$(PACKAGE)'

clean-local:
	rm -rf doc src/.doc
