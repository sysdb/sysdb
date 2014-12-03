/*
 * SysDB - src/include/client/sock.h
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

#ifndef SDB_CLIENT_SOCK_H
#define SDB_CLIENT_SOCK_H 1

#include "core/object.h"
#include "core/data.h"
#include "frontend/proto.h"
#include "utils/strbuf.h"

#include <sys/socket.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sdb_client;
typedef struct sdb_client sdb_client_t;

/*
 * sdb_client_create:
 * Allocates and initializes a client object to connect to the specified
 * address.
 *
 * Returns:
 *  - a new client object on success
 *  - NULL in case of an error
 */
sdb_client_t *
sdb_client_create(const char *address);

/*
 * sdb_client_destroy:
 * Destroyes the client connection and deallocates the client object.
 */
void
sdb_client_destroy(sdb_client_t *client);

/*
 * sdb_client_connect:
 * Connect to the client's address using the specified username.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_client_connect(sdb_client_t *client, const char *username);

/*
 * sdb_client_sockfd:
 * Return the client socket's file descriptor.
 */
int
sdb_client_sockfd(sdb_client_t *client);

/*
 * sdb_client_shutdown:
 * Shut down the client's send and/or receive operations.
 * See shutdown(3) for details.
 */
int
sdb_client_shutdown(sdb_client_t *client, int how);

/*
 * sdb_client_close:
 * Close the client connection.
 */
void
sdb_client_close(sdb_client_t *client);

/*
 * sdb_client_send:
 * Send the specified command and accompanying data to through the client
 * connection.
 *
 * Returns:
 *  - the number of bytes send
 *  - a negative value else.
 */
ssize_t
sdb_client_send(sdb_client_t *client,
		uint32_t cmd, uint32_t data_len, const char *data);

/*
 * sdb_client_recv:
 * Receive data from the connection. All data is written to the specified
 * buffer. If specified, the returned status code is written to the memory
 * location pointed to by 'code'. In case of an error or an incomplete
 * command, the status code is set to UINT32_MAX. The returned data does not
 * include the status code and message len as received from the remote side
 * but only the data associated with the message.
 *
 * Returns:
 *  - the number of bytes read
 *    (may be zero if the message did not include any data)
 *  - a negative value on error
 */
ssize_t
sdb_client_recv(sdb_client_t *client,
		uint32_t *code, sdb_strbuf_t *buf);

/*
 * sdb_client_eof:
 * Returns true if end of file on the client connection was reached, that is,
 * if the remote side closed the connection.
 */
bool
sdb_client_eof(sdb_client_t *client);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_CLIENT_SOCK_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

