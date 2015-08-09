dnl Process this file with autoconf to produce a configure script.
dnl
dnl This is the SysDB configure script.
dnl
dnl Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>
dnl All rights reserved.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions
dnl are met:
dnl 1. Redistributions of source code must retain the above copyright
dnl    notice, this list of conditions and the following disclaimer.
dnl 2. Redistributions in binary form must reproduce the above copyright
dnl    notice, this list of conditions and the following disclaimer in the
dnl    documentation and/or other materials provided with the distribution.
dnl
dnl THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
dnl ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
dnl TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
dnl PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
dnl CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
dnl EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
dnl PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
dnl OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
dnl WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
dnl OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
dnl ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

AC_INIT([System DataBase],[m4_esyscmd(./version-gen.sh)],
		[sysdb@sysdb.io],
		[sysdb],
		[https://sysdb.io/])
PACKAGE_MAINTAINER="Sebastian 'tokkee' Harl <tokkee@sysdb.io>"
AC_DEFINE_UNQUOTED([PACKAGE_MAINTAINER], ["$PACKAGE_MAINTAINER"],
		[Define to the name of the maintainer of this package.])
if test "x$PACKAGE_URL" = "x"; then
	PACKAGE_URL="https://sysdb.io/"
	AC_DEFINE_UNQUOTED([PACKAGE_URL], ["$PACKAGE_URL"],
			[Define to the home page for this package.])
fi

AC_CONFIG_SRCDIR([src/sysdb.c])
AC_CONFIG_HEADERS([src/config.h])
AC_PREFIX_DEFAULT([/opt/sysdb])

AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])

AC_LANG(C)

AC_SYS_LARGEFILE

AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
m4_ifdef([AM_PROG_AR],[AM_PROG_AR],[])

AM_PROG_CC_C_O
AM_PROG_LEX
AC_PROG_YACC

m4_ifdef([LT_INIT],
	[
	 LT_CONFIG_LTDL_DIR([libltdl])
	 LT_INIT([dlopen])
	 LTDL_INIT([convenience])
	],
	# else
	# (older libtools)
	[
	 AC_CONFIG_SUBDIRS(libltdl)
	 AC_LIBLTDL_CONVENIENCE
	 AC_SUBST(LTDLINCL)
	 AC_SUBST(LIBLTDL)
	 AC_LIBTOOL_DLOPEN
	]
)

test_cc_flags() {
	AC_LANG_CONFTEST([int main(void){}])
	$CC -c conftest.c $CFLAGS $STRICT_CFLAGS $@ > /dev/null 2> /dev/null
	ret=$?
	rm -f conftest.o
	return $ret
}

test_cxx_flags() {
	AC_LANG_PUSH(C++)
	AC_LANG_CONFTEST([int main(void){}])
	$CXX -c conftest.c $CXXFLAGS $STRICT_CXXFLAGS $@ > /dev/null 2> /dev/null
	ret=$?
	rm -f conftest.o
	AC_LANG_POP(C++)
	return $ret
}

STRICT_CFLAGS=""
STRICT_CXXFLAGS=""

m4_divert_once([HELP_ENABLE], [
Build options:])

dnl Optionally stick to standard C99 and POSIX:2001 as close as possible.
AC_ARG_ENABLE([standards],
		AS_HELP_STRING([--enable-standards],
				[C99 / POSIX standards compliance mode @<:@default=no@:>@]),
		[enable_standards="$enableval"],
		[enable_standards="no"])

