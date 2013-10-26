/*
 * SysDB - src/frontend/sock.c
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
#include "core/object.h"
#include "frontend/sock.h"

#include "utils/channel.h"
#include "utils/llist.h"

#include <assert.h>

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <pthread.h>

/* name of connection objects */
#define CONN_FD_PREFIX "conn#"
#define CONN_FD_PLACEHOLDER "XXXXXXX"

/*
 * private data types
 */

typedef struct {
	int fd;
	struct sockaddr_storage client_addr;
	socklen_t client_addr_len;
} connection_t;

typedef struct {
	sdb_object_t super;
	connection_t conn;
} connection_obj_t;
#define CONN(obj) ((connection_obj_t *)(obj))

typedef struct {
	char *address;
	int   type;

	int sock_fd;
} listener_t;

typedef struct {
	int type;
	const char *prefix;

	int (*opener)(listener_t *);
} fe_listener_impl_t;

struct sdb_fe_socket {
	listener_t *listeners;
	size_t listeners_num;

	sdb_llist_t *open_connections;

	/* channel used for communication between main
	 * and connection handler threads */
	sdb_channel_t *chan;
};

/*
 * connection management functions
 */

static int
open_unix_sock(listener_t *listener)
{
	struct sockaddr_un sa;
	int status;

	listener->sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (listener->sock_fd < 0) {
		char buf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to open UNIX socket %s: %s",
				listener->address, sdb_strerror(errno, buf, sizeof(buf)));
		return -1;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, listener->address + strlen("unix:"),
			sizeof(sa.sun_path));

	status = bind(listener->sock_fd, (struct sockaddr *)&sa, sizeof(sa));
	if (status) {
		char buf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to bind to UNIX socket %s: %s",
				listener->address, sdb_strerror(errno, buf, sizeof(buf)));
		return -1;
	}
	return 0;
} /* open_unix_sock */

/*
 * private variables
 */

/* the enum has to be sorted the same as the implementations array
 * to ensure that the type may be used as index into the array */
enum {
	LISTENER_UNIXSOCK = 0,
};
static fe_listener_impl_t listener_impls[] = {
	{ LISTENER_UNIXSOCK, "unix", open_unix_sock },
};

/*
 * private helper functions
 */

static int
get_type(const char *address)
{
	char *sep;
	size_t len;
	size_t i;

	sep = strchr(address, (int)':');
	if (! sep)
		return -1;

	assert(sep > address);
	len = (size_t)(sep - address);

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(listener_impls); ++i) {
		fe_listener_impl_t *impl = listener_impls + i;

		if (!strncmp(address, impl->prefix, len)) {
			assert(impl->type == (int)i);
			return impl->type;
		}
	}
	return -1;
} /* get_type */

static void
listener_destroy(listener_t *listener)
{
	if (! listener)
		return;

	if (listener->sock_fd >= 0)
		close(listener->sock_fd);

	if (listener->address)
		free(listener->address);
} /* listener_destroy */

static listener_t *
listener_create(sdb_fe_socket_t *sock, const char *address)
{
	listener_t *listener;
	int type;

	type = get_type(address);
	if (type < 0) {
		sdb_log(SDB_LOG_ERR, "frontend: Unsupported address type specified "
				"in listen address '%s'", address);
		return NULL;
	}

	listener = realloc(sock->listeners,
			sock->listeners_num * sizeof(*sock->listeners));
	if (! listener) {
		char buf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to allocate memory: %s",
				sdb_strerror(errno, buf, sizeof(buf)));
		return NULL;
	}

	sock->listeners = listener;
	listener = sock->listeners + sock->listeners_num;

	listener->sock_fd = -1;
	listener->address = strdup(address);
	if (! listener->address) {
		char buf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to allocate memory: %s",
				sdb_strerror(errno, buf, sizeof(buf)));
		listener_destroy(listener);
		return NULL;
	}
	listener->type = type;

	if (listener_impls[type].opener(listener)) {
		/* prints error */
		listener_destroy(listener);
		return NULL;
	}

	++sock->listeners_num;
	return listener;
} /* listener_create */

/*
 * private data types
 */

