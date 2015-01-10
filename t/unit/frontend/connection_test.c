/*
 * SysDB - t/unit/frontend/connection_test.c
 * Copyright (C) 2014 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include "frontend/connection.h"
#include "frontend/connection-private.h"
#include "utils/os.h"
#include "libsysdb_test.h"

#include "utils/strbuf.h"

#include <assert.h>
#include <check.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

static char username[1024];

/*
 * private helper functions
 */

static void
mock_conn_destroy(sdb_conn_t *conn)
{
	if (SDB_OBJ(conn)->name)
		free(SDB_OBJ(conn)->name);
	sdb_strbuf_destroy(conn->buf);
	sdb_strbuf_destroy(conn->errbuf);
	if (conn->fd >= 0)
		close(conn->fd);
	if (conn->username)
		free(conn->username);
	free(conn);
} /* mock_conn_destroy */

static ssize_t
mock_conn_read(sdb_conn_t *conn, size_t len)
{
       return sdb_strbuf_read(conn->buf, conn->fd, len);
} /* conn_read */

static ssize_t
mock_conn_write(sdb_conn_t *conn, const void *buf, size_t len)
{
       return sdb_write(conn->fd, len, buf);
} /* conn_write */

static sdb_conn_t *
mock_conn_create(void)
{
	sdb_conn_t *conn;

	char tmp_file[] = "connection_test_socket.XXXXXX";

	conn = calloc(1, sizeof(*conn));
	if (! conn) {
		fail("INTERNAL ERROR: failed to allocate connection object");
		return NULL;
	}

	SDB_OBJ(conn)->name = strdup("mock_connection");
	SDB_OBJ(conn)->ref_cnt = 1;

	conn->buf = sdb_strbuf_create(0);
	conn->errbuf = sdb_strbuf_create(0);
	if ((! conn->buf) || (! conn->errbuf)) {
		mock_conn_destroy(conn);
		fail("INTERNAL ERROR: failed to allocate connection object");
		return NULL;
	}

	conn->fd = mkstemp(tmp_file);
	if (conn->fd < 0) {
		mock_conn_destroy(conn);
		fail("INTERNAL ERROR: failed to allocate connection object");
		return NULL;
	}

	unlink(tmp_file);

	conn->read = mock_conn_read;
	conn->write = mock_conn_write;

	conn->username = strdup(username);
	assert(conn->username);

	conn->cmd = SDB_CONNECTION_IDLE;
	conn->cmd_len = 0;
	return conn;
} /* mock_conn_create */

static void
mock_conn_rewind(sdb_conn_t *conn)
{
	lseek(conn->fd, 0, SEEK_SET);
} /* mock_conn_rewind */

static void
mock_conn_truncate(sdb_conn_t *conn)
{
	int status;
	lseek(conn->fd, 0, SEEK_SET);
	status = ftruncate(conn->fd, 0);
	fail_unless(status == 0,
			"INTERNAL ERROR: ftruncate(%d, 0) = %d; expected: 0",
			conn->fd, status);
} /* mock_conn_truncate */

static int
mock_unixsock_listener(char *socket_path)
{
	struct sockaddr_un sa;
	int fd, status;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	fail_unless(fd >= 0,
			"INTERNAL ERROR: socket() = %d; expected: >=0", fd);

	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, socket_path, sizeof(sa.sun_path));

	status = bind(fd, (struct sockaddr *)&sa, sizeof(sa));
	fail_unless(status == 0,
			"INTERNAL ERROR: bind() = %d; expected: 0", status);
	status = listen(fd, 32);
	fail_unless(status == 0,
			"INTERNAL ERROR: listen() = %d; expected: 0", status);

	return fd;
} /* mock_unixsock */

static void *
mock_client(void *arg)
{
	char *socket_path = arg;

	struct sockaddr_un sa;
	int fd, check;

	fd = socket(AF_UNIX, SOCK_STREAM, /* protocol = */ 0);
	fail_unless(fd >= 0,
			"INTERNAL ERROR: socket() = %d; expected: >= 0", fd);

	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, socket_path, sizeof(sa.sun_path));

	check = connect(fd, (struct sockaddr *)&sa, sizeof(sa));
	fail_unless(check == 0,
			"INTERNAL ERROR: connect() = %d; expected: 0", check);

	close(fd);
	return NULL;
} /* mock_client */

