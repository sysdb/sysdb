SUBDIRS = liboconfig

# coverage related flags are only set when gcov is enabled
AM_CFLAGS = @STRICT_CFLAGS@ @COVERAGE_CFLAGS@ @PROFILING_CFLAGS@
AM_LDFLAGS = @COVERAGE_LDFLAGS@ @PROFILING_LDFLAGS@
AM_CPPFLAGS  = -I"${top_srcdir}/src/include"
AM_CPPFLAGS += -DSYSCONFDIR='"${sysconfdir}"'
AM_CPPFLAGS += -DLOCALSTATEDIR='"${localstatedir}"'
AM_CPPFLAGS += -DPKGLIBDIR='"${pkglibdir}"'

AM_YFLAGS = -d

BUILT_SOURCES = include/client/sysdb.h include/sysdb.h \
		frontend/grammar.h
EXTRA_DIST = include/client/sysdb.h.in include/sysdb.h.in

pkginclude_HEADERS = include/sysdb.h
pkgcoreincludedir = $(pkgincludedir)/core
pkgcoreinclude_HEADERS = \
		include/core/data.h \
		include/core/object.h \
		include/core/plugin.h \
		include/core/store.h \
		include/core/time.h
pkgfeincludedir = $(pkgincludedir)/frontend
pkgfeinclude_HEADERS = \
		include/frontend/connection.h \
		include/frontend/parser.h \
		include/frontend/proto.h \
		include/frontend/sock.h
pkgutilsincludedir = $(pkgincludedir)/utils
pkgutilsinclude_HEADERS = \
		include/utils/channel.h \
		include/utils/dbi.h \
		include/utils/error.h \
		include/utils/llist.h \
		include/utils/proto.h \
		include/utils/strbuf.h \
		include/utils/unixsock.h

pkgclientincludedir = $(pkgincludedir)/client
pkgclientinclude_HEADERS = \
		include/client/sock.h \
		include/client/sysdb.h

noinst_LTLIBRARIES = libsysdbclient.la libsysdb.la

libsysdbclient_la_SOURCES = \
		client/client.c include/client/sysdb.h \
		client/sock.c include/client/sock.h \
		utils/error.c include/utils/error.h \
		utils/proto.c include/utils/proto.h \
		utils/strbuf.c include/utils/strbuf.h
libsysdbclient_la_CFLAGS = $(AM_CFLAGS)
libsysdbclient_la_CPPFLAGS = $(AM_CPPFLAGS) $(LTDLINCL)
libsysdbclient_la_LDFLAGS = $(AM_LDFLAGS) -version-info 0:0:0 -pthread
libsysdbclient_la_LIBADD = $(LIBLTDL)

# don't use strict CFLAGS for flex code
noinst_LTLIBRARIES += libsysdb_fe_parser.la
libsysdb_fe_parser_la_SOURCES = \
		frontend/grammar.y frontend/scanner.l
libsysdb_fe_parser_la_CFLAGS = @COVERAGE_CFLAGS@ @PROFILING_CFLAGS@ \
		-DBUILD_DATE="\"$$( date --utc '+%F %T' ) (UTC)\""
libsysdb_la_SOURCES = \
		sysdb.c include/sysdb.h \
		core/object.c include/core/object.h \
		core/plugin.c include/core/plugin.h \
		core/store.c include/core/store.h \
		core/store-private.h \
		core/store_lookup.c \
		core/data.c include/core/data.h \
		frontend/connection.c include/frontend/connection.h \
		frontend/connection-private.h \
		frontend/parser.c include/frontend/parser.h \
		frontend/sock.c include/frontend/sock.h \
		frontend/session.c \
		frontend/query.c \
		utils/channel.c include/utils/channel.h \
		utils/error.c include/utils/error.h \
		utils/llist.c include/utils/llist.h \
		utils/os.c include/utils/os.h \
		utils/proto.c include/utils/proto.h \
		utils/strbuf.c include/utils/strbuf.h \
		core/time.c include/core/time.h \
		utils/unixsock.c include/utils/unixsock.h
libsysdb_la_CFLAGS = $(AM_CFLAGS)
libsysdb_la_CPPFLAGS = $(AM_CPPFLAGS) $(LTDLINCL)
libsysdb_la_LDFLAGS = $(AM_LDFLAGS) -version-info 0:0:0 -pthread
libsysdb_la_LIBADD = libsysdb_fe_parser.la \
		$(LIBLTDL) -lrt liboconfig/liboconfig.la
libsysdb_la_DEPENDENCIES = libsysdb_fe_parser.la liboconfig/liboconfig.la

if BUILD_WITH_LIBDBI
libsysdb_la_SOURCES += \
		utils/dbi.c include/utils/dbi.h
libsysdb_la_LIBADD += -ldbi
endif

bin_PROGRAMS = sysdbd
if BUILD_CLIENT
bin_PROGRAMS += sysdb

# don't use strict CFLAGS for flex code
noinst_LTLIBRARIES += libsysdb_scanner.la
libsysdb_scanner_la_SOURCES = tools/sysdb/scanner.l
libsysdb_scanner_la_CFLAGS = @COVERAGE_CFLAGS@ @PROFILING_CFLAGS@ \
		-DBUILD_DATE="\"$$( date --utc '+%F %T' ) (UTC)\""
