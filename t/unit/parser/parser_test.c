/*
 * SysDB - t/unit/parser/parser_test.c
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

#include "parser/parser.h"
#include "core/object.h"
#include "core/store.h"
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
	int expected_type;
	int expected_extra; /* type-specific extra information */
} parse_data[] = {
	/* empty commands */
	{ NULL,                  -1, -1, 0, 0 },
	{ "",                    -1,  0, 0, 0 },
	{ ";",                   -1,  0, 0, 0 },
	{ ";;",                  -1,  0, 0, 0 },

	/* FETCH commands */
	{ "FETCH host 'host'",   -1,  1, SDB_AST_TYPE_FETCH, SDB_HOST },
	{ "FETCH host 'host' FILTER "
	  "age > 60s",           -1,  1, SDB_AST_TYPE_FETCH, SDB_HOST },
	{ "FETCH service "
	  "'host'.'service'",    -1,  1, SDB_AST_TYPE_FETCH, SDB_SERVICE },
	{ "FETCH metric "
	  "'host'.'metric'",     -1,  1, SDB_AST_TYPE_FETCH, SDB_METRIC },

	/* LIST commands */
	{ "LIST hosts",            -1,  1, SDB_AST_TYPE_LIST, SDB_HOST },
	{ "LIST hosts -- foo",     -1,  1, SDB_AST_TYPE_LIST, SDB_HOST },
	{ "LIST hosts;",           -1,  1, SDB_AST_TYPE_LIST, SDB_HOST },
	{ "LIST hosts; INVALID",   11,  1, SDB_AST_TYPE_LIST, SDB_HOST },
	{ "LIST hosts FILTER "
	  "age > 60s",             -1,  1, SDB_AST_TYPE_LIST, SDB_HOST },
	{ "LIST services",         -1,  1, SDB_AST_TYPE_LIST, SDB_SERVICE },
	{ "LIST services FILTER "
	  "age > 60s",             -1,  1, SDB_AST_TYPE_LIST, SDB_SERVICE },
	{ "LIST metrics",          -1,  1, SDB_AST_TYPE_LIST, SDB_METRIC },
	{ "LIST metrics FILTER "
	  "age > 60s",             -1,  1, SDB_AST_TYPE_LIST, SDB_METRIC },
	/* field access */
	{ "LIST hosts FILTER "
	  "name = 'a'",            -1,  1, SDB_AST_TYPE_LIST, SDB_HOST },
	{ "LIST hosts FILTER "
	  "last_update > 1s",      -1,  1, SDB_AST_TYPE_LIST, SDB_HOST },
	{ "LIST hosts FILTER "
	  "age > 120s",            -1,  1, SDB_AST_TYPE_LIST, SDB_HOST },
	{ "LIST hosts FILTER "
	  "interval > 10s",        -1,  1, SDB_AST_TYPE_LIST, SDB_HOST },
	{ "LIST hosts FILTER "
	  "backend = ['b']",       -1,  1, SDB_AST_TYPE_LIST, SDB_HOST },
	{ "LIST hosts FILTER ANY "
	  "attribute.value = 'a'", -1,  1, SDB_AST_TYPE_LIST, SDB_HOST },
	{ "LIST services FILTER "
	  "name = 'a'",            -1,  1, SDB_AST_TYPE_LIST, SDB_SERVICE },
	{ "LIST services FILTER "
	  "last_update > 1s",      -1,  1, SDB_AST_TYPE_LIST, SDB_SERVICE },
	{ "LIST services FILTER "
	  "age > 120s",            -1,  1, SDB_AST_TYPE_LIST, SDB_SERVICE },
	{ "LIST services FILTER "
	  "interval > 10s",        -1,  1, SDB_AST_TYPE_LIST, SDB_SERVICE },
	{ "LIST services FILTER "
	  "backend = ['b']",       -1,  1, SDB_AST_TYPE_LIST, SDB_SERVICE },
	{ "LIST services FILTER ANY "
	  "attribute.value = 'a'", -1,  1, SDB_AST_TYPE_LIST, SDB_SERVICE },
	{ "LIST metrics FILTER "
	  "name = 'a'",            -1,  1, SDB_AST_TYPE_LIST, SDB_METRIC },
	{ "LIST metrics FILTER "
	  "last_update > 1s",      -1,  1, SDB_AST_TYPE_LIST, SDB_METRIC },
	{ "LIST metrics FILTER "
	  "age > 120s",            -1,  1, SDB_AST_TYPE_LIST, SDB_METRIC },
	{ "LIST metrics FILTER "
	  "interval > 10s",        -1,  1, SDB_AST_TYPE_LIST, SDB_METRIC },
	{ "LIST metrics FILTER "
	  "backend = ['b']",       -1,  1, SDB_AST_TYPE_LIST, SDB_METRIC },
	{ "LIST metrics FILTER ANY "
	  "attribute.value = 'a'", -1,  1, SDB_AST_TYPE_LIST, SDB_METRIC },

	/* LOOKUP commands */
	{ "LOOKUP hosts",        -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "name = 'host'",       -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING NOT "
	  "name = 'host'",       -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "name =~ 'p' AND "
	  "ANY service.name =~ 'p'", -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING NOT "
	  "name =~ 'p' AND "
	  "ANY service.name =~ 'p'", -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "name =~ 'p' AND "
	  "ANY service.name =~ 'p' OR "
	  "ANY service.name =~ 'r'", -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING NOT "
	  "name =~ 'p' AND "
	  "ANY service.name =~ 'p' OR "
	  "ANY service.name =~ 'r'", -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "name =~ 'p' "
	  "FILTER age > 1D",         -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "name =~ 'p' "
	  "FILTER age > 1D AND "
	  "interval < 240s" ,        -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "name =~ 'p' "
	  "FILTER NOT age>1D",       -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "name =~ 'p' "
	  "FILTER age>"
	  "interval",                -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "host.name =~ 'p'",        -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP services",         -1,   1, SDB_AST_TYPE_LOOKUP, SDB_SERVICE },
	{ "LOOKUP services MATCHING ANY "
	  "attribute.name =~ 'a'",   -1,   1, SDB_AST_TYPE_LOOKUP, SDB_SERVICE },
	{ "LOOKUP services MATCHING "
	  "host.name = 'p'",         -1,   1, SDB_AST_TYPE_LOOKUP, SDB_SERVICE },
	{ "LOOKUP services MATCHING "
	  "service.name = 'p'",      -1,   1, SDB_AST_TYPE_LOOKUP, SDB_SERVICE },
	{ "LOOKUP metrics",          -1,   1, SDB_AST_TYPE_LOOKUP, SDB_METRIC },
	{ "LOOKUP metrics MATCHING ANY "
	  "attribute.name =~ 'a'",   -1,   1, SDB_AST_TYPE_LOOKUP, SDB_METRIC },
	{ "LOOKUP metrics MATCHING "
	  "host.name = 'p'",         -1,   1, SDB_AST_TYPE_LOOKUP, SDB_METRIC },
	{ "LOOKUP metrics MATCHING "
	  "metric.name = 'p'",       -1,   1, SDB_AST_TYPE_LOOKUP, SDB_METRIC },

	/* TIMESERIES commands */
	{ "TIMESERIES 'host'.'metric' "
	  "START 2014-01-01 "
	  "END 2014-12-31 "
	  "23:59:59",            -1,  1, SDB_AST_TYPE_TIMESERIES, 0 },
	{ "TIMESERIES 'host'.'metric' "
	  "START 2014-02-02 "
	  "14:02",               -1,  1, SDB_AST_TYPE_TIMESERIES, 0 },
	/* the end time has to be greater than the start time;
	 * we'll be safe for about 200 years ;-) */
	{ "TIMESERIES 'host'.'metric' "
	  "END 2214-02-02",      -1,  1, SDB_AST_TYPE_TIMESERIES, 0 },
	{ "TIMESERIES "
	  "'host'.'metric'",     -1,  1, SDB_AST_TYPE_TIMESERIES, 0 },

	/* STORE commands */
	{ "STORE host 'host'",   -1,  1, SDB_AST_TYPE_STORE, SDB_HOST },
	{ "STORE host 'host' "
	  "LAST UPDATE "
	  "2015-02-01",          -1,  1, SDB_AST_TYPE_STORE, SDB_HOST },
	{ "STORE host attribute "
	  "'host'.'key' 123",    -1,  1, SDB_AST_TYPE_STORE, SDB_ATTRIBUTE },
	{ "STORE host attribute "
	  "'host'.'key' 123 "
	  "LAST UPDATE "
	  "2015-02-01",          -1,  1, SDB_AST_TYPE_STORE, SDB_ATTRIBUTE },
	{ "STORE service "
	  "'host'.'svc'",        -1,  1, SDB_AST_TYPE_STORE, SDB_SERVICE },
	{ "STORE service "
	  "'host'.'svc' "
	  "LAST UPDATE "
	  "2015-02-01",          -1,  1, SDB_AST_TYPE_STORE, SDB_SERVICE },
	{ "STORE service attribute "
	  "'host'.'svc'.'key' "
	  "123",                 -1,  1, SDB_AST_TYPE_STORE, SDB_ATTRIBUTE },
	{ "STORE service attribute "
	  "'host'.'svc'.'key' "
	  "123 "
	  "LAST UPDATE "
	  "2015-02-01",          -1,  1, SDB_AST_TYPE_STORE, SDB_ATTRIBUTE },
	{ "STORE metric "
	  "'host'.'metric'",     -1,  1, SDB_AST_TYPE_STORE, SDB_METRIC },
	{ "STORE metric "
	  "'host'.'metric' "
	  "LAST UPDATE "
	  "2015-02-01",          -1,  1, SDB_AST_TYPE_STORE, SDB_METRIC },
	{ "STORE metric "
	  "'host'.'metric' "
	  "STORE 'typ' 'id' "
	  "LAST UPDATE "
	  "2015-02-01",          -1,  1, SDB_AST_TYPE_STORE, SDB_METRIC },
	{ "STORE metric attribute "
	  "'host'.'metric'.'key' "
	  "123",                 -1,  1, SDB_AST_TYPE_STORE, SDB_ATTRIBUTE },
	{ "STORE metric attribute "
	  "'host'.'metric'.'key' "
	  "123 "
	  "LAST UPDATE "
	  "2015-02-01",          -1,  1, SDB_AST_TYPE_STORE, SDB_ATTRIBUTE },

	/* string constants */
	{ "LOOKUP hosts MATCHING "
	  "name = ''''",         -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "name = '''foo'",      -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "name = 'f''oo'",      -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "name = 'foo'''",      -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "name = '''",          -1, -1, 0, SDB_HOST },

	/* numeric constants */
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] = "
	  "1234",                -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] != "
	  "+234",                -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] < "
	  "-234",                -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] > "
	  "12.4",                -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] <= "
	  "12. + .3",            -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] <= "
	  "'f' || 'oo'",         -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] >= "
	  ".4",                  -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] = "
	  "+12e3",               -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] = "
	  "+12e-3",              -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] = "
	  "-12e+3",              -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },

	/* date, time, interval constants */
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] = "
	  "1 Y 42D",             -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] = "
	  "1s 42D",              -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	/*
	 * TODO: Something like 1Y42D should work as well but it doesn't since
	 * the scanner will tokenize it into {digit}{identifier} :-/
	 *
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] = "
	  "1Y42D",               -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	 */

	/* array constants */
	{ "LOOKUP hosts MATCHING "
	  "backend = ['foo']",   -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "backend = ['a','b']", -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },

	/* array iteration */
	{ "LOOKUP hosts MATCHING "
	  "'foo' IN backend",   -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING 'foo' "
	  "NOT IN backend",     -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "['foo','bar'] "
	  "IN backend ",        -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	/* attribute type is unknown */
	{ "LOOKUP hosts MATCHING "
	  "attribute['backend'] "
	  "IN backend ",        -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "ANY backend < 'b'",  -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "ANY backend <= 'b'", -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "ANY backend = 'b'",  -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "ANY backend != 'b'", -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "ANY backend >= 'b'", -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "ANY backend > 'b'",  -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "ANY backend =~ 'b'", -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "ANY backend !~ 'b'", -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "ALL backend < 'b'",  -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "ALL backend <= 'b'", -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "ALL backend = 'b'",  -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "ALL backend != 'b'", -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "ALL backend >= 'b'", -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "ALL backend > 'b'",  -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "ALL backend =~ 'b'", -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "ALL backend !~ 'b'", -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	/* attribute type is unknown */
	{ "LOOKUP hosts MATCHING "
	  "ANY backend = attribute['backend']",
	                        -1,   1, SDB_AST_TYPE_LOOKUP, SDB_HOST },

	/* valid operand types */
	{ "LOOKUP hosts MATCHING "
	  "age * 1 > 0s",        -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "age / 1 > 0s",        -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "name > ''",           -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "name >= ''",          -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "name != ''",          -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "name = ''",           -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "name <= ''",          -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "name < ''",           -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },

	/* typed expressions */
	{ "LOOKUP services MATCHING "
	  "host.attribute['a'] = 'a'",
	                         -1,  1, SDB_AST_TYPE_LOOKUP, SDB_SERVICE },
	/* TODO: this should work but the analyzer currently sees ATTRIBUTE
	 * (instead of SERVICE-ATTRIBUTE) as the child type
	{ "LOOKUP services MATCHING "
	  "ANY attribute.service.name = 's'",
	                         -1,  1, SDB_AST_TYPE_LOOKUP, SDB_SERVICE },
	 */
	{ "LOOKUP hosts MATCHING "
	  "ANY service.service.name = 's'",
	                         -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },

	/* NULL / TRUE / FALSE */
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] "
	  "IS NULL",             -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] "
	  "IS NOT NULL",         -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "NOT attribute['foo'] "
	  "IS NULL",             -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "ANY service.name IS NULL", -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] "
	  "IS TRUE",             -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] "
	  "IS NOT TRUE",         -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "NOT attribute['foo'] "
	  "IS TRUE",             -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] "
	  "IS FALSE",            -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] "
	  "IS NOT FALSE",        -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP hosts MATCHING "
	  "NOT attribute['foo'] "
	  "IS FALSE",            -1,  1, SDB_AST_TYPE_LOOKUP, SDB_HOST },
	{ "LOOKUP metrics MATCHING "
	  "timeseries IS TRUE",  -1,  1, SDB_AST_TYPE_LOOKUP, SDB_METRIC },

	/* invalid numeric constants */
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] = "
	  "+-12e+3",             -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] = "
	  "-12e-+3",             -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] = "
	  "e+3",                 -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] = "
	  "3e",                  -1, -1, 0, 0 },
	/* following SQL standard, we don't support hex numbers */
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] = "
	  "0x12",                -1, -1, 0, 0 },

	/* invalid expressions */
	{ "LOOKUP hosts MATCHING "
	  "attr['foo'] = 1.23",  -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "attr['foo'] IS NULL", -1, -1, 0, 0 },

	/* comments */
	{ "/* some comment */",  -1,  0, 0, 0 },
	{ "-- another comment",  -1,  0, 0, 0 },

	/* syntax errors */
	{ "INVALID",             -1, -1, 0, 0 },
	{ "FETCH host",          -1, -1, 0, 0 },
	{ "FETCH 'host'",        -1, -1, 0, 0 },
	{ "LIST hosts; INVALID", -1, -1, 0, 0 },
	{ "/* some incomplete",  -1, -1, 0, 0 },

	/*
	 * syntactically correct but semantically invalid commands
	 */

	/* invalid fields */
	{ "LIST hosts FILTER "
	  "field = 'a'",           -1, -1, 0, 0 },
	{ "LIST services FILTER "
	  "field = 'a'",           -1, -1, 0, 0 },
	{ "LIST metrics FILTER "
	  "field = 'a'",           -1, -1, 0, 0 },
	{ "LIST hosts FILTER "
	  "value = 'a'",           -1, -1, 0, 0 },
	{ "LIST services FILTER "
	  "value = 'a'",           -1, -1, 0, 0 },
	{ "LIST metrics FILTER "
	  "value = 'a'",           -1, -1, 0, 0 },
	{ "LIST metrics FILTER "
	  "name.1 = 'a'",          -1, -1, 0, 0 },
	{ "LIST hosts FILTER "
	  "timeseries IS TRUE",    -1, -1, 0, 0 },
	{ "LIST services FILTER "
	  "timeseries IS TRUE",    -1, -1, 0, 0 },

	/* type mismatches */
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] = "
	  "1.23 + 'foo'",       -1,  -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "1 IN backend ",      -1,  -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "1 NOT IN backend ",  -1,  -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age > 0",             -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "NOT age > 0",         -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age >= 0",            -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age = 0",             -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age != 0",            -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age <= 0",            -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age < 0",             -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age + 1 > 0s",        -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age - 1 > 0s",        -1, -1, 0, 0 },

	/* datetime <mul/div> integer is allowed */
	{ "LOOKUP hosts MATCHING "
	  "age || 1 > 0s",       -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name + 1 = ''",       -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name - 1 = ''",       -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name * 1 = ''",       -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name / 1 = ''",       -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name % 1 = ''",       -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "(name % 1) + 1 = ''", -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "1 + (name % 1) = ''", -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "'' = 1 + (name % 1)", -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age > 0 AND "
	  "age = 0s",            -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "age = 0s AND "
	  "age > 0",             -1, -1, 0, 0 },
	{ "LOOKUP services MATCHING "
	  "host.name > 0",       -1, -1, 0, 0 },
	{ "LOOKUP services MATCHING "
	  "backend > 'b'",       -1, -1, 0, 0 },
	{ "LOOKUP services MATCHING "
	  "'b' > backend",       -1, -1, 0, 0 },
	{ "LOOKUP services MATCHING "
	  "attribute['a'] > backend",
	                         -1, -1, 0, 0 },
	{ "LOOKUP services MATCHING "
	  "backend > attribute['a']",
	                         -1, -1, 0, 0 },
	{ "LOOKUP services MATCHING "
	  "host.name + 1 = ''",  -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "'a' + 1 IN 'b'",      -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "'a' IN 'b' - 1",      -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name + 1 IN 'b'",     -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "'a' IN name - 1",     -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "'b' IN 'abc'",        -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "1 IN age",            -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name =~ 'a' + 1",     -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name =~ name + 1",    -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name + 1 =~ 'a'",     -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name =~ 1",           -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name + 1 IS NULL",    -1, -1, 0, 0 },
	{ "LOOKUP hosts FILTER "
	  "name + 1 IS NULL",    -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name + 1 IS TRUE",    -1, -1, 0, 0 },
	{ "LOOKUP hosts FILTER "
	  "name + 1 IS TRUE",    -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "name + 1 IS FALSE",   -1, -1, 0, 0 },
	{ "LOOKUP hosts FILTER "
	  "name + 1 IS FALSE",   -1, -1, 0, 0 },

	/* invalid iterators */
	{ "LOOKUP hosts MATCHING "
	  "ANY backend !~ backend",
	                        -1,  -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "ANY backend = 1",    -1,  -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "ANY 'patt' =~ 'p'",  -1,  -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "ALL 1 || '2' < '3'", -1,  -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "ALL name =~ 'a'",    -1,  -1, 0, 0 },
	/* this could work in theory but is not supported atm */
	{ "LOOKUP hosts MATCHING "
	  "ANY backend || 'a' = 'b'",
	                        -1,  -1, 0, 0 },

	/* invalid LIST commands */
	{ "LIST",                -1, -1, 0, 0 },
	{ "LIST foo",            -1, -1, 0, 0 },
	{ "LIST hosts MATCHING "
	  "name = 'host'",       -1, -1, 0, 0 },
	{ "LIST foo FILTER "
	  "age > 60s",           -1, -1, 0, 0 },

	/* invalid FETCH commands */
	{ "FETCH host 'host' MATCHING "
	  "name = 'host'",       -1, -1, 0, 0 },
	{ "FETCH service 'host'",-1, -1, 0, 0 },
	{ "FETCH metric 'host'", -1, -1, 0, 0 },
	{ "FETCH host "
	  "'host'.'localhost'",  -1, -1, 0, 0 },
	{ "FETCH foo 'host'",    -1, -1, 0, 0 },
	{ "FETCH foo 'host' FILTER "
	  "age > 60s",           -1, -1, 0, 0 },

	/* invalid LOOKUP commands */
	{ "LOOKUP foo",          -1, -1, 0, 0 },
	{ "LOOKUP foo MATCHING "
	  "name = 'host'",       -1, -1, 0, 0 },
	{ "LOOKUP foo FILTER "
	  "age > 60s",           -1, -1, 0, 0 },
	{ "LOOKUP foo MATCHING "
	  "name = 'host' FILTER "
	  "age > 60s",           -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] <= "
	  "f || 'oo'",           -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "attribute['foo'] <= "
	  "'f' || oo",           -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "ANY host.name = 'host'",   -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "ANY service.name > 1",     -1, -1, 0, 0 },
	{ "LOOKUP hosts MATCHING "
	  "service.name = 's'",       -1, -1, 0, 0 },
	{ "LOOKUP services MATCHING "
	  "ANY host.name = 'host'",   -1, -1, 0, 0 },
	{ "LOOKUP services MATCHING "
	  "ANY service.name = 'svc'", -1, -1, 0, 0 },
	{ "LOOKUP services MATCHING "
	  "ANY metric.name = 'm'",    -1, -1, 0, 0 },
	{ "LOOKUP services MATCHING "
	  "metric.name = 'm'",        -1, -1, 0, 0 },
	{ "LOOKUP metrics MATCHING "
	  "ANY host.name = 'host'",   -1, -1, 0, 0 },
	{ "LOOKUP metrics MATCHING "
	  "ANY service.name = 'svc'", -1, -1, 0, 0 },
	{ "LOOKUP metrics MATCHING "
	  "ANY metric.name = 'm'",    -1, -1, 0, 0 },
	{ "LOOKUP metrics MATCHING "
	  "service.name = 'm'",       -1, -1, 0, 0 },

	/* invalid STORE commands */
	{ "STORE host "
	  "'obj'.'host'",        -1, -1, 0, 0 },
	{ "STORE host attribute "
	  ".'key' 123",          -1, -1, 0, 0 },
	{ "STORE host attribute "
	  "'o'.'h'.'key' 123",   -1, -1, 0, 0 },
	{ "STORE service 'svc'", -1, -1, 0, 0 },
	{ "STORE service "
	  "'host'.'svc' "
	  "STORE 'typ' 'id' "
	  "LAST UPDATE "
	  "2015-02-01",          -1, -1, 0, 0 },
	{ "STORE service attribute "
	  "'svc'.'key' 123",     -1, -1, 0, 0 },
	{ "STORE metric 'm'",    -1, -1, 0, 0 },
	{ "STORE metric "
	  "'host'.'metric' "
	  "STORE 'typ'.'id' "
	  "LAST UPDATE "
	  "2015-02-01",          -1, -1, 0, 0 },
	{ "STORE metric attribute "
	  "'metric'.'key' 123",  -1, -1, 0, 0 },
};