if test "x$enable_standards" = "xyes"; then
	AC_DEFINE([_ISOC99_SOURCE], 1,
			[Define to enforce ISO/IEC 9899:1999 (C99) compliance.])
	AC_DEFINE([_POSIX_C_SOURCE], 200112L,
			[Define to enforce IEEE 1003.1-2001 (POSIX:2001) compliance.])
	AC_DEFINE([_XOPEN_SOURCE], 700,
			[Define to enforce X/Open 7 (XSI) compliance.])
	AC_DEFINE([_REENTRANT], 1,
			[Define to enable reentrant interfaces.])
	AC_DEFINE([_THREAD_SAFE], 1,
			[Define to enable reentrant interfaces.])

	dnl GNU libc defines strcasecmp() only when using _BSD_SOURCE even though
	dnl the function is conforming to POSIX.1-2001 as well. Let's weaken
	dnl strict standards compliance a bit to work around this.
	AC_DEFINE([_BSD_SOURCE], 1, [Define to enable 4.3BSD support.])

	for flag in -std=c99; do
		AC_MSG_CHECKING([whether $CC accepts $flag])

		if test_cc_flags $flag; then
			STRICT_CFLAGS="$STRICT_CFLAGS $flag"
			AC_MSG_RESULT([yes])
		else
			AC_MSG_RESULT([no])
		fi
	done
fi

dnl We need C++11 for facter.
AC_MSG_CHECKING([whether $CXX accepts -std=c++11])
if test_cxx_flags -std=c++11; then
	CXXFLAGS="$CXXFLAGS -std=c++11"
	AC_MSG_RESULT([yes])
else
	# Oh well, the header check will determine if it works anyway.
	AC_MSG_RESULT([no])
fi

dnl Hardening (see e.g. http://wiki.debian.org/Hardening for a motivation).
AC_DEFINE([_FORTIFY_SOURCE], 2,
		[Define to enable protection against static sized buffer overflows.])
AC_ARG_ENABLE([hardening],
		AS_HELP_STRING([--disable-hardening],
				[hardening options @<:@default=yes@:>@]),
		[enable_hardening="$enableval"],
		[enable_hardening="yes"])

if test "x$enable_hardening" = "xyes"; then
	hardening_cc=0
	hardening_cxx=0
	hardening_tests=0
	for flag in -Wformat -Wformat-security; do
		hardening_tests=$(($hardening_tests + 1))

		AC_MSG_CHECKING([whether $CC accepts $flag])
		if test_cc_flags $flag; then
			STRICT_CFLAGS="$STRICT_CFLAGS $flag"
			hardening_cc=$(($hardening_cc + 1))
			AC_MSG_RESULT([yes])
		else
			AC_MSG_RESULT([no])
		fi

		AC_MSG_CHECKING([whether $CXX accepts $flag])
		if test_cxx_flags $flag; then
			STRICT_CXXFLAGS="$STRICT_CXXFLAGS $flag"
			hardening_cxx=$(($hardening_cxx + 1))
			AC_MSG_RESULT([yes])
		else
			AC_MSG_RESULT([no])
		fi
	done
	if test $hardening_cc -ne $hardening_tests; then
		AC_MSG_WARN(
				[Some hardening options are not supported by your C compiler!])
	fi
	if test $hardening_cxx -ne $hardening_tests; then
		AC_MSG_WARN(
				[Some hardening options are not supported by your C++ compiler!])
	fi
fi

dnl Strict checking for potential problems.
AC_ARG_ENABLE([strict-checks],
		AS_HELP_STRING([--disable-strict-checks],
				[strict compiler checks @<:@default=yes@:>@]),
		[enable_strict_checks="$enableval"],
		[enable_strict_checks="yes"])

for flag in -Wall -Werror; do
	AC_MSG_CHECKING([whether $CC accepts $flag])
	if test_cc_flags $flag; then
		STRICT_CFLAGS="$STRICT_CFLAGS $flag"
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
	fi

	AC_MSG_CHECKING([whether $CXX accepts $flag])
	if test_cxx_flags $flag; then
		STRICT_CXXFLAGS="$STRICT_CXXFLAGS $flag"
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
	fi
done

if test "x$enable_strict_checks" = "xyes"; then
	dnl -Wsign-conversion may cause problems in expanded macros from libc
	for flag in -Wextra \
			-Wbad-function-cast \
			-Wcast-align \
			-Wcast-qual \
			-Wconversion \
			-Wno-sign-conversion \
			-Wdeclaration-after-statement \
			-Wmissing-prototypes \
			-Wpointer-arith \
			-Wshadow \
			-Wstrict-prototypes; do
		AC_MSG_CHECKING([whether $CC accepts $flag])
		if test_cc_flags $flag; then
			STRICT_CFLAGS="$STRICT_CFLAGS $flag"
			AC_MSG_RESULT([yes])
		else
			AC_MSG_RESULT([no])
		fi

		dnl -Wshadow produces unnecessary warnings/errors in case a
		dnl C function name "shadows" a struct constructor.
		if test "x$flag" != "x-Wshadow"; then
			AC_MSG_CHECKING([whether $CXX accepts $flag])
			if test_cxx_flags $flag; then
				STRICT_CXXFLAGS="$STRICT_CXXFLAGS $flag"
				AC_MSG_RESULT([yes])
			else
				AC_MSG_RESULT([no])
			fi
		fi
	done
fi
AC_SUBST([STRICT_CFLAGS])
AC_SUBST([STRICT_CXXFLAGS])

AC_ARG_ENABLE([gcov],
		AS_HELP_STRING([--enable-gcov],
				[Gcov coverage statistics @<:@default=no@:>@]),
		[enable_gcov="$enableval"],
		[enable_gcov="no"])

dnl $GCC is based on some heuristics which might apply to clang as well.
dnl However, clang does not support gcov.
cc_is_gcc="no"
case "x$CC" in
	xgcc)
		cc_is_gcc="yes"
		;;
	xgcc-*)
		cc_is_gcc="yes"
		;;
