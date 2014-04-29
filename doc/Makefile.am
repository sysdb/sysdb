SUFFIXES = .1 .5 .1.html .5.html .1.txt .5.txt

EXTRA_DIST = \
		sysdb.1.txt \
		sysdbd.1.txt \
		sysdbd.conf.5.txt
CLEANFILES = \
		sysdb.1 \
		sysdbd.1 \
		sysdbd.conf.5

man_MANS = \
		sysdb.1 \
		sysdbd.1 \
		sysdbd.conf.5

html_DATA = \
		sysdb.1.html \
		sysdbd.1.html \
		sysdbd.conf.5.html

ADOC_ATTRS = -apackage_version=$(PACKAGE_VERSION) \
		-abuild_date="$$( date --utc '+%F' )" \
		-alocalstatedir=$(localstatedir)

sysdb.1: sysdb.1.txt ../version
sysdbd.1: sysdbd.1.txt ../version
sysdbd.conf.5: sysdbd.conf.5.txt ../version

sysdb.1.html: sysdb.1.txt ../version
sysdbd.1.html: sysdbd.1.txt ../version
sysdbd.conf.5.html: sysdbd.conf.5.txt ../version

.1.txt.1:
	@A2X@ -d manpage -f manpage $(ADOC_ATTRS) $<
.1.txt.1.html:
	@A2X@ -d manpage -f xhtml $(ADOC_ATTRS) $<

.5.txt.5:
	@A2X@ -d manpage -f manpage $(ADOC_ATTRS) $<
.5.txt.5.html:
	@A2X@ -d manpage -f xhtml $(ADOC_ATTRS) $<

