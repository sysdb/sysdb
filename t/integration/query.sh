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

LoadBackend mock_plugin
<Backend "mock_plugin">
</Backend>
EOF

run_sysdbd -D -C "$SYSDBD_CONF"

wait_for_sysdbd
sleep 3

# On parse errors, expect a non-zero exit code.
output="$( run_sysdb -H "$SOCKET_FILE" -c INVALID )" && exit 1
echo "$output" | grep "Failed to parse query 'INVALID'"
echo "$output" | grep "parse error: syntax error"

# Simple, successful commands.
output="$( run_sysdb -H "$SOCKET_FILE" -c 'LIST hosts' )"
echo "$output" \
	| grep -F '"host1.example.com"' \
	| grep -F '"host2.example.com"' \
	| grep -F '"localhost"' \
	| grep -F '"other.host.name"' \
	| grep -F '"some.host.name"'

output="$( echo 'LIST hosts;' | run_sysdb -H "$SOCKET_FILE" )" || echo $?
echo "$output" \
	| grep -F '"host1.example.com"' \
	| grep -F '"host2.example.com"' \
	| grep -F '"localhost"' \
	| grep -F '"other.host.name"' \
	| grep -F '"some.host.name"'

output="$( run_sysdb -H "$SOCKET_FILE" -c 'LIST services' )"
echo "$output" \
	| grep -F '"host1.example.com"' \
	| grep -F '"host2.example.com"' \
	| grep -F '"localhost"' \
	| grep -F '"some.host.name"' \
	| grep -F '"mock service"' \
	| grep -F '"other service"' \
	| grep -F '"database"' \
	| grep -F '"example service one"' \
	| grep -F '"example service two"' \
	| grep -F '"example service three"'

output="$( run_sysdb -H "$SOCKET_FILE" -c "FETCH host 'host1.example.com'" )"
echo "$output" \
	| grep -F '"host1.example.com"' \
	| grep -F '"mock service"' \
	| grep -E '"other attribute".*"special value"'
echo "$output" | grep -F 'host2.example.com' && exit 1
echo "$output" | grep -F 'localhost' && exit 1
echo "$output" | grep -F 'other.host.name' && exit 1
echo "$output" | grep -F 'some.host.name' && exit 1

output="$( run_sysdb -H "$SOCKET_FILE" \
  -c "FETCH host 'host1.example.com' FILTER last_update < 0" )" \
  && exit 1
echo "$output" | grep -F 'not found'

(echo 'LIST hosts;'; sleep 1; echo "FETCH host 'host1.example.com'") \
	| run_sysdb -H "$SOCKET_FILE"

# When requesting information for unknown hosts, expect a non-zero exit code.
output="$( run_sysdb -H "$SOCKET_FILE" -c "FETCH host 'does.not.exist'" )" \
	&& exit 1
echo "$output" | grep -F 'not found'

run_sysdb -H "$SOCKET_FILE" \
		-c "TIMESERIES 'invalid.host'.'invalid-metric'" && exit 1

# Does not work yet since there is no fetcher plugin.
run_sysdb -H "$SOCKET_FILE" \
		-c "TIMESERIES 'some.host.name'.'foo/bar/qux'" && exit 1

stop_sysdbd

