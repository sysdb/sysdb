EXTRA_DIST = syscollectord.1.txt
CLEANFILES = syscollectord.1

man_MANS = syscollectord.1

syscollectord.1: syscollectord.1.txt ../version

%.1: %.1.txt
	@A2X@ -d manpage -f manpage \
		-apackage_version=$(PACKAGE_VERSION) \
		-abuild_date="$$( date --utc '+%F' )" \
		$<

