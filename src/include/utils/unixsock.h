/*
 * SysDB - src/include/utils/unixsock.h
 * Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#ifndef SDB_UTILS_UNIXSOCK_H
#define SDB_UTILS_UNIXSOCK_H 1

#include "core/object.h"
#include "core/data.h"

#include <sys/socket.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sdb_unixsock_client;
typedef struct sdb_unixsock_client sdb_unixsock_client_t;

typedef int (*sdb_unixsock_client_data_cb)(sdb_unixsock_client_t *,
		size_t, sdb_data_t *, sdb_object_t *);

sdb_unixsock_client_t *
sdb_unixsock_client_create(const char *path);

int
sdb_unixsock_client_connect(sdb_unixsock_client_t *client);

int
sdb_unixsock_client_send(sdb_unixsock_client_t *client,
		const char *msg);

char *
sdb_unixsock_client_recv(sdb_unixsock_client_t *client,
		char *buffer, size_t buflen);

/*
 * sdb_unixsock_client_process_lines:
 * Reads up to 'max_lines' lines from the socket, splits each line at the
 * specified 'delim' and passes the data on to the specified 'callback'. If
 * 'max_lines' is less than zero, the function will read until EOF or an error
 * is encountered. If 'n_cols' is greater than zero, the function will expect
 * that number of columns to appear in each line. Also, it will expect that
 * number of further arguments, specifying the data-type to be returned for
 * the respective column (see sdb_data_t). The content of each column will
 * then be converted accordingly.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_unixsock_client_process_lines(sdb_unixsock_client_t *client,
		sdb_unixsock_client_data_cb callback, sdb_object_t *user_data,
		long int max_lines, const char *delim, int n_cols, ...);

/*
 * sdb_unixsock_client_shutdown:
 * Shut down the client's send and/or receive operations. If appropriate, the
 * client will automatically re-connect on the next send / receive operation
 * after that.
 *
 * See shutdown(3) for details.
 */
int
sdb_unixsock_client_shutdown(sdb_unixsock_client_t *client, int how);

/*
 * sdb_unixsock_client_clearerr, sdb_unixsock_client_eof,
 * sdb_unixsock_client_error:
 * Check and reset the client status. See the clearerr(3), feof(3), and
 * ferror(3) manpages for details.
 */
void
sdb_unixsock_client_clearerr(sdb_unixsock_client_t *client);
int
sdb_unixsock_client_eof(sdb_unixsock_client_t *client);
int
sdb_unixsock_client_error(sdb_unixsock_client_t *client);

void
sdb_unixsock_client_destroy(sdb_unixsock_client_t *client);

const char *
sdb_unixsock_client_path(sdb_unixsock_client_t *client);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_UTILS_UNIXSOCK_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

