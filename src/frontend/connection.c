/*
 * SysDB - src/frontend/connection.c
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

#include "sysdb.h"
#include "frontend/connection.h"
#include "utils/strbuf.h"

#include <assert.h>
#include <errno.h>

#include <arpa/inet.h>

#include <string.h>

/*
 * connection handler functions
 */

static uint32_t
connection_get_int32(sdb_conn_t *conn, size_t offset)
{
	const char *data;
	uint32_t n;

	assert(conn && (sdb_strbuf_len(conn->buf) >= offset + sizeof(uint32_t)));

	data = sdb_strbuf_string(conn->buf);
	memcpy(&n, data + offset, sizeof(n));
	n = ntohl(n);
	return n;
} /* connection_get_int32 */

static int
command_handle(sdb_conn_t *conn)
{
	assert(conn && conn->cmd && conn->cmd_len);
	/* XXX */
	sdb_strbuf_skip(conn->buf, conn->cmd_len);
	return 0;
} /* command_handle */

/* initialize the connection state information */
static int
command_init(sdb_conn_t *conn)
{
	assert(conn && (! conn->cmd) && (! conn->cmd_len));

	conn->cmd = connection_get_int32(conn, 0);
	conn->cmd_len = connection_get_int32(conn, sizeof(uint32_t));
	sdb_strbuf_skip(conn->buf, 2 * sizeof(uint32_t));
	return 0;
} /* command_init */

/* returns negative value on error, 0 on EOF, number of octets else */
static ssize_t
connection_read(sdb_conn_t *conn)
{
	ssize_t n = 0;

	while (42) {
		ssize_t status;

		errno = 0;
		status = sdb_strbuf_read(conn->buf, conn->fd, 1024);
		if (status < 0) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
				break;
			return (int)status;
		}
		else if (! status) /* EOF */
			break;

		n += status;
	}

	return n;
} /* connection_read */

/*
 * public API
 */

ssize_t
sdb_connection_read(sdb_conn_t *conn)
{
	ssize_t n = 0;

	while (42) {
		ssize_t status = connection_read(conn);

		if ((! conn->cmd) && (! conn->cmd_len)
				&& (sdb_strbuf_len(conn->buf) >= 2 * sizeof(int32_t)))
			command_init(conn);
		if (conn->cmd_len && (sdb_strbuf_len(conn->buf) >= conn->cmd_len))
			command_handle(conn);

		if (status <= 0)
			break;

		n += status;
	}
	return n;
} /* sdb_connection_read */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

