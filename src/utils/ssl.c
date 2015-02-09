/*
 * SysDB - src/utils/ssl.c
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

#if HAVE_CONFIG_H
#	include "config.h"
#endif

#include "utils/ssl.h"
#include "utils/error.h"

#include <errno.h>

#include <stdlib.h>
#include <string.h>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/err.h>

/*
 * data types
 */

struct sdb_ssl_client {
	SSL_CTX *ctx;
	sdb_ssl_options_t opts;
};

struct sdb_ssl_server {
	SSL_CTX *ctx;
	sdb_ssl_options_t opts;
};

struct sdb_ssl_session {
	SSL *ssl;
};

/*
 * private helper functions
 */

/* log all pending SSL errors */
static void
ssl_log(int prio, const char *prefix, ...)
{
	char msg[1024];
	va_list ap;

	va_start(ap, prefix);
	vsnprintf(msg, sizeof(msg), prefix, ap);
	msg[sizeof(msg) - 1] = '\0';
	va_end(ap);

	while (42) {
		unsigned long e = ERR_get_error();
		if (! e)
			break;
		sdb_log(prio, "%s: %s", msg, ERR_reason_error_string(e));
	}
} /* ssl_log */

static void
ssl_log_err(int prio, SSL *ssl, int status, const char *prefix, ...)
{
	int err = SSL_get_error(ssl, status);
	char msg[1024];
	va_list ap;

	va_start(ap, prefix);
	vsnprintf(msg, sizeof(msg), prefix, ap);
	msg[sizeof(msg) - 1] = '\0';
	va_end(ap);

	errno = 0;
	switch (err) {
		case SSL_ERROR_NONE:
			sdb_log(prio, "%s: success", msg);
			break;
		case SSL_ERROR_ZERO_RETURN:
			errno = ECONNRESET;
			break;
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			errno = EWOULDBLOCK;
			break;
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
			sdb_log(prio, "%s: connection not set up", msg);
			break;
		case SSL_ERROR_WANT_X509_LOOKUP:
			sdb_log(prio, "%s: application error", msg);
			break;
		case SSL_ERROR_SYSCALL:
			if (ERR_peek_error())
				return ssl_log(prio, msg);
			if (! status)
				sdb_log(prio, "%s: unexpected end-of-file", msg);
			else if (! errno)
				errno = EIO;
		case SSL_ERROR_SSL:
			return ssl_log(prio, msg);
		default:
			sdb_log(prio, "%s: unkown SSL error %d", msg, err);
			break;
	}

	if (errno) {
		char errbuf[1024];
		sdb_log(prio, "%s: %s", msg,
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
	}
} /* ssl_log_err */

static int
copy_options(sdb_ssl_options_t *dst, const sdb_ssl_options_t *src)
{
	sdb_ssl_options_t tmp;
	sdb_ssl_options_t def = SDB_SSL_DEFAULT_OPTIONS;

	if (src)
		tmp = *src;
	else
		tmp = def;

	if (! tmp.ca_file)
		tmp.ca_file = def.ca_file;
	if (! tmp.key_file)
		tmp.key_file = def.key_file;
	if (! tmp.cert_file)
		tmp.cert_file = def.cert_file;

	dst->ca_file = strdup(tmp.ca_file);
	dst->key_file = strdup(tmp.key_file);
	dst->cert_file = strdup(tmp.cert_file);
	if ((! dst->ca_file) || (! dst->key_file) || (! dst->cert_file))
		return -1;
	if (tmp.crl_file) {
		dst->crl_file = strdup(tmp.crl_file);
		if (! dst->crl_file)
			return -1;
	}
	return 0;
} /* copy_options */

/*
 * public API
 */

void
sdb_ssl_init(void)
{
	SSL_load_error_strings();
	OpenSSL_add_ssl_algorithms();
} /* sdb_ssl_init */

void
sdb_ssl_shutdown(void)
{
	ERR_free_strings();
} /* sdb_ssl_shutdown */

sdb_ssl_client_t *
sdb_ssl_client_create(const sdb_ssl_options_t *opts)
{
	sdb_ssl_client_t *client;

	client = calloc(1, sizeof(*client));
	if (! client)
		return NULL;

	if (copy_options(&client->opts, opts)) {
		sdb_ssl_client_destroy(client);
		return NULL;
	}

	client->ctx = SSL_CTX_new(SSLv23_client_method());
	if (! client->ctx) {
		ssl_log(SDB_LOG_ERR, "ssl: Failed to create SSL context");
		sdb_ssl_client_destroy(client);
		return NULL;
	}

	if (! SSL_CTX_load_verify_locations(client->ctx,
				client->opts.ca_file, NULL)) {
		ssl_log(SDB_LOG_ERR, "ssl: Failed to load CA file '%s'",
				client->opts.ca_file);
		sdb_ssl_client_destroy(client);
		return NULL;
	}
	if (! SSL_CTX_use_certificate_file(client->ctx,
				client->opts.cert_file, SSL_FILETYPE_PEM)) {
		ssl_log(SDB_LOG_ERR, "ssl: Failed to load cert file '%s'",
				client->opts.cert_file);
		sdb_ssl_client_destroy(client);
		return NULL;
	}
	if (! SSL_CTX_use_PrivateKey_file(client->ctx,
				client->opts.key_file, SSL_FILETYPE_PEM)) {
		ssl_log(SDB_LOG_ERR, "ssl: Failed to load key file '%s'",
				client->opts.key_file);
		sdb_ssl_client_destroy(client);
		return NULL;
	}
	if (! SSL_CTX_check_private_key(client->ctx)) {
		ssl_log(SDB_LOG_ERR, "ssl: Failed to verify key (%s)",
				client->opts.key_file);
		sdb_ssl_client_destroy(client);
		return NULL;
	}

	SSL_CTX_set_mode(client->ctx, SSL_MODE_AUTO_RETRY);
	SSL_CTX_set_verify(client->ctx, SSL_VERIFY_PEER, NULL);
	SSL_CTX_set_verify_depth(client->ctx, 1);
	return client;
} /* sdb_ssl_client_create */

void
sdb_ssl_client_destroy(sdb_ssl_client_t *client)
{
	if (! client)
		return;

	if (client->ctx)
		SSL_CTX_free(client->ctx);
	sdb_ssl_free_options(&client->opts);
	free(client);
} /* sdb_ssl_client_destroy */

sdb_ssl_session_t *
sdb_ssl_client_connect(sdb_ssl_client_t *client, int fd)
{
	sdb_ssl_session_t *session;
	int status;
	BIO *bio;

	if ((! client) || (fd < 0))
		return NULL;

	session = calloc(1, sizeof(*session));
	if (! session)
		return NULL;

	bio = BIO_new_socket(fd, BIO_NOCLOSE);
	if (! bio) {
		ssl_log(SDB_LOG_ERR, "ssl: Failed to create SSL socket");
		sdb_ssl_session_destroy(session);
		return NULL;
	}
	session->ssl = SSL_new(client->ctx);
	if (! session->ssl) {
		ssl_log(SDB_LOG_ERR, "ssl: Failed to create SSL object");
		sdb_ssl_session_destroy(session);
		return NULL;
	}
	SSL_set_bio(session->ssl, bio, bio);

	if ((status = SSL_connect(session->ssl)) <= 0) {
		ssl_log_err(SDB_LOG_ERR, session->ssl, status,
				"ssl: Failed to initialize SSL session");
		sdb_ssl_session_destroy(session);
		return NULL;
	}
	if (SSL_get_verify_result(session->ssl) != X509_V_OK) {
		sdb_log(SDB_LOG_ERR, "Failed to verify SSL connection");
		sdb_ssl_session_destroy(session);
		return NULL;
	}
	return session;
} /* sdb_ssl_client_connect */

sdb_ssl_server_t *
sdb_ssl_server_create(const sdb_ssl_options_t *opts)
{
	sdb_ssl_server_t *server;

	server = calloc(1, sizeof(*server));
	if (! server)
		return NULL;

	if (copy_options(&server->opts, opts)) {
		sdb_ssl_server_destroy(server);
		return NULL;
	}

	server->ctx = SSL_CTX_new(SSLv23_server_method());
	if (! server->ctx) {
		ssl_log(SDB_LOG_ERR, "ssl: Failed to create SSL context");
		sdb_ssl_server_destroy(server);
		return NULL;
	}

	/* Recommendation documented at
	 * https://hynek.me/articles/hardening-your-web-servers-ssl-ciphers/ */
	if (! SSL_CTX_set_cipher_list(server->ctx,
				"ECDH+AESGCM:DH+AESGCM:ECDH+AES256:DH+AES256:ECDH+AES128:"
				"DH+AES:ECDH+3DES:DH+3DES:RSA+AESGCM:RSA+AES:RSA+3DES:"
				"!aNULL:!MD5:!DSS")) {
		sdb_log(SDB_LOG_ERR, "ssl: Invalid cipher list");
		sdb_ssl_server_destroy(server);
		return NULL;
	}

	if (! SSL_CTX_load_verify_locations(server->ctx,
				server->opts.ca_file, NULL)) {
		ssl_log(SDB_LOG_ERR, "Failed to load CA file %s",
				server->opts.ca_file);
		return NULL;
	}
	SSL_CTX_set_client_CA_list(server->ctx,
			SSL_load_client_CA_file(server->opts.ca_file));

	if (! SSL_CTX_use_certificate_file(server->ctx,
				server->opts.cert_file, SSL_FILETYPE_PEM)) {
		ssl_log(SDB_LOG_ERR, "Failed to load SSL cert file %s",
				server->opts.cert_file);
		return NULL;
	}
	if (! SSL_CTX_use_PrivateKey_file(server->ctx,
				server->opts.key_file, SSL_FILETYPE_PEM)) {
		ssl_log(SDB_LOG_ERR, "Failed to load SSL key file %s",
				server->opts.key_file);
		return NULL;
	}
	if (! SSL_CTX_check_private_key(server->ctx)) {
		ssl_log(SDB_LOG_ERR, "Failed to verify SSL private key");
		return NULL;
	}

	SSL_CTX_set_mode(server->ctx, SSL_MODE_AUTO_RETRY);
	SSL_CTX_set_verify(server->ctx,
			SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
	SSL_CTX_set_verify_depth(server->ctx, 1);

	/* TODO: handle server->opts.crl_file */
	return server;
} /* sdb_ssl_server_create */

void
sdb_ssl_server_destroy(sdb_ssl_server_t *server)
{
	if (! server)
		return;

	if (server->ctx)
		SSL_CTX_free(server->ctx);
	sdb_ssl_free_options(&server->opts);
	free(server);
} /* sdb_ssl_server_destroy */

sdb_ssl_session_t *
sdb_ssl_server_accept(sdb_ssl_server_t *server, int fd)
{
	sdb_ssl_session_t *session;
	int status;
	BIO *bio;

	if ((! server) || (fd < 0))
		return NULL;

	session = calloc(1, sizeof(*session));
	if (! session)
		return NULL;

	bio = BIO_new_socket(fd, BIO_NOCLOSE);
	if (! bio) {
		ssl_log(SDB_LOG_ERR, "ssl: Failed to create SSL socket");
		sdb_ssl_session_destroy(session);
		return NULL;
	}
	session->ssl = SSL_new(server->ctx);
	if (! session->ssl) {
		ssl_log(SDB_LOG_ERR, "ssl: Failed to create SSL object");
		sdb_ssl_session_destroy(session);
		return NULL;
	}
	SSL_set_bio(session->ssl, bio, bio);

	while (42) {
		if ((status = SSL_accept(session->ssl)) <= 0) {
			if (SSL_get_error(session->ssl, status) == SSL_ERROR_WANT_READ)
				continue;

			ssl_log_err(SDB_LOG_ERR, session->ssl, status,
					"ssl: Failed to initialize SSL session");
			sdb_ssl_session_destroy(session);
			return NULL;
		}
		break;
	}
	if (SSL_get_verify_result(session->ssl) != X509_V_OK) {
		sdb_log(SDB_LOG_ERR, "Failed to verify SSL connection");
		sdb_ssl_session_destroy(session);
		return NULL;
	}
	return session;
} /* sdb_ssl_server_accept */

void
sdb_ssl_session_destroy(sdb_ssl_session_t *session)
{
	if (! session)
		return;

	if (session->ssl) {
		SSL_shutdown(session->ssl);
		SSL_clear(session->ssl);
		SSL_free(session->ssl);
	}
	free(session);
} /* sdb_ssl_session_destroy */

char *
sdb_ssl_session_peer(sdb_ssl_session_t *session)
{
	X509 *x509;
	X509_NAME *name;

	char *peer = NULL;
	char p[1024];

	if (! session)
		return NULL;

	x509 = SSL_get_peer_certificate(session->ssl);
	if (! x509)
		return NULL;
	name = X509_get_subject_name(x509);
	if (! name) {
		X509_free(x509);
		return NULL;
	}

	if (X509_NAME_get_text_by_NID(name, NID_commonName, p, sizeof(p)) > 0)
		peer = strdup(p);

	X509_free(x509);
	return peer;
} /* sdb_ssl_session_peer */

ssize_t
sdb_ssl_session_write(sdb_ssl_session_t *session, const void *buf, size_t n)
{
	int status;

	if (! session)
		return -1;

	status = SSL_write(session->ssl, buf, (int)n);
	if (status) {
		if ((status < 0) && (errno != EAGAIN))
			ssl_log_err(SDB_LOG_ERR, session->ssl, status, "ssl: Write error");
		return (ssize_t)status;
	}

	status = SSL_get_error(session->ssl, status);
	if (status == SSL_ERROR_ZERO_RETURN)
		return 0;

	if ((status == SSL_ERROR_WANT_READ) || (status == SSL_ERROR_WANT_WRITE)) {
		errno = EWOULDBLOCK;
		return -1;
	}
	errno = ECONNRESET;
	return -1;
} /* sdb_ssl_session_write */

ssize_t
sdb_ssl_session_read(sdb_ssl_session_t *session, void *buf, size_t n)
{
	int status;

	if (! session)
		return -1;

	status = SSL_read(session->ssl, buf, (int)n);
	if (status) {
		if ((status < 0) && (errno != EAGAIN))
			ssl_log_err(SDB_LOG_ERR, session->ssl, status, "ssl: Read error");
		return (ssize_t)status;
	}

	status = SSL_get_error(session->ssl, status);
	if (status == SSL_ERROR_ZERO_RETURN)
		return 0;

	if ((status == SSL_ERROR_WANT_READ) || (status == SSL_ERROR_WANT_WRITE)) {
		errno = EWOULDBLOCK;
		return -1;
	}
	errno = ECONNRESET;
	return -1;
} /* sdb_ssl_session_read */

void
sdb_ssl_free_options(sdb_ssl_options_t *opts)
{
	if (! opts)
		return;

	if (opts->ca_file)
		free(opts->ca_file);
	if (opts->key_file)
		free(opts->key_file);
	if (opts->cert_file)
		free(opts->cert_file);
	if (opts->crl_file)
		free(opts->crl_file);

	opts->ca_file = opts->key_file = opts->cert_file = opts->crl_file = NULL;
} /* sdb_ssl_free_options */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

