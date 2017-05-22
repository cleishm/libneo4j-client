# Copyright 2016, Chris Leishman (http://github.com/cleishm)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
Summary: Client library for the Neo4j graph database
Name: libneo4j-client
Version: 2.1.1
Release: 1%{?dist}
Group: System Environment/Libraries
License: Apache-2.0
URL: https://github.com/cleishm/libneo4j-client
Source0: https://github.com/cleishm/libneo4j-client/releases/download/%{version}/%{name}-%{version}.tar.gz
BuildRequires: libcypher-parser-devel openssl-devel libedit-devel doxygen pkgconfig

%description
libneo4j-client is a client library and command line shell for Neo4j.

%define sover 11
%define libname %{name}%{sover}

%prep
%setup -q

%build
%configure --disable-static
make
make doc

%check
make check

%install
%make_install
rm $RPM_BUILD_ROOT%{_libdir}/*.la

#------------------------------------------------------------------------------

%package -n %{libname}
Summary: Client library for the Neo4j graph database
Group: System Environment/Libraries
Provides: %{name}

%description -n %{libname}
libneo4j-client takes care of all the detail of establishing a session with a
Neo4j server, sending statements for evaluation, and retrieving results.

%post -n %{libname} -p /sbin/ldconfig
%postun -n %{libname} -p /sbin/ldconfig

%files -n %{libname}
%defattr(-, root, root)
%doc README.md
%{_libdir}/*.so.*


#------------------------------------------------------------------------------

%package -n %{name}-devel
Summary: Development files for %{name}
Group: Development/Libraries
Requires: %{libname}%{?_isa} = %{version}-%{release}

%description -n %{name}-devel
libneo4j-client takes care of all the detail of establishing a session with a
Neo4j server, sending statements for evaluation, and retrieving results.

This package contains the development files (headers, static libraries)

%files -n %{name}-devel
%defattr(-, root, root)
%{_includedir}/neo4j-client.h
%{_libdir}/*.so
%{_libdir}/pkgconfig/*.pc

#------------------------------------------------------------------------------

%package -n %{name}-devel-doc
Summary: Development documentation for %{name}
Group: Development/Libraries
BuildArch: noarch

%description -n %{name}-devel-doc
libneo4j-client takes care of all the detail of establishing a session with a
Neo4j server, sending statements for evaluation, and retrieving results.

This package contains the API documentation that is also available on the
libneo4j-client website (https://github.com/cleishm/libneo4j-client).

%files -n %{name}-devel-doc
%defattr(-, root, root)
%doc doc/html

#------------------------------------------------------------------------------

%package -n neo4j-client
Summary: Command line shell for Neo4j
Group: Applications/Databases

%description -n neo4j-client
neo4j-client supports secure connections to Neo4j server, sending of statements
(including multiline statements), persistent command history, and rendering
of results to tables or CSV.

%files -n neo4j-client
%defattr(-, root, root)
%{_bindir}/neo4j-client
%{_mandir}/man1/neo4j-client.1*

#------------------------------------------------------------------------------

%changelog
* Tue Apr 25 2017 Chris Leishman <chris@leishman.org> - 2.0.0-1
- Upstream release 2.0.0
* Sat Aug 13 2016 Chris Leishman <chris@leishman.org> - 1.2.0-1
- Upstream release 1.2.0
* Mon Jul 18 2016 Chris Leishman <chris@leishman.org> - 1.1.0-1
- Upstream release 1.1.0
* Thu Jun 30 2016 Chris Leishman <chris@leishman.org> - 1.0.0-2
- Changed -devel package names to remove soname
* Mon Jun 27 2016 Chris Leishman <chris@leishman.org> - 1.0.0-1
- Upstream release 1.0.0
* Sat May 21 2016 Chris Leishman <chris@leishman.org> - 0.9.2-1
- Upstream release 0.9.2
* Mon Apr 25 2016 Chris Leishman <chris@leishman.org> - 0.9.1-1
- Upstream release 0.9.1
* Wed Mar 09 2016 Chris Leishman <chris@leishman.org> - 0.9.0-1
- Upstream release 0.9.0
* Thu Feb 11 2016 Chris Leishman <chris@leishman.org> - 0.8.1-1
- Upstream release 0.8.1
* Tue Feb 09 2016 Chris Leishman <chris@leishman.org> - 0.8.0-1
- Upstream release 0.8.0
* Tue Feb 02 2016 Chris Leishman <chris@leishman.org> - 0.7.1-1
-Initial RPM Release
