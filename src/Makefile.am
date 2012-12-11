SUBDIRS = liboconfig

AM_CFLAGS = @STRICT_CFLAGS@ -Iinclude
AM_CPPFLAGS  = -DSYSCONFDIR='"${sysconfdir}"'
AM_CPPFLAGS += -DPKGLIBDIR='"${pkglibdir}"'

BUILT_SOURCES = include/syscollector.h

pkginclude_HEADERS = include/syscollector.h
pkgcoreincludedir = $(pkgincludedir)/core
pkgcoreinclude_HEADERS = \
		include/core/object.h \
		include/core/plugin.h
pkgutilsincludedir = $(pkgincludedir)/utils
pkgutilsinclude_HEADERS = \
		include/utils/llist.h \
		include/utils/string.h

lib_LTLIBRARIES = libsyscollector.la

libsyscollector_la_SOURCES = \
		syscollector.c include/syscollector.h \
		core/object.c include/core/object.h \
		core/plugin.c include/core/plugin.h \
		core/store.c include/core/store.h \
		utils/llist.c include/utils/llist.h \
		utils/string.c include/utils/string.h \
		utils/time.c include/utils/time.h \
		utils/unixsock.c include/utils/unixsock.h
libsyscollector_la_CFLAGS = $(AM_CFLAGS)
libsyscollector_la_CPPFLAGS = $(AM_CPPFLAGS) $(LTDLINCL)
libsyscollector_la_LDFLAGS = -version-info 0:0:0 -pthread
libsyscollector_la_LIBADD = $(LIBLTDL) -lrt liboconfig/liboconfig.la
libsyscollector_la_DEPENDENCIES = liboconfig/liboconfig.la

if BUILD_WITH_LIBDBI
libsyscollector_la_SOURCES += \
		utils/dbi.c include/utils/dbi.h
libsyscollector_la_LIBADD += -ldbi
endif

bin_PROGRAMS = syscollectord

syscollectord_SOURCES = daemon/syscollectord.c include/syscollector.h \
		daemon/config.c include/daemon/config.h
syscollectord_CFLAGS = $(AM_CFLAGS) -DBUILD_DATE="\"$$( date --utc '+%F %T' ) (UTC)\""
syscollectord_LDADD = libsyscollector.la liboconfig/liboconfig.la
syscollectord_DEPENDENCIES = liboconfig/liboconfig.la

pkgbackendlibdir = $(pkglibdir)/backend

pkgbackendlib_LTLIBRARIES =

if BUILD_PLUGIN_COLLECTD
pkgbackendlib_LTLIBRARIES += backend/collectd.la
backend_collectd_la_SOURCES = backend/collectd.c
backend_collectd_la_LDFLAGS = -module -avoid-version
libsyscollector_la_LIBADD += -dlopen backend/collectd.la
libsyscollector_la_DEPENDENCIES += backend/collectd.la
endif

if BUILD_PLUGIN_MKLIVESTATUS
pkgbackendlib_LTLIBRARIES += backend/mk-livestatus.la
backend_mk_livestatus_la_SOURCES = backend/mk-livestatus.c
backend_mk_livestatus_la_LDFLAGS = -module -avoid-version
libsyscollector_la_LIBADD += -dlopen backend/mk-livestatus.la
libsyscollector_la_DEPENDENCIES += backend/mk-livestatus.la
endif

if BUILD_PLUGIN_PUPPETSTORECONFIGS
pkgbackendlib_LTLIBRARIES += backend/puppet-storeconfigs.la
backend_puppet_storeconfigs_la_SOURCES = backend/puppet-storeconfigs.c
backend_puppet_storeconfigs_la_LDFLAGS = -module -avoid-version
libsyscollector_la_LIBADD += -dlopen backend/puppet-storeconfigs.la
libsyscollector_la_DEPENDENCIES += backend/puppet-storeconfigs.la
endif

include/syscollector.h: include/syscollector.h.in ../version
	source ../version; sed \
	    -e "s/@SC_VERSION_MAJOR@/$$VERSION_MAJOR/g" \
	    -e "s/@SC_VERSION_MINOR@/$$VERSION_MINOR/g" \
	    -e "s/@SC_VERSION_PATCH@/$$VERSION_PATCH/g" \
	    -e "s/@SC_VERSION_EXTRA@/$$VERSION_EXTRA/g" \
	    -e "s/@SC_VERSION_STRING@/$$VERSION_STRING/g" \
	    include/syscollector.h.in > include/syscollector.h

install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/syscollector
	if test -e $(DESTDIR)$(sysconfdir)/syscollector/syscollectord.conf; then \
		$(INSTALL) -m 0640 daemon/syscollectord.conf \
			$(DESTDIR)$(sysconfdir)/syscollector/syscollectord.conf.pkg-orig; \
	else \
		$(INSTALL) -m 0640 daemon/syscollectord.conf \
			$(DESTDIR)$(sysconfdir)/syscollector/syscollectord.conf; \
	fi

