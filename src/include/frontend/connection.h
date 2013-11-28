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
	/* connection handling */
	CONNECTION_IDLE = 0,
	CONNECTION_PING,
	CONNECTION_STARTUP,

	/* querying */
	CONNECTION_LIST,
} sdb_conn_state_t;

typedef struct sdb_conn sdb_conn_t;

/*
 * sdb_connection_accpet:
 * Accept a new connection on the specified file-descriptor 'fd' and return a
 * newly allocated connection object.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
sdb_conn_t *
sdb_connection_accept(int fd);

/*
 * sdb_connection_close:
 * Close a open connection and deallocate any memory. The connection object is
 * no longer valid after calling this function.
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
		uint32_t msg_len, const char *msg);

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
 * sdb_fe_session_start:
 * Start a new user session on the specified connection.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_fe_session_start(sdb_conn_t *conn);

/*
 * store access
 */

/*
 * sdb_fe_list:
 * Send a complete listing of the store, serialized as JSON, to the client.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_fe_list(sdb_conn_t *conn);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_FRONTEND_CONNECTION_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

