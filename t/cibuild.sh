#! /bin/bash
#
# SysDB -- t/cibuild.sh
#
# Run a continuous integration build for the System DataBase.

set -ex

./autogen.sh
./configure --enable-gcov CFLAGS="-O0 -Wno-sign-conversion"
make clean all
make test
