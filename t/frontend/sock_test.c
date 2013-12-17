/*
 * SysDB - t/frontend/sock_test.c
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

#include "frontend/sock.h"
#include "libsysdb_test.h"

#include <check.h>

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#include <pthread.h>

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
	char sock_addr[strlen("unix:") + L_tmpnam + 1];
	char *filename;

	int check;

	filename = tmpnam(tmp_file);
	fail_unless(filename != NULL,
			"INTERNAL ERROR: tmpnam() = NULL; expected: a string");

	sprintf(sock_addr, "unix:%s", tmp_file);
	check = sdb_fe_sock_add_listener(sock, sock_addr);
	fail_unless(check == 0,
			"sdb_fe_sock_add_listener(%s) = %i; expected: 0",
			sock_addr, check);
} /* conn */

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

	char tmp_file[L_tmpnam];
	int check;

	pthread_t thr;

	check = sdb_fe_sock_listen_and_serve(sock, &loop);
	fail_unless(check < 0,
			"sdb_fe_sock_listen_and_serve() = %i; "
			"expected: <0 (before adding listeners)", check);

	sock_listen(tmp_file);

	loop.do_loop = 1;
	check = pthread_create(&thr, /* attr = */ NULL, sock_handler, &loop);
	fail_unless(check == 0,
			"INTERNAL ERROR: pthread_create() = %i; expected: 0", check);

	/* wait for the socket to appear */
	while (access(tmp_file, F_OK)) {
		struct timespec ts = { 0, 100000000 };
		nanosleep(&ts, NULL);
	}

	loop.do_loop = 0;
	pthread_join(thr, NULL);

	unlink(tmp_file);
}
END_TEST

Suite *
fe_sock_suite(void)
{
	Suite *s = suite_create("frontend::sock");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_listen_and_serve);
	suite_add_tcase(s, tc);

	return s;
} /* util_unixsock_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

