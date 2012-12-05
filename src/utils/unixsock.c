/*
 * syscollector - src/utils/unixsock.c
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

#include "utils/unixsock.h"
#include "utils/string.h"

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

/*
 * private data types
 */

struct sc_unixsock_client {
	char *path;
	FILE *fh;
};

/*
 * public API
 */

sc_unixsock_client_t *
sc_unixsock_client_create(const char *path)
{
	sc_unixsock_client_t *client;

	if (! path)
		return NULL;

	client = malloc(sizeof(*client));
	if (! client)
		return NULL;
	memset(client, 0, sizeof(*client));
	client->fh = NULL;

	client->path = strdup(path);
	if (! client->path) {
		sc_unixsock_client_destroy(client);
		return NULL;
	}
	return client;
} /* sc_unixsock_client_create */

int
sc_unixsock_client_connect(sc_unixsock_client_t *client)
{
	struct sockaddr_un sa;
	int fd;

	if ((! client) || (! client->path))
		return -1;

	memset(&sa, 0, sizeof(sa));

	if (client->fh)
		fclose(client->fh);

	fd = socket(AF_UNIX, SOCK_STREAM, /* protocol = */ 0);
	if (fd < 0) {
		char errbuf[1024];
		fprintf(stderr, "unixsock: Failed to open socket: %s\n",
				sc_strerror(errno, errbuf, sizeof(errbuf)));
		return -1;
	}

	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, client->path, sizeof(sa.sun_path));
	sa.sun_path[sizeof(sa.sun_path) - 1] = '\0';

	if (connect(fd, (struct sockaddr *)&sa, sizeof(sa))) {
		char errbuf[1024];
		fprintf(stderr, "unixsock: Failed to connect to %s: %s\n",
				sa.sun_path, sc_strerror(errno, errbuf, sizeof(errbuf)));
		close(fd);
		return -1;
	}

	client->fh = fdopen(fd, "r+");
	if (! client->fh) {
		char errbuf[1024];
		fprintf(stderr, "unixsock: Failed to open I/O stream for %s: %s\n",
				sa.sun_path, sc_strerror(errno, errbuf, sizeof(errbuf)));
		close(fd);
		return -1;
	}
	return 0;
} /* sc_unixsock_client_connect */

int
sc_unixsock_client_send(sc_unixsock_client_t *client, const char *msg)
{
	int status;

	if ((! client) || (! client->fh))
		return -1;

	status = fprintf(client->fh, "%s\r\n", msg);
	if (status < 0) {
		char errbuf[1024];
		fprintf(stderr, "unixsock: Failed to write to socket (%s): %s\n",
				client->path, sc_strerror(errno, errbuf, sizeof(errbuf)));
		return status;
	}
	return status;
} /* sc_unixsock_client_send */

char *
sc_unixsock_client_recv(sc_unixsock_client_t *client, char *buffer, size_t buflen)
{
	if ((! client) || (! client->fh) || (! buffer))
		return NULL;

	buffer = fgets(buffer, (int)buflen - 1, client->fh);
	if ((! buffer) && (! feof(client->fh))) {
		char errbuf[1024];
		fprintf(stderr, "unixsock: Failed to read from socket (%s): %s\n",
				client->path, sc_strerror(errno, errbuf, sizeof(errbuf)));
		return buffer;
	}
	buffer[buflen - 1] = '\0';

	buflen = strlen(buffer);
	while ((buffer[buflen - 1] == '\n') || (buffer[buflen - 1] == '\r')) {
		buffer[buflen - 1] = '\0';
		--buflen;
	}
	return buffer;
} /* sc_unixsock_client_recv */

void
sc_unixsock_client_destroy(sc_unixsock_client_t *client)
{
	if (! client)
		return;

	if (client->path)
		free(client->path);
	client->path = NULL;

	if (client->fh)
		fclose(client->fh);
	client->fh = NULL;

	free(client);
} /* sc_unixsock_client_destroy */

const char *
sc_unixsock_client_path(sc_unixsock_client_t *client)
{
	if (! client)
		return NULL;
	return client->path;
} /* sc_unixsock_client_path */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

