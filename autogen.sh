#! /bin/sh

if ! which yacc > /dev/null 2>&1 || ! which lex > /dev/null 2>&1; then
	echo "yacc and lex are required to build SysDB" >&2
	exit 1
fi

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