static int
connection_init(sdb_object_t *obj, va_list ap)
{
	connection_t *conn;
	int sock_fd;
	int sock_fl;

	assert(obj);
	conn = &CONN(obj)->conn;

	sock_fd = va_arg(ap, int);

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

	if (conn->client_addr.ss_family != AF_UNIX) {
		sdb_log(SDB_LOG_ERR, "frontend: Accepted connection using "
				"unexpected family type %d", conn->client_addr.ss_family);
		return -1;
	}

	sock_fl = fcntl(conn->fd, F_GETFL);
	if (fcntl(conn->fd, F_SETFL, sock_fl | O_NONBLOCK)) {
		char buf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to switch connection conn#%i "
				"to non-blocking mode: %s", conn->fd,
				sdb_strerror(errno, buf, sizeof(buf)));
		return -1;
	}

	sdb_log(SDB_LOG_DEBUG, "frontend: Accepted connection on fd=%i",
			conn->fd);

	/* update the object name */
	snprintf(obj->name + strlen(CONN_FD_PREFIX),
			strlen(CONN_FD_PLACEHOLDER), "%i", conn->fd);
	return 0;
} /* connection_init */

static void
connection_destroy(sdb_object_t *obj)
{
	connection_t *conn;

	assert(obj);
	conn = &CONN(obj)->conn;

	sdb_log(SDB_LOG_DEBUG, "frontend: Closing connection on fd=%i", conn->fd);
	close(conn->fd);
	conn->fd = -1;
} /* connection_destroy */

static sdb_type_t connection_type = {
	/* size = */ sizeof(connection_obj_t),
	/* init = */ connection_init,
	/* destroy = */ connection_destroy,
};

/*
 * connection handler functions
 */

/* returns negative value on error, 0 on EOF, number of packets else */
static int
connection_read(int fd)
{
	int n = 0;

	while (42) {
		int32_t cmd;
		ssize_t status;

		errno = 0;
		status = read(fd, &cmd, sizeof(cmd));
		if (status < 0) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
				return n + 1;
			return (int)status;
		}
		else if (! status) /* EOF */
			return 0;

		/* XXX */
		sdb_log(SDB_LOG_DEBUG, "frontend: read command %i from fd=%i",
				cmd, fd);
		++n;
	}

	return n + 1;
} /* connection_read */

static void *
connection_handler(void *data)
{
	sdb_fe_socket_t *sock = data;

	assert(sock);

	while (42) {
		struct timespec timeout = { 0, 500000000 }; /* .5 seconds */
		connection_obj_t *conn;
		int status;

		errno = 0;
		status = sdb_channel_select(sock->chan, /* read */ NULL, &conn,
				/* write */ NULL, NULL, &timeout);
		if (status) {
			char buf[1024];

			if (errno == ETIMEDOUT)
				continue;
			if (errno == EBADF) /* channel shut down */
				break;

			sdb_log(SDB_LOG_ERR, "frontend: Failed to read from channel: %s",
					sdb_strerror(errno, buf, sizeof(buf)));
			continue;
		}

		status = connection_read(conn->conn.fd);
		if (status <= 0) {
			/* error or EOF -> close connection */
			sdb_object_deref(SDB_OBJ(conn));
		}
		else {
			if (sdb_llist_append(sock->open_connections, SDB_OBJ(conn))) {
				sdb_log(SDB_LOG_ERR, "frontend: Failed to re-append "
						"connection %s to list of open connections",
						SDB_OBJ(conn)->name);
			}

			/* pass ownership back to list; or destroy in case of an error */
			sdb_object_deref(SDB_OBJ(conn));
		}
	}
	return NULL;
} /* connection_handler */

static int
connection_accept(sdb_fe_socket_t *sock, listener_t *listener)
{
	sdb_object_t *obj;

	/* the placeholder will be replaced with the accepted file
	 * descriptor when initializing the object */
	obj = sdb_object_create(CONN_FD_PREFIX CONN_FD_PLACEHOLDER,
			connection_type, listener->sock_fd);
	if (! obj)
		return -1;

	if (sdb_llist_append(sock->open_connections, obj)) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to append "
				"connection %s to list of open connections",
				obj->name);
		sdb_object_deref(obj);
		return -1;
	}

	/* hand ownership over to the list */
	sdb_object_deref(obj);
	return 0;
} /* connection_accept */

/*
 * public API
 */

sdb_fe_socket_t *
sdb_fe_sock_create(void)
{
	sdb_fe_socket_t *sock;

	sock = calloc(1, sizeof(*sock));
	if (! sock)
		return NULL;

	sock->open_connections = sdb_llist_create();
	if (! sock->open_connections) {
		sdb_fe_sock_destroy(sock);
		return NULL;
	}
	return sock;
} /* sdb_fe_sock_create */

void
sdb_fe_sock_destroy(sdb_fe_socket_t *sock)
{
	size_t i;

	if (! sock)
		return;

	for (i = 0; i < sock->listeners_num; ++i) {
		listener_destroy(sock->listeners + i);
	}
	if (sock->listeners)
		free(sock->listeners);
	sock->listeners = NULL;

	sdb_llist_destroy(sock->open_connections);
	sock->open_connections = NULL;
	free(sock);
} /* sdb_fe_sock_destroy */

