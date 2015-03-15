/*
 * SysDB - t/unit/frontend/parser_test.c
 * Copyright (C) 2013-2015 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include "frontend/connection.h"
#include "frontend/parser.h"
#include "core/store-private.h"
#include "core/object.h"
#include "testutils.h"

#include <check.h>
#include <limits.h>

/*
 * tests
 */

struct {
	const char *query;
	int len;
	int expected;
	sdb_conn_state_t expected_cmd;
} parse_data[] = {
	/* empty commands */
	{ NULL,                  -1, -1, 0 },
	{ "",                    -1,  0, 0 },
	{ ";",                   -1,  0, 0 },
	{ ";;",                  -1,  0, 0 },

	/* FETCH commands */
	{ "FETCH host 'host'",   -1,  1, SDB_CONNECTION_FETCH  },
	{ "FETCH host 'host' FILTER "
	  "age > 60s",           -1,  1, SDB_CONNECTION_FETCH  },
	{ "FETCH service "
	  "'host'.'service'",    -1,  1, SDB_CONNECTION_FETCH  },
	{ "FETCH metric "
	  "'host'.'metric'",     -1,  1, SDB_CONNECTION_FETCH  },

	/* LIST commands */
	{ "LIST hosts",            -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST hosts -- foo",     -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST hosts;",           -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST hosts; INVALID",   11,  1, SDB_CONNECTION_LIST   },
	{ "LIST hosts FILTER "
	  "age > 60s",             -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST services",         -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST services FILTER "
	  "age > 60s",             -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST metrics",          -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST metrics FILTER "
	  "age > 60s",             -1,  1, SDB_CONNECTION_LIST   },
	/* field access */
	{ "LIST hosts FILTER "
	  "name = 'a'",            -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST hosts FILTER "
	  "last_update > 1s",      -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST hosts FILTER "
	  "age > 120s",            -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST hosts FILTER "
	  "interval > 10s",        -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST hosts FILTER "
	  "backend = ['b']",       -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST hosts FILTER "
	  "value = 'a'",           -1, -1, 0 },
	{ "LIST hosts FILTER ANY "
	  "attribute.value = 'a'", -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST services FILTER "
	  "name = 'a'",            -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST services FILTER "
	  "last_update > 1s",      -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST services FILTER "
	  "age > 120s",            -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST services FILTER "
	  "interval > 10s",        -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST services FILTER "
	  "backend = ['b']",       -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST services FILTER "
	  "value = 'a'",           -1, -1, 0 },
	{ "LIST services FILTER ANY "
	  "attribute.value = 'a'", -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST metrics FILTER "
	  "name = 'a'",            -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST metrics FILTER "
	  "last_update > 1s",      -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST metrics FILTER "
	  "age > 120s",            -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST metrics FILTER "
	  "interval > 10s",        -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST metrics FILTER "
	  "backend = ['b']",       -1,  1, SDB_CONNECTION_LIST   },
	{ "LIST metrics FILTER "
	  "value = 'a'",           -1, -1, 0 },
	{ "LIST metrics FILTER ANY "
	  "attribute.value = 'a'", -1,  1, SDB_CONNECTION_LIST   },

	/* LOOKUP commands */
	{ "LOOKUP hosts",        -1,  1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "name = 'host'",       -1,  1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING NOT "
	  "name = 'host'",       -1,  1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "name =~ 'p' AND "
	  "ANY service.name =~ 'p'", -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING NOT "
	  "name =~ 'p' AND "
	  "ANY service.name =~ 'p'", -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "name =~ 'p' AND "
	  "ANY service.name =~ 'p' OR "
	  "ANY service.name =~ 'r'", -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING NOT "
	  "name =~ 'p' AND "
	  "ANY service.name =~ 'p' OR "
	  "ANY service.name =~ 'r'", -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "name =~ 'p' "
	  "FILTER age > 1D",         -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "name =~ 'p' "
	  "FILTER age > 1D AND "
	  "interval < 240s" ,        -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "name =~ 'p' "
	  "FILTER NOT age>1D",       -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "name =~ 'p' "
	  "FILTER age>"
	  "interval",                -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "host.name =~ 'p'",        -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP services",         -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP services MATCHING ANY "
	  "attribute.name =~ 'a'",   -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP services MATCHING "
	  "host.name = 'p'",         -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP services MATCHING "
	  "service.name = 'p'",      -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP metrics",          -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP metrics MATCHING ANY "
	  "attribute.name =~ 'a'",   -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP metrics MATCHING "
	  "host.name = 'p'",         -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP metrics MATCHING "
	  "metric.name = 'p'",       -1,   1, SDB_CONNECTION_LOOKUP },

	/* TIMESERIES commands */
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

	/* STORE commands */
	{ "STORE host 'host'",   -1,  1, SDB_CONNECTION_STORE_HOST },
	{ "STORE host 'host' "
	  "LAST UPDATE "
	  "2015-02-01",          -1,  1, SDB_CONNECTION_STORE_HOST },
	{ "STORE host attribute "
	  "'host'.'key' 123",    -1,  1, SDB_CONNECTION_STORE_ATTRIBUTE },
	{ "STORE host attribute "
	  "'host'.'key' 123 "
	  "LAST UPDATE "
	  "2015-02-01",          -1,  1, SDB_CONNECTION_STORE_ATTRIBUTE },
	{ "STORE service "
	  "'host'.'svc'",        -1,  1, SDB_CONNECTION_STORE_SERVICE },
	{ "STORE service "
	  "'host'.'svc' "
	  "LAST UPDATE "
	  "2015-02-01",          -1,  1, SDB_CONNECTION_STORE_SERVICE },
	{ "STORE service attribute "
	  "'host'.'svc'.'key' "
	  "123",                 -1,  1, SDB_CONNECTION_STORE_ATTRIBUTE },
	{ "STORE service attribute "
	  "'host'.'svc'.'key' "
	  "123 "
	  "LAST UPDATE "
	  "2015-02-01",          -1,  1, SDB_CONNECTION_STORE_ATTRIBUTE },
	{ "STORE metric "
	  "'host'.'metric'",     -1,  1, SDB_CONNECTION_STORE_METRIC },
	{ "STORE metric "
	  "'host'.'metric' "
	  "LAST UPDATE "
	  "2015-02-01",          -1,  1, SDB_CONNECTION_STORE_METRIC },
	{ "STORE metric "
	  "'host'.'metric' "
	  "STORE 'typ' 'id' "
	  "LAST UPDATE "
	  "2015-02-01",          -1,  1, SDB_CONNECTION_STORE_METRIC },
	{ "STORE metric attribute "
	  "'host'.'metric'.'key' "
	  "123",                 -1,  1, SDB_CONNECTION_STORE_ATTRIBUTE },
	{ "STORE metric attribute "
	  "'host'.'metric'.'key' "
	  "123 "
	  "LAST UPDATE "
	  "2015-02-01",          -1,  1, SDB_CONNECTION_STORE_ATTRIBUTE },

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

	/* array iteration */
	{ "LOOKUP hosts MATCHING "
	  "'foo' IN backend",   -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING 'foo' "
	  "NOT IN backend",   -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "['foo','bar'] "
	  "IN backend ",        -1,   1, SDB_CONNECTION_LOOKUP },
	/* attribute type is unknown */
	{ "LOOKUP hosts MATCHING "
	  "attribute['backend'] "
	  "IN backend ",        -1,   1, SDB_CONNECTION_LOOKUP },
	/* type mismatch */
	{ "LOOKUP hosts MATCHING "
	  "1 IN backend ",      -1,  -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "1 NOT IN backend ",  -1,  -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "ANY backend < 'b'",  -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "ANY backend <= 'b'", -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "ANY backend = 'b'",  -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "ANY backend != 'b'", -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "ANY backend >= 'b'", -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "ANY backend > 'b'",  -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "ANY backend =~ 'b'", -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "ANY backend !~ 'b'", -1,   1, SDB_CONNECTION_LOOKUP },
	/* right operand is an array */
	{ "LOOKUP hosts MATCHING "
	  "ANY backend !~ backend",
	                        -1,  -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "ALL backend < 'b'",  -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "ALL backend <= 'b'", -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "ALL backend = 'b'",  -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "ALL backend != 'b'", -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "ALL backend >= 'b'", -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "ALL backend > 'b'",  -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "ALL backend =~ 'b'", -1,   1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "ALL backend !~ 'b'", -1,   1, SDB_CONNECTION_LOOKUP },
	/* attribute type is unknown */
	{ "LOOKUP hosts MATCHING "
	  "ANY backend = attribute['backend']",
	                        -1,   1, SDB_CONNECTION_LOOKUP },
	/* type mismatch */
	{ "LOOKUP hosts MATCHING "
	  "ANY backend = 1",    -1,  -1, 0 },

	/* valid operand types */
	{ "LOOKUP hosts MATCHING "
	  "age * 1 > 0s",        -1,  1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "age / 1 > 0s",        -1,  1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "name > ''",           -1,  1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "name >= ''",          -1,  1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "name != ''",          -1,  1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "name = ''",           -1,  1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "name <= ''",          -1,  1, SDB_CONNECTION_LOOKUP },
	{ "LOOKUP hosts MATCHING "
	  "name < ''",           -1,  1, SDB_CONNECTION_LOOKUP },

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
	  "ANY service.name IS NULL", -1, -1, 0 },

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

	/* type mismatches */
	{ "LOOKUP hosts MATCHING "
	  "age > 0",             -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "NOT age > 0",         -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age >= 0",            -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age = 0",             -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age != 0",            -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age <= 0",            -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age < 0",             -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age + 1 > 0s",        -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age - 1 > 0s",        -1, -1, 0 },
	/* datetime <mul/div> integer is allowed */
	{ "LOOKUP hosts MATCHING "
	  "age || 1 > 0s",       -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name + 1 = ''",       -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name - 1 = ''",       -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name * 1 = ''",       -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name / 1 = ''",       -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name % 1 = ''",       -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "(name % 1) + 1 = ''", -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "1 + (name % 1) = ''", -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "'' = 1 + (name % 1)", -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age > 0 AND "
	  "age = 0s",            -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age = 0s AND "
	  "age > 0",             -1, -1, 0 },
	{ "LOOKUP services MATCHING "
	  "host.name > 0",       -1, -1, 0 },
	{ "LOOKUP services MATCHING "
	  "backend > 'b'",       -1, -1, 0 },
	{ "LOOKUP services MATCHING "
	  "'b' > backend",       -1, -1, 0 },
	{ "LOOKUP services MATCHING "
	  "attribute['a'] > backend",
	                         -1, -1, 0 },
	{ "LOOKUP services MATCHING "
	  "backend > attribute['a']",
	                         -1, -1, 0 },
	{ "LOOKUP services MATCHING "
	  "host.name + 1 = ''",  -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "'a' + 1 IN 'b'",      -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "'a' IN 'b' - 1",      -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name + 1 IN 'b'",     -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "'a' IN name - 1",     -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "'b' IN 'abc'",        -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "1 IN age",            -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name =~ 'a' + 1",     -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name =~ name + 1",    -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name + 1 =~ 'a'",     -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name =~ 1",           -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name + 1 IS NULL",    -1, -1, 0 },
	{ "LOOKUP hosts FILTER "
	  "name + 1 IS NULL",    -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "ANY 'patt' =~ 'p'",  -1,  -1, 0 },

	/* comments */
	{ "/* some comment */",  -1,  0, 0 },
	{ "-- another comment",  -1,  0, 0 },

	/* syntax errors */
	{ "INVALID",             -1, -1, 0 },
	{ "FETCH host",          -1, -1, 0 },
	{ "FETCH 'host'",        -1, -1, 0 },
	{ "LIST hosts; INVALID", -1, -1, 0 },
	{ "/* some incomplete",  -1, -1, 0 },

	/* invalid LIST commands */
	{ "LIST",                -1, -1, 0 },
	{ "LIST foo",            -1, -1, 0 },
	{ "LIST hosts MATCHING "
	  "name = 'host'",       -1, -1, 0 },
	{ "LIST foo FILTER "
	  "age > 60s",           -1, -1, 0 },

	/* invalid FETCH commands */
	{ "FETCH host 'host' MATCHING "
	  "name = 'host'",       -1, -1, 0 },
	{ "FETCH service 'host'",-1, -1, 0 },
	{ "FETCH metric 'host'", -1, -1, 0 },
	{ "FETCH host "
	  "'host'.'localhost'",  -1, -1, 0 },
	{ "FETCH foo 'host'",    -1, -1, 0 },
	{ "FETCH foo 'host' FILTER "
	  "age > 60s",           -1, -1, 0 },

	/* invalid LOOKUP commands */
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
	  "ANY host.name = 'host'",   -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "ANY service.name > 1",     -1, -1, 0 },
	{ "LOOKUP hosts MATCHING "
	  "service.name = 's'",       -1, -1, 0 },
	{ "LOOKUP services MATCHING "
	  "ANY host.name = 'host'",   -1, -1, 0 },
	{ "LOOKUP services MATCHING "
	  "ANY service.name = 'svc'", -1, -1, 0 },
	{ "LOOKUP services MATCHING "
	  "ANY metric.name = 'm'",    -1, -1, 0 },
	{ "LOOKUP services MATCHING "
	  "metric.name = 'm'",        -1, -1, 0 },
	{ "LOOKUP metrics MATCHING "
	  "ANY host.name = 'host'",   -1, -1, 0 },
	{ "LOOKUP metrics MATCHING "
	  "ANY service.name = 'svc'", -1, -1, 0 },
	{ "LOOKUP metrics MATCHING "
	  "ANY metric.name = 'm'",    -1, -1, 0 },
	{ "LOOKUP metrics MATCHING "
	  "service.name = 'm'",       -1, -1, 0 },

	/* invalid STORE commands */
	{ "STORE host "
	  "'obj'.'host'",        -1, -1, 0 },
	{ "STORE host attribute "
	  ".'key' 123",          -1, -1, 0 },
	{ "STORE host attribute "
	  "'o'.'h'.'key' 123",   -1, -1, 0 },
	{ "STORE service 'svc'", -1, -1, 0 },
	{ "STORE service "
	  "'host'.'svc' "
	  "STORE 'typ' 'id' "
	  "LAST UPDATE "
	  "2015-02-01",          -1, -1, 0 },
	{ "STORE service attribute "
	  "'svc'.'key' 123",     -1, -1, 0 },
	{ "STORE metric 'm'",    -1, -1, 0 },
	{ "STORE metric "
	  "'host'.'metric' "
	  "STORE 'typ'.'id' "
	  "LAST UPDATE "
	  "2015-02-01",          -1, -1, 0 },
	{ "STORE metric attribute "
	  "'metric'.'key' 123",  -1, -1, 0 },
};

