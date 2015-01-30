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

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "client/sock.h"
#include "utils/error.h"
#include "utils/strbuf.h"
#include "utils/proto.h"
#include "utils/os.h"
#include "utils/ssl.h"

#include <arpa/inet.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>

#include <stdlib.h>

#include <string.h>
#include <strings.h>

#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <netdb.h>

/*
 * private data types
 */

struct sdb_client {
	char *address;
	int   fd;
	bool  eof;

	/* optional SSL settings */
	sdb_ssl_options_t ssl_opts;
	sdb_ssl_client_t *ssl;
	sdb_ssl_session_t *ssl_session;

	ssize_t (*read)(sdb_client_t *, sdb_strbuf_t *, size_t);
	ssize_t (*write)(sdb_client_t *, const void *, size_t);
};

/*
 * private helper functions
 */

static ssize_t
ssl_read(sdb_client_t *client, sdb_strbuf_t *buf, size_t n)
{
	char tmp[n];
	ssize_t ret;

	ret = sdb_ssl_session_read(client->ssl_session, tmp, n);
	if (ret <= 0)
		return ret;

	sdb_strbuf_memappend(buf, tmp, ret);
	return ret;
} /* ssl_read */

static ssize_t
ssl_write(sdb_client_t *client, const void *buf, size_t n)
{
	return sdb_ssl_session_write(client->ssl_session, buf, n);
} /* ssl_write */

static ssize_t
client_read(sdb_client_t *client, sdb_strbuf_t *buf, size_t n)
{
	return sdb_strbuf_read(buf, client->fd, n);
} /* client_read */

static ssize_t
client_write(sdb_client_t *client, const void *buf, size_t n)
{
	return sdb_write(client->fd, n, buf);
} /* client_write */

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

static int
connect_tcp(sdb_client_t *client, const char *address)
{
	struct addrinfo *ai, *ai_list = NULL;
	int status;

	if ((status = sdb_resolve(SDB_NET_TCP, address, &ai_list))) {
		sdb_log(SDB_LOG_ERR, "Failed to resolve '%s': %s",
				address, gai_strerror(status));
		return -1;
	}

	for (ai = ai_list; ai != NULL; ai = ai->ai_next) {
		client->fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (client->fd < 0) {
			char errbuf[1024];
			sdb_log(SDB_LOG_ERR, "Failed to open socket: %s",
					sdb_strerror(errno, errbuf, sizeof(errbuf)));
			continue;
		}

		if (connect(client->fd, ai->ai_addr, ai->ai_addrlen)) {
			char host[1024], port[32], errbuf[1024];
			sdb_client_close(client);
			getnameinfo(ai->ai_addr, ai->ai_addrlen, host, sizeof(host),
					port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
			sdb_log(SDB_LOG_ERR, "Failed to connect to '%s:%s': %s",
					host, port, sdb_strerror(errno, errbuf, sizeof(errbuf)));
			continue;
		}
		break;
	}
	freeaddrinfo(ai_list);

	if (client->fd < 0)
		return -1;

	client->ssl = sdb_ssl_client_create(&client->ssl_opts);
	if (! client->ssl) {
		sdb_client_close(client);
		return -1;
	}
	client->ssl_session = sdb_ssl_client_connect(client->ssl, client->fd);
	if (! client->ssl_session) {
		sdb_client_close(client);
		return -1;
	}

	client->read = ssl_read;
	client->write = ssl_write;
	return client->fd;
} /* connect_tcp */

static void
free_ssl_options(sdb_ssl_options_t *opts)
{
	if (opts->ca_file)
		free(opts->ca_file);
	if (opts->key_file)
		free(opts->key_file);
	if (opts->cert_file)
		free(opts->cert_file);
	if (opts->crl_file)
		free(opts->crl_file);
	opts->ca_file = opts->key_file = opts->cert_file = opts->crl_file = NULL;
} /* free_ssl_options */

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
	client->eof = 1;

	client->ssl = NULL;
	client->read = client_read;
	client->write = client_write;

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

	free_ssl_options(&client->ssl_opts);

	free(client);
} /* sdb_client_destroy */

