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
		[sh@tokkee.org],
		[sysdb],
		[http://git.tokkee.org/?p=sysdb.git])
PACKAGE_MAINTAINER="Sebastian 'tokkee' Harl <sh@tokkee.org>"
AC_DEFINE_UNQUOTED([PACKAGE_MAINTAINER], ["$PACKAGE_MAINTAINER"],
		[Define to the name of the maintainer of this package.])
AC_CONFIG_SRCDIR([src/sysdb.c])
AC_CONFIG_HEADERS([src/config.h])
AC_PREFIX_DEFAULT([/opt/sysdb])

AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])

AC_LANG(C)

AC_SYS_LARGEFILE

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_AR

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
	AC_LANG_CONFTEST([AC_LANG_PROGRAM([[ ]], [[ ]])])
	$CC -c conftest.c $CFLAGS $@ > /dev/null 2> /dev/null
	ret=$?
	rm -f conftest.o
	return $ret
}

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
	AC_DEFINE([_XOPEN_SOURCE], 600,
			[Define to enforce X/Open 6 (XSI) compliance.])
	AC_DEFINE([_REENTRANT], 1,
			[Define to enable reentrant interfaces.])
	AC_DEFINE([_THREAD_SAFE], 1,
			[Define to enable reentrant interfaces.])

	for flag in -std=c99 -pedantic; do
		AC_MSG_CHECKING([whether $CC accepts $flag])

		if test_cc_flags $flag; then
			CFLAGS="$CFLAGS $flag"
			AC_MSG_RESULT([yes])
		else
			AC_MSG_RESULT([no])
		fi
	done
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
	hardening=0
	hardening_tests=0
	for flag in -Wformat -Wformat-security; do
		hardening_tests=$(($hardening_tests + 1))
		AC_MSG_CHECKING([whether $CC accepts $flag])

		if test_cc_flags $flag; then
			CFLAGS="$CFLAGS $flag"
			hardening=$(($hardening + 1))
			AC_MSG_RESULT([yes])
		else
			AC_MSG_RESULT([no])
		fi
	done
	if test $hardening -ne $hardening_tests; then
		AC_MSG_WARN(
				[Some hardening options are not supported by your compiler!])
	fi
fi

dnl Strict checking for potential problems.
AC_ARG_ENABLE([strict-checks],
		AS_HELP_STRING([--disable-strict-checks],
				[strict compiler checks @<:@default=yes@:>@]),
		[enable_strict_checks="$enableval"],
		[enable_strict_checks="yes"])

STRICT_CFLAGS=""
for flag in -Wall -Werror; do
	AC_MSG_CHECKING([whether $CC accepts $flag])

	if test_cc_flags $flag; then
		STRICT_CFLAGS="$STRICT_CFLAGS $flag"
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
	fi
done

if test "x$enable_strict_checks" = "xyes"; then
	for flag in -Wextra \
			-Wbad-function-cast \
			-Wcast-align \
			-Wcast-qual \
			-Wconversion \
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
	done
fi
AC_SUBST([STRICT_CFLAGS])

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

m4_divert_once([HELP_ENABLE], [
Build dependencies:])

dnl Testing.
PKG_CHECK_MODULES([CHECK], [check >= 0.9.4],
		[build_testing="yes"], [build_testing="no"])

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

dnl Feature checks.
build_documentation="yes"

have_xmlto="yes"
AC_PATH_PROG([XMLTO], [xmlto])
if test "x$XMLTO" = "x"; then
	have_xmlto="no"
	build_documentation="no (missing xmlto)"
fi

have_xsltproc="yes"
AC_PATH_PROG([XSLTPROC], [xsltproc])
if test "x$XSLTPROC" = "x"; then
       have_xsltproc="no"
       build_documentation="no (missing xsltproc)"
fi

have_a2x="yes"
AC_PATH_PROG([A2X], [a2x])
if test "x$A2X" = "x"; then
       have_a2x="no"
       build_documentation="no (missing a2x)"
fi
AC_SUBST([A2X])

dnl Plugin checks.
puppet_storeconfigs_default=$with_libdbi
if test "x$puppet_storeconfigs_default" != "xyes"; then
	puppet_storeconfigs_default="no (requires libdbi)"
fi

m4_divert_once([HELP_ENABLE], [
Backends:])

AC_SDB_PLUGIN_INIT
AC_SDB_PLUGIN([collectd], [yes],
		[backend accessing the system statistics collection daemon])
AC_SDB_PLUGIN([mk-livestatus], [yes],
		[backend accessing Nagios/Icinga/Shinken using MK Livestatus])
AC_SDB_PLUGIN([puppet-storeconfigs], [$puppet_storeconfigs_default],
		[backend accessing the Puppet stored configuration database])

m4_divert_once([HELP_ENABLE], [
Plugins:])

AC_SDB_PLUGIN([cname-dns], [yes],
		[canonicalize hostnames by querying DNS])
AC_SDB_PLUGIN([syslog], [yes],
		[plugin logging to syslog])

AM_CONDITIONAL([BUILD_DOCUMENTATION], test "x$build_documentation" = "xyes")
AM_CONDITIONAL([BUILD_TESTING], test "x$build_testing" = "xyes")

AC_CONFIG_FILES([Makefile doc/Makefile src/Makefile
		src/liboconfig/Makefile t/Makefile])
AC_OUTPUT

BUILD_DATE="`date --utc '+%F %T'` (UTC)"

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
AC_MSG_RESULT([    AsciiDoc (a2x): . . . . . . $have_a2x])
AC_MSG_RESULT([    xmlto / xsltproc: . . . . . $have_xmlto / $have_xsltproc])
AC_MSG_RESULT()
AC_MSG_RESULT([  Features:])
AC_MSG_RESULT([    documentation:  . . . . . . $build_documentation])
AC_MSG_RESULT([    unit testing: . . . . . . . $build_testing])
AC_MSG_RESULT([      stdio mocking:  . . . . . $have_fopencookie])
AC_MSG_RESULT([    coverage testing: . . . . . $enable_gcov])
AC_MSG_RESULT()
AC_MSG_RESULT([  Libraries:])
AC_MSG_RESULT([    libdbi: . . . . . . . . . . $with_libdbi])
AC_MSG_RESULT([    libedit:  . . . . . . . . . $have_libedit])
AC_MSG_RESULT([    libreadline:  . . . . . . . $have_libreadline])
AC_MSG_RESULT()
AC_MSG_RESULT([  Backends:])
AC_MSG_RESULT([    collectd: . . . . . . . . . $enable_collectd])
AC_MSG_RESULT([    mk-livestatus:  . . . . . . $enable_mk_livestatus])
AC_MSG_RESULT([    puppet-storeconfigs:  . . . $enable_puppet_storeconfigs])
AC_MSG_RESULT()
AC_MSG_RESULT([  Plugins:])
AC_MSG_RESULT([    cname::dns: . . . . . . . . $enable_cname_dns])
AC_MSG_RESULT([    syslog: . . . . . . . . . . $enable_syslog])
AC_MSG_RESULT()
AC_MSG_RESULT([This package is maintained by $PACKAGE_MAINTAINER.])
AC_MSG_RESULT([Please report bugs to $PACKAGE_BUGREPORT.])
AC_MSG_RESULT()

dnl vim: set tw=78 sw=4 ts=4 noexpandtab :
