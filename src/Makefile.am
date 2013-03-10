SUBDIRS = liboconfig

AM_CFLAGS = @STRICT_CFLAGS@ -Iinclude
AM_CPPFLAGS  = -DSYSCONFDIR='"${sysconfdir}"'
AM_CPPFLAGS += -DPKGLIBDIR='"${pkglibdir}"'

BUILT_SOURCES = include/sysdb.h

pkginclude_HEADERS = include/sysdb.h
pkgcoreincludedir = $(pkgincludedir)/core
pkgcoreinclude_HEADERS = \
		include/core/object.h \
		include/core/plugin.h
pkgutilsincludedir = $(pkgincludedir)/utils
pkgutilsinclude_HEADERS = \
		include/utils/llist.h \
		include/utils/string.h

lib_LTLIBRARIES = libsysdb.la

libsysdb_la_SOURCES = \
		sysdb.c include/sysdb.h \
		core/object.c include/core/object.h \
		core/plugin.c include/core/plugin.h \
		core/store.c include/core/store.h \
		include/utils/data.h \
		utils/error.c include/utils/error.h \
		utils/llist.c include/utils/llist.h \
		utils/string.c include/utils/string.h \
		utils/time.c include/utils/time.h \
		utils/unixsock.c include/utils/unixsock.h
libsysdb_la_CFLAGS = $(AM_CFLAGS)
libsysdb_la_CPPFLAGS = $(AM_CPPFLAGS) $(LTDLINCL)
libsysdb_la_LDFLAGS = -version-info 0:0:0 -pthread
libsysdb_la_LIBADD = $(LIBLTDL) -lrt liboconfig/liboconfig.la
libsysdb_la_DEPENDENCIES = liboconfig/liboconfig.la

if BUILD_WITH_LIBDBI
libsysdb_la_SOURCES += \
		utils/dbi.c include/utils/dbi.h
libsysdb_la_LIBADD += -ldbi
endif

bin_PROGRAMS = sysdbd

sysdbd_SOURCES = daemon/sysdbd.c include/sysdb.h \
		daemon/config.c include/daemon/config.h
sysdbd_CFLAGS = $(AM_CFLAGS) -DBUILD_DATE="\"$$( date --utc '+%F %T' ) (UTC)\""
sysdbd_LDADD = libsysdb.la liboconfig/liboconfig.la
sysdbd_DEPENDENCIES = liboconfig/liboconfig.la

sdbconfdir = $(sysconfdir)/sysdb
dist_sdbconf_DATA = daemon/sysdbd.conf.sample

pkgbackendlibdir = $(pkglibdir)/backend

pkgbackendlib_LTLIBRARIES =

if BUILD_PLUGIN_COLLECTD
pkgbackendlib_LTLIBRARIES += backend/collectd.la
backend_collectd_la_SOURCES = backend/collectd.c
backend_collectd_la_LDFLAGS = -module -avoid-version
libsysdb_la_LIBADD += -dlopen backend/collectd.la
libsysdb_la_DEPENDENCIES += backend/collectd.la
endif

if BUILD_PLUGIN_MKLIVESTATUS
pkgbackendlib_LTLIBRARIES += backend/mk-livestatus.la
backend_mk_livestatus_la_SOURCES = backend/mk-livestatus.c
backend_mk_livestatus_la_LDFLAGS = -module -avoid-version
libsysdb_la_LIBADD += -dlopen backend/mk-livestatus.la
libsysdb_la_DEPENDENCIES += backend/mk-livestatus.la
endif

if BUILD_PLUGIN_PUPPETSTORECONFIGS
pkgbackendlib_LTLIBRARIES += backend/puppet-storeconfigs.la
backend_puppet_storeconfigs_la_SOURCES = backend/puppet-storeconfigs.c
backend_puppet_storeconfigs_la_LDFLAGS = -module -avoid-version
libsysdb_la_LIBADD += -dlopen backend/puppet-storeconfigs.la
libsysdb_la_DEPENDENCIES += backend/puppet-storeconfigs.la
endif

include/sysdb.h: include/sysdb.h.in ../version
	source ../version; sed \
	    -e "s/@SDB_VERSION_MAJOR@/$$VERSION_MAJOR/g" \
	    -e "s/@SDB_VERSION_MINOR@/$$VERSION_MINOR/g" \
	    -e "s/@SDB_VERSION_PATCH@/$$VERSION_PATCH/g" \
	    -e "s/@SDB_VERSION_EXTRA@/$$VERSION_EXTRA/g" \
	    -e "s/@SDB_VERSION_STRING@/$$VERSION_STRING/g" \
	    include/sysdb.h.in > include/sysdb.h

