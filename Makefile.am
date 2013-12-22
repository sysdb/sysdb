ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src
if BUILD_DOCUMENTATION
SUBDIRS += doc
endif
if BUILD_TESTING
SUBDIRS += t
endif

EXTRA_DIST = autogen.sh coverage.sh version-gen.sh

version: FORCE
	@# As a side-effect, this updates version.
	@echo Building $(PACKAGE_NAME) version $$( ./version-gen.sh )
.PHONY: FORCE

test: check

