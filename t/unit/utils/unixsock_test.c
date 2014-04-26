/*
 * SysDB - t/unit/utils/unixsock_test.c
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

/* required for fopencookie support */
#define _GNU_SOURCE

#include "utils/unixsock.h"
#include "libsysdb_test.h"

#include <check.h>

#include <dlfcn.h>

#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

/*
 * I/O "hook" functions
 */

typedef struct {
	int fd;
	size_t pos;
} io_cookie_t;

static struct {
	const char *data;
	size_t len;
} golden_data[] = {
	{ "a", 1 },
	{ "abc", 3 },
	{ "12345", 5 },
	{ "", 0 },
};

static char *last_write = NULL;

static unsigned long long mock_read_called = 0;
static ssize_t
mock_read(void *cookie, char *buf, size_t size)
{
	io_cookie_t *c = cookie;
	ssize_t ret;

	++mock_read_called;

	if (c->pos >= SDB_STATIC_ARRAY_LEN(golden_data))
		return 0;

	ret = snprintf(buf, size, "%s\n", golden_data[c->pos].data);
	++c->pos;
	return ret;
} /* mock_read */

static unsigned long long mock_write_called = 0;
static ssize_t
mock_write(void *cookie, const char *buf, size_t size)
{
	io_cookie_t *c = cookie;

	++mock_write_called;

	if (c->pos >= SDB_STATIC_ARRAY_LEN(golden_data))
		return 0;

	if (last_write)
		free(last_write);
	last_write = strdup(buf);
	++c->pos;
	return (ssize_t)size;
} /* mock_write */

/* unsupported: int seek(void *cookie, off64_t *offset, int whence) */

static int
mock_close(void *cookie)
{
	io_cookie_t *c = cookie;

	if (! c)
		return EBADF;

	close(c->fd);
	free(c);
	return 0;
} /* mock_close */

static cookie_io_functions_t mock_io = {
	/* read = */  mock_read,
	/* write = */ mock_write,
	/* seek = */  NULL,
	/* close = */ mock_close,
};

/*
 * mocked functions
 */

static int myfd = -1;

static void *
dlopen_libc(void)
{
	static void *libc = NULL;

	if (libc)
		return libc;

	libc = dlopen("libc.so.6", RTLD_LAZY);
	if (! libc)
		fail("INTERNAL ERROR: failed to load libc");
	return libc;
} /* dlopen_libc */

int
socket(int domain, int __attribute__((unused)) type,
		int __attribute__((unused)) protocol)
{
	char tmp_file[] = "unixsock_test_socket.XXXXXX";
	int tmp_fd;

	/* we only want to mock UNIX sockets; return an error else */
	if (domain != AF_UNIX) {
		errno = EAFNOSUPPORT;
		return -1;
	}

	/* create an 'anonymous' file to have a valid file-descriptor
	 * which can be close()ed by the caller */
	tmp_fd = mkstemp(tmp_file);
	if (tmp_fd < 0)
		return -1;

	unlink(tmp_file);
	myfd = tmp_fd;
	return tmp_fd;
} /* socket */

int
connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	if (sockfd < 0) {
		errno = EBADF;
		return -1;
	}

	/* we only want to mock UNIX sockets; return an error else */
	if ((addrlen != sizeof(struct sockaddr_un)) || (! addr)
			|| (! ((const struct sockaddr_un *)addr)->sun_path)) {
		errno = EAFNOSUPPORT;
		return -1;
	}
	return 0;
} /* connect */

FILE *
fdopen(int fd, const char *mode)
{
	io_cookie_t *cookie;

	if (fd < 0) {
		errno = EBADF;
		return NULL;
	}

	/* check also uses fdopen; in that case we need
	 * to use the original implementation */
	if (fd != myfd) {
		void *libc = dlopen_libc();
		FILE *(*orig_fdopen)(int, const char *);

		orig_fdopen = (FILE *(*)(int, const char *))dlsym(libc, "fdopen");

		if (! orig_fdopen)
			fail("INTERNAL ERROR: failed to load fdopen() from libc");

		return orig_fdopen(fd, mode);
	}

	cookie = calloc(sizeof(*cookie), 1);
	if (! cookie)
		return NULL;

	cookie->fd = fd;
	cookie->pos = 0;
	return fopencookie(cookie, mode, mock_io);
} /* fdopen */

/*
 * private variables
 */

static sdb_unixsock_client_t *client;