sysdb_SOURCES = tools/sysdb/main.c include/client/sysdb.h \
		tools/sysdb/command.c tools/sysdb/command.h \
		tools/sysdb/input.c tools/sysdb/input.h \
		core/object.c include/core/object.h \
		utils/llist.c include/utils/llist.h
sysdb_CFLAGS = -DBUILD_DATE="\"$$( date --utc '+%F %T' ) (UTC)\"" \
		$(AM_CFLAGS) @READLINE_CFLAGS@
sysdb_LDADD = libsysdb_scanner.la libsysdbclient.la @READLINE_LIBS@
endif

sysdbd_SOURCES = tools/sysdbd/main.c include/sysdb.h \
		tools/sysdbd/configfile.c tools/sysdbd/configfile.h \
		$(libsysdb_la_SOURCES)
sysdbd_CFLAGS = $(AM_CFLAGS) -DBUILD_DATE="\"$$( date --utc '+%F %T' ) (UTC)\""
sysdbd_CPPFLAGS = $(AM_CPPFLAGS) $(LTDLINCL)
sysdbd_LDFLAGS = $(AM_LDFLAGS) -export-dynamic -pthread
sysdbd_LDADD = libsysdb_fe_parser.la liboconfig/liboconfig.la \
		$(LIBLTDL) -lrt
sysdbd_DEPENDENCIES = libsysdb_fe_parser.la liboconfig/liboconfig.la

if BUILD_WITH_LIBDBI
sysdbd_LDADD += -ldbi
endif

sdbconfdir = $(sysconfdir)/sysdb
dist_sdbconf_DATA = tools/sysdbd/sysdbd.conf.sample

pkgbackendlibdir = $(pkglibdir)/backend
pkgbackendcollectdlibdir = $(pkgbackendlibdir)/collectd
pkgbackendpuppetlibdir = $(pkgbackendlibdir)/puppet
pkgcnamelibdir = $(pkglibdir)/cname

pkglib_LTLIBRARIES =
pkgbackendlib_LTLIBRARIES =
pkgbackendcollectdlib_LTLIBRARIES =
pkgbackendpuppetlib_LTLIBRARIES =
pkgcnamelib_LTLIBRARIES =

if BUILD_PLUGIN_CNAMEDNS
pkgcnamelib_LTLIBRARIES += plugins/cname/dns.la
plugins_cname_dns_la_SOURCE = plugins/cname/dns.c
plugins_cname_dns_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version
sysdbd_LDADD += -dlopen plugins/cname/dns.la
sysdbd_DEPENDENCIES += plugins/cname/dns.la
endif

if BUILD_PLUGIN_COLLECTD
pkgbackendcollectdlib_LTLIBRARIES += backend/collectd/unixsock.la
backend_collectd_unixsock_la_SOURCES = backend/collectd/unixsock.c
backend_collectd_unixsock_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version
sysdbd_LDADD += -dlopen backend/collectd/unixsock.la
sysdbd_DEPENDENCIES += backend/collectd/unixsock.la
endif

if BUILD_PLUGIN_MKLIVESTATUS
pkgbackendlib_LTLIBRARIES += backend/mk-livestatus.la
backend_mk_livestatus_la_SOURCES = backend/mk-livestatus.c
backend_mk_livestatus_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version
sysdbd_LDADD += -dlopen backend/mk-livestatus.la
sysdbd_DEPENDENCIES += backend/mk-livestatus.la
endif

if BUILD_PLUGIN_PUPPETSTORECONFIGS
pkgbackendpuppetlib_LTLIBRARIES += backend/puppet/store-configs.la
backend_puppet_store_configs_la_SOURCES = backend/puppet/store-configs.c
backend_puppet_store_configs_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version
sysdbd_LDADD += -dlopen backend/puppet/store-configs.la
sysdbd_DEPENDENCIES += backend/puppet/store-configs.la
endif

if BUILD_PLUGIN_SYSLOG
pkglib_LTLIBRARIES += plugins/syslog.la
plugins_syslog_la_SOURCE = plugins/syslog.c
plugins_syslog_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version
sysdbd_LDADD += -dlopen plugins/syslog.la
sysdbd_DEPENDENCIES += plugins/syslog.la
endif

include/client/sysdb.h: include/client/sysdb.h.in ../version
	source ../version; sed \
	    -e "s/@SDB_VERSION_MAJOR@/$$VERSION_MAJOR/g" \
	    -e "s/@SDB_VERSION_MINOR@/$$VERSION_MINOR/g" \
	    -e "s/@SDB_VERSION_PATCH@/$$VERSION_PATCH/g" \
	    -e "s/@SDB_VERSION_EXTRA@/$$VERSION_EXTRA/g" \
	    -e "s/@SDB_VERSION_STRING@/$$VERSION_STRING/g" \
	    include/client/sysdb.h.in > include/client/sysdb.h

include/sysdb.h: include/sysdb.h.in ../version
	source ../version; sed \
	    -e "s/@SDB_VERSION_MAJOR@/$$VERSION_MAJOR/g" \
	    -e "s/@SDB_VERSION_MINOR@/$$VERSION_MINOR/g" \
	    -e "s/@SDB_VERSION_PATCH@/$$VERSION_PATCH/g" \
	    -e "s/@SDB_VERSION_EXTRA@/$$VERSION_EXTRA/g" \
	    -e "s/@SDB_VERSION_STRING@/$$VERSION_STRING/g" \
	    include/sysdb.h.in > include/sysdb.h

../version:
	cd .. && make version
