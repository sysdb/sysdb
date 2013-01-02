EXTRA_DIST = sysdbd.1.txt
CLEANFILES = sysdbd.1

man_MANS = sysdbd.1

sysdbd.1: sysdbd.1.txt ../version

%.1: %.1.txt
	@A2X@ -d manpage -f manpage \
		-apackage_version=$(PACKAGE_VERSION) \
		-abuild_date="$$( date --utc '+%F' )" \
		$<

