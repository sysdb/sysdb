/*
 * SysDB - t/frontend/parser_test.c
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

#include "frontend/connection.h"
#include "libsysdb_test.h"

#include <check.h>

/*
 * tests
 */

START_TEST(test_parse)
{
	struct {
		const char *query;
		int len;
		int expected;
	} golden_data[] = {
		/* empty commands */
		{ NULL,                 -1, -1 },
		{ "",                   -1,  0 },
		{ ";",                  -1,  0 },
		{ ";;",                 -1,  0 },

		/* valid commands */
		{ "LIST",               -1,  1 },
		{ "LIST;",              -1,  1 },

		/* comments */
		{ "/* some comment */", -1,  0 },
		{ "-- another comment", -1,  0 },

		/* syntax errors */
		{ "INVALID",            -1, -1 },
		{ "/* some incomplete", -1, -1 },
	};

	size_t i;
	sdb_llist_t *check;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		_Bool ok;

		check = sdb_fe_parse(golden_data[i].query, golden_data[i].len);
		if (golden_data[i].expected < 0)
			ok = check == 0;
		else
			ok = sdb_llist_len(check) == (size_t)golden_data[i].expected;

		fail_unless(ok, "sdb_fe_parse(%s) = %p (len: %zu); expected: %d",
				golden_data[i].query, check, sdb_llist_len(check),
				golden_data[i].expected);

		if (! check)
			continue;

		if ((! strcmp(golden_data[i].query, "LIST"))
				|| (! strcmp(golden_data[i].query, "LIST;"))) {
			sdb_object_t *obj = sdb_llist_get(check, 0);
			fail_unless(SDB_CONN_NODE(obj)->cmd == CONNECTION_LIST,
					"sdb_fe_parse(LIST)->cmd = %i; expected: %d "
					"(CONNECTION_LIST)", SDB_CONN_NODE(obj)->cmd,
					CONNECTION_LIST);
		}

		sdb_llist_destroy(check);
	}
}
END_TEST

Suite *
fe_parser_suite(void)
{
	Suite *s = suite_create("frontend::parser");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_test(tc, test_parse);
	suite_add_tcase(s, tc);

	return s;
} /* util_parser_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

