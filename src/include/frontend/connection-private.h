/*
 * SysDB - src/include/frontend/connection-private.h
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

/*
 * private data structures used by frontend modules
 */

#ifndef SDB_FRONTEND_CONNECTION_PRIVATE_H
#define SDB_FRONTEND_CONNECTION_PRIVATE_H 1

#include "core/object.h"
#include "utils/strbuf.h"
#include "frontend/connection.h"

#include <inttypes.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sdb_conn {
	sdb_object_t super;

	/* file-descriptor of the open connection */
	int fd;

	/* connection and client information */
	struct sockaddr_storage client_addr;
	socklen_t client_addr_len;

	/* read buffer */
	sdb_strbuf_t *buf;

	/* connection / protocol state information */
	uint32_t cmd;
	uint32_t cmd_len;

	sdb_strbuf_t *errbuf;

	/* user information */
	char *username; /* NULL if the user has not been authenticated */
};
#define CONN(obj) ((sdb_conn_t *)(obj))

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_FRONTEND_CONNECTION_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

