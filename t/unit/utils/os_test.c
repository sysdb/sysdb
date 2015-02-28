/*
 * SysDB - t/unit/utils/os_test.c
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
#endif

#include "utils/os.h"
#include "testutils.h"

#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

START_TEST(test_mkdir_remove)
{
	char tmpdir[] = "os_test_dir.XXXXXX";
	char testdir[1024];
	char testfile[1024];
	int check;

	mode_t mask;
	struct stat st;

	if (! mkdtemp(tmpdir)) {
		fail("INTERNAL ERROR: mkdtemp failed");
		return;
	}
	snprintf(testdir, sizeof(testdir), "%s/%s", tmpdir, "test1");

	mask = umask(0022);

	check = sdb_mkdir_all(testdir, 0777);
	fail_unless(check == 0,
			"sdb_mkdir_all(%s, %o) = %d (errno = %d); expected: 0",
			testdir, 0777, errno);
	check = stat(testdir, &st);
	fail_unless(check == 0,
			"stat(%s) = %d (errno = %d); expected: 0",
			testdir, check, errno);
	fail_unless((st.st_mode & 0777) == 0755,
			"sdb_mkdir_all(%s, %o) created permissons %o; expected: %o",
			testdir, 0777, st.st_mode, 0755);

	check = sdb_mkdir_all(testdir, 0777);
	fail_unless(check == 0,
			"sdb_mkdir_all(%s, %o) = %d (errno = %d) (second attempt); "
			"expected: 0", testdir, 0777, errno);

	/* populate the directory */
	snprintf(testfile, sizeof(testfile), "%s/%s", tmpdir, "testfile1");
	check = creat(testfile, 0666);
	fail_unless(check >= 0,
			"INTERNAL ERROR: creat(%s) = %d (errno = %d); expected: 0",
			testfile, check, errno);
	close(check);
	snprintf(testfile, sizeof(testfile), "%s/%s", testdir, "testfile2");
	check = creat(testfile, 0666);
	fail_unless(check >= 0,
			"INTERNAL ERROR: creat(%s) = %d (errno = %d); expected: 0",
			testfile, check, errno);
	close(check);

	check = sdb_remove_all(tmpdir);
	fail_unless(check == 0,
			"sdb_remove_all(%s) = %d (errno = %d); expected: 0",
			tmpdir, check, errno);
	fail_unless(access(tmpdir, F_OK),
			"sdb_remove_all(%s) did not remove the directory");

	umask(mask);
}
END_TEST

TEST_MAIN("utils::os")
{
	TCase *tc = tcase_create("core");
	tcase_add_test(tc, test_mkdir_remove);
	ADD_TCASE(tc);
}
TEST_MAIN_END

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