START_TEST(test_parse)
{
	sdb_strbuf_t *errbuf = sdb_strbuf_create(64);
	sdb_llist_t *check;
	sdb_ast_node_t *node;
	sdb_store_query_t *q;
	_Bool ok;

	check = sdb_parser_parse(parse_data[_i].query,
			parse_data[_i].len, errbuf);
	if (parse_data[_i].expected < 0)
		ok = check == 0;
	else
		ok = sdb_llist_len(check) == (size_t)parse_data[_i].expected;

	fail_unless(ok, "sdb_parser_parse(%s) = %p (len: %zu); expected: %d "
			"(parse error: %s)", parse_data[_i].query, check,
			sdb_llist_len(check), parse_data[_i].expected,
			sdb_strbuf_string(errbuf));

	if (! check) {
		sdb_strbuf_destroy(errbuf);
		return;
	}

	if ((! parse_data[_i].expected_type)
			|| (parse_data[_i].expected <= 0)) {
		sdb_llist_destroy(check);
		sdb_strbuf_destroy(errbuf);
		return;
	}

	node = SDB_AST_NODE(sdb_llist_get(check, 0));
	fail_unless(node->type == parse_data[_i].expected_type,
			"sdb_parser_parse(%s)->type = %i; expected: %d",
			parse_data[_i].query, node->type,
			parse_data[_i].expected_type);

	if (node->type == SDB_AST_TYPE_FETCH) {
		sdb_ast_fetch_t *f = SDB_AST_FETCH(node);
		fail_unless(f->obj_type == parse_data[_i].expected_extra,
				"sdb_parser_parse(%s)->obj_type = %s; expected: %s",
				parse_data[_i].query, SDB_STORE_TYPE_TO_NAME(f->obj_type),
				SDB_STORE_TYPE_TO_NAME(parse_data[_i].expected_extra));
	}
	else if (node->type == SDB_AST_TYPE_LIST) {
		sdb_ast_list_t *l = SDB_AST_LIST(node);
		fail_unless(l->obj_type == parse_data[_i].expected_extra,
				"sdb_parser_parse(%s)->obj_type = %s; expected: %s",
				parse_data[_i].query, SDB_STORE_TYPE_TO_NAME(l->obj_type),
				SDB_STORE_TYPE_TO_NAME(parse_data[_i].expected_extra));
	}
	else if (node->type == SDB_AST_TYPE_LOOKUP) {
		sdb_ast_lookup_t *l = SDB_AST_LOOKUP(node);
		fail_unless(l->obj_type == parse_data[_i].expected_extra,
				"sdb_parser_parse(%s)->obj_type = %s; expected: %s",
				parse_data[_i].query, SDB_STORE_TYPE_TO_NAME(l->obj_type),
				SDB_STORE_TYPE_TO_NAME(parse_data[_i].expected_extra));
	}
	else if (node->type == SDB_AST_TYPE_STORE) {
		sdb_ast_store_t *s = SDB_AST_STORE(node);
		fail_unless(s->obj_type == parse_data[_i].expected_extra,
				"sdb_parser_parse(%s)->obj_type = %s; expected: %s",
				parse_data[_i].query, SDB_STORE_TYPE_TO_NAME(s->obj_type),
				SDB_STORE_TYPE_TO_NAME(parse_data[_i].expected_extra));
	}

	/* TODO: this should move into front-end specific tests */
	q = sdb_store_query_prepare(node);
	fail_unless(q != NULL,
			"sdb_store_query_prepare(AST<%s>) = NULL; expected: <query>",
			parse_data[_i].query);

	sdb_object_deref(SDB_OBJ(node));
	sdb_object_deref(SDB_OBJ(q));
	sdb_llist_destroy(check);
	sdb_strbuf_destroy(errbuf);
}
END_TEST

