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
		{ "FETCH host 'host'",   -1,  1, SDB_CONNECTION_FETCH  },
		{ "FETCH host 'host' FILTER "
		  "age > 60s",           -1,  1, SDB_CONNECTION_FETCH  },
		{ "FETCH service "
		  "'host'.'service'",    -1,  1, SDB_CONNECTION_FETCH  },
		{ "FETCH metric "
		  "'host'.'metric'",     -1,  1, SDB_CONNECTION_FETCH  },

		{ "LIST hosts",          -1,  1, SDB_CONNECTION_LIST   },
		{ "LIST hosts -- foo",   -1,  1, SDB_CONNECTION_LIST   },
		{ "LIST hosts;",         -1,  1, SDB_CONNECTION_LIST   },
		{ "LIST hosts; INVALID", 11,  1, SDB_CONNECTION_LIST   },
		{ "LIST hosts FILTER "
		  "age > 60s",           -1,  1, SDB_CONNECTION_LIST   },
		{ "LIST services",       -1,  1, SDB_CONNECTION_LIST   },
		{ "LIST services FILTER "
		  "age > 60s",           -1,  1, SDB_CONNECTION_LIST   },
		{ "LIST metrics",        -1,  1, SDB_CONNECTION_LIST   },
		{ "LIST metrics FILTER "
		  "age > 60s",           -1,  1, SDB_CONNECTION_LIST   },

		{ "LOOKUP hosts",        -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "name = 'host'",       -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING NOT "
		  "name = 'host'",       -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "name =~ 'p' AND "
		  "ANY service =~ 'p'",  -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING NOT "
		  "name =~ 'p' AND "
		  "ANY service =~ 'p'",  -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "name =~ 'p' AND "
		  "ANY service =~ 'p' OR "
		  "ANY service =~ 'r'",  -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING NOT "
		  "name =~ 'p' AND "
		  "ANY service =~ 'p' OR "
		  "ANY service =~ 'r'",  -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "name =~ 'p' "
		  "FILTER age > 1D",    -1,   1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "name =~ 'p' "
		  "FILTER age > 1D AND "
		  "interval < 240s" ,   -1,   1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "name =~ 'p' "
		  "FILTER NOT age>1D",  -1,   1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "name =~ 'p' "
		  "FILTER age>"
		  "interval",           -1,   1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "host.name =~ 'p'",   -1,   1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP services",    -1,   1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP services MATCHING ANY "
		  "attribute =~ 'a'",   -1,   1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP services MATCHING "
		  "host.name = 'p'",    -1,   1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP services MATCHING "
		  "service.name = 'p'", -1,   1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP metrics",     -1,   1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP metrics MATCHING ANY "
		  "attribute =~ 'a'",   -1,   1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP metrics MATCHING "
		  "host.name = 'p'",    -1,   1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP metrics MATCHING "
		  "metric.name = 'p'",  -1,   1, SDB_CONNECTION_LOOKUP },

		{ "TIMESERIES 'host'.'metric' "
		  "START 2014-01-01 "
		  "END 2014-12-31 "
		  "23:59:59",            -1,  1, SDB_CONNECTION_TIMESERIES },
		{ "TIMESERIES 'host'.'metric' "
		  "START 2014-02-02 "
		  "14:02",               -1,  1, SDB_CONNECTION_TIMESERIES },
		{ "TIMESERIES 'host'.'metric' "
		  "END 2014-02-02",      -1,  1, SDB_CONNECTION_TIMESERIES },
		{ "TIMESERIES "
		  "'host'.'metric'",     -1,  1, SDB_CONNECTION_TIMESERIES },

		/* string constants */
		{ "LOOKUP hosts MATCHING "
		  "name = ''''",         -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "name = '''foo'",      -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "name = 'f''oo'",      -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "name = 'foo'''",      -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "name = '''",          -1, -1, 0 },

		/* numeric constants */
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "1234",                -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] != "
		  "+234",                -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] < "
		  "-234",                -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] > "
		  "12.4",                -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] <= "
		  "12. + .3",            -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] <= "
		  "'f' || 'oo'",         -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] >= "
		  ".4",                  -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "+12e3",               -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "+12e-3",              -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "-12e+3",              -1,  1, SDB_CONNECTION_LOOKUP },

		/* date, time, interval constants */
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "1 Y 42D",             -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "1s 42D",              -1,  1, SDB_CONNECTION_LOOKUP },
		/*
		 * TODO: Something like 1Y42D should work as well but it doesn't since
		 * the scanner will tokenize it into {digit}{identifier} :-/
		 *
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] = "
		  "1Y42D",               -1,  1, SDB_CONNECTION_LOOKUP },
		 */

		/* array constants */
		{ "LOOKUP hosts MATCHING "
		  "backend = ['foo']",   -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "backend = ['a','b']", -1,  1, SDB_CONNECTION_LOOKUP },

		/* NULL */
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] "
		  "IS NULL",             -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] "
		  "IS NOT NULL",         -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "NOT attribute['foo'] "
		  "IS NULL",             -1,  1, SDB_CONNECTION_LOOKUP },
		{ "LOOKUP hosts MATCHING "
		  "ANY service IS NULL", -1, -1, 0 },

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
		{ "LOOKUP hosts MATCHING "
		  "attr['foo'] = 1.23",  -1, -1, 0 },
		{ "LOOKUP hosts MATCHING "
		  "attr['foo'] IS NULL", -1, -1, 0 },

		/* comments */
		{ "/* some comment */",  -1,  0, 0 },
		{ "-- another comment",  -1,  0, 0 },

		/* syntax errors */
		{ "INVALID",             -1, -1, 0 },
		{ "FETCH host",          -1, -1, 0 },
		{ "FETCH 'host'",        -1, -1, 0 },
		{ "LIST hosts; INVALID", -1, -1, 0 },
		{ "/* some incomplete",  -1, -1, 0 },

		/* invalid commands */
		{ "LIST",                -1, -1, 0 },
		{ "LIST foo",            -1, -1, 0 },
		{ "LIST hosts MATCHING "
		  "name = 'host'",       -1, -1, 0 },
		{ "LIST foo FILTER "
		  "age > 60s",           -1, -1, 0 },
		{ "FETCH host 'host' MATCHING "
		  "name = 'host'",       -1, -1, 0 },
		{ "FETCH service 'host'",-1, -1, 0 },
		{ "FETCH metric 'host'", -1, -1, 0 },
		{ "FETCH host "
		  "'host'.'localhost'",  -1, -1, 0 },
		{ "FETCH foo 'host'",    -1, -1, 0 },
		{ "FETCH foo 'host' FILTER "
		  "age > 60s",           -1, -1, 0 },

		{ "LOOKUP foo",          -1, -1, 0 },
		{ "LOOKUP foo MATCHING "
		  "name = 'host'",       -1, -1, 0 },
		{ "LOOKUP foo FILTER "
		  "age > 60s",           -1, -1, 0 },
		{ "LOOKUP foo MATCHING "
		  "name = 'host' FILTER "
		  "age > 60s",           -1, -1, 0 },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] <= "
		  "f || 'oo'",           -1, -1, 0 },
		{ "LOOKUP hosts MATCHING "
		  "attribute['foo'] <= "
		  "'f' || oo",           -1, -1, 0 },
		{ "LOOKUP hosts MATCHING "
		  "ANY host = 'host'",   -1, -1, 0 },
		{ "LOOKUP hosts MATCHING "
		  "service.name = 's'",  -1, -1, 0 },
		{ "LOOKUP services MATCHING "
		  "ANY host = 'host'",   -1, -1, 0 },
		{ "LOOKUP services MATCHING "
		  "ANY service = 'svc'", -1, -1, 0 },
		{ "LOOKUP services MATCHING "
		  "ANY metric = 'm'",    -1, -1, 0 },
		{ "LOOKUP services MATCHING "
		  "metric.name = 'm'",   -1, -1, 0 },
		{ "LOOKUP metrics MATCHING "
		  "ANY host = 'host'",   -1, -1, 0 },
		{ "LOOKUP metrics MATCHING "
		  "ANY service = 'svc'", -1, -1, 0 },
		{ "LOOKUP metrics MATCHING "
		  "ANY metric = 'm'",    -1, -1, 0 },
		{ "LOOKUP metrics MATCHING "
		  "service.name = 'm'",   -1, -1, 0 },
	};

	sdb_strbuf_t *errbuf = sdb_strbuf_create(64);
	sdb_llist_t *check;

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_object_t *obj;
		_Bool ok;

		check = sdb_fe_parse(golden_data[i].query,
				golden_data[i].len, errbuf);
		if (golden_data[i].expected < 0)
			ok = check == 0;
		else
			ok = sdb_llist_len(check) == (size_t)golden_data[i].expected;

		fail_unless(ok, "sdb_fe_parse(%s) = %p (len: %zu); expected: %d "
				"(parser error: %s)", golden_data[i].query, check,
				sdb_llist_len(check), golden_data[i].expected,
				sdb_strbuf_string(errbuf));

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

	sdb_strbuf_destroy(errbuf);
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

		/* match hosts by name */
		{ "name < 'localhost'",           -1,  MATCHER_LT },
		{ "name <= 'localhost'",          -1,  MATCHER_LE },
		{ "name = 'localhost'",           -1,  MATCHER_EQ },
		{ "name != 'localhost'",          -1,  MATCHER_NE },
		{ "name >= 'localhost'",          -1,  MATCHER_GE },
		{ "name > 'localhost'",           -1,  MATCHER_GT },
		{ "name =~ 'host'",               -1,  MATCHER_REGEX },
		{ "name !~ 'host'",               -1,  MATCHER_NREGEX },
		{ "name = 'localhost' -- foo",    -1,  MATCHER_EQ },
		{ "name = 'host' <garbage>",      13,  MATCHER_EQ },
		{ "name &^ 'localhost'",          -1,  -1 },
		/* match by backend */
		{ "ANY backend < 'be'",           -1,  MATCHER_ANY },
		{ "ANY backend <= 'be'",          -1,  MATCHER_ANY },
		{ "ANY backend = 'be'",           -1,  MATCHER_ANY },
		{ "ANY backend != 'be'",          -1,  MATCHER_ANY },
		{ "ANY backend >= 'be'",          -1,  MATCHER_ANY },
		{ "ANY backend > 'be'",           -1,  MATCHER_ANY },
		{ "ALL backend < 'be'",           -1,  MATCHER_ALL },
		{ "ALL backend <= 'be'",          -1,  MATCHER_ALL },
		{ "ALL backend = 'be'",           -1,  MATCHER_ALL },
		{ "ALL backend != 'be'",          -1,  MATCHER_ALL },
		{ "ALL backend >= 'be'",          -1,  MATCHER_ALL },
		{ "ALL backend > 'be'",           -1,  MATCHER_ALL },
		{ "ANY backend &^ 'be'",          -1,  -1 },
		/* match hosts by service */
		{ "ANY service < 'name'",         -1,  MATCHER_ANY },
		{ "ANY service <= 'name'",        -1,  MATCHER_ANY },
		{ "ANY service = 'name'",         -1,  MATCHER_ANY },
		{ "ANY service != 'name'",        -1,  MATCHER_ANY },
		{ "ANY service >= 'name'",        -1,  MATCHER_ANY },
		{ "ANY service > 'name'",         -1,  MATCHER_ANY },
		{ "ANY service =~ 'pattern'",     -1,  MATCHER_ANY },
		{ "ANY service !~ 'pattern'",     -1,  MATCHER_ANY },
		{ "ANY service &^ 'name'",        -1,  -1 },
		{ "ALL service < 'name'",         -1,  MATCHER_ALL },
		{ "ALL service <= 'name'",        -1,  MATCHER_ALL },
		{ "ALL service = 'name'",         -1,  MATCHER_ALL },
		{ "ALL service != 'name'",        -1,  MATCHER_ALL },
		{ "ALL service >= 'name'",        -1,  MATCHER_ALL },
		{ "ALL service > 'name'",         -1,  MATCHER_ALL },
		{ "ALL service =~ 'pattern'",     -1,  MATCHER_ALL },
		{ "ALL service !~ 'pattern'",     -1,  MATCHER_ALL },
		{ "ALL service &^ 'name'",        -1,  -1 },
		/* match hosts by metric */
		{ "ANY metric < 'name'",          -1,  MATCHER_ANY },
		{ "ANY metric <= 'name'",         -1,  MATCHER_ANY },
		{ "ANY metric = 'name'",          -1,  MATCHER_ANY },
		{ "ANY metric != 'name'",         -1,  MATCHER_ANY },
		{ "ANY metric >= 'name'",         -1,  MATCHER_ANY },
		{ "ANY metric > 'name'",          -1,  MATCHER_ANY },
		{ "ANY metric =~ 'pattern'",      -1,  MATCHER_ANY },
		{ "ANY metric !~ 'pattern'",      -1,  MATCHER_ANY },
		{ "ANY metric &^ 'pattern'",      -1,  -1 },
		{ "ALL metric < 'name'",          -1,  MATCHER_ALL },
		{ "ALL metric <= 'name'",         -1,  MATCHER_ALL },
		{ "ALL metric = 'name'",          -1,  MATCHER_ALL },
		{ "ALL metric != 'name'",         -1,  MATCHER_ALL },
		{ "ALL metric >= 'name'",         -1,  MATCHER_ALL },
		{ "ALL metric > 'name'",          -1,  MATCHER_ALL },
		{ "ALL metric =~ 'pattern'",      -1,  MATCHER_ALL },
		{ "ALL metric !~ 'pattern'",      -1,  MATCHER_ALL },
		{ "ALL metric &^ 'pattern'",      -1,  -1 },
		/* match hosts by attribute */
		{ "ANY attribute < 'name'",       -1,  MATCHER_ANY },
		{ "ANY attribute <= 'name'",      -1,  MATCHER_ANY },
		{ "ANY attribute = 'name'",       -1,  MATCHER_ANY },
		{ "ANY attribute != 'name'",      -1,  MATCHER_ANY },
		{ "ANY attribute >= 'name'",      -1,  MATCHER_ANY },
		{ "ANY attribute > 'name'",       -1,  MATCHER_ANY },
		{ "ANY attribute =~ 'pattern'",   -1,  MATCHER_ANY },
		{ "ANY attribute !~ 'pattern'",   -1,  MATCHER_ANY },
		{ "ANY attribute &^ 'pattern'",   -1,  -1 },
		{ "ALL attribute < 'name'",       -1,  MATCHER_ALL },
		{ "ALL attribute <= 'name'",      -1,  MATCHER_ALL },
		{ "ALL attribute = 'name'",       -1,  MATCHER_ALL },
		{ "ALL attribute != 'name'",      -1,  MATCHER_ALL },
		{ "ALL attribute >= 'name'",      -1,  MATCHER_ALL },
		{ "ALL attribute > 'name'",       -1,  MATCHER_ALL },
		{ "ALL attribute =~ 'pattern'",   -1,  MATCHER_ALL },
		{ "ALL attribute !~ 'pattern'",   -1,  MATCHER_ALL },
		{ "ALL attribute &^ 'pattern'",   -1,  -1 },
		/* composite expressions */
		{ "name =~ 'pattern' AND "
		  "ANY service =~ 'pattern'",     -1,  MATCHER_AND },
		{ "name =~ 'pattern' OR "
		  "ANY service =~ 'pattern'",     -1,  MATCHER_OR },
		{ "NOT name = 'host'",            -1,  MATCHER_NOT },
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
		/* array expressions */
		{ "backend < ['a']",              -1,  MATCHER_LT },
		{ "backend <= ['a']",             -1,  MATCHER_LE },
		{ "backend = ['a']",              -1,  MATCHER_EQ },
		{ "backend != ['a']",             -1,  MATCHER_NE },
		{ "backend >= ['a']",             -1,  MATCHER_GE },
		{ "backend > ['a']",              -1,  MATCHER_GT },
		{ "backend &^ ['a']",             -1,  -1 },

		/* object field matchers */
		{ "name < 'a'",                   -1,  MATCHER_LT },
		{ "name <= 'a'",                  -1,  MATCHER_LE },
		{ "name = 'a'",                   -1,  MATCHER_EQ },
		{ "name != 'a'",                  -1,  MATCHER_NE },
		{ "name >= 'a'",                  -1,  MATCHER_GE },
		{ "name > 'a'",                   -1,  MATCHER_GT },
		{ "last_update < 2014-10-01",     -1,  MATCHER_LT },
		{ "last_update <= 2014-10-01",    -1,  MATCHER_LE },
		{ "last_update = 2014-10-01",     -1,  MATCHER_EQ },
		{ "last_update != 2014-10-01",    -1,  MATCHER_NE },
		{ "last_update >= 2014-10-01",    -1,  MATCHER_GE },
		{ "last_update > 2014-10-01",     -1,  MATCHER_GT },
		{ "Last_Update >= 24D",           -1,  MATCHER_GE },
		{ "age < 20s",                    -1,  MATCHER_LT },
		{ "age <= 20s",                   -1,  MATCHER_LE },
		{ "age = 20s",                    -1,  MATCHER_EQ },
		{ "age != 20s",                   -1,  MATCHER_NE },
		{ "age >= 20s",                   -1,  MATCHER_GE },
		{ "age > 20s",                    -1,  MATCHER_GT },
		{ "AGE <= 1m",                    -1,  MATCHER_LE },
		{ "age > 1M",                     -1,  MATCHER_GT },
		{ "age != 20Y",                   -1,  MATCHER_NE },
		{ "age <= 2 * interval",          -1,  MATCHER_LE },
		{ "interval < 20s",               -1,  MATCHER_LT },
		{ "interval <= 20s",              -1,  MATCHER_LE },
		{ "interval = 20s",               -1,  MATCHER_EQ },
		{ "interval != 20s",              -1,  MATCHER_NE },
		{ "interval >= 20s",              -1,  MATCHER_GE },
		{ "interval > 20s",               -1,  MATCHER_GT },
		{ "'be' IN backend",              -1,  MATCHER_IN },

		/* check operator precedence */
		{ "name = 'name' OR "
		  "ANY service = 'name' AND "
		  "ANY attribute = 'name' OR "
		  "attribute['foo'] = 'bar'",     -1,  MATCHER_OR },
		{ "name = 'name' AND "
		  "ANY service = 'name' AND "
		  "ANY attribute = 'name' OR "
		  "attribute['foo'] = 'bar'",     -1,  MATCHER_OR },
		{ "name = 'name' AND "
		  "ANY service = 'name' OR "
		  "ANY attribute = 'name' AND "
		  "attribute['foo'] = 'bar'",     -1,  MATCHER_OR },
		{ "(name = 'name' OR "
		  "ANY service = 'name') AND "
		  "(ANY attribute = 'name' OR "
		  "attribute['foo'] = 'bar')",    -1,  MATCHER_AND },
		{ "NOT name = 'name' OR "
		  "ANY service = 'name'",         -1,  MATCHER_OR },
		{ "NOT name = 'name' OR "
		  "NOT ANY service = 'name'",     -1,  MATCHER_OR },
		{ "NOT (name = 'name' OR "
		  "NOT ANY service = 'name')",    -1,  MATCHER_NOT },

		/* syntax errors */
		{ "LIST",                         -1, -1 },
		{ "foo &^ bar",                   -1, -1 },
		{ "invalid",                      -1, -1 },
	};

	sdb_strbuf_t *errbuf = sdb_strbuf_create(64);
	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_matcher_t *m;
		m = sdb_fe_parse_matcher(golden_data[i].expr,
				golden_data[i].len, errbuf);

		if (golden_data[i].expected < 0) {
			fail_unless(m == NULL,
					"sdb_fe_parse_matcher(%s) = %p; expected: NULL",
					golden_data[i].expr, m);
			continue;
		}

		fail_unless(m != NULL, "sdb_fe_parse_matcher(%s) = NULL; "
				"expected: <matcher> (parser error: %s)",
				golden_data[i].expr, sdb_strbuf_string(errbuf));
		fail_unless(M(m)->type == golden_data[i].expected,
				"sdb_fe_parse_matcher(%s) returned matcher of type %d; "
				"expected: %d", golden_data[i].expr, M(m)->type,
				golden_data[i].expected);

		sdb_object_deref(SDB_OBJ(m));
	}

	sdb_strbuf_destroy(errbuf);
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
		{ "last_update",          -1, FIELD_VALUE },
		{ "AGE",                  -1, FIELD_VALUE },
		{ "interval",             -1, FIELD_VALUE },
		{ "Last_Update",          -1, FIELD_VALUE },
		{ "backend",              -1, FIELD_VALUE },

		/* attributes */
		{ "attribute['foo']",     -1, ATTR_VALUE },

		/* arithmetic expressions */
		{ "age + age",            -1, SDB_DATA_ADD },
		{ "age - age",            -1, SDB_DATA_SUB },
		{ "age * age",            -1, SDB_DATA_MUL },
		{ "age / age",            -1, SDB_DATA_DIV },
		{ "age % age",            -1, SDB_DATA_MOD },
		{ "age || age",           -1, SDB_DATA_CONCAT },

		/* operator precedence */
		{ "age + age * age",      -1, SDB_DATA_ADD },
		{ "age * age + age",      -1, SDB_DATA_ADD },
		{ "age + age - age",      -1, SDB_DATA_SUB },
		{ "age - age + age",      -1, SDB_DATA_ADD },
		{ "(age + age) * age",    -1, SDB_DATA_MUL },
		{ "age + (age * age)",    -1, SDB_DATA_ADD },

		/* syntax errors */
		{ "LIST",                 -1, INT_MAX },
		{ "foo &^ bar",           -1, INT_MAX },
		{ "invalid",              -1, INT_MAX },
	};

	sdb_strbuf_t *errbuf = sdb_strbuf_create(64);
	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_expr_t *e;
		e = sdb_fe_parse_expr(golden_data[i].expr,
				golden_data[i].len, errbuf);

		if (golden_data[i].expected == INT_MAX) {
			fail_unless(e == NULL,
					"sdb_fe_parse_expr(%s) = %p; expected: NULL",
					golden_data[i].expr, e);
			continue;
		}

		fail_unless(e != NULL, "sdb_fe_parse_expr(%s) = NULL; "
				"expected: <expr> (parser error: %s)",
				golden_data[i].expr, sdb_strbuf_string(errbuf));
		fail_unless(e->type == golden_data[i].expected,
				"sdb_fe_parse_expr(%s) returned expression of type %d; "
				"expected: %d", golden_data[i].expr, e->type,
				golden_data[i].expected);

		sdb_object_deref(SDB_OBJ(e));
	}

	sdb_strbuf_destroy(errbuf);
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

