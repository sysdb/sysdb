#! /bin/bash
#
# SysDB -- t/integration/basic_query.sh
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
# Integration tests covering basic client functionality.
#

set -ex

source "$( dirname "$0" )/test_lib.sh"

cat <<EOF > "$SYSDBD_CONF"
Listen "$SOCKET_FILE"
PluginDir "$PLUGIN_DIR"
Interval 2

LoadPlugin "store::memory"
LoadBackend "mock_plugin"
<Backend "mock_plugin">
</Backend>

LoadPlugin "mock_timeseries"
EOF

run_sysdbd -D -C "$SYSDBD_CONF"
wait_for_sysdbd

# wait for initial data
sleep 3

# Usage errors.
output="$( run_sysdb -H "$SOCKET_FILE" --invalid 2>&1 )" && exit 1
echo "$output" | grep -F 'Usage:'
output="$( run_sysdb -H "$SOCKET_FILE" extra 2>&1 )" && exit 1
echo "$output" | grep -F 'Usage:'

# Invalid user.
output="$( run_sysdb_nouser -H "$SOCKET_FILE" \
  -U $SYSDB_USER-invalid -c 'LIST hosts' 2>&1 )" && exit 1
echo "$output" | grep -F 'Access denied'

# Unreachable server.
output="$( run_sysdb -H "${SOCKET_FILE}.doesnotexist" -c '' 2>&1 )" && exit 1
echo "$output" | grep "Failed to connect to SysDBd"

# On parse errors, expect a non-zero exit code.
output="$( run_sysdb -H "$SOCKET_FILE" -c INVALID 2>&1 )" && exit 1
echo "$output" | grep "Failed to parse query 'INVALID'"
echo "$output" | grep "parse error: syntax error"

# Empty query.
output="$( run_sysdb -H "$SOCKET_FILE" -c '' )"
test -z "$output"

# Default user.
output="$( run_sysdb_nouser -H "$SOCKET_FILE" -c '' )"

stop_sysdbd

# vim: set tw=78 sw=4 ts=4 noexpandtab :