START_TEST(test_parse)
{
	sdb_strbuf_t *errbuf = sdb_strbuf_create(64);
	sdb_llist_t *check;
	sdb_object_t *obj;
	_Bool ok;

	check = sdb_fe_parse(parse_data[_i].query,
			parse_data[_i].len, errbuf);
	if (parse_data[_i].expected < 0)
		ok = check == 0;
	else
		ok = sdb_llist_len(check) == (size_t)parse_data[_i].expected;

	fail_unless(ok, "sdb_fe_parse(%s) = %p (len: %zu); expected: %d "
			"(parser error: %s)", parse_data[_i].query, check,
			sdb_llist_len(check), parse_data[_i].expected,
			sdb_strbuf_string(errbuf));

	if (! check) {
		sdb_strbuf_destroy(errbuf);
		return;
	}

	if ((! parse_data[_i].expected_cmd)
			|| (parse_data[_i].expected <= 0)) {
		sdb_llist_destroy(check);
		sdb_strbuf_destroy(errbuf);
		return;
	}

	obj = sdb_llist_get(check, 0);
	fail_unless(SDB_CONN_NODE(obj)->cmd == parse_data[_i].expected_cmd,
			"sdb_fe_parse(%s)->cmd = %i; expected: %d",
			parse_data[_i].query, SDB_CONN_NODE(obj)->cmd,
			parse_data[_i].expected_cmd);

	sdb_object_deref(obj);
	sdb_llist_destroy(check);
	sdb_strbuf_destroy(errbuf);
}
END_TEST

