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
#include "frontend/parser.h"
#include "core/object.h"
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
		sdb_conn_state_t expected_cmd;
	} golden_data[] = {
		/* empty commands */
		{ NULL,                 -1, -1, 0 },
		{ "",                   -1,  0, 0 },
		{ ";",                  -1,  0, 0 },
		{ ";;",                 -1,  0, 0 },

		/* valid commands */
		{ "FETCH 'host'",       -1,  1, CONNECTION_FETCH  },
		{ "LIST",               -1,  1, CONNECTION_LIST   },
		{ "LIST -- comment",    -1,  1, CONNECTION_LIST   },
		{ "LIST;",              -1,  1, CONNECTION_LIST   },
		{ "LIST; INVALID",       5,  1, CONNECTION_LIST   },

		{ "LOOKUP hosts WHERE "
		  "host.name = 'host'", -1,  1, CONNECTION_LOOKUP },

		/* comments */
		{ "/* some comment */", -1,  0, 0 },
		{ "-- another comment", -1,  0, 0 },

		/* syntax errors */
		{ "INVALID",            -1, -1, 0 },
		{ "FETCH host",         -1, -1, 0 },
		{ "LIST; INVALID",       8, -1, 0 },
		{ "/* some incomplete", -1, -1, 0 },

		{ "LOOKUP hosts",       -1, -1, 0 },
		{ "LOOKUP foo WHERE "
		  "host.name = 'host'", -1, -1, 0 },
	};

	size_t i;
	sdb_llist_t *check;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_object_t *obj;
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

		if ((! golden_data[i].expected_cmd)
				|| (golden_data[i].expected <= 0)) {
			sdb_llist_destroy(check);
			continue;
		}

		obj = sdb_llist_get(check, 0);
		fail_unless(SDB_CONN_NODE(obj)->cmd == golden_data[i].expected_cmd,
				"sdb_fe_parse(%s)->cmd = %i; expected: %d",
				golden_data[i].query, SDB_CONN_NODE(obj)->cmd,
				golden_data[i].expected_cmd);
		sdb_object_deref(obj);
		sdb_llist_destroy(check);
	}
}
END_TEST

START_TEST(test_parse_matcher)
{
	struct {
		const char *expr;
		int len;
		int expected;
	} golden_data[] = {
		/* empty expressions */
		{ NULL,                             -1, -1 },
		{ "",                               -1, -1 },

		/* valid expressions */
		{ "host.name = 'localhost'",        -1,  0 },
		{ "host.name = 'localhost' -- foo", -1,  0 },
		{ "host.name = 'host' <garbage>",   18,  0 },

		/* syntax errors */
		{ "LIST",                           -1, -1 },
		{ "foo &^ bar",                     -1, -1 },
	};

	size_t i;
	sdb_store_matcher_t *m;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		_Bool ok;

		m = sdb_fe_parse_matcher(golden_data[i].expr, golden_data[i].len);
		if (golden_data[i].expected < 0)
			ok = m == NULL;
		else
			ok = m != NULL;

		fail_unless(ok, "sdb_fe_parse_matcher(%s) = %p; expected: %s",
				golden_data[i].expr, m, (golden_data[i].expected < 0)
				? "NULL" : "<matcher>");

		sdb_object_deref(SDB_OBJ(m));
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
	tcase_add_test(tc, test_parse_matcher);
	suite_add_tcase(s, tc);

	return s;
} /* util_parser_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

