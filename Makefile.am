ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src
if BUILD_DOCUMENTATION
SUBDIRS += doc
endif
SUBDIRS += t

EXTRA_DIST = autogen.sh version-gen.sh

version: FORCE
	@# As a side-effect, this updates version.
	@echo Building $(PACKAGE_NAME) version $$( ./version-gen.sh )
.PHONY: FORCE

test: check

