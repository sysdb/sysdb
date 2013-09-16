AM_CFLAGS = @STRICT_CFLAGS@
AM_CPPFLAGS = -I$(top_builddir)/src/include

TESTS = libsysdb_test libsysdb_net_test
check_PROGRAMS = libsysdb_test libsysdb_net_test

libsysdb_test_SOURCES = \
		libsysdb_test.c libsysdb_test.h \
		utils/dbi_test.c \
		utils/llist_test.c \
		utils/strbuf_test.c
libsysdb_test_CFLAGS = $(AM_CFLAGS) @CHECK_CFLAGS@
libsysdb_test_LDADD = $(top_builddir)/src/libsysdb.la @CHECK_LIBS@

libsysdb_net_test_SOURCES = \
		libsysdb_net_test.c libsysdb_test.h
if BUILD_WITH_FOPENCOOKIE
libsysdb_net_test_SOURCES += utils/unixsock_test.c
endif
libsysdb_net_test_CFLAGS = $(AM_CFLAGS) @CHECK_CFLAGS@
libsysdb_net_test_LDADD = $(top_builddir)/src/libsysdb.la @CHECK_LIBS@

test: check