struct {
	const char *expr;
	int len;
	int expected;
} parse_conditional_data[] = {
	/* empty expressions */
	{ NULL,                           -1, -1 },
	{ "",                             -1, -1 },

	/* match hosts by name */
	{ "name < 'localhost'",           -1,  SDB_AST_LT },
	{ "name <= 'localhost'",          -1,  SDB_AST_LE },
	{ "name = 'localhost'",           -1,  SDB_AST_EQ },
	{ "name != 'localhost'",          -1,  SDB_AST_NE },
	{ "name >= 'localhost'",          -1,  SDB_AST_GE },
	{ "name > 'localhost'",           -1,  SDB_AST_GT },
	{ "name =~ 'host'",               -1,  SDB_AST_REGEX },
	{ "name !~ 'host'",               -1,  SDB_AST_NREGEX },
	{ "name = 'localhost' -- foo",    -1,  SDB_AST_EQ },
	{ "name = 'host' <garbage>",      13,  SDB_AST_EQ },
	{ "name &^ 'localhost'",          -1,  -1 },
	/* match by backend */
	{ "ANY backend < 'be'",           -1,  SDB_AST_ANY },
	{ "ANY backend <= 'be'",          -1,  SDB_AST_ANY },
	{ "ANY backend = 'be'",           -1,  SDB_AST_ANY },
	{ "ANY backend != 'be'",          -1,  SDB_AST_ANY },
	{ "ANY backend >= 'be'",          -1,  SDB_AST_ANY },
	{ "ANY backend > 'be'",           -1,  SDB_AST_ANY },
	{ "ALL backend < 'be'",           -1,  SDB_AST_ALL },
	{ "ALL backend <= 'be'",          -1,  SDB_AST_ALL },
	{ "ALL backend = 'be'",           -1,  SDB_AST_ALL },
	{ "ALL backend != 'be'",          -1,  SDB_AST_ALL },
	{ "ALL backend >= 'be'",          -1,  SDB_AST_ALL },
	{ "ALL backend > 'be'",           -1,  SDB_AST_ALL },
	{ "ANY backend &^ 'be'",          -1,  -1 },
	/* match hosts by service */
	{ "ANY service.name < 'name'",         -1,  SDB_AST_ANY },
	{ "ANY service.name <= 'name'",        -1,  SDB_AST_ANY },
	{ "ANY service.name = 'name'",         -1,  SDB_AST_ANY },
	{ "ANY service.name != 'name'",        -1,  SDB_AST_ANY },
	{ "ANY service.name >= 'name'",        -1,  SDB_AST_ANY },
	{ "ANY service.name > 'name'",         -1,  SDB_AST_ANY },
	{ "ANY service.name =~ 'pattern'",     -1,  SDB_AST_ANY },
	{ "ANY service.name !~ 'pattern'",     -1,  SDB_AST_ANY },
	{ "ANY service.name &^ 'name'",        -1,  -1 },
	{ "ALL service.name < 'name'",         -1,  SDB_AST_ALL },
	{ "ALL service.name <= 'name'",        -1,  SDB_AST_ALL },
	{ "ALL service.name = 'name'",         -1,  SDB_AST_ALL },
	{ "ALL service.name != 'name'",        -1,  SDB_AST_ALL },
	{ "ALL service.name >= 'name'",        -1,  SDB_AST_ALL },
	{ "ALL service.name > 'name'",         -1,  SDB_AST_ALL },
	{ "ALL service.name =~ 'pattern'",     -1,  SDB_AST_ALL },
	{ "ALL service.name !~ 'pattern'",     -1,  SDB_AST_ALL },
	{ "ALL service.name &^ 'name'",        -1,  -1 },
	{ "ANY service < 'name'",              -1,  -1 },
	/* match hosts by metric */
	{ "ANY metric.name < 'name'",          -1,  SDB_AST_ANY },
	{ "ANY metric.name <= 'name'",         -1,  SDB_AST_ANY },
	{ "ANY metric.name = 'name'",          -1,  SDB_AST_ANY },
	{ "ANY metric.name != 'name'",         -1,  SDB_AST_ANY },
	{ "ANY metric.name >= 'name'",         -1,  SDB_AST_ANY },
	{ "ANY metric.name > 'name'",          -1,  SDB_AST_ANY },
	{ "ANY metric.name =~ 'pattern'",      -1,  SDB_AST_ANY },
	{ "ANY metric.name !~ 'pattern'",      -1,  SDB_AST_ANY },
	{ "ANY metric.name &^ 'pattern'",      -1,  -1 },
	{ "ALL metric.name < 'name'",          -1,  SDB_AST_ALL },
	{ "ALL metric.name <= 'name'",         -1,  SDB_AST_ALL },
	{ "ALL metric.name = 'name'",          -1,  SDB_AST_ALL },
	{ "ALL metric.name != 'name'",         -1,  SDB_AST_ALL },
	{ "ALL metric.name >= 'name'",         -1,  SDB_AST_ALL },
	{ "ALL metric.name > 'name'",          -1,  SDB_AST_ALL },
	{ "ALL metric.name =~ 'pattern'",      -1,  SDB_AST_ALL },
	{ "ALL metric.name !~ 'pattern'",      -1,  SDB_AST_ALL },
	{ "ALL metric.name &^ 'pattern'",      -1,  -1 },
	{ "ANY metric <= 'name'",              -1,  -1 },
	/* match hosts by attribute */
	{ "ANY attribute.name < 'name'",       -1,  SDB_AST_ANY },
	{ "ANY attribute.name <= 'name'",      -1,  SDB_AST_ANY },
	{ "ANY attribute.name = 'name'",       -1,  SDB_AST_ANY },
	{ "ANY attribute.name != 'name'",      -1,  SDB_AST_ANY },
	{ "ANY attribute.name >= 'name'",      -1,  SDB_AST_ANY },
	{ "ANY attribute.name > 'name'",       -1,  SDB_AST_ANY },
	{ "ANY attribute.name =~ 'pattern'",   -1,  SDB_AST_ANY },
	{ "ANY attribute.name !~ 'pattern'",   -1,  SDB_AST_ANY },
	{ "ANY attribute.name &^ 'pattern'",   -1,  -1 },
	{ "ALL attribute.name < 'name'",       -1,  SDB_AST_ALL },
	{ "ALL attribute.name <= 'name'",      -1,  SDB_AST_ALL },
	{ "ALL attribute.name = 'name'",       -1,  SDB_AST_ALL },
	{ "ALL attribute.name != 'name'",      -1,  SDB_AST_ALL },
	{ "ALL attribute.name >= 'name'",      -1,  SDB_AST_ALL },
	{ "ALL attribute.name > 'name'",       -1,  SDB_AST_ALL },
	{ "ALL attribute.name =~ 'pattern'",   -1,  SDB_AST_ALL },
	{ "ALL attribute.name !~ 'pattern'",   -1,  SDB_AST_ALL },
	{ "ALL attribute.name &^ 'pattern'",   -1,  -1 },
	{ "ANY attribute !~ 'pattern'",        -1,  -1 },

	/* composite expressions */
	{ "name =~ 'pattern' AND "
	  "ANY service.name =~ 'pattern'",  -1,  SDB_AST_AND },
	{ "name =~ 'pattern' OR "
	  "ANY service.name =~ 'pattern'",  -1,  SDB_AST_OR },
	{ "NOT name = 'host'",              -1,  SDB_AST_NOT },
	/* numeric expressions */
	{ "attribute['foo'] < 123",         -1,  SDB_AST_LT },
	{ "attribute['foo'] <= 123",        -1,  SDB_AST_LE },
	{ "attribute['foo'] = 123",         -1,  SDB_AST_EQ },
	{ "attribute['foo'] >= 123",        -1,  SDB_AST_GE },
	{ "attribute['foo'] > 123",         -1,  SDB_AST_GT },
	/* datetime expressions */
	{ "attribute['foo'] = "
	  "2014-08-16",                     -1,  SDB_AST_EQ },
	{ "attribute['foo'] = "
	  "17:23",                          -1,  SDB_AST_EQ },
	{ "attribute['foo'] = "
	  "17:23:53",                       -1,  SDB_AST_EQ },
	{ "attribute['foo'] = "
	  "17:23:53.123",                   -1,  SDB_AST_EQ },
	{ "attribute['foo'] = "
	  "17:23:53.123456789",             -1,  SDB_AST_EQ },
	{ "attribute['foo'] = "
	  "2014-08-16 17:23",               -1,  SDB_AST_EQ },
	{ "attribute['foo'] = "
	  "2014-08-16 17:23:53",            -1,  SDB_AST_EQ },
	/* NULL / TRUE / FALSE */
	{ "attribute['foo'] IS NULL",       -1,  SDB_AST_ISNULL },
	{ "attribute['foo'] IS NOT NULL",   -1,  SDB_AST_NOT },
	{ "attribute['foo'] IS TRUE",       -1,  SDB_AST_ISTRUE },
	{ "attribute['foo'] IS NOT TRUE",   -1,  SDB_AST_NOT },
	{ "attribute['foo'] IS FALSE",      -1,  SDB_AST_ISFALSE },
	{ "attribute['foo'] IS NOT FALSE",  -1,  SDB_AST_NOT },
	/* array expressions */
	{ "backend < ['a']",                -1,  SDB_AST_LT },
	{ "backend <= ['a']",               -1,  SDB_AST_LE },
	{ "backend = ['a']",                -1,  SDB_AST_EQ },
	{ "backend != ['a']",               -1,  SDB_AST_NE },
	{ "backend >= ['a']",               -1,  SDB_AST_GE },
	{ "backend > ['a']",                -1,  SDB_AST_GT },
	{ "backend &^ ['a']",               -1,  -1 },

	/* object field comparison */
	{ "name < 'a'",                     -1,  SDB_AST_LT },
	{ "name <= 'a'",                    -1,  SDB_AST_LE },
	{ "name = 'a'",                     -1,  SDB_AST_EQ },
	{ "name != 'a'",                    -1,  SDB_AST_NE },
	{ "name >= 'a'",                    -1,  SDB_AST_GE },
	{ "name > 'a'",                     -1,  SDB_AST_GT },
	{ "last_update < 2014-10-01",       -1,  SDB_AST_LT },
	{ "last_update <= 2014-10-01",      -1,  SDB_AST_LE },
	{ "last_update = 2014-10-01",       -1,  SDB_AST_EQ },
	{ "last_update != 2014-10-01",      -1,  SDB_AST_NE },
	{ "last_update >= 2014-10-01",      -1,  SDB_AST_GE },
	{ "last_update > 2014-10-01",       -1,  SDB_AST_GT },
	{ "Last_Update >= 24D",             -1,  SDB_AST_GE },
	{ "age < 20s",                      -1,  SDB_AST_LT },
	{ "age <= 20s",                     -1,  SDB_AST_LE },
	{ "age = 20s",                      -1,  SDB_AST_EQ },
	{ "age != 20s",                     -1,  SDB_AST_NE },
	{ "age >= 20s",                     -1,  SDB_AST_GE },
	{ "age > 20s",                      -1,  SDB_AST_GT },
	{ "AGE <= 1m",                      -1,  SDB_AST_LE },
	{ "age > 1M",                       -1,  SDB_AST_GT },
	{ "age != 20Y",                     -1,  SDB_AST_NE },
	{ "age <= 2 * interval",            -1,  SDB_AST_LE },
	{ "interval < 20s",                 -1,  SDB_AST_LT },
	{ "interval <= 20s",                -1,  SDB_AST_LE },
	{ "interval = 20s",                 -1,  SDB_AST_EQ },
	{ "interval != 20s",                -1,  SDB_AST_NE },
	{ "interval >= 20s",                -1,  SDB_AST_GE },
	{ "interval > 20s",                 -1,  SDB_AST_GT },
	{ "'be' IN backend",                -1,  SDB_AST_IN },
	{ "'be' NOT IN backend",            -1,  SDB_AST_NOT },
	{ "['a','b'] IN backend",           -1,  SDB_AST_IN },
	{ "['a','b'] NOT IN backend",       -1,  SDB_AST_NOT },
	{ "timeseries IS TRUE",             -1,  SDB_AST_ISTRUE },
	{ "timeseries IS FALSE",            -1,  SDB_AST_ISFALSE },
	{ "timeseries IS NOT TRUE",         -1,  SDB_AST_NOT },
	{ "timeseries IS NOT FALSE",        -1,  SDB_AST_NOT },
	{ "timeseries > 0",                 -1,  -1 },
	{ "timeseries = TRUE",              -1,  -1 },
	{ "timeseries != FALSE",            -1,  -1 },

	/* check operator precedence */
	{ "name = 'name' OR "
	  "ANY service.name = 'name' AND "
	  "ANY attribute.name = 'name' OR "
	  "attribute['foo'] = 'bar'",       -1,  SDB_AST_OR },
	{ "name = 'name' AND "
	  "ANY service.name = 'name' AND "
	  "ANY attribute.name = 'name' OR "
	  "attribute['foo'] = 'bar'",       -1,  SDB_AST_OR },
	{ "name = 'name' AND "
	  "ANY service.name = 'name' OR "
	  "ANY attribute.name = 'name' AND "
	  "attribute['foo'] = 'bar'",       -1,  SDB_AST_OR },
	{ "(name = 'name' OR "
	  "ANY service.name = 'name') AND "
	  "(ANY attribute.name = 'name' OR "
	  "attribute['foo'] = 'bar')",      -1,  SDB_AST_AND },
	{ "NOT name = 'name' OR "
	  "ANY service.name = 'name'",      -1,  SDB_AST_OR },
	{ "NOT name = 'name' OR "
	  "NOT ANY service.name = 'name'",  -1,  SDB_AST_OR },
	{ "NOT (name = 'name' OR "
	  "NOT ANY service.name = 'name')", -1,  SDB_AST_NOT },

	/* syntax errors */
	{ "LIST hosts",                     -1, -1 },
	{ "foo &^ bar",                     -1, -1 },
	{ "invalid",                        -1, -1 },
};

