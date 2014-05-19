SUFFIXES = .1 .5 .7 .1.html .5.html .7.html .1.txt .5.txt .7.txt

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
		docbook-xsl.css \
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
		docbook-xsl.css \
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

ADOC_ATTRS = -apackage_version=$(PACKAGE_VERSION) \
		-abuild_date="$$( date --utc '+%F' )" \
		-alocalstatedir=$(localstatedir)
HTML_ATTRS = --stylesheet=$(srcdir)/sysdb-man.css

sysdb.1: sysdb.1.txt ../version
sysdbd.1: sysdbd.1.txt ../version
sysdbd.conf.5: sysdbd.conf.5.txt ../version
sysdbd-cname-dns.5: sysdbd-cname-dns.5.txt ../version
sysdbd-collectd-unixsock.5: sysdbd-collectd-unixsock.5.txt ../version
sysdbd-mk-livestatus.5: sysdbd-mk-livestatus.5.txt ../version
sysdbd-puppet-store-configs.5: sysdbd-puppet-store-configs.5.txt ../version
sysdbd-syslog.5: sysdbd-syslog.5.txt ../version
sysdb.7: sysdb.7.txt sysdb-description.txt ../version
sysdbql.7: sysdbql.7.txt sysdb-description.txt ../version

sysdb.1.html: sysdb.1.txt ../version
sysdbd.1.html: sysdbd.1.txt ../version
sysdbd.conf.5.html: sysdbd.conf.5.txt ../version
sysdbd-cname-dns.5.html: sysdbd-cname-dns.5.txt ../version
sysdbd-collectd-unixsock.5.html: sysdbd-collectd-unixsock.5.txt ../version
sysdbd-mk-livestatus.5.html: sysdbd-mk-livestatus.5.txt ../version
sysdbd-puppet-store-configs.5.html: sysdbd-puppet-store-configs.5.txt ../version
sysdbd-syslog.5.html: sysdbd-syslog.5.txt ../version
sysdb.7.html: sysdb.7.txt sysdb-description.txt ../version
sysdbql.7.html: sysdbql.7.txt sysdb-description.txt ../version

# This is an ugly hack but fine for now.
docbook-xsl.css:
	test -e docbook-xsl.css || rm -f sysdb.1.html && make sysdb.1.html

.1.txt.1:
	@A2X@ -d manpage -f manpage $(ADOC_ATTRS) $<
.1.txt.1.html:
	@A2X@ -d manpage -f xhtml $(HTML_ATTRS) $(ADOC_ATTRS) $<

.5.txt.5:
	@A2X@ -d manpage -f manpage $(ADOC_ATTRS) $<
.5.txt.5.html:
	@A2X@ -d manpage -f xhtml $(HTML_ATTRS) $(ADOC_ATTRS) $<

.7.txt.7:
	@A2X@ -d manpage -f manpage $(ADOC_ATTRS) $<
.7.txt.7.html:
	@A2X@ -d manpage -f xhtml $(HTML_ATTRS) $(ADOC_ATTRS) $<

