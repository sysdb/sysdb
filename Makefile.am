ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src
if BUILD_DOCUMENTATION
SUBDIRS += doc
endif

EXTRA_DIST = autogen.sh version-gen.sh

version: FORCE
	@# As a side-effect, this updates version.
	@echo Building $(PACKAGE_NAME) version $$( cd .. && ./version-gen.sh )
.PHONY: FORCE

