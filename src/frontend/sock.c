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

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "sysdb.h"
#include "core/object.h"
#include "frontend/connection-private.h"
#include "frontend/sock.h"

#include "utils/channel.h"
#include "utils/error.h"
#include "utils/llist.h"
#include "utils/strbuf.h"

#include <assert.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <pthread.h>

/*
 * private data types
 */

typedef struct {
	char *address;
	int   type;

	int sock_fd;
} listener_t;

typedef struct {
	int type;
	const char *prefix;

	int (*opener)(listener_t *);
	void (*closer)(listener_t *);
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

	if (unlink(listener->address + strlen("unix:")) && (errno != ENOENT)) {
		char errbuf[1024];
		sdb_log(SDB_LOG_WARNING, "frontend: Failed to remove stale UNIX "
				"socket %s: %s", listener->address + strlen("unix:"),
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
	}

	status = bind(listener->sock_fd, (struct sockaddr *)&sa, sizeof(sa));
	if (status) {
		char buf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to bind to UNIX socket %s: %s",
				listener->address, sdb_strerror(errno, buf, sizeof(buf)));
		return -1;
	}
	return 0;
} /* open_unix_sock */

static void
close_unix_sock(listener_t *listener)
{
	assert(listener);
	if (! listener->address)
		return;

	if (listener->sock_fd >= 0)
		close(listener->sock_fd);
	listener->sock_fd = -1;

	unlink(listener->address + strlen("unix:"));
} /* close_unix_sock */

/*
 * private variables
 */

/* the enum has to be sorted the same as the implementations array
 * to ensure that the type may be used as index into the array */
enum {
	LISTENER_UNIXSOCK = 0,
};
static fe_listener_impl_t listener_impls[] = {
	{ LISTENER_UNIXSOCK, "unix", open_unix_sock, close_unix_sock },
};

/*
 * private helper functions
 */

static int
listener_listen(listener_t *listener)
{
	assert(listener);

	/* try to reopen */
	if (listener->sock_fd < 0)
		if (listener_impls[listener->type].opener(listener))
			return -1;
	assert(listener->sock_fd >= 0);

	if (listen(listener->sock_fd, /* backlog = */ 32)) {
		char buf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to listen on socket %s: %s",
				listener->address, sdb_strerror(errno, buf, sizeof(buf)));
		return -1;
	}
	return 0;
} /* listener_listen */

static void
listener_close(listener_t *listener)
{
	assert(listener);

	if (listener_impls[listener->type].closer)
		listener_impls[listener->type].closer(listener);

	if (listener->sock_fd >= 0)
		close(listener->sock_fd);
	listener->sock_fd = -1;
} /* listener_close */

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

	listener_close(listener);

	if (listener->address)
		free(listener->address);
	listener->address = NULL;
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
			(sock->listeners_num + 1) * sizeof(*sock->listeners));
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

static void
socket_close(sdb_fe_socket_t *sock)
{
	size_t i;

	assert(sock);
	for (i = 0; i < sock->listeners_num; ++i)
		listener_close(sock->listeners + i);
} /* socket_close */

/*
 * connection handler functions
 */

static void *
connection_handler(void *data)
{
	sdb_fe_socket_t *sock = data;

	assert(sock);

	while (42) {
		struct timespec timeout = { 0, 500000000 }; /* .5 seconds */
		sdb_conn_t *conn;
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

		status = (int)sdb_connection_read(conn);
		if (status <= 0) {
			/* error or EOF -> close connection */
			sdb_object_deref(SDB_OBJ(conn));
			continue;
		}

		/* return the connection to the main loop */
		if (sdb_llist_append(sock->open_connections, SDB_OBJ(conn))) {
			sdb_log(SDB_LOG_ERR, "frontend: Failed to re-append "
					"connection %s to list of open connections",
					SDB_OBJ(conn)->name);
		}

		/* pass ownership back to list; or destroy in case of an error */
		sdb_object_deref(SDB_OBJ(conn));
	}
	return NULL;
} /* connection_handler */

static int
connection_accept(sdb_fe_socket_t *sock, listener_t *listener)
{
	sdb_object_t *obj;
	int status;

	obj = SDB_OBJ(sdb_connection_accept(listener->sock_fd));
	if (! obj)
		return -1;

	status = sdb_llist_append(sock->open_connections, obj);
	if (status)
		sdb_log(SDB_LOG_ERR, "frontend: Failed to append "
				"connection %s to list of open connections",
				obj->name);

	/* hand ownership over to the list; or destroy in case of an error */
	sdb_object_deref(obj);
	return status;
} /* connection_accept */

