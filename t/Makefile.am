EXTRA_DIST = \
		coverage.sh \
		testwrapper.sh \
		integration/config.sh \
		integration/ssl.sh \
		integration/query.sh \
		integration/matching.sh \
		integration/filter.sh \
		integration/test_lib.sh \
		valgrind.suppress

# coverage related flags are only set when gcov is enabled
AM_CFLAGS = @STRICT_CFLAGS@ @COVERAGE_CFLAGS@
AM_LDFLAGS = @COVERAGE_LDFLAGS@
AM_CPPFLAGS = -I$(top_srcdir)/src/include

TESTS =
check_PROGRAMS =
check_LTLIBRARIES =

#
# unit tests
#

LOG_COMPILER = $(abs_srcdir)/testwrapper.sh

if UNIT_TESTING
TESTS += unit/libsysdb_test unit/libsysdb_net_test
check_PROGRAMS += unit/libsysdb_test unit/libsysdb_net_test
endif

unit_libsysdb_test_SOURCES = \
		unit/libsysdb_test.c unit/libsysdb_test.h \
		unit/libsysdb_testutils.c unit/libsysdb_testutils.h \
		unit/core/data_test.c \
		unit/core/object_test.c \
		unit/core/store_test.c \
		unit/core/store_json_test.c \
		unit/core/store_lookup_test.c \
		unit/core/time_test.c \
		unit/frontend/connection_test.c \
		unit/frontend/parser_test.c \
		unit/frontend/sock_test.c \
		unit/utils/avltree_test.c \
		unit/utils/channel_test.c \
		unit/utils/dbi_test.c \
		unit/utils/llist_test.c \
		unit/utils/os_test.c \
		unit/utils/proto_test.c \
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

#
# integration tests
#

if INTEGRATION_TESTING
TESTS += \
		integration/config.sh \
		integration/ssl.sh \
		integration/query.sh \
		integration/matching.sh \
		integration/filter.sh
endif

check_LTLIBRARIES += integration/mock_plugin.la
integration_mock_plugin_la_SOURCES = integration/mock_plugin.c
# -rpath is a work-around to enforce a shared library
integration_mock_plugin_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version \
		-rpath /nonexistent

check_LTLIBRARIES += integration/mock_timeseries.la
integration_mock_timeseries_la_SOURCES = integration/mock_timeseries.c
# -rpath is a work-around to enforce a shared library
integration_mock_timeseries_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version \
		-rpath /nonexistent

test: check

