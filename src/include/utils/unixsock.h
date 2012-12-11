/*
 * syscollector - src/include/utils/unixsock.h
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

#ifndef SC_UTILS_UNIXSOCK_H
#define SC_UTILS_UNIXSOCK_H 1

#include <sys/socket.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sc_unixsock_client;
typedef struct sc_unixsock_client sc_unixsock_client_t;

sc_unixsock_client_t *
sc_unixsock_client_create(const char *path);

int
sc_unixsock_client_connect(sc_unixsock_client_t *client);

int
sc_unixsock_client_send(sc_unixsock_client_t *client, const char *msg);

char *
sc_unixsock_client_recv(sc_unixsock_client_t *client, char *buffer, size_t buflen);

/*
 * sc_unixsock_client_shutdown:
 * Shut down the client's send and/or receive operations. If appropriate, the
 * client will automatically re-connect on the next send / receive operation
 * after that.
 *
 * See shutdown(3) for details.
 */
int
sc_unixsock_client_shutdown(sc_unixsock_client_t *client, int how);

/*
 * sc_unixsock_client_clearerr, sc_unixsock_client_eof,
 * sc_unixsock_client_error:
 * Check and reset the client status. See the clearerr(3), feof(3), and
 * ferror(3) manpages for details.
 */
void
sc_unixsock_client_clearerr(sc_unixsock_client_t *client);
int
sc_unixsock_client_eof(sc_unixsock_client_t *client);
int
sc_unixsock_client_error(sc_unixsock_client_t *client);

void
sc_unixsock_client_destroy(sc_unixsock_client_t *client);

const char *
sc_unixsock_client_path(sc_unixsock_client_t *client);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SC_UTILS_UNIXSOCK_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