int
sdb_client_set_ssl_options(sdb_client_t *client, const sdb_ssl_options_t *opts)
{
	int ret = 0;

	if ((! client) || (! opts))
		return -1;

	free_ssl_options(&client->ssl_opts);

	if (opts->ca_file) {
		client->ssl_opts.ca_file = strdup(opts->ca_file);
		if (! client->ssl_opts.ca_file)
			ret = -1;
	}
	if (opts->key_file) {
		client->ssl_opts.key_file = strdup(opts->key_file);
		if (! client->ssl_opts.key_file)
			ret = -1;
	}
	if (opts->cert_file) {
		client->ssl_opts.cert_file = strdup(opts->cert_file);
		if (! client->ssl_opts.cert_file)
			ret = -1;
	}
	if (opts->crl_file) {
		client->ssl_opts.crl_file = strdup(opts->crl_file);
		if (! client->ssl_opts.crl_file)
			ret = -1;
	}

	if (ret)
		free_ssl_options(&client->ssl_opts);
	return ret;
} /* sdb_client_set_ssl_options */

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

	if (*client->address == '/')
		connect_unixsock(client, client->address);
	else if (!strncasecmp(client->address, "unix:", strlen("unix:")))
		connect_unixsock(client, client->address + strlen("unix:"));
	else if (!strncasecmp(client->address, "tcp:", strlen("tcp:")))
		connect_tcp(client, client->address + strlen("tcp:"));
	else
		connect_tcp(client, client->address);

	if (client->fd < 0)
		return -1;
	client->eof = 0;

	/* XXX */
	if (! username)
		username = "";

	buf = sdb_strbuf_create(64);
	rstatus = 0;
	status = sdb_client_rpc(client, SDB_CONNECTION_STARTUP,
			(uint32_t)strlen(username), username, &rstatus, buf);
	if ((status >= 0) && (rstatus == SDB_CONNECTION_OK)) {
		sdb_strbuf_destroy(buf);
		return 0;
	}

	if (status < 0) {
		sdb_log(SDB_LOG_ERR, "%s", sdb_strbuf_string(buf));
		sdb_client_close(client);
		sdb_strbuf_destroy(buf);
		return (int)status;
	}
	if (client->eof)
		sdb_log(SDB_LOG_ERR, "Encountered end-of-file while waiting "
				"for server response");

	if (rstatus == SDB_CONNECTION_ERROR) {
		sdb_log(SDB_LOG_ERR, "Access denied for user '%s': %s",
				username, sdb_strbuf_string(buf));
		status = -((int)rstatus);
	}
	else if (rstatus != SDB_CONNECTION_OK) {
		sdb_log(SDB_LOG_ERR, "Received unsupported authentication request "
				"(status %d) during startup", (int)rstatus);
		status = -((int)rstatus);
	}

	sdb_client_close(client);
	sdb_strbuf_destroy(buf);
	return (int)status;
} /* sdb_client_connect */

int
sdb_client_sockfd(sdb_client_t *client)
{
	if (! client)
		return -1;
	return client->fd;
} /* sdb_client_sockfd */

int
sdb_client_shutdown(sdb_client_t *client, int how)
{
	if (! client) {
		errno = ENOTSOCK;
		return -1;
	}

	if (client->fd < 0) {
		errno = EBADF;
		return -1;
	}

	return shutdown(client->fd, how);
} /* sdb_client_shutdown */

void
sdb_client_close(sdb_client_t *client)
{
	if (! client)
		return;

	if (client->ssl_session) {
		sdb_ssl_session_destroy(client->ssl_session);
		client->ssl_session = NULL;
	}
	if (client->ssl) {
		sdb_ssl_client_destroy(client->ssl);
		client->ssl = NULL;
	}

	close(client->fd);
	client->fd = -1;
	client->eof = 1;
} /* sdb_client_close */

ssize_t
sdb_client_rpc(sdb_client_t *client,
		uint32_t cmd, uint32_t msg_len, const char *msg,
		uint32_t *code, sdb_strbuf_t *buf)
{
	uint32_t rcode = 0;
	ssize_t status;

	if (! buf)
		return -1;

	if (sdb_client_send(client, cmd, msg_len, msg) < 0) {
		char errbuf[1024];
		sdb_strbuf_sprintf(buf, "Failed to send %s message to server: %s",
				SDB_CONN_MSGTYPE_TO_STRING(cmd),
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		if (code)
			*code = SDB_CONNECTION_ERROR;
		return -1;
	}

	while (42) {
		size_t offset = sdb_strbuf_len(buf);

		status = sdb_client_recv(client, &rcode, buf);
		if (status < 0) {
			char errbuf[1024];
			sdb_strbuf_sprintf(buf, "Failed to receive server response: %s",
					sdb_strerror(errno, errbuf, sizeof(errbuf)));
			if (code)
				*code = SDB_CONNECTION_ERROR;
			return status;
		}

		if (rcode == SDB_CONNECTION_LOG) {
			uint32_t prio = 0;
			if (sdb_proto_unmarshal_int32(SDB_STRBUF_STR(buf), &prio) < 0) {
				sdb_log(SDB_LOG_WARNING, "Received a LOG message "
						"with invalid or missing priority");
				prio = (uint32_t)SDB_LOG_ERR;
			}
			sdb_log((int)prio, "%s", sdb_strbuf_string(buf) + offset);
			sdb_strbuf_skip(buf, offset, sdb_strbuf_len(buf) - offset);
			continue;
		}
		break;
	}

	if (code)
		*code = rcode;
	return status;
} /* sdb_client_rpc */

ssize_t
sdb_client_send(sdb_client_t *client,
		uint32_t cmd, uint32_t msg_len, const char *msg)
{
	char buf[2 * sizeof(uint32_t) + msg_len];

	if ((! client) || (! client->fd))
		return -1;
	if (sdb_proto_marshal(buf, sizeof(buf), cmd, msg_len, msg) < 0)
		return -1;

	return client->write(client, buf, sizeof(buf));
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

		errno = 0;
		status = client->read(client, buf, req);
		if (status < 0) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
				continue;
			return status;
		}
		else if (! status) {
			client->eof = 1;
			break;
		}

		total += (size_t)status;

		if (total != req)
			continue;

		if (rstatus == UINT32_MAX) {
			const char *str = sdb_strbuf_string(buf) + data_offset;
			size_t len = sdb_strbuf_len(buf) - data_offset;
			ssize_t n;

			/* retrieve status and data len */
			assert(len >= 2 * sizeof(uint32_t));
			n = sdb_proto_unmarshal_int32(str, len, &rstatus);
			str += n; len -= (size_t)n;
			sdb_proto_unmarshal_int32(str, len, &rlen);

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

bool
sdb_client_eof(sdb_client_t *client)
{
	if ((! client) || (client->fd < 0))
		return 1;
	return client->eof;
} /* sdb_client_eof */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

