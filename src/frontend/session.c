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
	char username[sdb_strbuf_len(conn->buf) + 1];
	const char *tmp;

	if ((! conn) || (conn->cmd != SDB_CONNECTION_STARTUP))
		return -1;

	tmp = sdb_strbuf_string(conn->buf);
	if ((! tmp) || (! conn->cmd_len) || (! *tmp)) {
		sdb_strbuf_sprintf(conn->errbuf, "Invalid empty username");
		return -1;
	}
	strncpy(username, tmp, conn->cmd_len);
	username[conn->cmd_len] = '\0';

	if (! conn->username) {
		/* We trust the remote peer.
		 * TODO: make the auth mechanism configurable */
		conn->username = strdup(username);
	}
	else if (strcmp(conn->username, username)) {
		sdb_strbuf_sprintf(conn->errbuf, "%s cannot act on behalf of %s",
				conn->username, username);
		return -1;
	}

	sdb_connection_send(conn, SDB_CONNECTION_OK, 0, NULL);
	conn->ready = 1;
	return 0;
} /* sdb_fe_session_start */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

