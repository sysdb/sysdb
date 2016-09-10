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
# Integration tests using basic queries.
#

set -ex

source "$( dirname "$0" )/test_lib.sh"

cat <<EOF > "$SYSDBD_CONF"
Listen "$SOCKET_FILE"
PluginDir "$PLUGIN_DIR"
Interval 2

LoadPlugin "store::memory"
LoadPlugin "mock_timeseries"
EOF

run_sysdbd -D -C "$SYSDBD_CONF"
wait_for_sysdbd

cat <<EOF > "${SYSDBD_CONF}.sender"
Listen "${SOCKET_FILE}.sender"
PluginDir "$PLUGIN_DIR"
Interval 2

LoadPlugin "store::network"
<Plugin "store::network">
  Server "$SOCKET_FILE"
</Plugin>

LoadBackend "mock_plugin"
<Backend "mock_plugin">
</Backend>
EOF

run_sysdbd_foreground -D -C "${SYSDBD_CONF}.sender" &
SYSDBD_PID2=$!
trap "kill \$SYSDBD_PID2" EXIT
wait_for_sysdbd "${SOCKET_FILE}.sender"

# wait for initial data
sleep 3

# Check data-source names.
output="$( run_sysdb -H "$SOCKET_FILE" \
	-c "FETCH metric 'some.host.name'.'foo/bar/qux'" )"
echo "$output" | grep -F '"data_names": ["nameA", "nameB"]'

# TIMESERIES commands.
run_sysdb -H "$SOCKET_FILE" \
		-c "TIMESERIES 'invalid.host'.'invalid-metric'" && exit 1

output="$( run_sysdb -H "$SOCKET_FILE" \
	-c "TIMESERIES 'some.host.name'.'foo/bar/qux'" )"
echo "$output" \
	| grep -F '"value": "1.000000"' \
	| grep -F '"value": "2.000000"' \
	| grep -F '"value": "3.000000"' \
	| grep -F '"value": "4.000000"' \
	| grep -F '"value": "5.000000"' \
	| grep -F '"value": "6.000000"' \
	| grep -F '"value": "7.000000"' \
	| grep -F '"value": "8.000000"' \
	| grep -F '"value": "9.000000"' \
	| grep -F '"value": "10.000000"'

stop_sysdbd

# vim: set tw=78 sw=4 ts=4 noexpandtab :
