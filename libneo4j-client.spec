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
Version: 0.7.1
Release: 1%{?dist}
Group: System Environment/Libraries
License: Apache License, Version 2.0
URL: https://github.com/cleishm/libneo4j-client
Source0: https://github.com/cleishm/libneo4j-client/releases/download/%{version}/%{name}-%{version}.tar.gz
BuildRequires: openssl-devel libedit-devel doxygen pkgconfig

%description
libneo4j-client is a client library and command line shell for Neo4j.

%define sover 4
%define libname %{name}%{sover}

%prep
%setup -q

%build
%configure --disable-static
make check
make doc

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
%doc README.md
%{_libdir}/*.so.*


#------------------------------------------------------------------------------

%package -n %{libname}-devel
Summary: Development files for %{libname}
Group: Development/Libraries
Requires: %{libname}%{?_isa} = %{version}-%{release}

%description -n %{libname}-devel
libneo4j-client takes care of all the detail of establishing a session with a
Neo4j server, sending statements for evaluation, and retrieving results.

This package contains the development files (headers, static libraries)

%files -n %{libname}-devel
%{_includedir}/neo4j-client.h
%{_libdir}/*.so
%{_libdir}/pkgconfig/*.pc

#------------------------------------------------------------------------------

%package -n %{libname}-devel-doc
Summary: Development documentation for %{libname}
Group: Development/Libraries
BuildArch: noarch

%description -n %{libname}-devel-doc
libneo4j-client takes care of all the detail of establishing a session with a
Neo4j server, sending statements for evaluation, and retrieving results.

This package contains the API documentation that is also available on the
libneo4j-client website (https://github.com/cleishm/libneo4j-client).

%files -n %{libname}-devel-doc
%doc doc/html

#------------------------------------------------------------------------------

%package -n neo4j-client
Summary: Command line shell for Neo4j
Group: Applications/Databases
Requires: %{libname}%{?_isa}

%description -n neo4j-client
neo4j-client supports secure connections to Neo4j server, sending of statements
(including multiline statements), persistent command history, and rendering
of results to tables or CSV.

%files -n neo4j-client
%{_bindir}/neo4j-client
%{_mandir}/man1/neo4j-client.1*

#------------------------------------------------------------------------------

%changelog
* Tue Feb 02 2016 Chris Leishman <chris@leishman.org> - 0.7.1-1
-Initial RPM Release
