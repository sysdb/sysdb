AM_CFLAGS = @STRICT_CFLAGS@
AM_CPPFLAGS = -I$(top_builddir)/src/include

TESTS = libsysdb_test

check_PROGRAMS = libsysdb_test
libsysdb_test_SOURCES = \
		libsysdb_test.c libsysdb_test.h \
		utils/dbi_test.c \
		utils/llist_test.c
libsysdb_test_CFLAGS = $(AM_CFLAGS) @CHECK_CFLAGS@
libsysdb_test_LDADD = $(top_builddir)/src/libsysdb.la @CHECK_LIBS@

test: check