esac

COVERAGE_CFLAGS=""
COVERAGE_LDFLAGS=""
if test "x$enable_gcov" = "xyes" && test "x$GCC$cc_is_gcc" == "xyesyes"; then
	COVERAGE_CFLAGS="-O0"
	cov_flag_have_problem="no"

	AC_MSG_CHECKING([whether $CC accepts --coverage])
	if test_cc_flags --coverage; then
		COVERAGE_CFLAGS="$COVERAGE_CFLAGS --coverage"
		COVERAGE_LDFLAGS="$COVERAGE_LDFLAGS --coverage"
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
		cov_flag_have_problem="yes"
	fi

	for flag in -fno-inline; do
		AC_MSG_CHECKING([whether $CC accepts $flag])
		if test_cc_flags $flag; then
			COVERAGE_CFLAGS="$COVERAGE_CFLAGS $flag"
			AC_MSG_RESULT([yes])
		else
			AC_MSG_RESULT([no])
			cov_flag_have_problem="yes"
		fi
	done

	if test "x$cov_flag_have_problem" != "xno"; then
		AC_MSG_WARN([Some coverage flags are not supported by your compiler!])
	fi
else if test "x$enable_gcov" = "xyes"; then
	AC_MSG_WARN([Your compiler ($CC) is not known to support Gcov!])
	enable_gcov="no (requires GCC)"
fi; fi
AC_SUBST([COVERAGE_CFLAGS])
AC_SUBST([COVERAGE_LDFLAGS])

AC_ARG_ENABLE([gprof],
		AS_HELP_STRING([--enable-gprof],
				[Gprof profiling @<:@default=no@:>@]),
		[enable_gprof="$enableval"],
		[enable_gprof="no"])

PROFILING_CFLAGS=""
PROFILING_LDFLAGS=""
if test "x$enable_gprof" = "xyes"; then
	PROFILING_CFLAGS="-O0"
	profiling_flag_have_problem="no"

	AC_MSG_CHECKING([whether $CC accepts -pg])
	if test_cc_flags -pg; then
		PROFILING_CFLAGS="$PROFILING_CFLAGS -pg"
		PROFILING_LDFLAGS="$PROFILING_LDFLAGS -pg"
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
		profiling_flag_have_problem="yes"
	fi

	for flag in -fprofile-arcs; do
		AC_MSG_CHECKING([whether $CC accepts $flag])
		if test_cc_flags $flag; then
			PROFILING_CFLAGS="$PROFILING_CFLAGS $flag"
			AC_MSG_RESULT([yes])
		fi
		# else: this is not a serious problem
	done

	if test "x$profiling_flag_have_problem" != "xno"; then
		AC_MSG_WARN([Some profiling flags are not supported by your compiler!])
	fi
