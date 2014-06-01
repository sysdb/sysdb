SUFFIXES = \
		.1 .5 .7 \
		.1.html .5.html .7.html \
		.1.txt .5.txt .7.txt \
		.1.xml .5.xml .7.xml

EXTRA_DIST = \
		sysdb.1.txt \
		sysdbd.1.txt \
		sysdbd.conf.5.txt \
		sysdbd-cname-dns.5.txt \
		sysdbd-collectd-unixsock.5.txt \
		sysdbd-mk-livestatus.5.txt \
		sysdbd-puppet-store-configs.5.txt \
		sysdbd-syslog.5.txt \
		sysdb.7.txt \
		sysdbql.7.txt
CLEANFILES = \
		sysdb.1 \
		sysdbd.1 \
		sysdbd.conf.5 \
		sysdbd-cname-dns.5 \
		sysdbd-collectd-unixsock.5 \
		sysdbd-mk-livestatus.5 \
		sysdbd-puppet-store-configs.5 \
		sysdbd-syslog.5 \
		sysdb.7 \
		sysdbql.7 \
		sysdb.1.html \
		sysdbd.1.html \
		sysdbd.conf.5.html \
		sysdbd-cname-dns.5.html \
		sysdbd-collectd-unixsock.5.html \
		sysdbd-mk-livestatus.5.html \
		sysdbd-puppet-store-configs.5.html \
		sysdbd-syslog.5.html \
		sysdb.7.html \
		sysdbql.7.html \
		sysdb.1.xml \
		sysdbd.1.xml \
		sysdbd.conf.5.xml \
		sysdbd-cname-dns.5.xml \
		sysdbd-collectd-unixsock.5.xml \
		sysdbd-mk-livestatus.5.xml \
		sysdbd-puppet-store-configs.5.xml \
		sysdbd-syslog.5.xml \
		sysdb.7.xml \
		sysdbql.7.xml

man_MANS = \
		sysdb.1 \
		sysdbd.1 \
		sysdbd.conf.5 \
		sysdbd-cname-dns.5 \
		sysdbd-collectd-unixsock.5 \
		sysdbd-mk-livestatus.5 \
		sysdbd-puppet-store-configs.5 \
		sysdbd-syslog.5 \
		sysdb.7 \
		sysdbql.7

html_DATA = \
		sysdb.1.html \
		sysdbd.1.html \
		sysdbd.conf.5.html \
		sysdbd-cname-dns.5.html \
		sysdbd-collectd-unixsock.5.html \
		sysdbd-mk-livestatus.5.html \
		sysdbd-puppet-store-configs.5.html \
		sysdbd-syslog.5.html \
		sysdb.7.html \
		sysdbql.7.html

ADOC_FLAGS = \
		-f$(srcdir)/asciidoc.conf \
		-apackage_version=$(PACKAGE_VERSION) \
		-alocalstatedir=$(localstatedir) \
		$(ADOCFLAGS)

MAN_DEPS = asciidoc.conf ../version

sysdb.1.xml: sysdb.1.txt $(MAN_DEPS)
sysdbd.1.xml: sysdbd.1.txt $(MAN_DEPS)
sysdbd.conf.5.xml: sysdbd.conf.5.txt $(MAN_DEPS)
sysdbd-cname-dns.5.xml: sysdbd-cname-dns.5.txt $(MAN_DEPS)
sysdbd-collectd-unixsock.5.xml: sysdbd-collectd-unixsock.5.txt $(MAN_DEPS)
sysdbd-mk-livestatus.5.xml: sysdbd-mk-livestatus.5.txt $(MAN_DEPS)
sysdbd-puppet-store-configs.5.xml: sysdbd-puppet-store-configs.5.txt $(MAN_DEPS)
sysdbd-syslog.5.xml: sysdbd-syslog.5.txt $(MAN_DEPS)
sysdb.7.xml: sysdb.7.txt sysdb-description.txt $(MAN_DEPS)
sysdbql.7.xml: sysdbql.7.txt sysdb-description.txt $(MAN_DEPS)

sysdb.1: sysdb.1.xml
sysdbd.1: sysdbd.1.xml
sysdbd.conf.5: sysdbd.conf.5.xml
sysdbd-cname-dns.5: sysdbd-cname-dns.5.xml
sysdbd-collectd-unixsock.5: sysdbd-collectd-unixsock.5.xml
sysdbd-mk-livestatus.5: sysdbd-mk-livestatus.5.xml
sysdbd-puppet-store-configs.5: sysdbd-puppet-store-configs.5.xml
sysdbd-syslog.5: sysdbd-syslog.5.xml
sysdb.7: sysdb.7.xml
sysdbql.7: sysdbql.7.xml

sysdb.1.html: sysdb.1.txt $(MAN_DEPS)
sysdbd.1.html: sysdbd.1.txt $(MAN_DEPS)
sysdbd.conf.5.html: sysdbd.conf.5.txt $(MAN_DEPS)
sysdbd-cname-dns.5.html: sysdbd-cname-dns.5.txt $(MAN_DEPS)
sysdbd-collectd-unixsock.5.html: sysdbd-collectd-unixsock.5.txt $(MAN_DEPS)
sysdbd-mk-livestatus.5.html: sysdbd-mk-livestatus.5.txt $(MAN_DEPS)
sysdbd-puppet-store-configs.5.html: sysdbd-puppet-store-configs.5.txt $(MAN_DEPS)
sysdbd-syslog.5.html: sysdbd-syslog.5.txt $(MAN_DEPS)
sysdb.7.html: sysdb.7.txt sysdb-description.txt $(MAN_DEPS)
sysdbql.7.html: sysdbql.7.txt sysdb-description.txt $(MAN_DEPS)

html-local: $(html_DATA)
install-html-local: install-htmlDATA

.1.txt.1.xml:
	@ASCIIDOC@ $(ADOC_FLAGS) -b docbook -d manpage $<
.5.txt.5.xml:
	@ASCIIDOC@ $(ADOC_FLAGS) -b docbook -d manpage $<
.7.txt.7.xml:
	@ASCIIDOC@ $(ADOC_FLAGS) -b docbook -d manpage $<

.1.xml.1:
	@XMLTO@ man $<
.5.xml.5:
	@XMLTO@ man $<
.7.xml.7:
	@XMLTO@ man $<

.1.txt.1.html:
	@ASCIIDOC@ $(ADOC_FLAGS) -b xhtml11 -d manpage $<
.5.txt.5.html:
	@ASCIIDOC@ $(ADOC_FLAGS) -b xhtml11 -d manpage $<
.7.txt.7.html:
	@ASCIIDOC@ $(ADOC_FLAGS) -b xhtml11 -d manpage $<

