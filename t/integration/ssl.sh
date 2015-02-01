#! /bin/bash
#
# SysDB -- t/integration/ssl.sh
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
# Integration tests using SSL connections.
#

set -ex

source "$( dirname "$0" )/test_lib.sh"

setup_ssl

cat <<EOF > "$SYSDBD_CONF"
<Listen "tcp:localhost:12345">
	SSLCertificate "$SERVER_CERT"
	SSLCertificateKey "$SERVER_KEY"
	SSLCACertificates "$CA_CERT"
</Listen>
EOF
run_sysdbd -D -C "$SYSDBD_CONF"
wait_for_sysdbd_tcp localhost 12345

run_sysdb -H "localhost:12345" -c 'LIST hosts' -U "$SYSDB_USER-invalid" \
	-A "$CA_CERT" -C "$CLIENT_CERT" -K "$CLIENT_KEY" && exit 1

run_sysdb -H "localhost:12345" -c 'LIST hosts' -U "$SYSDB_USER" \
	-A "$CA_CERT" -C "$CLIENT_CERT" -K "$CLIENT_KEY"

# vim: set tw=78 sw=4 ts=4 noexpandtab :
