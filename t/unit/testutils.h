/*
 * SysDB - t/unit/libsysdb_testutils.h
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

/*
 * Utility functions for test suites.
 */

#ifndef T_LIBSYSDB_UTILS_H
#define T_LIBSYSDB_UTILS_H 1

#include "sysdb.h"

#include <check.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_MAIN(name) \
	int main(void) \
	{ \
		SRunner *sr; \
		Suite *s; \
		int failed; \
		putenv("TZ=UTC"); \
		s = suite_create(name); \

#define TC_ADD_LOOP_TEST(tc, name) \
	tcase_add_loop_test((tc), test_ ## name, \
			0, SDB_STATIC_ARRAY_LEN(name ## _data))

#define ADD_TCASE(tc) suite_add_tcase(s, (tc))

#define TEST_MAIN_END \
		sr = srunner_create(s); \
		srunner_run_all(sr, CK_NORMAL); \
		failed = srunner_ntests_failed(sr); \
		srunner_free(sr); \
		return failed == 0 ? 0 : 1; \
	}

/*
 * sdb_regmatches:
 * Check if a regex matches a string.
 *
 * Returns:
 *  - 0 if the regex matches
 *  - a non-zero error value else (see regcomp(3) for details)
 */
int
sdb_regmatches(const char *regex, const char *string);

#endif /* T_LIBSYSDB_UTILS_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

