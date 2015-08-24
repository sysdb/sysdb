#! /bin/bash
#
# SysDB -- t/integration/store.sh
# Copyright (C) 2015 Sebastian 'tokkee' Harl <sh@tokkee.org>
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
# Integration tests for the STORE command.
#

set -ex

source "$( dirname "$0" )/test_lib.sh"

cat <<EOF > "$SYSDBD_CONF"
Listen "$SOCKET_FILE"
PluginDir "$PLUGIN_DIR"

LoadPlugin "store::memory"
EOF

run_sysdbd -D -C "$SYSDBD_CONF"
wait_for_sysdbd

# wait for things to settle and no data to show up
sleep 3

output="$( run_sysdb -H "$SOCKET_FILE" -c 'LIST hosts' )"
test "$output" = '[]' || exit 1

# populate the database
run_sysdb -H "$SOCKET_FILE" -c "STORE host 'h1'"
run_sysdb -H "$SOCKET_FILE" -c "STORE service 'h1'.'s1'"
run_sysdb -H "$SOCKET_FILE" -c "STORE metric 'h1'.'m1'"
run_sysdb -H "$SOCKET_FILE" -c "STORE host attribute 'h1'.'attrH' 'valueH'"
run_sysdb -H "$SOCKET_FILE" -c "STORE service attribute 'h1'.'s1'.'attrS' 'valueS'"
run_sysdb -H "$SOCKET_FILE" -c "STORE metric attribute 'h1'.'m1'.'attrM' 'valueM'"

# check the database
output="$( run_sysdb -H "$SOCKET_FILE" -c 'LIST hosts' )"
echo "$output" \
	| grep -F '"h1"'
output="$( run_sysdb -H "$SOCKET_FILE" -c "FETCH host 'h1'" )"
echo "$output" \
	| grep -F '"h1"' \
	| grep -F '"attrH"' \
	| grep -F '"valueH"'

output="$( run_sysdb -H "$SOCKET_FILE" -c 'LIST services' )"
echo "$output" \
	| grep -F '"h1"' \
	| grep -F '"s1"'
output="$( run_sysdb -H "$SOCKET_FILE" -c "FETCH service 'h1'.'s1'" )"
echo "$output" \
	| grep -F '"h1"' \
	| grep -F '"s1"' \
	| grep -F '"attrS"' \
	| grep -F '"valueS"'

output="$( run_sysdb -H "$SOCKET_FILE" -c 'LIST metrics' )"
echo "$output" \
	| grep -F '"h1"' \
	| grep -F '"m1"'
output="$( run_sysdb -H "$SOCKET_FILE" -c "FETCH metric 'h1'.'m1'" )"
echo "$output" \
	| grep -F '"h1"' \
	| grep -F '"m1"' \
	| grep -F '"attrM"' \
	| grep -F '"valueM"'

stop_sysdbd

# vim: set tw=78 sw=4 ts=4 noexpandtab :