fi
AC_SUBST([PROFILING_CFLAGS])
AC_SUBST([PROFILING_LDFLAGS])

ieee754_layout="unknown"
AC_MSG_CHECKING([the memory layout of double precision values])
AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM(
			[[
#include <string.h>
#include <inttypes.h>
			]],
			[[
double d = 3.141592653e130;
char c[8];
if (sizeof(d) != 8)
	return 2;
memcpy(&c, &d, 8);
if (c[0] == '\x04' && c[1] == '\x10' && c[2] == '\x1E' && c[3] == '\x66' &&
	c[4] == '\x40' && c[5] == '\xA9' && c[6] == '\x06' && c[7] == '\x5B')
	return 0;
else
	return 1;
			]]
		)],
		[ieee754_layout="little-endian"], [ieee754_layout="unknown"])
if test "x$ieee754_layout" = "xunknown"; then
	AC_COMPILE_IFELSE(
			[AC_LANG_PROGRAM(
				[[
#include <string.h>
#include <inttypes.h>
				]],
				[[
double d = 3.141592653e130;
char c[8];
if (sizeof(d) != 8)
	return 2;
memcpy(&c, &d, 8);
if (c[7] == '\x04' && c[6] == '\x10' && c[5] == '\x1E' && c[4] == '\x66' &&
	c[3] == '\x40' && c[2] == '\xA9' && c[1] == '\x06' && c[0] == '\x5B')
	return 0;
else
	return 1;
				]]
			)],
			[ieee754_layout="big-endian"], [ieee754_layout="unknown"])
fi
AC_MSG_RESULT([IEEE-754 $ieee754_layout])

AC_DEFINE([IEEE754_DOUBLE_LITTLE_ENDIAN], 1234,
		[Identifier for IEEE-754 little-endian encoding of double precision values])
AC_DEFINE([IEEE754_DOUBLE_BIG_ENDIAN], 4321,
		[Identifier for IEEE-754 big-endian encoding of double precision values])
if test "x$ieee754_layout" = "xlittle-endian"; then
	AC_DEFINE([IEEE754_DOUBLE_BYTE_ORDER], 1234,
			[Define to 1 if double precision values use IEEE-754 little-endian encoding])
else if test "x$ieee754_layout" = "xbig-endian"; then
	AC_DEFINE([IEEE754_DOUBLE_BYTE_ORDER], 4321,
			[Define to 1 if double precision values use IEEE-754 little-endian encoding])
else
	AC_MSG_ERROR([Unknown memory layout of double precision values])
fi; fi

m4_divert_once([HELP_ENABLE], [
Build dependencies:])

