/*
 * SysDB - src/frontend/session.c
 * Copyright (C) 2013 Sebastian 'tokkee' Harl <sh@tokkee.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "sysdb.h"

#include "frontend/connection-private.h"

#include <string.h>

/*
 * public API
 */

int
sdb_fe_session_start(sdb_conn_t *conn)
{
	const char *username;

	if ((! conn) || (conn->username))
		return -1;

	if (conn->cmd != CONNECTION_STARTUP)
		return -1;

	username = sdb_strbuf_string(conn->buf);
	if ((! username) || (! conn->cmd_len) || (! *username)) {
		sdb_strbuf_sprintf(conn->errbuf, "Invalid empty username");
		return -1;
	}

	/* XXX: for now, simply accept all connections */
	conn->username = strndup(username, conn->cmd_len);
	if (! conn->username) {
		sdb_strbuf_sprintf(conn->errbuf, "Authentication failed");
		return -1;
	}
	sdb_connection_send(conn, CONNECTION_OK, 0, NULL);
	return 0;
} /* sdb_fe_session_start */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

