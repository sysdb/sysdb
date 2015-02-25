#
# SysDB -- t/integration/test_lib.sh
# Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
# Shell library of test helpers for integration tests.
#

TOP_SRCDIR="$( readlink -f "$( dirname "$0" )/../.." )"
TESTDIR="$( mktemp -d )"
trap "rm -rf '$TESTDIR'; test -z \$SYSDBD_PID || kill \$SYSDBD_PID" EXIT

mkdir "$TESTDIR/backend"
cp "$TOP_SRCDIR/t/integration/.libs/mock_timeseries.so" "$TESTDIR"
cp "$TOP_SRCDIR/t/integration/.libs/mock_plugin.so" "$TESTDIR/backend"

mkdir "$TESTDIR/store"
cp "$TOP_SRCDIR/src/plugins/store/.libs/network.so" "$TESTDIR/store"

cp "$TOP_SRCDIR"/src/sysdb "$TESTDIR"
cp "$TOP_SRCDIR"/src/sysdbd "$TESTDIR"

MEMCHECK="valgrind --quiet --tool=memcheck --error-exitcode=1"
MEMCHECK="$MEMCHECK --trace-children=yes"
MEMCHECK="$MEMCHECK --track-fds=yes"
MEMCHECK="$MEMCHECK --leak-check=full"
MEMCHECK="$MEMCHECK --suppressions=$TOP_SRCDIR/t/valgrind.suppress"
MEMCHECK="$MEMCHECK --gen-suppressions=all"

SYSDBD_CONF="$TESTDIR/sysdbd.conf"

SOCKET_FILE="$TESTDIR/sock"
PLUGIN_DIR="$TESTDIR"

CA_KEY=""
CA_CERT=""
SERVER_KEY=""
SERVER_CERT=""
CLIENT_KEY=""
CLIENT_CERT=""

SYSDB_USER="$( id -un )"

function run_sysdb() {
	$MEMCHECK "$TESTDIR/sysdb" -U $SYSDB_USER "$@"
}

function run_sysdb_nouser() {
	$MEMCHECK "$TESTDIR/sysdb" "$@"
}

function run_sysdbd() {
	$MEMCHECK "$TESTDIR/sysdbd" "$@" &
	SYSDBD_PID=$!
}

function run_sysdbd_foreground() {
	$MEMCHECK "$TESTDIR/sysdbd" "$@"
}

function stop_sysdbd() {
	if test -z "$SYSDBD_PID"; then
		echo "Cannot stop sysdbd; PID unknown" >&2
		exit 1
	fi
	kill $SYSDBD_PID
	wait $SYSDBD_PID
	SYSDBD_PID=''
}

function wait_for_sysdbd() {
	local socket="$SOCKET_FILE"
	if test -n "$1"; then
		socket="$1"
	fi
	local i
	for (( i=0; i<10; i++ )); do
		if test -e "$socket"; then
			break
		fi
		sleep 1
	done
	if test $i -eq 10; then
		echo 'SysDBd did not start within 10 seconds' >&2
		exit 1
	fi
}

function wait_for_sysdbd_tcp() {
	local host="$1"
	local port="$2"
	local i
	for (( i=0; i<10; i++ )); do
		if echo | nc "$host" "$port"; then
			break
		fi
		sleep 1
	done
	if test $i -eq 10; then
		echo 'SysDBd did not start within 10 seconds' >&2
		exit 1
	fi
}

function setup_ssl() {
	CA_KEY="$TESTDIR/cacert.key"
	CA_CERT="$TESTDIR/cacert.cert"
	openssl genrsa -out "$CA_KEY" 2048
	openssl req -batch -subj '/CN=Some CA' \
		-x509 -new -key "$CA_KEY" -out "$CA_CERT" -days 1

	SERVER_KEY="$TESTDIR/server.key"
	SERVER_CERT="$TESTDIR/server.cert"
	openssl genrsa -out "$SERVER_KEY" 2048
	openssl req -batch -subj '/CN=localhost' \
		-new -out "${SERVER_CERT}.csr" -key "$SERVER_KEY"
	openssl x509 -req -in "${SERVER_CERT}.csr" -out "$SERVER_CERT" -days 1 \
		-CAkey "$CA_KEY" -CA "$CA_CERT" -CAcreateserial \
		-CAserial ${TESTDIR}/serial

	CLIENT_KEY="$TESTDIR/client.key"
	CLIENT_CERT="$TESTDIR/client.cert"
	openssl genrsa -out "$CLIENT_KEY" 2048
	openssl req -batch -subj "/CN=$SYSDB_USER" \
		-new -out "${CLIENT_CERT}.csr" -key "$CLIENT_KEY"
	openssl x509 -req -in "${CLIENT_CERT}.csr" -out "$CLIENT_CERT" -days 1 \
		-CAkey "$CA_KEY" -CA "$CA_CERT" -CAcreateserial \
		-CAserial ${TESTDIR}/serial
}

# vim: set tw=78 sw=4 ts=4 noexpandtab :
