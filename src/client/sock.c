/*
 * SysDB - src/client/sock.c
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

#include "client/sock.h"
#include "utils/error.h"
#include "utils/strbuf.h"
#include "utils/proto.h"

#include <arpa/inet.h>

#include <errno.h>
#include <limits.h>

#include <stdlib.h>

#include <string.h>
#include <strings.h>

#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

/*
 * private data types
 */

struct sdb_client {
	char *address;
	int   fd;
};

/*
 * private helper functions
 */

static int
connect_unixsock(sdb_client_t *client, const char *address)
{
	struct sockaddr_un sa;

	client->fd = socket(AF_UNIX, SOCK_STREAM, /* protocol = */ 0);
	if (client->fd < 0) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "Failed to open socket: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return -1;
	}

	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, address, sizeof(sa.sun_path));
	sa.sun_path[sizeof(sa.sun_path) - 1] = '\0';

	if (connect(client->fd, (struct sockaddr *)&sa, sizeof(sa))) {
		char errbuf[1024];
		sdb_client_close(client);
		sdb_log(SDB_LOG_ERR, "Failed to connect to '%s': %s",
				sa.sun_path, sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return -1;
	}
	return client->fd;
} /* connect_unixsock */

/*
 * public API
 */

sdb_client_t *
sdb_client_create(const char *address)
{
	sdb_client_t *client;

	if (! address)
		return NULL;

	client = malloc(sizeof(*client));
	if (! client) {
		sdb_log(SDB_LOG_ERR, "Out of memory");
		return NULL;
	}
	memset(client, 0, sizeof(*client));
	client->fd = -1;

	client->address = strdup(address);
	if (! client->address) {
		sdb_client_destroy(client);
		sdb_log(SDB_LOG_ERR, "Out of memory");
		return NULL;
	}

	return client;
} /* sdb_client_create */

void
sdb_client_destroy(sdb_client_t *client)
{
	if (! client)
		return;

	sdb_client_close(client);

	if (client->address)
		free(client->address);
	client->address = NULL;

	free(client);
} /* sdb_client_destroy */

int
sdb_client_connect(sdb_client_t *client, const char *username)
{
	sdb_strbuf_t *buf;
	ssize_t status;
	uint32_t rstatus;

	if ((! client) || (! client->address))
		return -1;

	if (client->fd >= 0)
		return -1;

	if (!strncasecmp(client->address, "unix:", strlen("unix:")))
		connect_unixsock(client, client->address + strlen("unix:"));
	else if (*client->address == '/')
		connect_unixsock(client, client->address);
	else {
		sdb_log(SDB_LOG_ERR, "Unknown address type: %s", client->address);
		return -1;
	}

	if (client->fd < 0)
		return -1;

	/* XXX */
	if (! username)
		username = "";

	status = sdb_client_send(client, CONNECTION_STARTUP,
			(uint32_t)strlen(username), username);
	if (status < 0) {
		char errbuf[1024];
		sdb_client_close(client);
		sdb_log(SDB_LOG_ERR, "Failed to send STARTUP message to server: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return (int)status;
	}

	buf = sdb_strbuf_create(64);
	rstatus = 0;
	status = sdb_client_recv(client, &rstatus, buf);
	if (status < 0) {
		char errbuf[1024];
		sdb_client_close(client);
		sdb_strbuf_destroy(buf);
		sdb_log(SDB_LOG_ERR, "Failed to receive server response: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return (int)status;
	}

	if (rstatus != CONNECTION_OK) {
		sdb_client_close(client);
		sdb_strbuf_destroy(buf);
		sdb_log(SDB_LOG_ERR, "Access denied for user '%s'", username);
		return -((int)rstatus);
	}
	return 0;
} /* sdb_client_connect */

void
sdb_client_close(sdb_client_t *client)
{
	if (! client)
		return;

	close(client->fd);
	client->fd = -1;
} /* sdb_client_close */

ssize_t
sdb_client_send(sdb_client_t *client,
		uint32_t cmd, uint32_t msg_len, const char *msg)
{
	if ((! client) || (! client->fd))
		return -1;

	return sdb_proto_send_msg(client->fd, cmd, msg_len, msg);
} /* sdb_client_send */

ssize_t
sdb_client_recv(sdb_client_t *client,
		uint32_t *code, sdb_strbuf_t *buf)
{
	uint32_t rstatus = UINT32_MAX;
	uint32_t rlen = UINT32_MAX;

	size_t total = 0;
	size_t req = 2 * sizeof(uint32_t);

	size_t data_offset = sdb_strbuf_len(buf);

	if (code)
		*code = UINT32_MAX;

	if ((! client) || (! client->fd) || (! buf)) {
		errno = EBADF;
		return -1;
	}

	while (42) {
		ssize_t status;

		if (sdb_proto_select(client->fd, SDB_PROTO_SELECTIN))
			return -1;

		errno = 0;
		status = sdb_strbuf_read(buf, client->fd, req);
		if (status < 0) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
				continue;
			return status;
		}
		else if (! status) /* EOF */
			break;

		total += (size_t)status;

		if (total != req)
			continue;

		if (rstatus == UINT32_MAX) {
			/* retrieve status and data len */
			rstatus = sdb_proto_get_int(buf, data_offset);
			rlen = sdb_proto_get_int(buf, data_offset + sizeof(rstatus));

			if (! rlen)
				break;

			req = (size_t)rlen;
			total = 0;
		}
		else /* finished reading data */
			break;
	}

	if (total != req) {
		/* unexpected EOF; clear partially read data */
		sdb_strbuf_skip(buf, data_offset, sdb_strbuf_len(buf));
		return 0;
	}

	if (rstatus != UINT32_MAX)
		/* remove status,len */
		sdb_strbuf_skip(buf, data_offset, 2 * sizeof(rstatus));

	if (code)
		*code = rstatus;

	return (ssize_t)total;
} /* sdb_client_recv */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

