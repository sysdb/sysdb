#! /bin/bash
#
# SysDB -- t/testwrapper.sh
#
# A wrapper script for running tests.

set -e

MEMCHECK="valgrind --quiet --tool=memcheck --error-exitcode=1"
MEMCHECK="$MEMCHECK --trace-children=yes"
MEMCHECK="$MEMCHECK --leak-check=full"

case "$1" in
	*unit/*)
		exec $MEMCHECK "$@"
		;;
	*)
		exec "$@"
		;;
esac

# vim: set tw=78 sw=4 ts=4 noexpandtab :

