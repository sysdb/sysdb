/*
 * SysDB - src/include/frontend/sock.h
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

#include <unistd.h>

#ifndef SDB_FRONTEND_SOCK_H
#define SDB_FRONTEND_SOCK_H 1

#ifdef __cplusplus
extern "C" {
#endif

/* manage a front-end listener loop */
typedef struct {
	/* number of handler threads to create */
	size_t num_threads;

	/* front-end listener shuts down when this is set to false */
	_Bool do_loop;
} sdb_fe_loop_t;
#define SDB_FE_LOOP_INIT { 5, 1 }

/*
 * sdb_fe_socket_t:
 * A front-end socket accepting connections from clients.
 */
typedef struct sdb_fe_socket sdb_fe_socket_t;

/*
 * sdb_fe_sock_create:
 * Create a socket object.
 *
 * Returns:
 *  - a socket object on success
 *  - NULL else
 */
sdb_fe_socket_t *
sdb_fe_sock_create(void);

/*
 * sdb_fe_sock_destroy:
 * Shut down all listeners and destroy a socket object.
 */
void
sdb_fe_sock_destroy(sdb_fe_socket_t *sock);

/*
 * sdb_fe_sock_add_listener:
 * Tell the specified socket to listen on the specified 'address'.
 * The address has to be specified as <type>:<address> where the following
 * types are currently supported:
 *
 *  - unix: listen on a UNIX socket
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_fe_sock_add_listener(sdb_fe_socket_t *sock, const char *address);

/*
 * sdb_fe_sock_clear_listeners:
 * Shut down all listeners from the socket object and clear the list of
 * listeners. All open connections will not be affected by this.
 */
void
sdb_fe_sock_clear_listeners(sdb_fe_socket_t *sock);

/*
 * sdb_fe_sock_listen_and_serve:
 * Listen on the specified socket and serve client requests. The loop
 * terminates on error or when the loop condition turns to false. All
 * listening sockets will be closed at that time.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_fe_sock_listen_and_serve(sdb_fe_socket_t *sock, sdb_fe_loop_t *loop);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_FRONTEND_SOCK_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

