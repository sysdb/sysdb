/*
 * SysDB - t/unit/frontend/parser_test.c
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
#include "core/store-private.h"
#include "core/object.h"
#include "libsysdb_test.h"

#include <check.h>
#include <limits.h>

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
		{ NULL,                  -1, -1, 0 },
		{ "",                    -1,  0, 0 },
		{ ";",                   -1,  0, 0 },
		{ ";;",                  -1,  0, 0 },

		/* valid commands */
		{ "FETCH host 'host'",   -1,  1, CONNECTION_FETCH  },
		{ "FETCH host 'host' FILTER "
		  "host = 'host'",       -1,  1, CONNECTION_FETCH  },

		{ "LIST hosts",          -1,  1, CONNECTION_LIST   },
		{ "LIST hosts -- foo",   -1,  1, CONNECTION_LIST   },
		{ "LIST hosts;",         -1,  1, CONNECTION_LIST   },
		{ "LIST hosts; INVALID", 11,  1, CONNECTION_LIST   },
		{ "LIST hosts FILTER "
		  "host = 'host'",       -1,  1, CONNECTION_LIST   },
		{ "LIST services",       -1,  1, CONNECTION_LIST   },
		{ "LIST services FILTER "
		  "service = 'svc'",     -1,  1, CONNECTION_LIST   },
		{ "LIST metrics",        -1,  1, CONNECTION_LIST   },
		{ "LIST metrics FILTER "
		  "metric = 'metric'",   -1,  1, CONNECTION_LIST   },

		{ "LOOKUP hosts",        -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "host = 'host'",       -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING NOT "
		  "host = 'host'",       -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "host =~ 'p' AND "
		  "service =~ 'p'",      -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING NOT "
		  "host =~ 'p' AND "
		  "service =~ 'p'",      -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "host =~ 'p' AND "
		  "service =~ 'p' OR "
		  "service =~ 'r'",      -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING NOT "
		  "host =~ 'p' AND "
		  "service =~ 'p' OR "
		  "service =~ 'r'",      -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "host =~ 'p' "
		  "FILTER .age > 1D",    -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "host =~ 'p' "
		  "FILTER .age > 1D AND "
		  ".interval < 240s" ,   -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "host =~ 'p' "
		  "FILTER NOT .age>1D",  -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "host =~ 'p' "
		  "FILTER .age>"
		  ".interval",           -1,  1, CONNECTION_LOOKUP },

		{ "TIMESERIES 'host'.'metric' "
		  "START 2014-01-01 "
		  "END 2014-12-31 "
		  "23:59:59",            -1,  1, CONNECTION_TIMESERIES },
		{ "TIMESERIES 'host'.'metric' "
		  "START 2014-02-02 "
		  "14:02",               -1,  1, CONNECTION_TIMESERIES },
		{ "TIMESERIES 'host'.'metric' "
		  "END 2014-02-02",      -1,  1, CONNECTION_TIMESERIES },
		{ "TIMESERIES "
		  "'host'.'metric'",     -1,  1, CONNECTION_TIMESERIES },

		/* string constants */
		{ "LOOKUP hosts MATCHING "
		  "host = ''''",         -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "host = '''foo'",      -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "host = 'f''oo'",      -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "host = 'foo'''",      -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "host = '''",          -1, -1, 0 },

		/* numeric constants */
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "1234",                -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] != "
		  "+234",                -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] < "
		  "-234",                -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] > "
		  "12.4",                -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] <= "
		  "12. + .3",            -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] <= "
		  "'f' || 'oo'",         -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] >= "
		  ".4",                  -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "+12e3",               -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "+12e-3",              -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "-12e+3",              -1,  1, CONNECTION_LOOKUP },

		/* date, time, interval constants */
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "1 Y 42D",             -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "1s 42D",              -1,  1, CONNECTION_LOOKUP },
		/*
		 * TODO: Something like 1Y42D should work as well but it doesn't since
		 * the scanner will tokenize it into {digit}{identifier} :-/
		 *
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "1Y42D",               -1,  1, CONNECTION_LOOKUP },
		 */

		/* NULL */
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] "
		  "IS NULL",             -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] "
		  "IS NOT NULL",         -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "NOT attribute['foo'] "
		  "IS NULL",             -1,  1, CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "host IS NULL",        -1, -1, 0 },
		{ "LOOKUP hosts MATCHING "
		  "service IS NULL",     -1, -1, 0 },

		/* invalid numeric constants */
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "+-12e+3",             -1, -1, 0 },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "-12e-+3",             -1, -1, 0 },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "e+3",                 -1, -1, 0 },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "3e",                  -1, -1, 0 },
		/* following SQL standard, we don't support hex numbers */
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "0x12",                -1, -1, 0 },

		/* invalid expressions */
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "1.23 + 'foo'",        -1, -1, 0 },

		/* comments */
		{ "/* some comment */",  -1,  0, 0 },
		{ "-- another comment",  -1,  0, 0 },

		/* syntax errors */
		{ "INVALID",             -1, -1, 0 },
		{ "FETCH host",          -1, -1, 0 },
		{ "FETCH 'host'",        -1, -1, 0 },
		{ "LIST hosts; INVALID", -1, -1, 0 },
		{ "/* some incomplete",  -1, -1, 0 },

		{ "LIST",                -1, -1, 0 },
		{ "LIST foo",            -1, -1, 0 },
		{ "LIST hosts MATCHING "
		  "host = 'host'",       -1, -1, 0 },
		{ "LIST foo FILTER "
		  "host = 'host'",       -1, -1, 0 },
		{ "FETCH host 'host' MATCHING "
		  "host = 'host'",       -1, -1, 0 },
		{ "FETCH foo 'host'",    -1, -1, 0 },
		{ "FETCH foo 'host' FILTER "
		  "host = 'host'",       -1, -1, 0 },

		{ "LOOKUP foo",          -1, -1, 0 },
		{ "LOOKUP foo MATCHING "
		  "host = 'host'",       -1, -1, 0 },
		{ "LOOKUP foo FILTER "
		  "host = 'host'",       -1, -1, 0 },
		{ "LOOKUP foo MATCHING "
		  "host = 'host' FILTER "
		  "host = 'host'",       -1, -1, 0 },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] <= "
		  "f || 'oo'",           -1, -1, 0 },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] <= "
		  "'f' || oo",           -1, -1, 0 },
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
		{ NULL,                           -1, -1 },
		{ "",                             -1, -1 },

		/* valid expressions */
		{ "host = 'localhost'",           -1,  MATCHER_NAME },
		{ "host != 'localhost'",          -1,  MATCHER_NOT },
		{ "host =~ 'host'",               -1,  MATCHER_NAME },
		{ "host !~ 'host'",               -1,  MATCHER_NOT },
		{ "host = 'localhost' -- foo",    -1,  MATCHER_NAME },
		{ "host = 'host' <garbage>",      13,  MATCHER_NAME },
		/* match hosts by service */
		{ "service = 'name'",             -1,  MATCHER_NAME },
		{ "service != 'name'",            -1,  MATCHER_NOT },
		{ "service =~ 'pattern'",         -1,  MATCHER_NAME },
		{ "service !~ 'pattern'",         -1,  MATCHER_NOT },
		/* match hosts by attribute */
		{ "attribute = 'name'",           -1,  MATCHER_NAME },
		{ "attribute != 'name'",          -1,  MATCHER_NOT },
		{ "attribute =~ 'pattern'",       -1,  MATCHER_NAME },
		{ "attribute !~ 'pattern'",       -1,  MATCHER_NOT },
		/* composite expressions */
		{ "host =~ 'pattern' AND "
		  "service =~ 'pattern'",         -1,  MATCHER_AND },
		{ "host =~ 'pattern' OR "
		  "service =~ 'pattern'",         -1,  MATCHER_OR },
		{ "NOT host = 'host'",            -1,  MATCHER_NOT },
		/* numeric expressions */
		{ "attribute['foo'] < 123",       -1,  MATCHER_LT },
		{ "attribute['foo'] <= 123",      -1,  MATCHER_LE },
		{ "attribute['foo'] = 123",       -1,  MATCHER_EQ },
		{ "attribute['foo'] >= 123",      -1,  MATCHER_GE },
		{ "attribute['foo'] > 123",       -1,  MATCHER_GT },
		/* datetime expressions */
		{ "attribute['foo'] = "
		  "2014-08-16",                   -1,  MATCHER_EQ },
		{ "attribute['foo'] = "
		  "17:23",                        -1,  MATCHER_EQ },
		{ "attribute['foo'] = "
		  "17:23:53",                     -1,  MATCHER_EQ },
		{ "attribute['foo'] = "
		  "17:23:53.123",                 -1,  MATCHER_EQ },
		{ "attribute['foo'] = "
		  "17:23:53.123456789",           -1,  MATCHER_EQ },
		{ "attribute['foo'] = "
		  "2014-08-16 17:23",             -1,  MATCHER_EQ },
		{ "attribute['foo'] = "
		  "2014-08-16 17:23:53",          -1,  MATCHER_EQ },
		/* NULL; while this is an implementation detail,
		 * IS NULL currently maps to an equality matcher */
		{ "attribute['foo'] IS NULL",     -1,  MATCHER_ISNULL },
		{ "attribute['foo'] IS NOT NULL", -1,  MATCHER_ISNNULL },

		/* object field matchers */
		{ ".last_update < 10s",           -1,  MATCHER_LT },
		{ ".AGE <= 1m",                   -1,  MATCHER_LE },
		{ ".interval = 10h",              -1,  MATCHER_EQ },
		{ ".Last_Update >= 24D",          -1,  MATCHER_GE },
		{ ".age > 1M",                    -1,  MATCHER_GT },
		{ ".age != 20Y",                  -1,  MATCHER_NOT },
		{ ".backend != 'be'",             -1,  MATCHER_NOT },
		{ ".age <= 2 * .interval",        -1,  MATCHER_LE },

		/* check operator precedence */
		{ "host = 'name' OR "
		  "service = 'name' AND "
		  "attribute = 'name' OR "
		  "attribute['foo'] = 'bar'",     -1,  MATCHER_OR },
		{ "host = 'name' AND "
		  "service = 'name' AND "
		  "attribute = 'name' OR "
		  "attribute['foo'] = 'bar'",     -1,  MATCHER_OR },
		{ "host = 'name' AND "
		  "service = 'name' OR "
		  "attribute = 'name' AND "
		  "attribute['foo'] = 'bar'",     -1,  MATCHER_OR },
		{ "(host = 'name' OR "
		  "service = 'name') AND "
		  "(attribute = 'name' OR "
		  "attribute['foo'] = 'bar')",    -1,  MATCHER_AND },
		{ "NOT host = 'name' OR "
		  "service = 'name'",             -1,  MATCHER_OR },
		{ "NOT host = 'name' OR "
		  "NOT service = 'name'",         -1,  MATCHER_OR },
		{ "NOT (host = 'name' OR "
		  "NOT service = 'name')",        -1,  MATCHER_NOT },

		/* syntax errors */
		{ "LIST",                         -1, -1 },
		{ "foo &^ bar",                   -1, -1 },
		{ ".invalid",                     -1, -1 },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_matcher_t *m;
		m = sdb_fe_parse_matcher(golden_data[i].expr, golden_data[i].len);

		if (golden_data[i].expected < 0) {
			fail_unless(m == NULL,
					"sdb_fe_parse_matcher(%s) = %p; expected: NULL",
					golden_data[i].expr, m);
			continue;
		}

		fail_unless(m != NULL, "sdb_fe_parse_matcher(%s) = NULL; "
				"expected: <matcher>", golden_data[i].expr);
		fail_unless(M(m)->type == golden_data[i].expected,
				"sdb_fe_parse_matcher(%s) returned matcher of type %d; "
				"expected: %d", golden_data[i].expr, M(m)->type,
				golden_data[i].expected);

		sdb_object_deref(SDB_OBJ(m));
	}
}
END_TEST

