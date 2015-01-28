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

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "sysdb.h"
#include "core/object.h"
#include "core/plugin.h"
#include "frontend/connection-private.h"
#include "utils/error.h"
#include "utils/strbuf.h"
#include "utils/proto.h"
#include "utils/os.h"

#include <assert.h>
#include <errno.h>

#include <arpa/inet.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <netdb.h>

/*
 * private variables
 */

static pthread_key_t conn_ctx_key;
static bool          conn_ctx_key_initialized = 0;

/*
 * private types
 */

/* name of connection objects */
#define CONN_FD_PREFIX "conn#"
#define CONN_FD_PLACEHOLDER "XXXXXXX"

static ssize_t
conn_read(sdb_conn_t *conn, size_t len)
{
	return sdb_strbuf_read(conn->buf, conn->fd, len);
} /* conn_read */

static ssize_t
conn_write(sdb_conn_t *conn, const void *buf, size_t len)
{
	return sdb_write(conn->fd, len, buf);
} /* conn_write */

static int
connection_init(sdb_object_t *obj, va_list ap)
{
	sdb_conn_t *conn;
	int sock_fd;
	int sock_fl;

	assert(obj);
	conn = CONN(obj);

	sock_fd = va_arg(ap, int);

	conn->buf = sdb_strbuf_create(/* size = */ 128);
	if (! conn->buf) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to allocate a read buffer "
				"for a new connection");
		return -1;
	}
	conn->errbuf = sdb_strbuf_create(0);
	if (! conn->errbuf) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to allocate an error buffer "
				"for a new connection");
		return -1;
	}

	conn->client_addr_len = sizeof(conn->client_addr);
	conn->fd = accept(sock_fd, (struct sockaddr *)&conn->client_addr,
			&conn->client_addr_len);

	if (conn->fd < 0) {
		char buf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to accept remote "
				"connection: %s", sdb_strerror(errno,
					buf, sizeof(buf)));
		return -1;
	}

	/* update the object name */
	snprintf(obj->name + strlen(CONN_FD_PREFIX),
			strlen(CONN_FD_PLACEHOLDER), "%i", conn->fd);

	/* defaults */
	conn->read = conn_read;
	conn->write = conn_write;
	conn->finish = NULL;
	conn->ssl_session = NULL;

	sock_fl = fcntl(conn->fd, F_GETFL);
	if (fcntl(conn->fd, F_SETFL, sock_fl | O_NONBLOCK)) {
		char buf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to switch connection conn#%i "
				"to non-blocking mode: %s", conn->fd,
				sdb_strerror(errno, buf, sizeof(buf)));
		return -1;
	}

	conn->username = NULL;
	conn->ready = 0;

	sdb_log(SDB_LOG_DEBUG, "frontend: Accepted connection on fd=%i",
			conn->fd);

	conn->cmd = SDB_CONNECTION_IDLE;
	conn->cmd_len = 0;
	conn->skip_len = 0;
	return 0;
} /* connection_init */

static void
connection_destroy(sdb_object_t *obj)
{
	sdb_conn_t *conn;
	size_t len;

	assert(obj);
	conn = CONN(obj);

	conn->ready = 0;

	if (conn->finish)
		conn->finish(conn);
	conn->finish = NULL;

	if (conn->buf) {
		len = sdb_strbuf_len(conn->buf);
		if (len)
			sdb_log(SDB_LOG_INFO, "frontend: Discarding incomplete command "
					"(%zu byte%s left in buffer)", len, len == 1 ? "" : "s");
	}

	sdb_log(SDB_LOG_DEBUG, "frontend: Closing connection %s", obj->name);
	sdb_connection_close(conn);

	if (conn->username)
		free(conn->username);
	conn->username = NULL;

	sdb_strbuf_destroy(conn->buf);
	conn->buf = NULL;
	sdb_strbuf_destroy(conn->errbuf);
	conn->errbuf = NULL;
} /* connection_destroy */

static sdb_type_t connection_type = {
	/* size = */ sizeof(sdb_conn_t),
	/* init = */ connection_init,
	/* destroy = */ connection_destroy,
};

/*
 * private helper functions
 */

static void
sdb_conn_ctx_destructor(void *c)
{
	sdb_object_t *conn = c;

	if (! conn)
		return;
	sdb_object_deref(conn);
} /* sdb_conn_ctx_destructor */

static void
sdb_conn_ctx_init(void)
{
	if (conn_ctx_key_initialized)
		return;

	pthread_key_create(&conn_ctx_key, sdb_conn_ctx_destructor);
	conn_ctx_key_initialized = 1;
} /* sdb_conn_ctx_init */

static void
sdb_conn_set_ctx(sdb_conn_t *conn)
{
	sdb_conn_t *old;

	sdb_conn_ctx_init();

	old = pthread_getspecific(conn_ctx_key);
	if (old)
		sdb_object_deref(SDB_OBJ(old));
	if (conn)
		sdb_object_ref(SDB_OBJ(conn));
	pthread_setspecific(conn_ctx_key, conn);
} /* sdb_conn_set_ctx */

