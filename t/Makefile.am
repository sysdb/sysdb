AM_CPPFLAGS = -I$(top_builddir)/src/include

TESTS = libsysdb_test

check_PROGRAMS = libsysdb_test
libsysdb_test_SOURCES = \
		libsysdb_test.c libsysdb_test.h \
		utils/llist_test.c
libsysdb_test_CFLAGS = @CHECK_CFLAGS@
libsysdb_test_LDADD = $(top_builddir)/src/libsysdb.la @CHECK_LIBS@
