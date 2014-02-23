#! /bin/bash
#
# SysDB -- t/coverage.sh
#
# Create a test coverage report for the System DataBase.

set -e

if ! which lcov || ! which genhtml; then
	echo "lcov is required in order to generate a test coverage report" >&2
	exit 1
fi

cd $( dirname "$0" )/..

if test -e t/coverage; then
	echo "t/coverage exists already; please remove manually" >&2
	exit 1
fi

V=$( grep '^PACKAGE_VERSION' Makefile | cut -d' ' -f3 )
if test -z "$V"; then
	echo "Unable to determine package version" >&2
	exit 1
fi

make dist

tmpdir=$( mktemp -d -t )
trap "rm -rf $tmpdir" EXIT

srcdir=$( pwd )

cd $tmpdir
tar --strip-components=1 -zxf "$srcdir"/sysdb-$V.tar.gz
if test -d "$srcdir"/.git/; then
	# copy Git database for version-gen.sh
	cp -a "$srcdir"/.git .

	# reset all files which are not part of the tarball
	git checkout HEAD .gitignore .travis.yml
fi

./configure --enable-gcov --disable-shared CFLAGS="-O0 -g"
make

V1=$( grep '^PACKAGE_VERSION' Makefile | cut -d' ' -f3 )
if test -z "$V1"; then
	# this should not happen
	V1="$V"
fi

lcov --base-directory src --directory src --zerocount
make test
lcov --base-directory src --directory src --no-external \
	--capture -o sysdb_coverage.info

genhtml -o "$srcdir"/t/coverage \
	-t "SysDB $V1 test coverage" --num-spaces=4 --legend \
	sysdb_coverage.info

