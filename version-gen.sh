#! /bin/sh

DEFAULT_VERSION="0.8.0"

VERSION="$( git describe --tags 2> /dev/null \
	| sed -e 's/sysdb-//' || true )"

if test -z "$VERSION"; then
	VERSION="$DEFAULT_VERSION"
else
	git update-index -q --refresh || true
	if test -n "$( git diff-index --name-only HEAD || true )"; then
		VERSION="$VERSION-dirty"
	fi
fi

VERSION="$( echo "$VERSION" | sed -e 's/-/./g' )"
if test "x`uname -s`" = "xAIX" || test "x`uname -s`" = "xSunOS" ; then
	echo "$VERSION\c"
else
	echo -n "$VERSION"
fi

OLD_VERSION=""
if test -e version; then
	OLD_VERSION=$( sed -ne 's/^VERSION="\(.*\)"/\1/p' version )
fi

if test "$OLD_VERSION" != "$VERSION"; then
	VERSION_MAJOR=$( echo $VERSION | cut -d'.' -f1 )
	VERSION_MINOR=$( echo $VERSION | cut -d'.' -f2 )
	VERSION_PATCH=$( echo $VERSION | cut -d'.' -f3 )
	VERSION_EXTRA="\"$( echo $VERSION | cut -d'.' -f4- )\""
	test -z "$VERSION_EXTRA" || VERSION_EXTRA=".$VERSION_EXTRA"
	(
	 echo "VERSION=\"$VERSION\""
	 echo "VERSION_MAJOR=$VERSION_MAJOR"
	 echo "VERSION_MINOR=$VERSION_MINOR"
	 echo "VERSION_PATCH=$VERSION_PATCH"
	 echo "VERSION_EXTRA=\"$VERSION_EXTRA\""
	 echo "VERSION_STRING=\"$VERSION_MAJOR.$VERSION_MINOR.$VERSION_PATCH\""
	) > version
fi

