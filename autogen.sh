#! /bin/sh

libtoolize=libtoolize

if which glibtoolize > /dev/null 2>&1; then
	libtoolize=glibtoolize
fi

set -ex

aclocal -I m4 --force --warnings=all
$libtoolize --automake --copy --force
aclocal -I m4
autoconf --force --warnings=all
autoheader --force --warnings=all
automake --add-missing --copy --foreign --warnings=all