static void
connection_startup(sdb_conn_t *conn)
{
	ssize_t check, expected;

	expected = 2 * sizeof(uint32_t) + strlen(username);
	check = sdb_connection_send(conn, SDB_CONNECTION_STARTUP,
			(uint32_t)strlen(username), username);
	fail_unless(check == expected,
			"sdb_connection_send(STARTUP, %s) = %zi; expected: %zi",
			username, check, expected);

	mock_conn_rewind(conn);
	check = sdb_connection_handle(conn);
	fail_unless(check == expected,
			"On startup: sdb_connection_handle() = %zi; expected: %zi",
			check, expected);

	fail_unless(sdb_strbuf_len(conn->errbuf) == 0,
			"sdb_connection_handle() left %zu bytes in the error "
			"buffer (%s); expected: 0", sdb_strbuf_len(conn->errbuf),
			sdb_strbuf_string(conn->errbuf));

	mock_conn_truncate(conn);
} /* connection_startup */

/*
 * tests
 */

START_TEST(test_conn_accept)
{
	char socket_path[] = "connection_test_socket.XXXXXX";
	int fd, check;

	sdb_conn_t *conn;

	pthread_t thr;

	conn = sdb_connection_accept(-1);
	fail_unless(conn == NULL,
			"sdb_connection_accept(-1) = %p; expected: NULL", conn);

	fd = mkstemp(socket_path);
	unlink(socket_path);
	close(fd);

	fd = mock_unixsock_listener(socket_path);
	check = pthread_create(&thr, /* attr = */ NULL, mock_client, socket_path);
	fail_unless(check == 0,
			"INTERNAL ERROR: pthread_create() = %i; expected: 0", check);

	conn = sdb_connection_accept(fd);
	fail_unless(conn != NULL,
			"sdb_connection_accept(%d) = %p; expected: <conn>", fd, conn);

	unlink(socket_path);
	sdb_connection_close(conn);
	sdb_object_deref(SDB_OBJ(conn));
	pthread_join(thr, NULL);
}
END_TEST

/* test connection setup and very basic commands */
START_TEST(test_conn_setup)
{
	sdb_conn_t *conn = mock_conn_create();

	struct {
		uint32_t code;
		const char *msg;
		const char *err;
	} golden_data[] = {
		/* code == UINT32_MAX => no data will be sent */
		{ UINT32_MAX,             NULL,       NULL },
		{ SDB_CONNECTION_IDLE,    "fakedata", "Authentication required" },
		{ SDB_CONNECTION_PING,    NULL,       "Authentication required" },
		{ SDB_CONNECTION_STARTUP, username,   NULL },
		{ SDB_CONNECTION_PING,    NULL,       NULL },
		{ SDB_CONNECTION_IDLE,    NULL,       "Invalid command 0" },
		{ SDB_CONNECTION_PING,    "fakedata", NULL },
		{ SDB_CONNECTION_IDLE,    NULL,       "Invalid command 0" },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		ssize_t check, expected = 0;

		mock_conn_truncate(conn);

		if (golden_data[i].code != UINT32_MAX) {
			expected = 2 * sizeof(uint32_t)
				+ (golden_data[i].msg ? strlen(golden_data[i].msg) : 0);

			check = sdb_connection_send(conn, golden_data[i].code,
					(uint32_t)(golden_data[i].msg
						? strlen(golden_data[i].msg) : 0),
					golden_data[i].msg);
			fail_unless(check == expected,
					"sdb_connection_send(%d, %s) = %zi; expected: %zi",
					golden_data[i].code,
					golden_data[i].msg ? golden_data[i].msg : "<null>",
					check, expected);
		}

		mock_conn_rewind(conn);
		check = sdb_connection_handle(conn);
		fail_unless(check == expected,
				"sdb_connection_handle() = %zi; expected: %zi",
				check, expected);

		fail_unless(sdb_strbuf_len(conn->buf) == 0,
				"sdb_connection_handle() left %zu bytes in the buffer; "
				"expected: 0", sdb_strbuf_len(conn->buf));

		if (golden_data[i].err) {
			const char *err = sdb_strbuf_string(conn->errbuf);
			fail_unless(strcmp(err, golden_data[i].err) == 0,
					"sdb_connection_handle(): got error '%s'; "
					"expected: '%s'", err, golden_data[i].err);
		}
		else
			fail_unless(sdb_strbuf_len(conn->errbuf) == 0,
					"sdb_connection_handle() left %zu bytes in the error "
					"buffer (%s); expected: 0", sdb_strbuf_len(conn->errbuf),
					sdb_strbuf_string(conn->errbuf));
	}

	mock_conn_destroy(conn);
}
END_TEST