int
sdb_fe_sock_add_listener(sdb_fe_socket_t *sock, const char *address)
{
	listener_t *listener;

	if ((! sock) || (! address))
		return -1;

	listener = listener_create(sock, address);
	if (! listener)
		return -1;
	return 0;
} /* sdb_fe_sock_add_listener */

int
sdb_fe_sock_listen_and_serve(sdb_fe_socket_t *sock, sdb_fe_loop_t *loop)
{
	fd_set sockets;
	int max_listen_fd = 0;
	size_t i;

	/* XXX: make the number of threads configurable */
	pthread_t handler_threads[5];

	if ((! sock) || (! sock->listeners_num) || (! loop))
		return -1;

	if (sock->chan)
		return -1;

	FD_ZERO(&sockets);

	for (i = 0; i < sock->listeners_num; ++i) {
		listener_t *listener = sock->listeners + i;

		if (listen(listener->sock_fd, /* backlog = */ 32)) {
			char buf[1024];
			sdb_log(SDB_LOG_ERR, "frontend: Failed to listen on socket %s: %s",
					listener->address, sdb_strerror(errno, buf, sizeof(buf)));
			return -1;
		}

		FD_SET(listener->sock_fd, &sockets);
		if (listener->sock_fd > max_listen_fd)
			max_listen_fd = listener->sock_fd;
	}

	sock->chan = sdb_channel_create(1024, sizeof(connection_obj_t *));
	if (! sock->chan)
		return -1;

	memset(&handler_threads, 0, sizeof(handler_threads));
	/* XXX: error handling */
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(handler_threads); ++i)
		pthread_create(&handler_threads[i], /* attr = */ NULL,
				connection_handler, /* arg = */ sock);

	while (loop->do_loop) {
		fd_set ready;
		fd_set exceptions;
		int max_fd;
		int n;

		struct timeval timeout = { 1, 0 }; /* one second */
		sdb_llist_iter_t *iter;

		FD_ZERO(&ready);
		FD_ZERO(&exceptions);

		ready = sockets;

		max_fd = max_listen_fd;

		iter = sdb_llist_get_iter(sock->open_connections);
		if (! iter) {
			sdb_log(SDB_LOG_ERR, "frontend: Failed to acquire iterator "
					"for open connections");
			break;
		}

		while (sdb_llist_iter_has_next(iter)) {
			sdb_object_t *obj = sdb_llist_iter_get_next(iter);
			FD_SET(CONN(obj)->conn.fd, &ready);
			FD_SET(CONN(obj)->conn.fd, &exceptions);

			if (CONN(obj)->conn.fd > max_fd)
				max_fd = CONN(obj)->conn.fd;
		}
		sdb_llist_iter_destroy(iter);

		errno = 0;
		n = select(max_fd + 1, &ready, NULL, &exceptions, &timeout);
		if (n < 0) {
			char buf[1024];

			if (errno == EINTR)
				continue;

			sdb_log(SDB_LOG_ERR, "frontend: Failed to monitor sockets: %s",
					sdb_strerror(errno, buf, sizeof(buf)));
			break;
		}

		if (! n)
			continue;

		for (i = 0; i < sock->listeners_num; ++i) {
			listener_t *listener = sock->listeners + i;
			if (FD_ISSET(listener->sock_fd, &ready))
				if (connection_accept(sock, listener))
					continue;
		}

		iter = sdb_llist_get_iter(sock->open_connections);
		if (! iter) {
			sdb_log(SDB_LOG_ERR, "frontend: Failed to acquire iterator "
					"for open connections");
			break;
		}

		while (sdb_llist_iter_has_next(iter)) {
			sdb_object_t *obj = sdb_llist_iter_get_next(iter);

			if (FD_ISSET(CONN(obj)->conn.fd, &exceptions))
				sdb_log(SDB_LOG_INFO, "Exception on fd %d",
						CONN(obj)->conn.fd);

			if (FD_ISSET(CONN(obj)->conn.fd, &ready)) {
				sdb_llist_iter_remove_current(iter);
				sdb_channel_write(sock->chan, &obj);
			}
		}
		sdb_llist_iter_destroy(iter);
	}

	sdb_log(SDB_LOG_INFO, "frontend: Waiting for connection handler threads "
			"to terminate");
	if (! sdb_channel_shutdown(sock->chan))
		for (i = 0; i < SDB_STATIC_ARRAY_LEN(handler_threads); ++i)
			pthread_join(handler_threads[i], NULL);
	/* else: we tried our best; let the operating system clean up */

	sdb_channel_destroy(sock->chan);
	sock->chan = NULL;
	return 0;
} /* sdb_fe_sock_listen_and_server */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

