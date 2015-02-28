/*
 * SysDB - t/unit/frontend/sock_test.c
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
#endif

#include "frontend/sock.h"
#include "testutils.h"

#include <check.h>

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

/*
 * private variables
 */

static sdb_fe_socket_t *sock;

static void
setup(void)
{
	sock = sdb_fe_sock_create();
	fail_unless(sock != NULL,
			"sdb_fe_sock_create() = NULL; expected frontend sock object");
} /* setup */

static void
teardown(void)
{
	sdb_fe_sock_destroy(sock);
	sock = NULL;
} /* teardown */

static void
sock_listen(char *tmp_file)
{
	char sock_addr[strlen("unix:") + strlen(tmp_file) + 1];
	int check;

	sprintf(sock_addr, "unix:%s", tmp_file);
	check = sdb_fe_sock_add_listener(sock, sock_addr, NULL);
	fail_unless(check == 0,
			"sdb_fe_sock_add_listener(%s) = %i; expected: 0",
			sock_addr, check);
} /* sock_listen */

/*
 * parallel testing
 */

static void *
sock_handler(void *data)
{
	sdb_fe_loop_t *loop = data;
	int check;

	check = sdb_fe_sock_listen_and_serve(sock, loop);
	fail_unless(check == 0,
			"sdb_fe_sock_listen_and_serve() = %i; "
			"expected: 0 (after adding listener)", check);
	return NULL;
} /* sock_handler */

/*
 * tests
 */

START_TEST(test_listen_and_serve)
{
	sdb_fe_loop_t loop = SDB_FE_LOOP_INIT;

	char tmp_file[] = "sock_test_socket.XXXXXX";
	int check;

	pthread_t thr;

	int fd, sock_fd;
	struct sockaddr_un sa;

	check = sdb_fe_sock_listen_and_serve(sock, &loop);
	fail_unless(check < 0,
			"sdb_fe_sock_listen_and_serve() = %i; "
			"expected: <0 (before adding listeners)", check);

	fd = mkstemp(tmp_file);
	unlink(tmp_file);
	close(fd);
	sock_listen(tmp_file);

	loop.do_loop = 1;
	check = pthread_create(&thr, /* attr = */ NULL, sock_handler, &loop);
	fail_unless(check == 0,
			"INTERNAL ERROR: pthread_create() = %i; expected: 0", check);

	sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	fail_unless(sock_fd >= 0,
			"INTERNAL ERROR: socket() = %d; expected: >= 0", sock_fd);

	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, tmp_file, sizeof(sa.sun_path));

	/* wait for socket to become available */
	errno = ECONNREFUSED;
	while (errno == ECONNREFUSED) {
		check = connect(sock_fd, (struct sockaddr *)&sa, sizeof(sa));
		if (! check)
			break;

		fail_unless(errno == ECONNREFUSED,
				"INTERNAL ERROR: connect() = %d [errno=%d]; expected: 0",
				check, errno);
	}

	close(sock_fd);

	loop.do_loop = 0;
	pthread_join(thr, NULL);

	fail_unless(access(tmp_file, F_OK),
			"sdb_fe_sock_listen_and_serve() did not clean up "
			"socket %s", tmp_file);

	/* should do nothing and not report errors */
	check = sdb_fe_sock_listen_and_serve(sock, &loop);
	fail_unless(check == 0,
			"sdb_fe_sock_listen_and_serve() = %i; "
			"expected: <0 (do_loop == 0)", check);
	fail_unless(access(tmp_file, F_OK),
			"sdb_fe_sock_listen_and_serve() recreated socket "
			"(do_loop == 0)");
}
END_TEST

TEST_MAIN("frontend::sock")
{
	TCase *tc = tcase_create("core");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_listen_and_serve);
	ADD_TCASE(tc);
}
TEST_MAIN_END

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