static sdb_conn_t *
sdb_conn_get_ctx(void)
{
	if (! conn_ctx_key_initialized)
		return NULL;
	return pthread_getspecific(conn_ctx_key);
} /* sdb_conn_get_ctx */

/*
 * connection handler functions
 */

/*
 * connection_log:
 * Send a log message originating from the current thread to the client.
 */
static int
connection_log(int prio, const char *msg,
		sdb_object_t __attribute__((unused)) *user_data)
{
	uint32_t len = (uint32_t)sizeof(uint32_t) + (uint32_t)strlen(msg);
	uint32_t p = htonl((uint32_t)prio);
	char tmp[len + 1];

	sdb_conn_t *conn;

	conn = sdb_conn_get_ctx();
	/* no connection associated to this thread
	 * or startup not done yet => don't leak any information */
	if ((! conn) || (! conn->ready))
		return 0;

	/* XXX: make the log-level configurable by the client at runtime */
	if (prio >= SDB_LOG_DEBUG)
		return 0;

	memcpy(tmp, &p, sizeof(p));
	strcpy(tmp + sizeof(p), msg);

	if (sdb_connection_send(conn, SDB_CONNECTION_LOG, len, tmp) < 0)
		return -1;
	return 0;
} /* connection_log */

static int
command_handle(sdb_conn_t *conn)
{
	int status = -1;

	assert(conn && (conn->cmd != SDB_CONNECTION_IDLE));
	assert(! conn->skip_len);

	if (conn->cmd == SDB_CONNECTION_PING)
		status = sdb_connection_ping(conn);
	else if (conn->cmd == SDB_CONNECTION_STARTUP)
		status = sdb_fe_session_start(conn);
	else if (conn->cmd == SDB_CONNECTION_QUERY)
		status = sdb_fe_query(conn);
	else if (conn->cmd == SDB_CONNECTION_FETCH)
		status = sdb_fe_fetch(conn);
	else if (conn->cmd == SDB_CONNECTION_LIST)
		status = sdb_fe_list(conn);
	else if (conn->cmd == SDB_CONNECTION_LOOKUP)
		status = sdb_fe_lookup(conn);
	else if (conn->cmd == SDB_CONNECTION_STORE)
		status = sdb_fe_store(conn);
	else {
		sdb_log(SDB_LOG_WARNING, "frontend: Ignoring invalid command %#x",
				conn->cmd);
		sdb_strbuf_sprintf(conn->errbuf, "Invalid command %#x", conn->cmd);
		status = -1;
	}

	if (status) {
		if (! sdb_strbuf_len(conn->errbuf))
			sdb_strbuf_sprintf(conn->errbuf, "Failed to execute command");
		sdb_connection_send(conn, SDB_CONNECTION_ERROR,
				(uint32_t)sdb_strbuf_len(conn->errbuf),
				sdb_strbuf_string(conn->errbuf));
	}
	return status;
} /* command_handle */

/* initialize the connection state information */
static int
command_init(sdb_conn_t *conn)
{
	const char *errmsg = NULL;

	assert(conn && (conn->cmd == SDB_CONNECTION_IDLE) && (! conn->cmd_len));

	if (conn->skip_len)
		return -1;

	/* reset */
	sdb_strbuf_clear(conn->errbuf);

	if (sdb_proto_unmarshal_header(SDB_STRBUF_STR(conn->buf),
				&conn->cmd, &conn->cmd_len) < 0)
		return -1;
	sdb_strbuf_skip(conn->buf, 0, 2 * sizeof(uint32_t));

	if ((! conn->ready) && (conn->cmd != SDB_CONNECTION_STARTUP))
		errmsg = "Authentication required";
	else if (conn->cmd == SDB_CONNECTION_IDLE)
		errmsg = "Invalid command 0";

	if (errmsg) {
		size_t len = sdb_strbuf_len(conn->buf);

		sdb_strbuf_sprintf(conn->errbuf, "%s", errmsg);
		sdb_connection_send(conn, SDB_CONNECTION_ERROR,
				(uint32_t)strlen(errmsg), errmsg);
		conn->skip_len += conn->cmd_len;
		conn->cmd = SDB_CONNECTION_IDLE;
		conn->cmd_len = 0;

		if (len > conn->skip_len)
			len = conn->skip_len;
		sdb_strbuf_skip(conn->buf, 0, len);
		conn->skip_len -= len;
		/* connection_read will handle anything else */
	}
	return 0;
} /* command_init */

