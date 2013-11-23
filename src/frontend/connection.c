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
#include "core/error.h"
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
	int status = -1;

	assert(conn && (conn->cmd != CONNECTION_IDLE));

	sdb_log(SDB_LOG_DEBUG, "frontend: Handling command %u (len: %u)",
			conn->cmd, conn->cmd_len);

	switch (conn->cmd) {
		case CONNECTION_PING:
			status = sdb_connection_ping(conn);
			break;
		case CONNECTION_STARTUP:
			status = sdb_session_start(conn);
			break;
		default:
			sdb_log(SDB_LOG_WARNING, "frontend: Ignoring invalid command");
			status = -1;
			break;
	}

	/* remove the command from the buffer */
	if (conn->cmd_len)
		sdb_strbuf_skip(conn->buf, conn->cmd_len);
	conn->cmd = CONNECTION_IDLE;
	conn->cmd_len = 0;
	return status;
} /* command_handle */

/* initialize the connection state information */
static int
command_init(sdb_conn_t *conn)
{
	size_t len;

	assert(conn && (conn->cmd == CONNECTION_IDLE) && (! conn->cmd_len));

	conn->cmd = connection_get_int32(conn, 0);
	conn->cmd_len = connection_get_int32(conn, sizeof(uint32_t));

	len = 2 * sizeof(uint32_t);
	if (conn->cmd == CONNECTION_IDLE)
		len += conn->cmd_len;
	sdb_strbuf_skip(conn->buf, len);
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

int
sdb_connection_init(sdb_conn_t *conn)
{
	if (conn->buf) {
		sdb_log(SDB_LOG_WARNING, "frontend: Attempted to re-initialize "
				"a frontend connection");
		return -1;
	}

	conn->buf = sdb_strbuf_create(/* size = */ 128);
	if (! conn->buf) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to allocate a read buffer "
				"for a new connection");
		sdb_connection_close(conn);
		return -1;
	}

	conn->cmd = CONNECTION_IDLE;
	conn->cmd_len = 0;
	conn->fd = -1;
	return 0;
} /* sdb_connection_init */

void
sdb_connection_close(sdb_conn_t *conn)
{
	size_t len;

	if (conn->buf) {
		len = sdb_strbuf_len(conn->buf);
		if (len)
			sdb_log(SDB_LOG_INFO, "frontend: Discarding incomplete command "
					"(%zu byte%s left in buffer)", len, len == 1 ? "" : "s");
	}

	sdb_log(SDB_LOG_DEBUG, "frontend: Closing connection on fd=%i",
			conn->fd);
	close(conn->fd);
	conn->fd = -1;

	sdb_strbuf_destroy(conn->buf);
	conn->buf = NULL;
} /* sdb_connection_fini */

ssize_t
sdb_connection_read(sdb_conn_t *conn)
{
	ssize_t n = 0;

	while (42) {
		ssize_t status = connection_read(conn);

		if ((conn->cmd == CONNECTION_IDLE) && (! conn->cmd_len)
				&& (sdb_strbuf_len(conn->buf) >= 2 * sizeof(int32_t)))
			command_init(conn);
		if ((conn->cmd != CONNECTION_IDLE)
				&& (sdb_strbuf_len(conn->buf) >= conn->cmd_len))
			command_handle(conn);

		if (status <= 0)
			break;

		n += status;
	}
	return n;
} /* sdb_connection_read */

ssize_t
sdb_connection_send(sdb_conn_t *conn, uint32_t code,
		uint32_t msg_len, char *msg)
{
	size_t len = 2 * sizeof(uint32_t) + msg_len;
	char buffer[len];
	char *buf;

	uint32_t tmp;

	if ((! conn) || (conn->fd < 0))
		return -1;

	tmp = htonl(code);
	memcpy(buffer, &tmp, sizeof(uint32_t));

	tmp = htonl(msg_len);
	memcpy(buffer + sizeof(uint32_t), &tmp, sizeof(uint32_t));

	if (msg_len)
		memcpy(buffer + 2 * sizeof(uint32_t), msg, msg_len);

	buf = buffer;
	while (len > 0) {
		ssize_t status;

		/* XXX: use select() */

		errno = 0;
		status = write(conn->fd, buf, len);
		if (status < 0) {
			char errbuf[1024];

			if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
				continue;
			if (errno == EINTR)
				continue;

			sdb_log(SDB_LOG_ERR, "frontend: Failed to send msg "
					"(code: %u, len: %u) to client: %s", code, msg_len,
					sdb_strerror(errno, errbuf, sizeof(errbuf)));
			return status;
		}

		len -= (size_t)status;
		buf += status;
	}
	return (ssize_t)len;
} /* sdb_connection_send */

int
sdb_connection_ping(sdb_conn_t *conn)
{
	if ((! conn) || (conn->cmd != CONNECTION_PING))
		return -1;

	/* we're alive */
	sdb_connection_send(conn, CONNECTION_OK, 0, NULL);
	return 0;
} /* sdb_connection_ping */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