START_TEST(test_parse_conditional)
{
	sdb_strbuf_t *errbuf = sdb_strbuf_create(64);
	sdb_ast_node_t *node;

	node = sdb_parser_parse_conditional(parse_conditional_data[_i].expr,
			parse_conditional_data[_i].len, errbuf);

	if (parse_conditional_data[_i].expected < 0) {
		fail_unless(node == NULL,
				"sdb_parser_parse_conditional(%s) = %p; expected: NULL",
				parse_conditional_data[_i].expr, node);
		sdb_object_deref(SDB_OBJ(node));
		sdb_strbuf_destroy(errbuf);
		return;
	}

	fail_unless(node != NULL, "sdb_parser_parse_conditional(%s) = NULL; "
			"expected: <cond> (parse error: %s)",
			parse_conditional_data[_i].expr, sdb_strbuf_string(errbuf));
	if (node->type == SDB_AST_TYPE_OPERATOR)
		fail_unless(SDB_AST_OP(node)->kind == parse_conditional_data[_i].expected,
				"sdb_parser_parse_conditional(%s) returned conditional of type %d; "
				"expected: %d", parse_conditional_data[_i].expr,
				SDB_AST_OP(node)->kind, parse_conditional_data[_i].expected);
	else if (node->type == SDB_AST_TYPE_ITERATOR)
		fail_unless(SDB_AST_ITER(node)->kind == parse_conditional_data[_i].expected,
				"sdb_parser_parse_conditional(%s) returned conditional of type %d; "
				"expected: %d", parse_conditional_data[_i].expr,
				SDB_AST_ITER(node)->kind, parse_conditional_data[_i].expected);

	fail_unless(node->data_type == -1,
			"sdb_parser_parse_conditional(%s) returned conditional of data-type %s; "
			"expected: %s", parse_conditional_data[_i].expr,
			SDB_TYPE_TO_STRING(node->data_type), SDB_TYPE_TO_STRING(-1));

	sdb_object_deref(SDB_OBJ(node));
	sdb_strbuf_destroy(errbuf);
}
END_TEST