/* returns negative value on error, 0 on EOF, number of octets else */
static ssize_t
connection_read(sdb_conn_t *conn)
{
	ssize_t n = 0;

	if ((! conn) || (conn->fd < 0))
		return -1;

	while (42) {
		ssize_t status;

		errno = 0;
		status = conn->read(conn, 1024);
		if (status < 0) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
				break;

			sdb_connection_close(conn);
			return (int)status;
		}
		else if (! status) /* EOF */
			break;

		if (conn->skip_len) {
			size_t len = (size_t)status < conn->skip_len
				? (size_t)status : conn->skip_len;
			sdb_strbuf_skip(conn->buf, 0, len);
			conn->skip_len -= len;
		}

		n += status;

		/* give the main loop a chance to execute commands (and free up buffer
		 * space) on large amounts of incoming traffic */
		if (n > 1024 * 1024)
			break;
	}

	return n;
} /* connection_read */

/*
 * public API
 */

int
sdb_connection_enable_logging(void)
{
	return sdb_plugin_register_log("connection-logger", connection_log,
			/* user_data = */ NULL);
} /* sdb_connection_enable_logging */

sdb_conn_t *
sdb_connection_accept(int fd, sdb_conn_setup_cb setup, void *user_data)
{
	sdb_conn_t *conn;
	const char *peer = "unknown";

	if (fd < 0)
		return NULL;

	/* the placeholder will be replaced with the accepted file
	 * descriptor when initializing the object */
	conn = CONN(sdb_object_create(CONN_FD_PREFIX CONN_FD_PLACEHOLDER,
				connection_type, fd));
	if (setup && (setup(conn, user_data) < 0)) {
		sdb_object_deref(SDB_OBJ(conn));
		return NULL;
	}

	if (conn->username)
		peer = conn->username;

	if (conn->client_addr.ss_family == AF_UNIX) {
		sdb_log(SDB_LOG_INFO,
				"frontend: Accepted connection from peer %s", peer);
	}
	else {
		char host[1024] = "<unknown>", port[32] = "";
		getnameinfo((struct sockaddr *)&conn->client_addr,
				conn->client_addr_len, host, sizeof(host), port, sizeof(port),
				NI_NUMERICHOST | NI_NUMERICSERV);
		sdb_log(SDB_LOG_INFO, "frontend: Accepted connection from "
				"peer %s at %s:%s", peer, host, port);
	}
	return conn;
} /* sdb_connection_create */

void
sdb_connection_close(sdb_conn_t *conn)
{
	if (! conn)
		return;

	if (conn->finish)
		conn->finish(conn);
	conn->finish = NULL;

	/* close the connection even if someone else still references it */
	if (conn->fd >= 0)
		close(conn->fd);
	conn->fd = -1;
} /* sdb_connection_close */

ssize_t
sdb_connection_handle(sdb_conn_t *conn)
{
	ssize_t n = 0;

	sdb_conn_set_ctx(conn);

	while (42) {
		ssize_t status = connection_read(conn);

		if ((conn->cmd == SDB_CONNECTION_IDLE) && (! conn->cmd_len)
				&& (sdb_strbuf_len(conn->buf) >= 2 * sizeof(int32_t)))
			command_init(conn);
		if ((conn->cmd != SDB_CONNECTION_IDLE)
				&& (sdb_strbuf_len(conn->buf) >= conn->cmd_len)) {
			command_handle(conn);

			/* remove the command from the buffer */
			if (conn->cmd_len)
				sdb_strbuf_skip(conn->buf, 0, conn->cmd_len);
			conn->cmd = SDB_CONNECTION_IDLE;
			conn->cmd_len = 0;
		}

		if (status <= 0)
			break;

		n += status;
	}

	sdb_conn_set_ctx(NULL);
	return n;
} /* sdb_connection_handle */

ssize_t
sdb_connection_send(sdb_conn_t *conn, uint32_t code,
		uint32_t msg_len, const char *msg)
{
	char buf[2 * sizeof(uint32_t) + msg_len];
	ssize_t status;

	if ((! conn) || (conn->fd < 0))
		return -1;
	if (sdb_proto_marshal(buf, sizeof(buf), code, msg_len, msg) < 0)
		return -1;

	status = conn->write(conn, buf, sizeof(buf));
	if (status < 0) {
		char errbuf[1024];

		/* tell other code that there was a problem and, more importantly,
		 * make sure we don't try to send further logs to the connection */
		sdb_connection_close(conn);
		conn->ready = 0;

		sdb_log(SDB_LOG_ERR, "frontend: Failed to send msg "
				"(code: %u, len: %u) to client: %s", code, msg_len,
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
	}
	return status;
} /* sdb_connection_send */

int
sdb_connection_ping(sdb_conn_t *conn)
{
	if ((! conn) || (conn->cmd != SDB_CONNECTION_PING))
		return -1;

	/* we're alive */
	sdb_connection_send(conn, SDB_CONNECTION_OK, 0, NULL);
	return 0;
} /* sdb_connection_ping */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

