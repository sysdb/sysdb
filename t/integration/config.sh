#! /bin/bash
#
# SysDB -- t/integration/simple_config.sh
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
# Integration tests using simple configuration snippets.
#

set -ex

source "$( dirname "$0" )/test_lib.sh"

# Very basics ;-)
run_sysdb -V
run_sysdb -h

run_sysdbd -V
run_sysdbd -h

# Simple invalid configuration examples.
cat <<EOF > "$SYSDBD_CONF"
Invalid "option"
EOF
if run_sysdbd_foreground -D -C "$SYSDBD_CONF"; then
	echo 'SysDBd accepted invalid option; expected: failure' >&2
	exit 1
fi

cat <<EOF > "$SYSDBD_CONF"
Listen "invalid://address"
EOF
if run_sysdbd_foreground -D -C "$SYSDBD_CONF"; then
	echo 'SysDBd accepted invalid listen address; expected: failure' >&2
	exit 1
fi

cat <<EOF > "$SYSDBD_CONF"
Interval "foo"
EOF
if run_sysdbd_foreground -D -C "$SYSDBD_CONF"; then
	echo 'SysDBd accepted invalid interval; expected: failure' >&2
	exit 1
fi

cat <<EOF > "$SYSDBD_CONF"
Interval -3.0
EOF
if run_sysdbd_foreground -D -C "$SYSDBD_CONF"; then
	echo 'SysDBd accepted invalid interval; expected: failure' >&2
	exit 1
fi

# Simple configuration examples.
cat <<EOF > "$SYSDBD_CONF"
Listen "$SOCKET_FILE"
EOF

run_sysdbd -D -C "$SYSDBD_CONF"
wait_for_sysdbd

# reconfigure
SOCKET_FILE="$SOCKET_FILE-2"
cat <<EOF > "$SYSDBD_CONF"
Listen "${SOCKET_FILE}"
EOF
kill -HUP $SYSDBD_PID
wait_for_sysdbd

stop_sysdbd

# vim: set tw=78 sw=4 ts=4 noexpandtab :
