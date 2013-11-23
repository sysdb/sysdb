/*
 * SysDB - src/include/frontend/connection.h
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

#ifndef SDB_FRONTEND_CONNECTION_H
#define SDB_FRONTEND_CONNECTION_H 1

#include "utils/strbuf.h"

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* status codes returned to a client */
typedef enum {
	CONNECTION_OK = 0,
	CONNECTION_ERROR
} sdb_conn_status_t;

/* accepted commands / state of the connection */
typedef enum {
	CONNECTION_IDLE = 0,
	CONNECTION_PING,
	CONNECTION_STARTUP,
} sdb_conn_state_t;

/* a generic connection object */
typedef struct {
	/* file-descriptor of the open connection */
	int fd;

	/* read buffer */
	sdb_strbuf_t *buf;

	/* connection / protocol state information */
	uint32_t cmd;
	uint32_t cmd_len;

	/* user information */
	char *username; /* NULL if the user has not been authenticated */
} sdb_conn_t;

/*
 * sdb_connection_init:
 * Initialize an (already allocated) connection. This function allocates and
 * initialized any attributes. It's an error to call init on an already
 * initialized object.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_connection_init(sdb_conn_t *conn);

/*
 * sdb_connection_close:
 * Close a open connection and deallocate any memory used by its attributes.
 */
void
sdb_connection_close(sdb_conn_t *conn);

/*
 * sdb_connection_read:
 * Read from an open connection until reading would block.
 *
 * Returns:
 *  - the number of bytes read (0 on EOF)
 *  - a negative value on error
 */
ssize_t
sdb_connection_read(sdb_conn_t *conn);

/*
 * sdb_connection_send:
 * Send to an open connection.
 *
 * Returns:
 *  - the number of bytes written
 *  - a negative value on error
 */
ssize_t
sdb_connection_send(sdb_conn_t *conn, uint32_t code,
		uint32_t msg_len, char *msg);

/*
 * sdb_connection_ping:
 * Send back a backend status indicator to the connected client.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_connection_ping(sdb_conn_t *conn);

/*
 * session handling
 */

/*
 * sdb_session_start:
 * Start a new user session on the specified connection.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_session_start(sdb_conn_t *conn);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_FRONTEND_CONNECTION_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

