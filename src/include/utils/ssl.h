/*
 * SysDB - src/include/utils/ssl.h
 * Copyright (C) 2015 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#ifndef SDB_UTILS_SSL_H
#define SDB_UTILS_SSL_H 1

#include <sys/types.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SDB_SSL_KEYFILE
#	define SDB_SSL_KEYFILE SYSCONFDIR "/sysdb/ssl/key.pem"
#endif
#ifndef SDB_SSL_CERTFILE
#	define SDB_SSL_CERTFILE SYSCONFDIR "/sysdb/ssl/cert.pem"
#endif
#ifndef SDB_SSL_CRLFILE
#	define SDB_SSL_CRLFILE SYSCONFDIR "/sysdb/ssl/crl.pem"
#endif
#ifndef SDB_SSL_CAFILE
#	define SDB_SSL_CAFILE SYSCONFDIR "/ssl/certs/ca-certificates.crt"
#endif

typedef struct {
	char *ca_file;
	char *key_file;
	char *cert_file;
	char *crl_file;
} sdb_ssl_options_t;
#define SDB_SSL_DEFAULT_OPTIONS { \
	SDB_SSL_CAFILE, SDB_SSL_KEYFILE, SDB_SSL_CERTFILE, SDB_SSL_CRLFILE, \
}

struct sdb_ssl_client;
typedef struct sdb_ssl_client sdb_ssl_client_t;

struct sdb_ssl_server;
typedef struct sdb_ssl_server sdb_ssl_server_t;

struct sdb_ssl_session;
typedef struct sdb_ssl_session sdb_ssl_session_t;

/*
 * sdb_ssl_init, sdb_ssl_shutdown:
 * Global setup and shutdown of SSL/TLS. This is required before any other
 * function can be used.
 */
int
sdb_ssl_init(void);
void
sdb_ssl_shutdown(void);

/*
 * sdb_ssl_client_create:
 * Allocate and initialize a TLS/SSL client using the specified options. If no
 * options are specified, default values will be used instead.
 */
sdb_ssl_client_t *
sdb_ssl_client_create(const sdb_ssl_options_t *opts);

/*
 * sdb_ssl_client_destroy:
 * Destroy a TLS/SSL client and free all of its memory.
 */
void
sdb_ssl_client_destroy(sdb_ssl_client_t *client);

/*
 * sdb_ssl_client_connect:
 * Initialize a TLS/SSL session on the specified socket.
 */
sdb_ssl_session_t *
sdb_ssl_client_connect(sdb_ssl_client_t *client, int fd);

/*
 * sdb_ssl_server_create:
 * Allocate and initialize a TLS/SSL server using the specified options. If no
 * options are specified, default values will be used instead.
 */
sdb_ssl_server_t *
sdb_ssl_server_create(const sdb_ssl_options_t *opts);

/*
 * sdb_ssl_server_destroy:
 * Destroy a TLS/SSL server and free all of its memory.
 */
void
sdb_ssl_server_destroy(sdb_ssl_server_t *server);

/*
 * sdb_ssl_server_accept:
 * Initialize a TLS/SSL session on the specified socket.
 */
sdb_ssl_session_t *
sdb_ssl_server_accept(sdb_ssl_server_t *server, int fd);

/*
 * sdb_ssl_session_destroy:
 * Shutdown and destroy a TLS/SSL session.
 */
void
sdb_ssl_session_destroy(sdb_ssl_session_t *session);

/*
 * sdb_ssl_session_peer:
 * Return the name of the peer of a TLS/SSL session.
 *
 * Returns:
 *  - a dynamically allocated string on success
 *  - NULL else
 */
char *
sdb_ssl_session_peer(sdb_ssl_session_t *session);

/*
 * sdb_ssl_session_write:
 * Write a message to an open TLS/SSL session.
 */
ssize_t
sdb_ssl_session_write(sdb_ssl_session_t *session, const void *buf, size_t n);

/*
 * sdb_ssl_session_read:
 * Read from an open TLS/SSL session.
 */
ssize_t
sdb_ssl_session_read(sdb_ssl_session_t *session, void *buf, size_t n);

/*
 * sdb_ssl_free_options:
 * Free all strings stored in the specified options. All fields will be set to
 * NULL.
 */
void
sdb_ssl_free_options(sdb_ssl_options_t *opts);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_UTILS_SSL_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