AC_CHECK_HEADERS([ucred.h])
dnl On OpenBSD, sys/param.h is required for sys/ucred.h.
AC_CHECK_HEADERS([sys/ucred.h], [], [],
		[[ #include <sys/param.h> ]])

AC_CHECK_TYPES([struct ucred],
		[have_struct_ucred="yes"], [have_struct_ucred="no"],
		[[
#include <sys/socket.h>
#include <sys/param.h>
#if HAVE_UCRED_H
#	include <ucred.h>
#endif
		]])

if test "x$have_struct_ucred" != "xyes"; then
	AC_MSG_CHECKING([for struct ucred when using _GNU_SOURCE])
	orig_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS -D_GNU_SOURCE"
	dnl Don't reuse AC_CHECK_HEADERS; for one it'll use the cached value
	dnl but also, it will print the "checking for" message a second time.
	AC_COMPILE_IFELSE(
			[AC_LANG_PROGRAM(
				[[
#include <sys/socket.h>
#include <sys/param.h>
#if HAVE_UCRED_H
#	include <ucred.h>
#endif
				]],
				[if (sizeof(struct ucred)) return 0;]
			)],
			[have_struct_ucred="yes"], [have_struct_ucred="no"])
	CFLAGS="$orig_CFLAGS"
	if test "x$have_struct_ucred" = "xyes"; then
		AC_DEFINE([_GNU_SOURCE], 1, [Define to enable GNU features.])
	fi
	AC_MSG_RESULT([$have_struct_ucred])
fi

dnl Testing.
PKG_CHECK_MODULES([CHECK], [check >= 0.9.4],
		[unit_tests="yes"], [unit_tests="no"])

AC_CHECK_HEADERS(libgen.h)

dnl Check for dependencies.
AC_ARG_WITH([libdbi],
		[AS_HELP_STRING([--with-libdbi], [libdbi support (default: auto)])],
		[with_libdbi="$withval"],
		[with_libdbi="yes"])
if test "x$with_libdbi" = "xyes" || test "x$with_libdbi" = "xauto"; then
	AC_CHECK_HEADERS([dbi/dbi.h],
			[with_libdbi="yes"],
			[with_libdbi="no (dbi/dbi.h not found)"])
else if test "x$with_libdbi" = "xno"; then
	with_libdbi="$with_libdbi (disabled on command-line)"
else
	AC_MSG_ERROR([Invalid value for option --with-libdbi=$with_libdbi (expected "yes", "no", or "auto")])
fi; fi
if test "x$with_libdbi" = "xyes"; then
	AC_CHECK_LIB([dbi], [dbi_initialize],
			[with_libdbi="yes"],
			[with_libdbi="no (libdbi or symbol 'dbi_initialize' not found)"])
fi
AM_CONDITIONAL([BUILD_WITH_LIBDBI], test "x$with_libdbi" = "xyes")

dnl Required for mocking FILE related functions.
orig_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -D_GNU_SOURCE"
AC_CHECK_FUNCS([fopencookie],
		[have_fopencookie="yes"],
		[have_fopencookie="no (fopencookie not available)"])
CFLAGS="$orig_CFLAGS"
AM_CONDITIONAL([BUILD_WITH_FOPENCOOKIE], test "x$have_fopencookie" = "xyes")
if test "x$have_fopencookie" = "xyes"; then
	AC_DEFINE([HAVE_FOPENCOOKIE], 1)
fi

dnl OpenSSL support
PKG_CHECK_MODULES([OPENSSL], [openssl], [have_openssl="yes"], [have_openssl="no"])
if test "x$have_openssl" != "xyes"; then
	AC_MSG_ERROR([OpenSSL not found])
fi

dnl readline support
AC_ARG_WITH([readline],
		[AS_HELP_STRING([--with-readline],
			[readline support (libedit/libreadline) (default: auto, prefer libedit)])],
		[readline_support="$withval"],
		[readline_support="auto"])

if test "x$readline_support" = "xyes"; then
	readline_support="auto"
else if test "x$readline_support" != "xauto" \
		&& test "x$readline_support" != "xno" \
		&& test "x$readline_support" != "xlibedit" \
		&& test "x$readline_support" != "xlibreadline"; then
	AC_MSG_ERROR([Invalid value for option --with-readline=$readline_support (expected "yes", "no", "auto", "libedit", or "libreadline")])
fi; fi

have_libedit="no"
if test "x$readline_support" = "xauto" \
		|| test "x$readline_support" = "xlibedit"; then
	PKG_CHECK_MODULES([LIBEDIT], [libedit],
			[have_libedit="yes"], [have_libedit="no"])
	if test "x$have_libedit" = "xyes"; then
		AC_CHECK_HEADERS([editline/readline.h], [],
				[AC_CHECK_HEADERS([readline.h], [],
						[have_libedit="no (readline header not found"])])
	fi
	if test "x$have_libedit" = "xyes"; then
		AC_CHECK_HEADERS([editline/history.h], [],
				[AC_CHECK_HEADERS([history.h], [],
						[have_libedit="no (history header not found"])])
	fi
else
	have_libedit="disabled on command-line"
fi

have_libreadline="no"
if test "x$have_libedit" != "xyes"; then
	if test "x$readline_support" = "xauto" \
			|| test "x$readline_support" = "xlibreadline"; then
		AC_CHECK_LIB([readline], [readline],
				[have_libreadline="yes"],
				[have_libreadline="no (libreadline or symbol 'readline' not found)"])
	fi
	if test "x$have_libreadline" = "xyes"; then
		AC_CHECK_HEADERS([readline/readline.h], [],
				[AC_CHECK_HEADERS([readline.h], [],
						[have_libreadline="no (readline header not found"])])
	fi
	if test "x$have_libreadline" = "xyes"; then
		AC_CHECK_HEADERS([readline/history.h], [],
				[AC_CHECK_HEADERS([history.h], [],
						[have_libreadline="no (history header not found"])])
	fi
else
	have_libreadline="unchecked (prefer libedit)"
fi

if test "x$have_libedit" = "xyes"; then
	READLINE_LIBS="$LIBEDIT_LIBS"
	READLINE_CFLAGS="$LIBEDIT_CFLAGS"
	readline_support="libedit"
else if test "x$have_libreadline" = "xyes"; then
	READLINE_LIBS="-lreadline -lhistory"
	READLINE_CFLAGS=""
	readline_support="libreadline"
else
	READLINE_LIBS=""
	READLINE_CFLAGS=""
	if test "x$readline_support" = "xno"; then
		AC_MSG_WARN([*** readline support disabled; disabling SysDB client])
	else if test "x$readline_support" = "xauto"; then
		AC_MSG_WARN([*** readline not found; disabling SysDB client])
	else
		AC_MSG_ERROR([readline not found])
	fi; fi
	readline_support="no"
fi; fi
AC_SUBST([READLINE_LIBS])
AC_SUBST([READLINE_CFLAGS])
AM_CONDITIONAL([BUILD_CLIENT], test "x$readline_support" != "no")

AC_LANG_PUSH(C++)
AC_ARG_WITH([libfacter],
		[AS_HELP_STRING([--with-libfacter], [libfacter support (default: auto)])],
		[with_libfacter="$withval"],
		[with_libfacter="yes"])
if test "x$with_libfacter" = "xyes" || test "x$with_libfacter" = "xauto"; then
	AC_CHECK_HEADERS([facter/facts/collection.hpp],
			[have_libfacter="yes"],
			[have_libfacter="no (facter/facts/collection.hpp not found)"])
else if test "x$with_libfacter" = "xno"; then
	have_libfacter="$with_libfacter (disabled on command-line)"
else
	AC_MSG_ERROR([Invalid value for option --with-libfacter=$with_libfacter (expected "yes", "no", or "auto")])
fi; fi
if test "x$have_libfacter" = "xyes"; then
	AC_MSG_CHECKING([for facter::facts::collection in -lfacter])
	AC_LINK_IFELSE(
			[AC_LANG_PROGRAM(
				[[ #include <facter/facts/collection.hpp> ]],
				[[
					facter::facts::collection facts;
					facts.add_default_facts();
				]]
			)],
			[TEST_LIBS=$TEST_LIBS -lfacter],
			[have_libfacter="yes"],
			[have_libfacter="no (libfacter not found)"])
	AC_MSG_RESULT([$have_libfacter])
fi
AC_LANG_POP(C++)

AC_ARG_WITH([librrd],
		[AS_HELP_STRING([--with-librrd], [librrd support (default: auto)])],
		[with_librrd="$withval"],
		[with_librrd="yes"])
if test "x$with_librrd" = "xyes" || test "x$with_librrd" = "xauto"; then
	PKG_CHECK_MODULES([RRD], [librrd],
			[have_librrd="yes"], [have_librrd="no"])
else if test "x$with_librrd" = "xno"; then
	have_librrd="$with_librrd (disabled on command-line)"
else
	AC_MSG_ERROR([Invalid value for option --with-librrd=$with_librrd (expected "yes", "no", or "auto")])
fi; fi

if test "x$have_librrd" = "xyes"; then
	AC_CHECK_HEADERS([rrd_client.h])
fi

dnl Feature checks.
build_documentation="yes"

have_xmlto="yes"
AC_PATH_PROG([XMLTO], [xmlto])
if test "x$XMLTO" = "x"; then
	have_xmlto="no"
	build_documentation="no (missing xmlto)"
fi
AC_SUBST([XMLTO])

have_asciidoc="yes"
AC_PATH_PROG([ASCIIDOC], [asciidoc])
if test "x$ASCIIDOC" = "x"; then
       have_asciidoc="no"
       build_documentation="no (missing asciidoc)"
fi
AC_SUBST([ASCIIDOC])

AC_ARG_VAR([ADOCFLAGS], [AsciiDoc flags])

integration_tests="yes"
AC_PATH_PROG([VALGRIND], [valgrind])
if test "x$VALGRIND" = "x"; then
	integration_tests="no (missing valgrind)"
fi
AM_CONDITIONAL([INTEGRATION_TESTING], test "x$integration_tests" = "xyes")

dnl Plugin checks.
facter_default=$have_libfacter
if test "x$facter_default" != "xyes"; then
	facter_default="no (requires libfacter)"
fi
puppet_storeconfigs_default=$with_libdbi
if test "x$puppet_storeconfigs_default" != "xyes"; then
	puppet_storeconfigs_default="no (requires libdbi)"
fi
rrdtool_default=$have_librrd
if test "x$rrdtool_default" != "xyes"; then
	rrdtool_default="no (required librrd)"
fi

m4_divert_once([HELP_ENABLE], [
Backends:])

AC_SDB_PLUGIN_INIT
AC_SDB_PLUGIN([collectd-unixsock], [yes],
		[backend accessing the system statistics collection daemon])
AC_SDB_PLUGIN([facter], [$facter_default],
		[backend retrieving local facter facts])
AC_SDB_PLUGIN([mk-livestatus], [yes],
		[backend accessing Nagios/Icinga/Shinken using MK Livestatus])
AC_SDB_PLUGIN([puppet-storeconfigs], [$puppet_storeconfigs_default],
		[backend accessing the Puppet stored configuration database])

m4_divert_once([HELP_ENABLE], [
Time-series fetchers:])
AC_SDB_PLUGIN([timeseries-rrdtool], [$rrdtool_default],
		[fetch time-series data from RRD files])

m4_divert_once([HELP_ENABLE], [
Store implementations:])
AC_SDB_PLUGIN([store-memory], [yes],
		[store objects in a volatile, in-memory database (read/write)])
AC_SDB_PLUGIN([store-network], [yes],
		[send stored objects to a remote instance (write only)])

m4_divert_once([HELP_ENABLE], [
Plugins:])

AC_SDB_PLUGIN([cname-dns], [yes],
		[canonicalize hostnames by querying DNS])
AC_SDB_PLUGIN([syslog], [yes],
		[plugin logging to syslog])

AM_CONDITIONAL([BUILD_DOCUMENTATION], test "x$build_documentation" = "xyes")
AM_CONDITIONAL([UNIT_TESTING], test "x$unit_tests" = "xyes")

AC_CONFIG_FILES([Makefile doc/Makefile src/Makefile
		src/liboconfig/Makefile t/Makefile])
AC_OUTPUT

BUILD_DATE="`date --utc '+%F %T'` (UTC)"

asciidoc_info="$have_asciidoc"
if test "x$have_asciidoc" = "xyes"; then
	asciidoc_info="`$ASCIIDOC --version`"
fi
xmlto_info="$have_xmlto"
if test "x$have_xmlto" = "xyes"; then
	xmlto_info="`$XMLTO --version`"
fi

if test "x$unit_tests" = "xyes"; then
	unit_tests="yes (check `$PKG_CONFIG --modversion check`)"
fi

openssl_info="$have_openssl"
if test "x$have_openssl" = "xyes"; then
	openssl_info="yes (version `$PKG_CONFIG --modversion openssl`)"
fi
libedit_info="$have_libedit"
if test "x$have_libedit" = "xyes"; then
	libedit_info="yes (version `$PKG_CONFIG --modversion libedit`)"
fi
librrd_info="$have_librrd"
if test "x$have_librrd" = "xyes"; then
	librrd_info="yes (version `$PKG_CONFIG --modversion librrd`)"
fi

AC_MSG_RESULT()
AC_MSG_RESULT([$PACKAGE_NAME has been configured successfully.])
AC_MSG_RESULT()
AC_MSG_RESULT([Run 'make' to compile the software and use 'make install' to])
AC_MSG_RESULT([install the package into $prefix.])
AC_MSG_RESULT()
AC_MSG_RESULT([Configuration summary:])
AC_MSG_RESULT()
AC_MSG_RESULT([  package version: $PACKAGE_VERSION])
AC_MSG_RESULT([  build date: $BUILD_DATE])
AC_MSG_RESULT()
AC_MSG_RESULT([  Tools:])
AC_MSG_RESULT([    AsciiDoc: . . . . . . . . . $asciidoc_info])
AC_MSG_RESULT([    xmlto:  . . . . . . . . . . $xmlto_info])
AC_MSG_RESULT()
AC_MSG_RESULT([  Features:])
AC_MSG_RESULT([    documentation:  . . . . . . $build_documentation])
AC_MSG_RESULT([    unit testing: . . . . . . . $unit_tests])
AC_MSG_RESULT([      stdio mocking:  . . . . . $have_fopencookie])
AC_MSG_RESULT([    coverage testing: . . . . . $enable_gcov])
AC_MSG_RESULT([    integration testing:  . . . $integration_tests])
AC_MSG_RESULT([    profiling:  . . . . . . . . $enable_gprof])
AC_MSG_RESULT()
AC_MSG_RESULT([  Libraries:])
AC_MSG_RESULT([    libdbi: . . . . . . . . . . $with_libdbi])
AC_MSG_RESULT([    libedit:  . . . . . . . . . $libedit_info])
AC_MSG_RESULT([    libopenssl: . . . . . . . . $openssl_info])
AC_MSG_RESULT([    libreadline:  . . . . . . . $have_libreadline])
AC_MSG_RESULT([    librrd: . . . . . . . . . . $librrd_info])
AC_MSG_RESULT()
AC_MSG_RESULT([  Backends:])
AC_MSG_RESULT([    collectd::unixsock: . . . . $enable_collectd_unixsock])
AC_MSG_RESULT([    facter  . . . . . . . . . . $enable_facter])
AC_MSG_RESULT([    mk-livestatus:  . . . . . . $enable_mk_livestatus])
AC_MSG_RESULT([    puppet::storeconfigs: . . . $enable_puppet_storeconfigs])
AC_MSG_RESULT()
AC_MSG_RESULT([  Time-series fetchers:])
AC_MSG_RESULT([    rrdtool:  . . . . . . . . . $enable_timeseries_rrdtool])
AC_MSG_RESULT()
AC_MSG_RESULT([  Store implementations:])
AC_MSG_RESULT([    memory: . . . . . . . . . . $enable_store_memory])
AC_MSG_RESULT([    network (writer): . . . . . $enable_store_network])
AC_MSG_RESULT()
AC_MSG_RESULT([  Plugins:])
AC_MSG_RESULT([    cname::dns: . . . . . . . . $enable_cname_dns])
AC_MSG_RESULT([    syslog: . . . . . . . . . . $enable_syslog])
AC_MSG_RESULT()
AC_MSG_RESULT([This package is maintained by $PACKAGE_MAINTAINER.])
AC_MSG_RESULT([Please report bugs to $PACKAGE_BUGREPORT.])
AC_MSG_RESULT()

dnl vim: set tw=78 sw=4 ts=4 noexpandtab :
