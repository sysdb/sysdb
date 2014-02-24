#! /bin/bash
#
# SysDB -- t/cibuild.sh
#
# Run a continuous integration build for the System DataBase.

set -ex

./autogen.sh
./configure --enable-gcov $CIOPTIONS \
	CFLAGS="-O0 $CICFLAGS" \
	LDFLAGS="$CILDFLAGS"
make clean all
make test