static int
socket_handle_incoming(sdb_fe_socket_t *sock,
		fd_set *ready, fd_set *exceptions)
{
	sdb_llist_iter_t *iter;
	size_t i;

	for (i = 0; i < sock->listeners_num; ++i) {
		listener_t *listener = sock->listeners + i;
		if (FD_ISSET(listener->sock_fd, ready))
			if (connection_accept(sock, listener))
				continue;
	}

	iter = sdb_llist_get_iter(sock->open_connections);
	if (! iter) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to acquire iterator "
				"for open connections");
		return -1;
	}

	while (sdb_llist_iter_has_next(iter)) {
		sdb_object_t *obj = sdb_llist_iter_get_next(iter);

		if (FD_ISSET(CONN(obj)->fd, exceptions))
			sdb_log(SDB_LOG_INFO, "Exception on fd %d",
					CONN(obj)->fd);

		if (FD_ISSET(CONN(obj)->fd, ready)) {
			sdb_llist_iter_remove_current(iter);
			sdb_channel_write(sock->chan, &obj);
		}
	}
	sdb_llist_iter_destroy(iter);
	return 0;
} /* socket_handle_incoming */

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

	pthread_t handler_threads[loop->num_threads];
	size_t num_threads;

	if ((! sock) || (! sock->listeners_num) || sock->chan
			|| (! loop) || (loop->num_threads <= 0))
		return -1;

	if (! loop->do_loop)
		return 0;

	FD_ZERO(&sockets);
	for (i = 0; i < sock->listeners_num; ++i) {
		listener_t *listener = sock->listeners + i;

		if (listener_listen(listener)) {
			socket_close(sock);
			return -1;
		}

		FD_SET(listener->sock_fd, &sockets);
		if (listener->sock_fd > max_listen_fd)
			max_listen_fd = listener->sock_fd;
	}

	sock->chan = sdb_channel_create(1024, sizeof(sdb_conn_t *));
	if (! sock->chan) {
		socket_close(sock);
		return -1;
	}

	sdb_log(SDB_LOG_INFO, "frontend: Starting %d connection "
			"handler thread%s managing %d listener%s",
			loop->num_threads, loop->num_threads == 1 ? "" : "s",
			sock->listeners_num, sock->listeners_num == 1 ? "" : "s");

	num_threads = loop->num_threads;
	memset(&handler_threads, 0, sizeof(handler_threads));
	for (i = 0; i < num_threads; ++i) {
		errno = 0;
		if (pthread_create(&handler_threads[i], /* attr = */ NULL,
					connection_handler, /* arg = */ sock)) {
			char errbuf[1024];
			sdb_log(SDB_LOG_ERR, "frontend: Failed to create "
					"connection handler thread: %s",
					sdb_strerror(errno, errbuf, sizeof(errbuf)));
			num_threads = i;
			break;
		}
	}

	while (loop->do_loop && num_threads) {
		struct timeval timeout = { 1, 0 }; /* one second */
		sdb_llist_iter_t *iter;

		int max_fd = max_listen_fd;
		fd_set ready;
		fd_set exceptions;
		int n;

		FD_ZERO(&ready);
		FD_ZERO(&exceptions);

		ready = sockets;

		iter = sdb_llist_get_iter(sock->open_connections);
		if (! iter) {
			sdb_log(SDB_LOG_ERR, "frontend: Failed to acquire iterator "
					"for open connections");
			break;
		}

		while (sdb_llist_iter_has_next(iter)) {
			sdb_object_t *obj = sdb_llist_iter_get_next(iter);
			FD_SET(CONN(obj)->fd, &ready);
			FD_SET(CONN(obj)->fd, &exceptions);

			if (CONN(obj)->fd > max_fd)
				max_fd = CONN(obj)->fd;
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
		else if (! n)
			continue;

		/* handle new and open connections */
		if (socket_handle_incoming(sock, &ready, &exceptions))
			break;
	}

	socket_close(sock);

	sdb_log(SDB_LOG_INFO, "frontend: Waiting for connection handler threads "
			"to terminate");
	if (! sdb_channel_shutdown(sock->chan))
		for (i = 0; i < num_threads; ++i)
			pthread_join(handler_threads[i], NULL);
	/* else: we tried our best; let the operating system clean up */

	sdb_channel_destroy(sock->chan);
	sock->chan = NULL;

	if (! num_threads)
		return -1;
	return 0;
} /* sdb_fe_sock_listen_and_server */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