struct {
	const char *expr;
	int len;
	int expected;
} parse_matcher_data[] = {
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
	{ "ANY service.name < 'name'",         -1,  MATCHER_ANY },
	{ "ANY service.name <= 'name'",        -1,  MATCHER_ANY },
	{ "ANY service.name = 'name'",         -1,  MATCHER_ANY },
	{ "ANY service.name != 'name'",        -1,  MATCHER_ANY },
	{ "ANY service.name >= 'name'",        -1,  MATCHER_ANY },
	{ "ANY service.name > 'name'",         -1,  MATCHER_ANY },
	{ "ANY service.name =~ 'pattern'",     -1,  MATCHER_ANY },
	{ "ANY service.name !~ 'pattern'",     -1,  MATCHER_ANY },
	{ "ANY service.name &^ 'name'",        -1,  -1 },
	{ "ALL service.name < 'name'",         -1,  MATCHER_ALL },
	{ "ALL service.name <= 'name'",        -1,  MATCHER_ALL },
	{ "ALL service.name = 'name'",         -1,  MATCHER_ALL },
	{ "ALL service.name != 'name'",        -1,  MATCHER_ALL },
	{ "ALL service.name >= 'name'",        -1,  MATCHER_ALL },
	{ "ALL service.name > 'name'",         -1,  MATCHER_ALL },
	{ "ALL service.name =~ 'pattern'",     -1,  MATCHER_ALL },
	{ "ALL service.name !~ 'pattern'",     -1,  MATCHER_ALL },
	{ "ALL service.name &^ 'name'",        -1,  -1 },
	{ "ANY service < 'name'",              -1,  -1 },
	/* match hosts by metric */
	{ "ANY metric.name < 'name'",          -1,  MATCHER_ANY },
	{ "ANY metric.name <= 'name'",         -1,  MATCHER_ANY },
	{ "ANY metric.name = 'name'",          -1,  MATCHER_ANY },
	{ "ANY metric.name != 'name'",         -1,  MATCHER_ANY },
	{ "ANY metric.name >= 'name'",         -1,  MATCHER_ANY },
	{ "ANY metric.name > 'name'",          -1,  MATCHER_ANY },
	{ "ANY metric.name =~ 'pattern'",      -1,  MATCHER_ANY },
	{ "ANY metric.name !~ 'pattern'",      -1,  MATCHER_ANY },
	{ "ANY metric.name &^ 'pattern'",      -1,  -1 },
	{ "ALL metric.name < 'name'",          -1,  MATCHER_ALL },
	{ "ALL metric.name <= 'name'",         -1,  MATCHER_ALL },
	{ "ALL metric.name = 'name'",          -1,  MATCHER_ALL },
	{ "ALL metric.name != 'name'",         -1,  MATCHER_ALL },
	{ "ALL metric.name >= 'name'",         -1,  MATCHER_ALL },
	{ "ALL metric.name > 'name'",          -1,  MATCHER_ALL },
	{ "ALL metric.name =~ 'pattern'",      -1,  MATCHER_ALL },
	{ "ALL metric.name !~ 'pattern'",      -1,  MATCHER_ALL },
	{ "ALL metric.name &^ 'pattern'",      -1,  -1 },
	{ "ANY metric <= 'name'",              -1,  -1 },
	/* match hosts by attribute */
	{ "ANY attribute.name < 'name'",       -1,  MATCHER_ANY },
	{ "ANY attribute.name <= 'name'",      -1,  MATCHER_ANY },
	{ "ANY attribute.name = 'name'",       -1,  MATCHER_ANY },
	{ "ANY attribute.name != 'name'",      -1,  MATCHER_ANY },
	{ "ANY attribute.name >= 'name'",      -1,  MATCHER_ANY },
	{ "ANY attribute.name > 'name'",       -1,  MATCHER_ANY },
	{ "ANY attribute.name =~ 'pattern'",   -1,  MATCHER_ANY },
	{ "ANY attribute.name !~ 'pattern'",   -1,  MATCHER_ANY },
	{ "ANY attribute.name &^ 'pattern'",   -1,  -1 },
	{ "ALL attribute.name < 'name'",       -1,  MATCHER_ALL },
	{ "ALL attribute.name <= 'name'",      -1,  MATCHER_ALL },
	{ "ALL attribute.name = 'name'",       -1,  MATCHER_ALL },
	{ "ALL attribute.name != 'name'",      -1,  MATCHER_ALL },
	{ "ALL attribute.name >= 'name'",      -1,  MATCHER_ALL },
	{ "ALL attribute.name > 'name'",       -1,  MATCHER_ALL },
	{ "ALL attribute.name =~ 'pattern'",   -1,  MATCHER_ALL },
	{ "ALL attribute.name !~ 'pattern'",   -1,  MATCHER_ALL },
	{ "ALL attribute.name &^ 'pattern'",   -1,  -1 },
	{ "ANY attribute !~ 'pattern'",        -1,  -1 },
	/* composite expressions */
	{ "name =~ 'pattern' AND "
	  "ANY service.name =~ 'pattern'",     -1,  MATCHER_AND },
	{ "name =~ 'pattern' OR "
	  "ANY service.name =~ 'pattern'",     -1,  MATCHER_OR },
	{ "NOT name = 'host'",                 -1,  MATCHER_NOT },
	/* numeric expressions */
	{ "attribute['foo'] < 123",         -1,  MATCHER_LT },
	{ "attribute['foo'] <= 123",        -1,  MATCHER_LE },
	{ "attribute['foo'] = 123",         -1,  MATCHER_EQ },
	{ "attribute['foo'] >= 123",        -1,  MATCHER_GE },
	{ "attribute['foo'] > 123",         -1,  MATCHER_GT },
	/* datetime expressions */
	{ "attribute['foo'] = "
	  "2014-08-16",                     -1,  MATCHER_EQ },
	{ "attribute['foo'] = "
	  "17:23",                          -1,  MATCHER_EQ },
	{ "attribute['foo'] = "
	  "17:23:53",                       -1,  MATCHER_EQ },
	{ "attribute['foo'] = "
	  "17:23:53.123",                   -1,  MATCHER_EQ },
	{ "attribute['foo'] = "
	  "17:23:53.123456789",             -1,  MATCHER_EQ },
	{ "attribute['foo'] = "
	  "2014-08-16 17:23",               -1,  MATCHER_EQ },
	{ "attribute['foo'] = "
	  "2014-08-16 17:23:53",            -1,  MATCHER_EQ },
	/* NULL; while this is an implementation detail,
	 * IS NULL currently maps to an equality matcher */
	{ "attribute['foo'] IS NULL",       -1,  MATCHER_ISNULL },
	{ "attribute['foo'] IS NOT NULL",   -1,  MATCHER_ISNNULL },
	/* array expressions */
	{ "backend < ['a']",                -1,  MATCHER_LT },
	{ "backend <= ['a']",               -1,  MATCHER_LE },
	{ "backend = ['a']",                -1,  MATCHER_EQ },
	{ "backend != ['a']",               -1,  MATCHER_NE },
	{ "backend >= ['a']",               -1,  MATCHER_GE },
	{ "backend > ['a']",                -1,  MATCHER_GT },
	{ "backend &^ ['a']",               -1,  -1 },

	/* object field matchers */
	{ "name < 'a'",                     -1,  MATCHER_LT },
	{ "name <= 'a'",                    -1,  MATCHER_LE },
	{ "name = 'a'",                     -1,  MATCHER_EQ },
	{ "name != 'a'",                    -1,  MATCHER_NE },
	{ "name >= 'a'",                    -1,  MATCHER_GE },
	{ "name > 'a'",                     -1,  MATCHER_GT },
	{ "last_update < 2014-10-01",       -1,  MATCHER_LT },
	{ "last_update <= 2014-10-01",      -1,  MATCHER_LE },
	{ "last_update = 2014-10-01",       -1,  MATCHER_EQ },
	{ "last_update != 2014-10-01",      -1,  MATCHER_NE },
	{ "last_update >= 2014-10-01",      -1,  MATCHER_GE },
	{ "last_update > 2014-10-01",       -1,  MATCHER_GT },
	{ "Last_Update >= 24D",             -1,  MATCHER_GE },
	{ "age < 20s",                      -1,  MATCHER_LT },
	{ "age <= 20s",                     -1,  MATCHER_LE },
	{ "age = 20s",                      -1,  MATCHER_EQ },
	{ "age != 20s",                     -1,  MATCHER_NE },
	{ "age >= 20s",                     -1,  MATCHER_GE },
	{ "age > 20s",                      -1,  MATCHER_GT },
	{ "AGE <= 1m",                      -1,  MATCHER_LE },
	{ "age > 1M",                       -1,  MATCHER_GT },
	{ "age != 20Y",                     -1,  MATCHER_NE },
	{ "age <= 2 * interval",            -1,  MATCHER_LE },
	{ "interval < 20s",                 -1,  MATCHER_LT },
	{ "interval <= 20s",                -1,  MATCHER_LE },
	{ "interval = 20s",                 -1,  MATCHER_EQ },
	{ "interval != 20s",                -1,  MATCHER_NE },
	{ "interval >= 20s",                -1,  MATCHER_GE },
	{ "interval > 20s",                 -1,  MATCHER_GT },
	{ "'be' IN backend",                -1,  MATCHER_IN },
	{ "'be' NOT IN backend",            -1,  MATCHER_NIN },
	{ "['a','b'] IN backend",           -1,  MATCHER_IN },
	{ "['a','b'] NOT IN backend",       -1,  MATCHER_NIN },

	/* check operator precedence */
	{ "name = 'name' OR "
	  "ANY service.name = 'name' AND "
	  "ANY attribute.name = 'name' OR "
	  "attribute['foo'] = 'bar'",       -1,  MATCHER_OR },
	{ "name = 'name' AND "
	  "ANY service.name = 'name' AND "
	  "ANY attribute.name = 'name' OR "
	  "attribute['foo'] = 'bar'",       -1,  MATCHER_OR },
	{ "name = 'name' AND "
	  "ANY service.name = 'name' OR "
	  "ANY attribute.name = 'name' AND "
	  "attribute['foo'] = 'bar'",       -1,  MATCHER_OR },
	{ "(name = 'name' OR "
	  "ANY service.name = 'name') AND "
	  "(ANY attribute.name = 'name' OR "
	  "attribute['foo'] = 'bar')",      -1,  MATCHER_AND },
	{ "NOT name = 'name' OR "
	  "ANY service.name = 'name'",      -1,  MATCHER_OR },
	{ "NOT name = 'name' OR "
	  "NOT ANY service.name = 'name'",  -1,  MATCHER_OR },
	{ "NOT (name = 'name' OR "
	  "NOT ANY service.name = 'name')", -1,  MATCHER_NOT },

	/* syntax errors */
	{ "LIST",                           -1, -1 },
	{ "foo &^ bar",                     -1, -1 },
	{ "invalid",                        -1, -1 },
};

