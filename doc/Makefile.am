SUFFIXES = .1 .5 .1.txt .5.txt

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

ADOC_ATTRS = -apackage_version=$(PACKAGE_VERSION) \
		-abuild_date="$$( date --utc '+%F' )" \
		-alocalstatedir=$(localstatedir)

sysdb.1: sysdb.1.txt ../version

sysdbd.1: sysdbd.1.txt ../version

sysdbd.conf.5: sysdbd.conf.5.txt ../version

.1.txt.1:
	@A2X@ -d manpage -f manpage $(ADOC_ATTRS) $<

.5.txt.5:
	@A2X@ -d manpage -f manpage $(ADOC_ATTRS) $<