struct {
	const char *expr;
	int len;
	int expected;
	int data_type;
} parse_arith_data[] = {
	/* empty expressions */
	{ NULL,                   -1, -1, -1 },
	{ "",                     -1, -1, -1 },

	/* constant expressions */
	{ "'localhost'",          -1, SDB_AST_TYPE_CONST, SDB_TYPE_STRING },
	{ "123",                  -1, SDB_AST_TYPE_CONST, SDB_TYPE_INTEGER },
	{ "42.3",                 -1, SDB_AST_TYPE_CONST, SDB_TYPE_DECIMAL },
	{ "2014-08-16",           -1, SDB_AST_TYPE_CONST, SDB_TYPE_DATETIME },
	{ "17:23",                -1, SDB_AST_TYPE_CONST, SDB_TYPE_DATETIME },
	{ "17:23:53",             -1, SDB_AST_TYPE_CONST, SDB_TYPE_DATETIME },
	{ "17:23:53.123",         -1, SDB_AST_TYPE_CONST, SDB_TYPE_DATETIME },
	{ "17:23:53.123456789",   -1, SDB_AST_TYPE_CONST, SDB_TYPE_DATETIME },
	{ "2014-08-16 17:23",     -1, SDB_AST_TYPE_CONST, SDB_TYPE_DATETIME },
	{ "2014-08-16 17:23:53",  -1, SDB_AST_TYPE_CONST, SDB_TYPE_DATETIME },
	{ "10s",                  -1, SDB_AST_TYPE_CONST, SDB_TYPE_DATETIME },
	{ "60m",                  -1, SDB_AST_TYPE_CONST, SDB_TYPE_DATETIME },
	{ "10Y 24D 1h",           -1, SDB_AST_TYPE_CONST, SDB_TYPE_DATETIME },

	/* TODO: the analyzer and/or optimizer should turn these into constants */
	{ "123 + 456",            -1, SDB_AST_ADD, SDB_TYPE_INTEGER },
	{ "'foo' || 'bar'",       -1, SDB_AST_CONCAT, SDB_TYPE_STRING },
	{ "456 - 123",            -1, SDB_AST_SUB, SDB_TYPE_INTEGER },
	{ "1.2 * 3.4",            -1, SDB_AST_MUL, SDB_TYPE_DECIMAL },
	{ "1.2 / 3.4",            -1, SDB_AST_DIV, SDB_TYPE_DECIMAL },
	{ "5 % 2",                -1, SDB_AST_MOD, SDB_TYPE_INTEGER },

	/* queryable fields */
	{ "last_update",          -1, SDB_AST_TYPE_VALUE, SDB_TYPE_DATETIME },
	{ "AGE",                  -1, SDB_AST_TYPE_VALUE, SDB_TYPE_DATETIME },
	{ "interval",             -1, SDB_AST_TYPE_VALUE, SDB_TYPE_DATETIME },
	{ "Last_Update",          -1, SDB_AST_TYPE_VALUE, SDB_TYPE_DATETIME },
	{ "backend",              -1, SDB_AST_TYPE_VALUE, SDB_TYPE_ARRAY | SDB_TYPE_STRING },

	/* attributes */
	{ "attribute['foo']",     -1, SDB_AST_TYPE_VALUE, -1 },

	/* arithmetic expressions */
	{ "age + age",            -1, SDB_AST_ADD, SDB_TYPE_DATETIME },
	{ "age - age",            -1, SDB_AST_SUB, SDB_TYPE_DATETIME },
	{ "age * age",            -1, SDB_AST_MUL, SDB_TYPE_DATETIME },
	{ "age / age",            -1, SDB_AST_DIV, SDB_TYPE_DATETIME },
	{ "age \% age",           -1, SDB_AST_MOD, SDB_TYPE_DATETIME },

	/* operator precedence */
	{ "age + age * age",      -1, SDB_AST_ADD, SDB_TYPE_DATETIME },
	{ "age * age + age",      -1, SDB_AST_ADD, SDB_TYPE_DATETIME },
	{ "age + age - age",      -1, SDB_AST_SUB, SDB_TYPE_DATETIME },
	{ "age - age + age",      -1, SDB_AST_ADD, SDB_TYPE_DATETIME },
	{ "(age + age) * age",    -1, SDB_AST_MUL, SDB_TYPE_DATETIME },
	{ "age + (age * age)",    -1, SDB_AST_ADD, SDB_TYPE_DATETIME },

	/* boolean expressions */
	{ "timeseries + 1",               -1, -1, -1 },
	{ "timeseries - 1",               -1, -1, -1 },
	{ "timeseries * 1",               -1, -1, -1 },
	{ "timeseries / 1",               -1, -1, -1 },
	{ "timeseries \% 1",              -1, -1, -1 },
	{ "timeseries CONCAT 1",          -1, -1, -1 },
	{ "timeseries + timeseries",      -1, -1, -1 },
	{ "timeseries - timeseries",      -1, -1, -1 },
	{ "timeseries * timeseries",      -1, -1, -1 },
	{ "timeseries / timeseries",      -1, -1, -1 },
	{ "timeseries \% timeseries",     -1, -1, -1 },
	{ "timeseries CONCAT timeseries", -1, -1, -1 },

	/* syntax errors */
	{ "LIST",                 -1, -1, -1 },
	{ "foo &^ bar",           -1, -1, -1 },
	{ "invalid",              -1, -1, -1 },
};

