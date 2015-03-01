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
UNIT_TESTS = \
		unit/core/data_test \
		unit/core/object_test \
		unit/core/store_json_test \
		unit/core/store_lookup_test \
		unit/core/store_test \
		unit/core/time_test \
		unit/frontend/connection_test \
		unit/frontend/parser_test \
		unit/frontend/query_test \
		unit/frontend/sock_test \
		unit/utils/avltree_test \
		unit/utils/channel_test \
		unit/utils/dbi_test \
		unit/utils/llist_test \
		unit/utils/os_test \
		unit/utils/proto_test \
		unit/utils/strbuf_test

UNIT_TEST_SOURCES = unit/testutils.c unit/testutils.h
UNIT_TEST_CFLAGS = $(AM_CFLAGS) @CHECK_CFLAGS@ -I$(top_srcdir)/t/unit
UNIT_TEST_LDADD = $(top_builddir)/src/libsysdb.la @CHECK_LIBS@

if BUILD_WITH_FOPENCOOKIE
UNIT_TESTS += unit/utils/unixsock_test
unit_utils_unixsock_test_SOURCES = $(UNIT_TEST_SOURCES) unit/utils/unixsock_test.c
unit_utils_unixsock_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_utils_unixsock_test_LDADD = $(UNIT_TEST_LDADD)
endif

unit_core_data_test_SOURCES = $(UNIT_TEST_SOURCES) unit/core/data_test.c
unit_core_data_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_core_data_test_LDADD = $(UNIT_TEST_LDADD)

unit_core_object_test_SOURCES = $(UNIT_TEST_SOURCES) unit/core/object_test.c
unit_core_object_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_core_object_test_LDADD = $(UNIT_TEST_LDADD)

unit_core_store_json_test_SOURCES = $(UNIT_TEST_SOURCES) unit/core/store_json_test.c
unit_core_store_json_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_core_store_json_test_LDADD = $(UNIT_TEST_LDADD)

unit_core_store_lookup_test_SOURCES = $(UNIT_TEST_SOURCES) unit/core/store_lookup_test.c
unit_core_store_lookup_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_core_store_lookup_test_LDADD = $(UNIT_TEST_LDADD)

unit_core_store_test_SOURCES = $(UNIT_TEST_SOURCES) unit/core/store_test.c
unit_core_store_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_core_store_test_LDADD = $(UNIT_TEST_LDADD)

unit_core_time_test_SOURCES = $(UNIT_TEST_SOURCES) unit/core/time_test.c
unit_core_time_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_core_time_test_LDADD = $(UNIT_TEST_LDADD)

unit_frontend_connection_test_SOURCES = $(UNIT_TEST_SOURCES) unit/frontend/connection_test.c
unit_frontend_connection_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_frontend_connection_test_LDADD = $(UNIT_TEST_LDADD)

unit_frontend_parser_test_SOURCES = $(UNIT_TEST_SOURCES) unit/frontend/parser_test.c
unit_frontend_parser_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_frontend_parser_test_LDADD = $(UNIT_TEST_LDADD)

unit_frontend_query_test_SOURCES = $(UNIT_TEST_SOURCES) unit/frontend/query_test.c
unit_frontend_query_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_frontend_query_test_LDADD = $(UNIT_TEST_LDADD)

unit_frontend_sock_test_SOURCES = $(UNIT_TEST_SOURCES) unit/frontend/sock_test.c
unit_frontend_sock_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_frontend_sock_test_LDADD = $(UNIT_TEST_LDADD)

unit_utils_avltree_test_SOURCES = $(UNIT_TEST_SOURCES) unit/utils/avltree_test.c
unit_utils_avltree_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_utils_avltree_test_LDADD = $(UNIT_TEST_LDADD)

unit_utils_channel_test_SOURCES = $(UNIT_TEST_SOURCES) unit/utils/channel_test.c
unit_utils_channel_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_utils_channel_test_LDADD = $(UNIT_TEST_LDADD)

unit_utils_dbi_test_SOURCES = $(UNIT_TEST_SOURCES) unit/utils/dbi_test.c
unit_utils_dbi_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_utils_dbi_test_LDADD = $(UNIT_TEST_LDADD)

unit_utils_llist_test_SOURCES = $(UNIT_TEST_SOURCES) unit/utils/llist_test.c
unit_utils_llist_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_utils_llist_test_LDADD = $(UNIT_TEST_LDADD)

unit_utils_os_test_SOURCES = $(UNIT_TEST_SOURCES) unit/utils/os_test.c
unit_utils_os_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_utils_os_test_LDADD = $(UNIT_TEST_LDADD)

unit_utils_proto_test_SOURCES = $(UNIT_TEST_SOURCES) unit/utils/proto_test.c
unit_utils_proto_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_utils_proto_test_LDADD = $(UNIT_TEST_LDADD)

unit_utils_strbuf_test_SOURCES = $(UNIT_TEST_SOURCES) unit/utils/strbuf_test.c
unit_utils_strbuf_test_CFLAGS = $(UNIT_TEST_CFLAGS)
unit_utils_strbuf_test_LDADD = $(UNIT_TEST_LDADD)

TESTS += $(UNIT_TESTS)
check_PROGRAMS += $(UNIT_TESTS)
endif

#
# integration tests
#

if INTEGRATION_TESTING
INTEGRATION_TESTS = \
		integration/config.sh \
		integration/ssl.sh \
		integration/query.sh \
		integration/matching.sh \
		integration/filter.sh

TESTS += $(INTEGRATION_TESTS)

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
endif

test: check

