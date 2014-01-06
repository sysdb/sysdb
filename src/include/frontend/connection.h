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

#include "frontend/proto.h"
#include "utils/llist.h"
#include "utils/strbuf.h"

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sdb_conn sdb_conn_t;

/*
 * sdb_conn_node_t represents an interface for the result of parsing a command
 * string. The first field of an actual implementation of the interface is a
 * sdb_conn_state_t in order to fascilitate casting to and from the interface
 * type to the implementation type.
 */
typedef struct {
	sdb_object_t super;
	sdb_conn_state_t cmd;
} sdb_conn_node_t;
#define SDB_CONN_NODE(obj) ((sdb_conn_node_t *)(obj))

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
 * sdb_fe_parse:
 * Parse the query text specified in 'query' of length 'len' and return a list
 * of parse trees (for each command) to be executed by sdb_fe_exec. The list
 * has to be freed by the caller. If 'len' is less than zero, parse the whole
 * (nul-terminated) string.
 *
 * Returns:
 *  - an sdb_llist_t object of sdb_conn_node_t on success
 *  - NULL in case of an error
 */
sdb_llist_t *
sdb_fe_parse(const char *query, int len);

/*
 * sdb_fe_exec:
 * Execute the command identified by 'node' on the specified connection.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_fe_exec(sdb_conn_t *conn, sdb_conn_node_t *node);

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