/* test simple I/O on open connections */
START_TEST(test_conn_io)
{
	sdb_conn_t *conn = mock_conn_create();

	struct {
		uint32_t code;
		uint32_t msg_len;
		const char *msg;
		size_t buf_len; /* number of bytes we expect in conn->buf */
		const char *err;
	} golden_data[] = {
		/* code == UINT32_MAX => this is a follow-up package */
		{ SDB_CONNECTION_PING,    20, "9876543210",  0, "Authentication required" },
		{ UINT32_MAX,             -1, "9876543210",  0, "Authentication required" },
		{ SDB_CONNECTION_PING,    10, "9876543210",  0, "Authentication required" },
		{ SDB_CONNECTION_IDLE,    10, "9876543210",  0, "Authentication required" },
		{ SDB_CONNECTION_IDLE,    20, "9876543210",  0, "Authentication required" },
		{ UINT32_MAX,             -1, "9876543210",  0, "Authentication required" },
		{ SDB_CONNECTION_STARTUP, -1, NULL,          0, NULL },
		{ SDB_CONNECTION_PING,    20, "9876543210", 10, NULL },
		{ UINT32_MAX,             -1, "9876543210",  0, NULL },
		{ SDB_CONNECTION_IDLE,    20, "9876543210",  0, "Invalid command 0" },
		{ UINT32_MAX,             -1, "9876543210",  0, "Invalid command 0" },
		{ SDB_CONNECTION_IDLE,    20, "9876543210",  0, "Invalid command 0" },
		{ UINT32_MAX,             -1, "9876543210",  0, "Invalid command 0" },
		{ SDB_CONNECTION_PING,    10, "9876543210",  0, NULL },
		{ SDB_CONNECTION_PING,    20, "9876543210", 10, NULL },
		{ UINT32_MAX,             -1, "9876543210",  0, NULL },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		size_t msg_len = golden_data[i].msg ? strlen(golden_data[i].msg) : 0;
		char buffer[2 * sizeof(uint32_t) + msg_len];
		size_t offset = 0;

		ssize_t check;

		mock_conn_truncate(conn);

		if (golden_data[i].code == SDB_CONNECTION_STARTUP) {
			connection_startup(conn);
			continue;
		}

		if (golden_data[i].code != UINT32_MAX) {
			uint32_t tmp;

			tmp = htonl(golden_data[i].code);
			memcpy(buffer, &tmp, sizeof(tmp));
			tmp = htonl(golden_data[i].msg_len);
			memcpy(buffer + sizeof(tmp), &tmp, sizeof(tmp));

			msg_len += 2 * sizeof(uint32_t);
			offset += 2 * sizeof(uint32_t);
		}

		memcpy(buffer + offset, golden_data[i].msg,
				strlen(golden_data[i].msg));

		check = sdb_write(conn->fd, msg_len, buffer);
		fail_unless(check == (ssize_t)msg_len,
				"sdb_write(%s) = %zi; expected: %zu",
				check, msg_len);

		mock_conn_rewind(conn);
		check = sdb_connection_handle(conn);
		fail_unless(check == (ssize_t)msg_len,
				"sdb_connection_handle() = %zi; expected: %zu",
				check, msg_len);

		if (golden_data[i].buf_len) {
			/* partial commands need to be stored in the object */
			fail_unless(conn->cmd == golden_data[i].code,
					"sdb_connection_handle() set partial command "
					"to %u; expected: %u", conn->cmd, golden_data[i].code);
			fail_unless(conn->cmd_len > golden_data[i].buf_len,
					"sdb_connection_handle() set partial command length "
					"to %u; expected: > %u", conn->cmd_len,
					golden_data[i].buf_len);
		}
		else {
			fail_unless(conn->cmd == SDB_CONNECTION_IDLE,
					"sdb_connection_handle() did not reset command; "
					"got %u; expected: %u", conn->cmd, SDB_CONNECTION_IDLE);
			fail_unless(conn->cmd_len == 0,
					"sdb_connection_handle() did not reset command length; "
					"got %u; expected: 0", conn->cmd_len);
		}

		fail_unless(sdb_strbuf_len(conn->buf) == golden_data[i].buf_len,
				"sdb_connection_handle() left %zu bytes in the buffer; "
				"expected: %zu", sdb_strbuf_len(conn->buf),
				golden_data[i].buf_len);

		if (golden_data[i].err) {
			const char *err = sdb_strbuf_string(conn->errbuf);
			fail_unless(strcmp(err, golden_data[i].err) == 0,
					"sdb_connection_handle(): got error '%s'; "
					"expected: '%s'", err, golden_data[i].err);
		}
		else
			fail_unless(sdb_strbuf_len(conn->errbuf) == 0,
					"sdb_connection_handle() left %zu bytes in the error "
					"buffer; expected: 0", sdb_strbuf_len(conn->errbuf));
	}

	mock_conn_destroy(conn);
}
END_TEST

Suite *
fe_conn_suite(void)
{
	Suite *s = suite_create("frontend::connection");
	TCase *tc;

	char *tmp = sdb_get_current_user();
	assert(tmp);
	strcpy(username, tmp);
	free(tmp);

	tc = tcase_create("core");
	tcase_add_test(tc, test_conn_accept);
	tcase_add_test(tc, test_conn_setup);
	tcase_add_test(tc, test_conn_io);
	suite_add_tcase(s, tc);

	return s;
} /* fe_conn_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

