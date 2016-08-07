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
dnl Process this file with autoconf to produce a configure script
AC_PREREQ([2.69])
AC_INIT([libneo4j-client],[1.1.1~devel])
AC_CONFIG_SRCDIR([src/lib/neo4j-client.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE([enable])

WITH_LIBS

GCC_CFLAGS="-fvisibility=hidden -pipe -Wall -W -Werror \
    -Wno-unused-parameter -Wno-missing-field-initializers -Wpointer-arith \
    -Wstrict-prototypes -Wcast-qual -Wcast-align -Wno-error=unused-function \
    -Wno-error=unused-variable -Wno-error=deprecated-declarations"

LT_INIT
AC_LANG([C])
AC_PROG_CC
PROG_CC_C11
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

AC_C_CONST
AC_C_INLINE
AC_C_BIGENDIAN
AC_C_VARARRAYS

AS_IF([test "X$ac_cv_c_vararrays" != "Xyes"],
  [AC_MSG_ERROR([compiler support for variable-length arrays is required])])

AC_HEADER_ASSERT
AC_HEADER_STDC
AC_HEADER_STDBOOL
AC_CHECK_HEADERS([endian.h sys/endian.h libkern/OSByteOrder.h])
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_FUNC_STRERROR_R
MACRO_STATIC_ASSERT
LT_LIB_M

AC_CHECK_TYPE([socklen_t],[],
  [AC_DEFINE([socklen_t],[int],[Substitute for socklen_t.])],
  [
#include <sys/types.h>
#include <sys/socket.h>
  ])

AC_CHECK_DECL([AI_ADDRCONFIG],[AC_DEFINE([HAVE_AI_ADDRCONFIG],[1],
  [Define to 1 if you have AI_ADDRCONFIG in the <netdb.h> header file.])],
  [],
  [
#include <netdb.h>
  ])

AC_CHECK_DECL([htonll],[AC_DEFINE([HAVE_HTONLL],[1],
  [Define to 1 if you have htonll.])])
AC_CHECK_DECL([ntohll],[AC_DEFINE([HAVE_NTOHLL],[1],
  [Define to 1 if you have ntohll.])])


AC_CHECK_DECL([htobe64],[AC_DEFINE([HAVE_HTOBE64],[1],
  [Define to 1 if you have htobe64.])],
  [],
  [
#ifdef HAVE_ENDIAN_H
#include <endian.h>
#endif
#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif
  ])
AC_CHECK_DECL([be64toh],[AC_DEFINE([HAVE_BE64TOH],[1],
  [Define to 1 if you have be64toh.])],
  [],
  [
#ifdef HAVE_ENDIAN_H
#include <endian.h>
#endif
#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif
  ])


AC_CHECK_DECL([OSSwapHostToBigInt64],[AC_DEFINE([HAVE_OSSWAPHOSTTOBIGINT64],[1],
  [Define to 1 if you have OSSwapHostToBigInt64 in the <libkern/OSByteOrder.h> header file.])],
  [],
  [
#ifdef HAVE_LIBKERN_OSBYTEORDER_H
#include <libkern/OSByteOrder.h>
#endif
  ])
AC_CHECK_DECL([OSSwapBigToHostInt64],[AC_DEFINE([HAVE_OSSWAPBIGTOHOSTINT64],[1],
  [Define to 1 if you have OSSwapBigToHostInt64 in the <libkern/OSByteOrder.h> header file.])],
  [],
  [
#ifdef HAVE_LIBKERN_OSBYTEORDER_H
#include <libkern/OSByteOrder.h>
#endif
  ])

AC_CHECK_DECL([bswap_64],[AC_DEFINE([HAVE_BSWAP_64],[1],
  [Define to 1 if you have bswap_64.])])

AC_CHECK_FUNCS([mkstemp],[],[AC_MSG_ERROR([A working mkstemp is required])])
AC_CHECK_FUNCS([open_memstream],[],[])

AC_CHECK_HEADERS([readpassphrase.h bsd/readpassphrase.h])
AC_SEARCH_LIBS([readpassphrase],[bsd])
AC_CHECK_FUNCS([readpassphrase],[],[])


dnl Threading
AX_THREAD_LOCAL()
AX_PTHREAD([has_pthreads=yes])
AS_IF([test "X$has_pthreads" = "Xyes"],
  [AC_DEFINE([HAVE_PTHREADS], [1], [Define to 1 if you have pthreads.])])

AC_ARG_ENABLE([thread_local_iovectors], AS_HELP_STRING([--disable-thread-local-iovectors], [Disable thread local IO vectors]))
AS_IF([test "X$enable_thread_local_iovectors" = "Xno"],
  [AC_DEFINE([NO_THREAD_LOCAL_IOV], [1], [Define to 1 to disable thread local IO vectors.])])


dnl Check for TLS support
AC_ARG_WITH([tls],
  [AS_HELP_STRING([--without-tls], [Build without TLS support])],
  [], [with_tls=yes])

AS_IF([test "X$with_tls" = "Xyes" -o "X$with_tls" = "Xopenssl"],
  [PKG_CHECK_MODULES([OPENSSL], [openssl >= 0.9.7],
    [has_openssl=yes; has_tls=yes],
    [AC_MSG_ERROR([openssl >= 0.9.7 not found (specify --without-tls to ignore)])])])

AS_IF([test "X$has_openssl" = "Xyes"],
  [AC_DEFINE([HAVE_OPENSSL], [1], [Define to 1 if you have openssl.])
   REQUIRES="$REQUIRES, openssl >= 0.9.7"
   save_LIBS="$LIBS"
   LIBS="$OPENSSL_LIBS $LIBS"
   AC_CHECK_FUNCS([CRYPTO_set_locking_callback BIO_meth_new],[],[])
   LIBS="$save_LIBS"
  ])
AM_CONDITIONAL([HAVE_OPENSSL], [test "X$has_openssl" = "Xyes"])

AS_IF([test "X$has_tls" = "Xyes"],
  [AC_DEFINE([HAVE_TLS], [1], [Define to 1 if tls support is enabled.])],
  [test "X$with_tls" != "Xno"],
  [AC_MSG_ERROR([TLS library "$with_tls" not found])])
AM_CONDITIONAL([WITH_TLS], [test "X$has_tls" != "Xno"])


dnl Check for check test support
PKG_CHECK_MODULES([CHECK], [check >= 0.9.4], [has_check=yes], [has_check=no])
AM_CONDITIONAL([HAVE_CHECK], [test "X$has_check" = "Xyes"])


dnl Check if command line tools should be built
AC_ARG_ENABLE([tools],
  AS_HELP_STRING([--disable-tools], [Do not build command line tools]))
AM_CONDITIONAL([WANT_TOOLS], [test "X$enable_tools" != "Xno"])

dnl Check for required libraries for tools
PKG_CHECK_MODULES([LIBEDIT], [libedit], [has_libedit=yes], [has_libedit=no])
AS_IF([test "X$enable_tools" != "Xno" -a "X$has_libedit" != "Xyes"],
  [AC_MSG_ERROR([libedit is required for building command line tools (specify --disable-tools to ignore)])])

PKG_CHECK_MODULES([CYPHER_PARSER], [cypher-parser >= 0.5.0], [has_cypher_parser=yes], [has_cypher_parser=no])
AS_IF([test "X$enable_tools" != "Xno" -a "X$has_cypher_parser" != "Xyes"],
  [AC_MSG_ERROR([libcypher-parser (>= 0.5.0) is required for building command line tools (specify --disable-tools to ignore)])])


dnl Check if assertions should be disabled
AC_ARG_ENABLE([debug], AS_HELP_STRING([--disable-debug], [Disable debug]))
AS_IF([test "X$enable_debug" = "Xno"],
  [AC_DEFINE([NDEBUG], [1], [Define to 1 to disable assertions.])])


dnl Add GCC flags
AS_IF([test "X$GCC" = "Xyes"], [CFLAGS="$CFLAGS $GCC_CFLAGS"])


dnl Check for doxygen support
DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(ON)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(ON)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN(libneo4j-client, doxygen.cfg, doc)

AC_SUBST([REQUIRES], [`echo "$REQUIRES" | cut -c3-`])

AC_CONFIG_FILES([
    neo4j-client.1 \
    neo4j-client.pc \
    Makefile \
    m4/Makefile \
    src/Makefile \
    src/bin/Makefile \
    src/lib/Makefile \
    tests/Makefile
])
AC_OUTPUT