START_TEST(test_parse_arith)
{
	sdb_strbuf_t *errbuf = sdb_strbuf_create(64);
	sdb_ast_node_t *node;

	node = sdb_parser_parse_arith(parse_arith_data[_i].expr,
			parse_arith_data[_i].len, errbuf);

	if (parse_arith_data[_i].expected < 0) {
		fail_unless(node == NULL,
				"sdb_parser_parse_arith(%s) = %p; expected: NULL",
				parse_arith_data[_i].expr, node);
		sdb_object_deref(SDB_OBJ(node));
		sdb_strbuf_destroy(errbuf);
		return;
	}

	fail_unless(node != NULL, "sdb_parser_parse_arith(%s) = NULL; "
			"expected: <expr> (parse error: %s)",
			parse_arith_data[_i].expr, sdb_strbuf_string(errbuf));
	if (node->type == SDB_AST_TYPE_OPERATOR)
		fail_unless(SDB_AST_OP(node)->kind == parse_arith_data[_i].expected,
				"sdb_parser_parse_arith(%s) returned expression of type %d; "
				"expected: %d", parse_arith_data[_i].expr,
				SDB_AST_OP(node)->kind, parse_arith_data[_i].expected);
	else
		fail_unless(node->type == parse_arith_data[_i].expected,
				"sdb_parser_parse_arith(%s) returned expression of type %d; "
				"expected: %d", parse_arith_data[_i].expr, node->type,
				parse_arith_data[_i].expected);

	fail_unless(node->data_type == parse_arith_data[_i].data_type,
			"sdb_parser_parse_arith(%s) returned expression of data-type %s; "
			"expected: %s", parse_arith_data[_i].expr,
			SDB_TYPE_TO_STRING(node->data_type),
			SDB_TYPE_TO_STRING(parse_arith_data[_i].data_type));

	sdb_object_deref(SDB_OBJ(node));
	sdb_strbuf_destroy(errbuf);
}
END_TEST

TEST_MAIN("parser::parser")
{
	TCase *tc = tcase_create("core");
	TC_ADD_LOOP_TEST(tc, parse);
	TC_ADD_LOOP_TEST(tc, parse_conditional);
	TC_ADD_LOOP_TEST(tc, parse_arith);
	ADD_TCASE(tc);
}
TEST_MAIN_END

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