START_TEST(test_parse_matcher)
{
	sdb_strbuf_t *errbuf = sdb_strbuf_create(64);
	sdb_store_matcher_t *m;

	m = sdb_fe_parse_matcher(parse_matcher_data[_i].expr,
			parse_matcher_data[_i].len, errbuf);

	if (parse_matcher_data[_i].expected < 0) {
		fail_unless(m == NULL,
				"sdb_fe_parse_matcher(%s) = %p; expected: NULL",
				parse_matcher_data[_i].expr, m);
		sdb_object_deref(SDB_OBJ(m));
		sdb_strbuf_destroy(errbuf);
		return;
	}

	fail_unless(m != NULL, "sdb_fe_parse_matcher(%s) = NULL; "
			"expected: <matcher> (parser error: %s)",
			parse_matcher_data[_i].expr, sdb_strbuf_string(errbuf));
	fail_unless(M(m)->type == parse_matcher_data[_i].expected,
			"sdb_fe_parse_matcher(%s) returned matcher of type %d; "
			"expected: %d", parse_matcher_data[_i].expr, M(m)->type,
			parse_matcher_data[_i].expected);

	sdb_object_deref(SDB_OBJ(m));
	sdb_strbuf_destroy(errbuf);
}
END_TEST

struct {
	const char *expr;
	int len;
	int expected;
} parse_expr_data[] = {
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

START_TEST(test_parse_expr)
{
	sdb_strbuf_t *errbuf = sdb_strbuf_create(64);
	sdb_store_expr_t *e;

	e = sdb_fe_parse_expr(parse_expr_data[_i].expr,
			parse_expr_data[_i].len, errbuf);

	if (parse_expr_data[_i].expected == INT_MAX) {
		fail_unless(e == NULL,
				"sdb_fe_parse_expr(%s) = %p; expected: NULL",
				parse_expr_data[_i].expr, e);
		sdb_object_deref(SDB_OBJ(e));
		sdb_strbuf_destroy(errbuf);
		return;
	}

	fail_unless(e != NULL, "sdb_fe_parse_expr(%s) = NULL; "
			"expected: <expr> (parser error: %s)",
			parse_expr_data[_i].expr, sdb_strbuf_string(errbuf));
	fail_unless(e->type == parse_expr_data[_i].expected,
			"sdb_fe_parse_expr(%s) returned expression of type %d; "
			"expected: %d", parse_expr_data[_i].expr, e->type,
			parse_expr_data[_i].expected);

	sdb_object_deref(SDB_OBJ(e));
	sdb_strbuf_destroy(errbuf);
}
END_TEST

TEST_MAIN("frontend::parser")
{
	TCase *tc = tcase_create("core");
	TC_ADD_LOOP_TEST(tc, parse);
	TC_ADD_LOOP_TEST(tc, parse_matcher);
	TC_ADD_LOOP_TEST(tc, parse_expr);
	ADD_TCASE(tc);
}
TEST_MAIN_END

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