static void
setup(void)
{
	client = sdb_unixsock_client_create("unixsock_test_path");
	fail_unless(client != NULL,
			"sdb_unixsock_client_create() = NULL; "
			"expected unixsock client object");
} /* setup */

static void
teardown(void)
{
	sdb_unixsock_client_destroy(client);
	client = NULL;
} /* teardown */

static void
conn(void)
{
	int check;

	check = sdb_unixsock_client_connect(client);
	fail_unless(check == 0,
			"sdb_unixsock_client_connect() = %i; expected: 0", check);
} /* conn */

/*
 * tests
 */

START_TEST(test_sdb_unixsock_client_create)
{
	sdb_unixsock_client_t *c;
	const char *check;

	c = sdb_unixsock_client_create(NULL);
	fail_unless(c == NULL,
			"sdb_unixsock_client_create() = %p; expected: NULL", c);

	c = sdb_unixsock_client_create("unixsock_test_path");
	fail_unless(c != NULL,
			"sdb_unixsock_client_create() = NULL; "
			"expected unixsock client object");

	check = sdb_unixsock_client_path(c);
	fail_unless(check != NULL,
			"sdb_unixsock_client_create() did not store path name");
	fail_unless(!strcmp(check, "unixsock_test_path"),
			"sdb_unixsock_client_create() did not store correct path name; "
			"got: '%s'; expected: 'unixsock_test_path'", check);
	sdb_unixsock_client_destroy(c);
}
END_TEST

START_TEST(test_sdb_unixsock_client_connect)
{
	int check;

	check = sdb_unixsock_client_connect(client);
	fail_unless(check == 0,
			"sdb_unixsock_client_connect() = %i; expected: 0", check);
}
END_TEST

START_TEST(test_sdb_unixsock_client_send)
{
	size_t i;

	conn();

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		int check;

		mock_write_called = 0;
		check = sdb_unixsock_client_send(client, golden_data[i].data);
		/* client_send appends \r\n */
		fail_unless((size_t)check == golden_data[i].len + 2,
				"sdb_unixsock_client_send() = %i; expected: %zu",
				check, golden_data[i].len + 2);
		fail_unless(mock_write_called == 1,
				"sdb_unixsock_client_send() called mock_write %llu times; "
				"expected: 1", mock_write_called);
		fail_unless(last_write != NULL,
				"INTERNAL ERROR: mock_write did not record last write");
		fail_unless((last_write[check - 1] == '\n')
					&& (last_write[check - 2] == '\r'),
				"sdb_unixsock_client_send() did not append \\r\\n "
				"before sending; got: '%s'", last_write);
		fail_unless(!strncmp(last_write, golden_data[i].data,
					(size_t)check - 2),
				"sdb_unixsock_client_send() sent unexpected string '%s'; "
				"expected: '%s'", last_write, golden_data[i].data);
		free(last_write);
		last_write = NULL;
	}
}
END_TEST

START_TEST(test_sdb_unixsock_client_recv)
{
	size_t i;

	conn();

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		char *check;
		char buf[64];

		mock_read_called = 0;
		check = sdb_unixsock_client_recv(client, buf, sizeof(buf));
		fail_unless(check != NULL,
				"sdb_unixsock_client_recv() = NULL; expected: a string");
		fail_unless(check == buf,
				"sdb_unixsock_client_recv() did not return a pointer "
				"to the user-provided buffer");
		fail_unless(mock_read_called == 1,
				"sdb_unixsock_client_recv() called mock_read %llu times; "
				"expected: 1", mock_read_called);
		fail_unless(strlen(check) == golden_data[i].len,
				"sdb_unixsock_client_recv() returned string of length "
				"%zu ('%s'); expected: %zu",
				strlen(check), check, golden_data[i].len);
		fail_unless(check[golden_data[i].len] != '\n',
				"sdb_unixsock_client_recv() did not strip newline");
		fail_unless(!strcmp(check, golden_data[i].data),
				"sdb_unixsock_client_recv() = '%s'; expected: '%s'",
				check, golden_data[i].data);
	}
}
END_TEST

Suite *
util_unixsock_suite(void)
{
	Suite *s = suite_create("utils::unixsock");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_sdb_unixsock_client_create);
	tcase_add_test(tc, test_sdb_unixsock_client_connect);
	tcase_add_test(tc, test_sdb_unixsock_client_send);
	tcase_add_test(tc, test_sdb_unixsock_client_recv);
	suite_add_tcase(s, tc);

	return s;
} /* util_unixsock_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

