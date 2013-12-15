SUFFIXES = .1 .5 .1.txt .5.txt

EXTRA_DIST = \
		sysdbd.1.txt \
		sysdbd.conf.5.txt
CLEANFILES = \
		sysdbd.1 \
		sysdbd.conf.5

man_MANS = \
		sysdbd.1 \
		sysdbd.conf.5

sysdbd.1: sysdbd.1.txt ../version

sysdbd.conf.5: sysdbd.conf.5.txt ../version

.1.txt.1:
	@A2X@ -d manpage -f manpage \
		-apackage_version=$(PACKAGE_VERSION) \
		-abuild_date="$$( date --utc '+%F' )" \
		$<

.5.txt.5:
	@A2X@ -d manpage -f manpage \
		-apackage_version=$(PACKAGE_VERSION) \
		-abuild_date="$$( date --utc '+%F' )" \
		$<