START_TEST(test_parse_expr)
{
	struct {
		const char *expr;
		int len;
		int expected;
	} golden_data[] = {
		/* empty expressions */
		{ NULL,                   -1, INT_MAX },
		{ "",                     -1, INT_MAX },

		/* constant expressions */
		{ "'localhost'",          -1, 0 },
		{ "123",                  -1, 0 },
		{ "2014-08-16",           -1, 0 },
		{ "17:23",                -1, 0 },
		{ "17:23:53",             -1, 0 },
		{ "17:23:53.123",         -1, 0 },
		{ "17:23:53.123456789",   -1, 0 },
		{ "2014-08-16 17:23",     -1, 0 },
		{ "2014-08-16 17:23:53",  -1, 0 },
		{ "10s",                  -1, 0 },
		{ "60m",                  -1, 0 },
		{ "10Y 24D 1h",           -1, 0 },

		{ "123 + 456",            -1, 0 },
		{ "'foo' || 'bar'",       -1, 0 },
		{ "456 - 123",            -1, 0 },
		{ "1.2 * 3.4",            -1, 0 },
		{ "1.2 / 3.4",            -1, 0 },
		{ "5 % 2",                -1, 0 },

		/* queryable fields */
		{ ".last_update",         -1, FIELD_VALUE },
		{ ".AGE",                 -1, FIELD_VALUE },
		{ ".interval",            -1, FIELD_VALUE },
		{ ".Last_Update",         -1, FIELD_VALUE },
		{ ".backend",             -1, FIELD_VALUE },

		/* attributes */
		{ "attribute['foo']",     -1, ATTR_VALUE },

		/* arithmetic expressions */
		{ ".age + .age",          -1, SDB_DATA_ADD },
		{ ".age - .age",          -1, SDB_DATA_SUB },
		{ ".age * .age",          -1, SDB_DATA_MUL },
		{ ".age / .age",          -1, SDB_DATA_DIV },
		{ ".age % .age",          -1, SDB_DATA_MOD },
		{ ".age || .age",         -1, SDB_DATA_CONCAT },

		/* operator precedence */
		{ ".age + .age * .age",   -1, SDB_DATA_ADD },
		{ ".age * .age + .age",   -1, SDB_DATA_ADD },
		{ ".age + .age - .age",   -1, SDB_DATA_SUB },
		{ ".age - .age + .age",   -1, SDB_DATA_ADD },
		{ "(.age + .age) * .age", -1, SDB_DATA_MUL },
		{ ".age + (.age * .age)", -1, SDB_DATA_ADD },

		/* syntax errors */
		{ "LIST",                 -1, INT_MAX },
		{ "foo &^ bar",           -1, INT_MAX },
		{ ".invalid",             -1, INT_MAX },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_expr_t *e;
		e = sdb_fe_parse_expr(golden_data[i].expr, golden_data[i].len);

		if (golden_data[i].expected == INT_MAX) {
			fail_unless(e == NULL,
					"sdb_fe_parse_expr(%s) = %p; expected: NULL",
					golden_data[i].expr, e);
			continue;
		}

		fail_unless(e != NULL, "sdb_fe_parse_expr(%s) = NULL; "
				"expected: <expr>", golden_data[i].expr);
		fail_unless(e->type == golden_data[i].expected,
				"sdb_fe_parse_expr(%s) returned expression of type %d; "
				"expected: %d", golden_data[i].expr, e->type,
				golden_data[i].expected);

		sdb_object_deref(SDB_OBJ(e));
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
	tcase_add_test(tc, test_parse_expr);
	suite_add_tcase(s, tc);

	return s;
} /* util_parser_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

