#! /bin/bash
#
# SysDB -- t/cibuild.sh
#
# Run a continuous integration build for the System DataBase.

set -ex

./autogen.sh
./configure --enable-standards --enable-gcov $CIOPTIONS \
	CFLAGS="-O0 $CICFLAGS" \
	LDFLAGS="$CILDFLAGS"
make clean all

case "$CICFLAGS" in
	*sanitize=address*)
		# skip regular tests
		;;
	*)
		make -j10 test TESTS_ENVIRONMENT="./testwrapper.sh"
		;;
esac

