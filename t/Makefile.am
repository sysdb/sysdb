EXTRA_DIST = coverage.sh

# coverage related flags are only set when gcov is enabled
AM_CFLAGS = @STRICT_CFLAGS@ @COVERAGE_CFLAGS@
AM_LDFLAGS = @COVERAGE_LDFLAGS@
AM_CPPFLAGS = -I$(top_srcdir)/src/include

TESTS = unit/libsysdb_test unit/libsysdb_net_test
check_PROGRAMS = unit/libsysdb_test unit/libsysdb_net_test

unit_libsysdb_test_SOURCES = \
		unit/libsysdb_test.c unit/libsysdb_test.h \
		unit/libsysdb_testutils.c unit/libsysdb_testutils.h \
		unit/core/data_test.c \
		unit/core/object_test.c \
		unit/core/store_test.c \
		unit/core/store_lookup_test.c \
		unit/core/time_test.c \
		unit/frontend/connection_test.c \
		unit/frontend/parser_test.c \
		unit/frontend/sock_test.c \
		unit/utils/channel_test.c \
		unit/utils/dbi_test.c \
		unit/utils/llist_test.c \
		unit/utils/strbuf_test.c
unit_libsysdb_test_CFLAGS = $(AM_CFLAGS) @CHECK_CFLAGS@ -I$(top_srcdir)/t/unit
unit_libsysdb_test_LDADD = $(top_builddir)/src/libsysdb.la @CHECK_LIBS@

unit_libsysdb_net_test_SOURCES = \
		unit/libsysdb_net_test.c unit/libsysdb_test.h \
		unit/libsysdb_testutils.c unit/libsysdb_testutils.h
if BUILD_WITH_FOPENCOOKIE
unit_libsysdb_net_test_SOURCES += unit/utils/unixsock_test.c
endif
unit_libsysdb_net_test_CFLAGS = $(AM_CFLAGS) @CHECK_CFLAGS@ -I$(top_srcdir)/t/unit
unit_libsysdb_net_test_LDADD = $(top_builddir)/src/libsysdb.la @CHECK_LIBS@

test: check

