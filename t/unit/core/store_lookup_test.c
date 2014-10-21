/*
 * SysDB - t/unit/core/store_lookup_test.c
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

#include "core/store.h"
#include "core/store-private.h"
#include "frontend/parser.h"
#include "libsysdb_test.h"

#include <check.h>
#include <string.h>

static void
populate(void)
{
	const char *hosts[] = { "a", "b", "c" };

	struct {
		const char *host;
		const char *metric;
	} metrics[] = {
		{ "a", "m1" },
		{ "b", "m1" },
		{ "b", "m2" },
	};

	struct {
		const char *host;
		const char *service;
	} services[] = {
		{ "a", "s1" },
		{ "a", "s2" },
		{ "b", "s1" },
		{ "b", "s3" },
	};

	struct {
		const char *host;
		const char *name;
		sdb_data_t  value;
	} attrs[] = {
		{ "a", "k1", { SDB_TYPE_STRING, { .string = "v1" } } },
		{ "a", "k2", { SDB_TYPE_INTEGER, { .integer = 123 } } },
		{ "b", "k1", { SDB_TYPE_STRING, { .string = "v2" } } },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(hosts); ++i) {
		int status = sdb_store_host(hosts[i], 1);
		fail_unless(status == 0,
				"sdb_store_host(%s, 1) = %d; expected: 0",
				hosts[i], status);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(metrics); ++i) {
		int status = sdb_store_metric(metrics[i].host,
				metrics[i].metric, /* store */ NULL, 1);
		fail_unless(status == 0,
				"sdb_store_metric(%s, %s, NULL, 1) = %d; expected: 0",
				metrics[i].host, metrics[i].metric, status);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(services); ++i) {
		int status = sdb_store_service(services[i].host,
				services[i].service, 1);
		fail_unless(status == 0,
				"sdb_store_service(%s, %s, 1) = %d; expected: 0",
				services[i].host, services[i].service, status);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(attrs); ++i) {
		int status = sdb_store_attribute(attrs[i].host,
				attrs[i].name, &attrs[i].value, 1);
		fail_unless(status == 0,
				"sdb_store_attribute(%s, %s, <val>, 1) = %d; expected: 0",
				attrs[i].host, attrs[i].name, status);
	}
} /* populate */

START_TEST(test_store_match_name)
{
	sdb_store_obj_t *obj;

	struct {
		int type;
		const char *name;
		_Bool re;

		int expected;
	} golden_data[] = {
		{ SDB_HOST,      NULL,   0, 1 },
		{ SDB_HOST,      NULL,   1, 1 },
		{ SDB_HOST,      "a",    0, 1 },
		{ SDB_HOST,      "a",    1, 1 },
		{ SDB_HOST,      "b",    0, 0 },
		{ SDB_HOST,      "b",    1, 0 },
		{ SDB_HOST,      "^a$",  1, 1 },
		{ SDB_HOST,      "^b$",  1, 0 },
		{ SDB_HOST,      "^a$",  0, 0 },
		{ SDB_HOST,      "^b$",  0, 0 },
		{ SDB_METRIC,    NULL,   0, 1 },
		{ SDB_METRIC,    NULL,   1, 1 },
		{ SDB_METRIC,    "m1",   0, 1 },
		{ SDB_METRIC,    "m1",   1, 1 },
		{ SDB_METRIC,    "^m1$", 1, 1 },
		{ SDB_METRIC,    "m",    1, 1 },
		{ SDB_METRIC,    "s",    1, 0 },
		{ SDB_METRIC,    "m2",   0, 0 },
		{ SDB_METRIC,    "x1",   0, 0 },
		{ SDB_METRIC,    "x1",   1, 0 },
		{ SDB_SERVICE,   NULL,   0, 1 },
		{ SDB_SERVICE,   NULL,   1, 1 },
		{ SDB_SERVICE,   "s1",   0, 1 },
		{ SDB_SERVICE,   "s2",   0, 1 },
		{ SDB_SERVICE,   "s3",   0, 0 },
		{ SDB_SERVICE,   "s4",   0, 0 },
		{ SDB_SERVICE,   "^s1$", 1, 1 },
		{ SDB_SERVICE,   "^s1$", 0, 0 },
		{ SDB_SERVICE,   "x1",   0, 0 },
		{ SDB_SERVICE,   "x1",   1, 0 },
		{ SDB_SERVICE,   "x",    1, 0 },
		{ SDB_ATTRIBUTE, NULL,   0, 1 },
		{ SDB_ATTRIBUTE, NULL,   1, 1 },
		{ SDB_ATTRIBUTE, "k1",   0, 1 },
		{ SDB_ATTRIBUTE, "k2",   0, 1 },
		{ SDB_ATTRIBUTE, "k3",   0, 0 },
		{ SDB_ATTRIBUTE, "k4",   0, 0 },
		{ SDB_ATTRIBUTE, "k",    1, 1 },
		{ SDB_ATTRIBUTE, "1",    1, 1 },
		{ SDB_ATTRIBUTE, "3",    1, 0 },
	};

	size_t i;

	obj = sdb_store_get_host("a");
	fail_unless(obj != NULL,
			"sdb_store_get_host(a) = NULL; expected: <host>");

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_matcher_t *m, *n;
		int status;

		m = sdb_store_name_matcher(golden_data[i].type,
				golden_data[i].name, golden_data[i].re);
		fail_unless(m != NULL,
				"sdb_store_service_matcher(%d, %s, %d) = NULL; "
				"expected: <matcher>", golden_data[i].type,
				golden_data[i].name, golden_data[i].re);

		status = sdb_store_matcher_matches(m, obj, /* filter */ NULL);
		fail_unless(status == golden_data[i].expected,
				"sdb_store_matcher_matches(%s->%s, <host a>, NULL) = %d; "
				"expected: %d", SDB_STORE_TYPE_TO_NAME(golden_data[i].type),
				golden_data[i].name, status, golden_data[i].expected);

		n = sdb_store_inv_matcher(m);
		fail_unless(n != NULL,
				"sdb_store_inv_matcher() = NULL; expected: <matcher>");
		sdb_object_deref(SDB_OBJ(m));

		/* now match the inverted set of objects */
		status = sdb_store_matcher_matches(n, obj, /* filter */ NULL);
		fail_unless(status == !golden_data[i].expected,
				"sdb_store_matcher_matches(%s->%s, <host a>, NULL) = %d; "
				"expected: %d", SDB_STORE_TYPE_TO_NAME(golden_data[i].type),
				golden_data[i].name, status, !golden_data[i].expected);

		sdb_object_deref(SDB_OBJ(n));
	}

	sdb_object_deref(SDB_OBJ(obj));
}
END_TEST

START_TEST(test_store_match_attr)
{
	sdb_store_obj_t *obj;

	struct {
		const char *attr_name;
		const char *attr_value;
		_Bool re;

		int expected;
	} golden_data[] = {
		{ "k1", NULL,   0, 1 },
		{ "k",  NULL,   1, 0 },
		{ "1",  NULL,   1, 0 },
		{ "k3", NULL,   0, 0 },
		{ "k3", NULL,   1, 0 },
		{ "k1", "v1",   0, 1 },
		{ "k1", "v1",   1, 1 },
		{ "k1", "^v1$", 1, 1 },
		{ "k1", "v",    1, 1 },
		{ "k1", "1",    1, 1 },
		{ "k1", "v2",   0, 0 },
		{ "k1", "v2",   1, 0 },
		{ "k",  "v1",   0, 0 },
		{ "k",  "v1",   1, 0 },
		{ "k3", "1",    0, 0 },
		{ "k3", "1",    1, 0 },
	};

	size_t i;

	obj = sdb_store_get_host("a");
	fail_unless(obj != NULL,
			"sdb_store_get_host(a) = NULL; expected: <host>");

	fail_unless(sdb_store_attr_matcher(NULL, "re", 0) == NULL,
			"sdb_store_attr_matcher(NULL, \"re\", 0) = <m>; expected: NULL");
	fail_unless(sdb_store_attr_matcher(NULL, "re", 1) == NULL,
			"sdb_store_attr_matcher(NULL, \"re\", 1) = <m>; expected: NULL");

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_matcher_t *m, *n;
		int status;

		m = sdb_store_attr_matcher(golden_data[i].attr_name,
				golden_data[i].attr_value, golden_data[i].re);
		fail_unless(m != NULL,
				"sdb_store_attr_matcher() = NULL; expected: <matcher>");

		status = sdb_store_matcher_matches(m, obj, /* filter */ NULL);
		fail_unless(status == golden_data[i].expected,
				"sdb_store_matcher_matches(attribute[%s] = %s, "
				"<host a>, NULL) = %d; expected: %d", golden_data[i].attr_name,
				golden_data[i].attr_value, status, golden_data[i].expected);

		n = sdb_store_inv_matcher(m);
		fail_unless(n != NULL,
				"sdb_store_inv_matcher() = NULL; expected: <matcher>");
		sdb_object_deref(SDB_OBJ(m));

		/* now match the inverted set of objects */
		status = sdb_store_matcher_matches(n, obj, /* filter */ NULL);
		fail_unless(status == !golden_data[i].expected,
				"sdb_store_matcher_matches(attribute[%s] = %s, "
				"<host a>, NULL) = %d; expected: %d",
				golden_data[i].attr_name, golden_data[i].attr_value,
				status, !golden_data[i].expected);

		sdb_object_deref(SDB_OBJ(n));
	}

	sdb_object_deref(SDB_OBJ(obj));
}
END_TEST

START_TEST(test_attr_cond)
{
	sdb_store_obj_t *obj;

	struct {
		const char *attr;
		const sdb_data_t value;
		int expected_lt, expected_le, expected_eq, expected_ge, expected_gt;
	} golden_data[] = {
		{ "k1", { SDB_TYPE_STRING,  { .string  = "v1" } },  0, 1, 1, 1, 0 },
		{ "k1", { SDB_TYPE_STRING,  { .string  = "v2" } },  1, 1, 0, 0, 0 },
		{ "k1", { SDB_TYPE_STRING,  { .string  = "v0" } },  0, 0, 0, 1, 1 },
		{ "k1", { SDB_TYPE_STRING,  { .string  = "0" } },   0, 0, 0, 1, 1 },
		{ "k2", { SDB_TYPE_INTEGER, { .integer = 123 } },   0, 1, 1, 1, 0 },
		{ "k2", { SDB_TYPE_INTEGER, { .integer = 124 } },   1, 1, 0, 0, 0 },
		{ "k2", { SDB_TYPE_INTEGER, { .integer = 122 } },   0, 0, 0, 1, 1 },
		/* key does not exist */
		{ "k3", { SDB_TYPE_STRING,  { .string  = "v1" } },  0, 0, 0, 0, 0 },
		{ "k3", { SDB_TYPE_STRING,  { .string  = "123" } }, 0, 0, 0, 0, 0 },
		{ "k3", { SDB_TYPE_INTEGER, { .integer = 123 } },   0, 0, 0, 0, 0 },
		/* type mismatch */
		{ "k1", { SDB_TYPE_INTEGER, { .integer = 0 } },     0, 0, 0, 1, 1 },
		{ "k2", { SDB_TYPE_STRING,  { .string  = "123" } }, 0, 1, 1, 1, 0 },
	};

	int status;
	size_t i;

	obj = sdb_store_get_host("a");
	fail_unless(obj != NULL,
			"sdb_store_get_host(a) = NULL; expected: <host>");

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_expr_t *expr;
		sdb_store_cond_t *c;
		char buf[1024];
		size_t j;

		struct {
			sdb_store_matcher_t *(*matcher)(sdb_store_cond_t *);
			int expected;
		} tests[] = {
			{ sdb_store_lt_matcher, golden_data[i].expected_lt },
			{ sdb_store_le_matcher, golden_data[i].expected_le },
			{ sdb_store_eq_matcher, golden_data[i].expected_eq },
			{ sdb_store_ge_matcher, golden_data[i].expected_ge },
			{ sdb_store_gt_matcher, golden_data[i].expected_gt },
		};

		sdb_data_format(&golden_data[i].value,
				buf, sizeof(buf), SDB_UNQUOTED);

		expr = sdb_store_expr_constvalue(&golden_data[i].value);
		fail_unless(expr != NULL,
				"sdb_store_expr_constvalue(%s) = NULL; expected: <expr>",
				buf);

		c = sdb_store_attr_cond(golden_data[i].attr, expr);
		sdb_object_deref(SDB_OBJ(expr));
		fail_unless(c != NULL,
				"sdb_store_attr_cond(%s, expr{%s}) = NULL; expected: <cond>",
				golden_data[i].attr, buf);

		for (j = 0; j < SDB_STATIC_ARRAY_LEN(tests); ++j) {
			sdb_store_matcher_t *m;

			m = tests[j].matcher(c);
			fail_unless(m != NULL,
					"sdb_store_<cond>_matcher() = NULL; expected: <matcher>");

			status = sdb_store_matcher_matches(m, obj, /* filter */ NULL);
			fail_unless(status == tests[j].expected,
					"sdb_store_matcher_matches(<m>, <obj>, NULL) = %d; "
					"expected: %d", status, tests[j].expected);

			sdb_object_deref(SDB_OBJ(m));
		}

		sdb_object_deref(SDB_OBJ(c));
	}

	sdb_object_deref(SDB_OBJ(obj));
}
END_TEST

START_TEST(test_obj_cond)
{
	struct {
		const char *host;
		int field;
		const sdb_data_t value;
		int expected_lt, expected_le, expected_eq, expected_ge, expected_gt;
	} golden_data[] = {
		{ "b", SDB_FIELD_NAME,
			{ SDB_TYPE_STRING, { .string = "a" } },   0, 0, 0, 1, 1 },
		{ "b", SDB_FIELD_NAME,
			{ SDB_TYPE_STRING, { .string = "b" } },   0, 1, 1, 1, 0 },
		{ "b", SDB_FIELD_NAME,
			{ SDB_TYPE_STRING, { .string = "c" } },   1, 1, 0, 0, 0 },
		/* last-update = 1 for all objects */
		{ "a", SDB_FIELD_LAST_UPDATE,
			{ SDB_TYPE_DATETIME, { .datetime = 1 } }, 0, 1, 1, 1, 0 },
		{ "a", SDB_FIELD_LAST_UPDATE,
			{ SDB_TYPE_DATETIME, { .datetime = 2 } }, 1, 1, 0, 0, 0 },
		{ "a", SDB_FIELD_LAST_UPDATE,
			{ SDB_TYPE_DATETIME, { .datetime = 0 } }, 0, 0, 0, 1, 1 },
		/* age > 0 for all objects */
		{ "a", SDB_FIELD_AGE,
			{ SDB_TYPE_DATETIME, { .datetime = 0 } }, 0, 0, 0, 1, 1 },
		/* interval = 0 for all objects */
		{ "a", SDB_FIELD_INTERVAL,
			{ SDB_TYPE_DATETIME, { .datetime = 0 } }, 0, 1, 1, 1, 0 },
		{ "a", SDB_FIELD_INTERVAL,
			{ SDB_TYPE_DATETIME, { .datetime = 1 } }, 1, 1, 0, 0, 0 },
		/* type mismatch */
		{ "a", SDB_FIELD_LAST_UPDATE,
			{ SDB_TYPE_INTEGER, { .integer = 0 } }, 0, 0, 0, 0, 0 },
		{ "a", SDB_FIELD_AGE,
			{ SDB_TYPE_INTEGER, { .integer = 0 } }, 0, 0, 0, 0, 0 },
		{ "a", SDB_FIELD_INTERVAL,
			{ SDB_TYPE_INTEGER, { .integer = 0 } }, 0, 0, 0, 0, 0 },
		{ "a", SDB_FIELD_BACKEND,
			{ SDB_TYPE_INTEGER, { .integer = 0 } }, 0, 0, 0, 0, 0 },
		{ "a", SDB_FIELD_BACKEND,
			{ SDB_TYPE_INTEGER, { .integer = 0 } }, 0, 0, 0, 0, 0 },
		/* (64bit) integer value without zero-bytes */
		{ "a", SDB_FIELD_BACKEND,
			{ SDB_TYPE_INTEGER, { .integer = 0xffffffffffffffffL } },
			0, 0, 0, 0, 0 },
	};

	int status;
	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_obj_t *obj;
		sdb_store_expr_t *expr;
		sdb_store_cond_t *c;
		char buf[1024];
		size_t j;

		struct {
			sdb_store_matcher_t *(*matcher)(sdb_store_cond_t *);
			int expected;
		} tests[] = {
			{ sdb_store_lt_matcher, golden_data[i].expected_lt },
			{ sdb_store_le_matcher, golden_data[i].expected_le },
			{ sdb_store_eq_matcher, golden_data[i].expected_eq },
			{ sdb_store_ge_matcher, golden_data[i].expected_ge },
			{ sdb_store_gt_matcher, golden_data[i].expected_gt },
		};

		obj = sdb_store_get_host(golden_data[i].host);
		fail_unless(obj != NULL,
				"sdb_store_get_host(%s) = NULL; expected: <host>",
				golden_data[i].host);

		sdb_data_format(&golden_data[i].value,
				buf, sizeof(buf), SDB_UNQUOTED);

		expr = sdb_store_expr_constvalue(&golden_data[i].value);
		fail_unless(expr != NULL,
				"sdb_store_expr_constvalue(%s) = NULL; expected: <expr>",
				buf);

		c = sdb_store_obj_cond(golden_data[i].field, expr);
		sdb_object_deref(SDB_OBJ(expr));
		fail_unless(c != NULL,
				"sdb_store_obj_cond(%d, expr{%s}) = NULL; expected: <cond>",
				golden_data[i].field, buf);

		for (j = 0; j < SDB_STATIC_ARRAY_LEN(tests); ++j) {
			sdb_store_matcher_t *m;

			m = tests[j].matcher(c);
			fail_unless(m != NULL,
					"sdb_store_<cond>_matcher() = NULL; expected: <matcher>");

			status = sdb_store_matcher_matches(m, obj, /* filter */ NULL);
			fail_unless(status == tests[j].expected,
					"sdb_store_matcher_matches(<m>, <host '%s'>, NULL) = %d; "
					"expected: %d", status, tests[j].expected);

			sdb_object_deref(SDB_OBJ(m));
		}

		sdb_object_deref(SDB_OBJ(c));
		sdb_object_deref(SDB_OBJ(obj));
	}
}
END_TEST

START_TEST(test_store_match_op)
{
	sdb_store_obj_t *obj;

	sdb_store_matcher_t *always = sdb_store_name_matcher(SDB_HOST, "a", 0);
	sdb_store_matcher_t *never = sdb_store_name_matcher(SDB_HOST, "z", 0);

	struct {
		const char *op;
		sdb_store_matcher_t *left;
		sdb_store_matcher_t *right;
		int expected;
	} golden_data[] = {
		{ "OR",  always, always, 1 },
		{ "OR",  always, never,  1 },
		{ "OR",  never,  always, 1 },
		{ "OR",  never,  never,  0 },
		{ "AND", always, always, 1 },
		{ "AND", always, never,  0 },
		{ "AND", never,  always, 0 },
		{ "AND", never,  never,  0 },
	};

	int status;
	size_t i;

	obj = sdb_store_get_host("a");

	status = sdb_store_matcher_matches(always, obj, /* filter */ NULL);
	fail_unless(status == 1,
			"INTERNAL ERROR: 'always' did not match host");
	status = sdb_store_matcher_matches(never, obj, /* filter */ NULL);
	fail_unless(status == 0,
			"INTERNAL ERROR: 'never' matches host");

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_matcher_t *m;

		if (! strcmp(golden_data[i].op, "OR"))
			m = sdb_store_dis_matcher(golden_data[i].left,
					golden_data[i].right);
		else if (! strcmp(golden_data[i].op, "AND"))
			m = sdb_store_con_matcher(golden_data[i].left,
					golden_data[i].right);
		else {
			fail("INTERNAL ERROR: unexpected operator %s", golden_data[i].op);
			continue;
		}

#define TO_NAME(v) (((v) == always) ? "always" \
		: ((v) == never) ? "never" : "<unknown>")

		status = sdb_store_matcher_matches(m, obj, /* filter */ NULL);
		fail_unless(status == golden_data[i].expected,
				"%s(%s, %s, NULL) = %d; expected: %d", golden_data[i].op,
				TO_NAME(golden_data[i].left), TO_NAME(golden_data[i].right),
				status, golden_data[i].expected);

#undef TO_NAME

		sdb_object_deref(SDB_OBJ(m));
	}

	sdb_object_deref(SDB_OBJ(always));
	sdb_object_deref(SDB_OBJ(never));

	sdb_object_deref(SDB_OBJ(obj));
}
END_TEST

START_TEST(test_parse_cmp)
{
	sdb_data_t hostname   = { SDB_TYPE_STRING,  { .string  = "hostname" } };
	sdb_data_t metricname = { SDB_TYPE_STRING,  { .string  = "metricname" } };
	sdb_data_t srvname    = { SDB_TYPE_STRING,  { .string  = "srvname" } };
	sdb_data_t attrname   = { SDB_TYPE_STRING,  { .string  = "attrname" } };
	sdb_data_t attrvalue  = { SDB_TYPE_INTEGER, { .integer = 4711 } };

	sdb_store_matcher_t *check;

	size_t i;

	struct {
		const char *obj_type;
		const char *attr;
		const char *op;
		const sdb_data_t *value;
		int expected;
	} golden_data[] = {
		{ "host",      NULL,   "=",  &hostname,   MATCHER_NAME },
		{ "host",      NULL,   "!=", &hostname,   MATCHER_NOT },
		{ "host",      NULL,   "=~", &hostname,   MATCHER_NAME },
		{ "host",      NULL,   "!~", &hostname,   MATCHER_NOT },
		{ "host",      "attr", "=",  &hostname,   -1 },
		{ "host",      "attr", "!=", &hostname,   -1 },
		{ "host",      "attr", "!=", &attrvalue,  -1 },
		{ "host",      "attr", "<=", &attrvalue,  -1 },
		{ "host",      NULL,   "&^", &hostname,   -1 },
		{ "host",      NULL,   "<",  &hostname,   -1 },
		{ "host",      NULL,   "<=", &hostname,   -1 },
		{ "host",      NULL,   ">=", &hostname,   -1 },
		{ "host",      NULL,   ">",  &hostname,   -1 },
		{ "host",      NULL,   "=",  NULL,        -1 },
		{ "metric",    NULL,   "=",  &metricname, MATCHER_NAME },
		{ "metric",    NULL,   "!=", &metricname, MATCHER_NOT },
		{ "metric",    NULL,   "=~", &metricname, MATCHER_NAME },
		{ "metric",    NULL,   "!~", &metricname, MATCHER_NOT },
		{ "metric",    "attr", "=",  &metricname, -1 },
		{ "metric",    "attr", "!=", &metricname, -1 },
		{ "metric",    "attr", "!=", &attrvalue,  -1 },
		{ "metric",    "attr", "<=", &attrvalue,  -1 },
		{ "metric",    NULL,   "&^", &metricname, -1 },
		{ "metric",    NULL,   "<",  &metricname, -1 },
		{ "metric",    NULL,   "<=", &metricname, -1 },
		{ "metric",    NULL,   ">=", &metricname, -1 },
		{ "metric",    NULL,   ">",  &metricname, -1 },
		{ "metric",    NULL,   "=",  NULL,        -1 },
		{ "service",   NULL,   "=",  &srvname,    MATCHER_NAME },
		{ "service",   NULL,   "!=", &srvname,    MATCHER_NOT },
		{ "service",   NULL,   "=~", &srvname,    MATCHER_NAME },
		{ "service",   NULL,   "!~", &srvname,    MATCHER_NOT },
		{ "service",   "attr", "=",  &srvname,    -1 },
		{ "service",   "attr", "!=", &srvname,    -1 },
		{ "service",   "attr", "!=", &attrvalue,  -1 },
		{ "service",   "attr", "<=", &attrvalue,  -1 },
		{ "service",   NULL,   "&^", &srvname,    -1 },
		{ "service",   NULL,   "<",  &srvname,    -1 },
		{ "service",   NULL,   "<=", &srvname,    -1 },
		{ "service",   NULL,   ">=", &srvname,    -1 },
		{ "service",   NULL,   ">",  &srvname,    -1 },
		{ "service",   NULL,   "=",  NULL,        -1 },
		{ "attribute", NULL,   "=",  &attrname,   MATCHER_NAME },
		{ "attribute", NULL,   "!=", &attrname,   MATCHER_NOT },
		{ "attribute", NULL,   "=~", &attrname,   MATCHER_NAME },
		{ "attribute", NULL,   "!~", &attrname,   MATCHER_NOT },
		{ "attribute", NULL,   "<",  &attrname,   -1 },
		{ "attribute", NULL,   "<=", &attrname,   -1 },
		{ "attribute", NULL,   ">=", &attrname,   -1 },
		{ "attribute", NULL,   ">",  &attrname,   -1 },
		{ "attribute", NULL,   "=",  NULL,        -1 },
		{ "attribute", "attr", "=",  &attrname,   MATCHER_ATTR },
		{ "attribute", "attr", "!=", &attrname,   MATCHER_NOT },
		{ "attribute", "attr", "=~", &attrname,   MATCHER_ATTR },
		{ "attribute", "attr", "!~", &attrname,   MATCHER_NOT },
		{ "attribute", "attr", "&^", &attrname,   -1 },
		{ "attribute", "attr", "<",  NULL,        -1 },
		{ "attribute", "attr", "<",  &attrname,   MATCHER_LT },
		{ "attribute", "attr", "<=", &attrname,   MATCHER_LE },
/*		{ "attribute", "attr", "=",  &attrname,   MATCHER_EQ }, */
		{ "attribute", "attr", ">=", &attrname,   MATCHER_GE },
		{ "attribute", "attr", ">",  &attrname,   MATCHER_GT },
		{ "foo",       NULL,   "=",  &attrname,   -1 },
		{ "foo",       "attr", "=",  &attrname,   -1 },
	};

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_expr_t *expr;
		char buf[1024];

		if (sdb_data_format(golden_data[i].value,
					buf, sizeof(buf), SDB_UNQUOTED) < 0)
			snprintf(buf, sizeof(buf), "ERR");

		expr = sdb_store_expr_constvalue(golden_data[i].value);
		fail_unless(expr != NULL || golden_data[i].value == NULL,
				"sdb_store_expr_constvalue(%s) = NULL; expected: <expr>",
				buf);

		check = sdb_store_matcher_parse_cmp(golden_data[i].obj_type,
				golden_data[i].attr, golden_data[i].op, expr);
		sdb_object_deref(SDB_OBJ(expr));

		if (golden_data[i].expected == -1) {
			fail_unless(check == NULL,
					"sdb_store_matcher_parse_cmp(%s, %s, %s, expr{%s}) = %p; "
					"expected: NULL", golden_data[i].obj_type,
					golden_data[i].attr, golden_data[i].op, buf, check);
			continue;
		}

		fail_unless(check != NULL,
				"sdb_store_matcher_parse_cmp(%s, %s, %s, %s) = %p; "
				"expected: <expr>", golden_data[i].obj_type,
				golden_data[i].attr, golden_data[i].op, buf, check);
		fail_unless(M(check)->type == golden_data[i].expected,
				"sdb_store_matcher_parse_cmp(%s, %s, %s, %s) returned matcher "
				"of type %d; expected: %d", golden_data[i].obj_type,
				golden_data[i].attr, golden_data[i].op, buf,
				M(check)->type, golden_data[i].expected);

		sdb_object_deref(SDB_OBJ(check));
	}
}
END_TEST

START_TEST(test_parse_field_cmp)
{
	sdb_data_t datetime = { SDB_TYPE_DATETIME, { .datetime = 1 } };
	sdb_data_t string = { SDB_TYPE_STRING, { .string = "s" } };

	struct {
		const char *field;
		const char *op;
		const sdb_data_t *value;
		int expected;
	} golden_data[] = {
		{ "name",        "<",  &string,   MATCHER_LT },
		{ "name",        "<=", &string,   MATCHER_LE },
		{ "name",        "=",  &string,   MATCHER_EQ },
		{ "name",        ">=", &string,   MATCHER_GE },
		{ "name",        ">",  &string,   MATCHER_GT },
		{ "name",        "!=", &string,   MATCHER_NOT },
		{ "last_update", "<",  &datetime, MATCHER_LT },
		{ "last_update", "<=", &datetime, MATCHER_LE },
		{ "last_update", "=",  &datetime, MATCHER_EQ },
		{ "last_update", ">=", &datetime, MATCHER_GE },
		{ "last_update", ">",  &datetime, MATCHER_GT },
		{ "last_update", "!=", &datetime, MATCHER_NOT },
		{ "age",         "<",  &datetime, MATCHER_LT },
		{ "age",         "<=", &datetime, MATCHER_LE },
		{ "age",         "=",  &datetime, MATCHER_EQ },
		{ "age",         ">=", &datetime, MATCHER_GE },
		{ "age",         ">",  &datetime, MATCHER_GT },
		{ "age",         "!=", &datetime, MATCHER_NOT },
		{ "interval",    "<",  &datetime, MATCHER_LT },
		{ "interval",    "<=", &datetime, MATCHER_LE },
		{ "interval",    "=",  &datetime, MATCHER_EQ },
		{ "interval",    ">=", &datetime, MATCHER_GE },
		{ "interval",    ">",  &datetime, MATCHER_GT },
		{ "interval",    "!=", &datetime, MATCHER_NOT },
		{ "backend",     "=",  &string,   MATCHER_EQ },
		{ "backend",     "!=", &string,   MATCHER_NOT },
		/* the behavior for other operators on .backend
		 * is currently unspecified */
		{ "last_update", "=",  NULL,      -1 },
		{ "last_update", "IS", NULL,      -1 },
		{ "age",         "=",  NULL,      -1 },
		{ "interval",    "=",  NULL,      -1 },
		{ "backend",     "=",  NULL,      -1 },
		{ "backend",     "=~", &string,   -1 },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_matcher_t *check;
		sdb_store_expr_t *expr;
		char buf[1024];

		if (sdb_data_format(golden_data[i].value,
					buf, sizeof(buf), SDB_UNQUOTED) < 0)
			snprintf(buf, sizeof(buf), "ERR");

		expr = sdb_store_expr_constvalue(golden_data[i].value);
		fail_unless(expr != NULL || golden_data[i].value == NULL,
				"sdb_store_expr_constvalue(%s) = NULL; expected: <expr>",
				buf);

		check = sdb_store_matcher_parse_field_cmp(golden_data[i].field,
				golden_data[i].op, expr);
		sdb_object_deref(SDB_OBJ(expr));

		if (golden_data[i].expected == -1) {
			fail_unless(check == NULL,
					"sdb_store_matcher_parse_field_cmp(%s, %s, expr{%s}) = %p; "
					"expected: NULL", golden_data[i].field,
					golden_data[i].op, buf, check);
			continue;
		}

		fail_unless(check != NULL,
				"sdb_store_matcher_parse_field_cmp(%s, %s, %s) = %p; "
				"expected: NULL", golden_data[i].field,
				golden_data[i].op, buf, check);
		fail_unless(M(check)->type == golden_data[i].expected,
				"sdb_store_matcher_parse_field_cmp(%s, %s, %s) returned "
				"matcher of type %d; expected: %d", golden_data[i].field,
				golden_data[i].op, buf, M(check)->type,
				golden_data[i].expected);

		sdb_object_deref(SDB_OBJ(check));
	}
}
END_TEST

static int
scan_cb(sdb_store_obj_t *obj, void *user_data)
{
	int *i = user_data;

	fail_unless(obj != NULL,
			"sdb_store_scan callback received NULL obj; expected: "
			"<store base obj>");
	fail_unless(i != NULL,
			"sdb_store_scan callback received NULL user_data; "
			"expected: <pointer to data>");

	++(*i);
	return 0;
} /* scan_cb */

START_TEST(test_scan)
{
	struct {
		const char *query;
		const char *filter;
		int expected;
	} golden_data[] = {
		{ "host = 'a'", NULL,                  1 },
		{ "host = 'a'", "host = 'x'",          0 }, /* filter never matches */
		{ "host = 'a'",
			"NOT attribute['x'] = ''",         1 }, /* filter always matches */
		{ "host =~ 'a|b'", NULL,               2 },
		{ "host =~ 'host'", NULL,              0 },
		{ "host =~ '.'", NULL,                 3 },
		{ "metric = 'm1'", NULL,               2 },
		{ "metric= 'm1'", "host = 'x'",        0 }, /* filter never matches */
		{ "metric = 'm1'",
			"NOT attribute['x'] = ''",         2 }, /* filter always matches */
		{ "metric =~ 'm'", NULL,               2 },
		{ "metric !~ 'm'", NULL,               1 },
		{ "metric =~ 'x'", NULL,               0 },
		{ "service = 's1'", NULL,              2 },
		{ "service = 's1'", "host = 'x'",      0 }, /* filter never matches */
		{ "service = 's1'",
			"NOT attribute['x'] = ''",         2 }, /* filter always matches */
		{ "service =~ 's'", NULL,              2 },
		{ "service !~ 's'", NULL,              1 },
		{ "attribute = 'k1'", NULL,            2 },
		{ "attribute = 'k1'", "host = 'x'",    0 }, /* filter never matches */
		{ "attribute = 'k1'",
			"NOT attribute['x'] = ''",         2 }, /* filter always matches */
		{ "attribute = 'x'", NULL,             0 },
		{ "attribute['k1'] = 'v1'", NULL,      1 },
		{ "attribute['k1'] =~ 'v'", NULL,      2 },
		{ "attribute['k1'] !~ 'v'", NULL,      1 },
		{ "attribute['x1'] =~ 'v'", NULL,      0 },
		{ "attribute['x1'] =~ 'NULL'", NULL,   0 },
		{ "attribute['x1'] !~ 'v'", NULL,      3 },
		{ "attribute['k1'] IS NULL", NULL,     1 },
		{ "attribute['x1'] IS NULL", NULL,     3 },
		{ "attribute['k1'] IS NOT NULL", NULL, 2 },
		{ "attribute['x1'] IS NOT NULL", NULL, 0 },
		{ "attribute['k2'] < 123", NULL,       0 },
		{ "attribute['k2'] <= 123", NULL,      1 },
		{ "attribute['k2'] >= 123", NULL,      1 },
		{ "attribute['k2'] > 123", NULL,       0 },
		{ "attribute['k2'] = 123", NULL,       1 },
		{ "attribute['k2'] != 123", NULL,      0 },
		{ "attribute['k1'] != 'v1'", NULL,     1 },
		{ "attribute['k1'] != 'v2'", NULL,     1 },
		{ "attribute != 'x' "
		  "AND attribute['y'] !~ 'x'", NULL,   3 },
	};

	int check, n;
	size_t i;

	n = 0;
	check = sdb_store_scan(/* matcher */ NULL, /* filter */ NULL,
			scan_cb, &n);
	fail_unless(check == 0,
			"sdb_store_scan() = %d; expected: 0", check);
	fail_unless(n == 3,
			"sdb_store_scan called callback %d times; expected: 3", (int)n);

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_matcher_t *m, *filter = NULL;

		m = sdb_fe_parse_matcher(golden_data[i].query, -1);
		fail_unless(m != NULL,
				"sdb_fe_parse_matcher(%s, -1) = NULL; expected: <matcher>",
				golden_data[i].query);

		if (golden_data[i].filter) {
			filter = sdb_fe_parse_matcher(golden_data[i].filter, -1);
			fail_unless(filter != NULL,
					"sdb_fe_parse_matcher(%s, -1) = NULL; "
					"expected: <matcher>", golden_data[i].filter);
		}

		n = 0;
		sdb_store_scan(m, filter, scan_cb, &n);
		fail_unless(n == golden_data[i].expected,
				"sdb_store_scan(matcher{%s}, filter{%s}) found %d hosts; "
				"expected: %d", golden_data[i].query, golden_data[i].filter,
				n, golden_data[i].expected);
		sdb_object_deref(SDB_OBJ(filter));
		sdb_object_deref(SDB_OBJ(m));
	}
}
END_TEST

Suite *
core_store_lookup_suite(void)
{
	Suite *s = suite_create("core::store_lookup");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_checked_fixture(tc, populate, sdb_store_clear);
	tcase_add_test(tc, test_store_match_name);
	tcase_add_test(tc, test_store_match_attr);
	tcase_add_test(tc, test_attr_cond);
	tcase_add_test(tc, test_obj_cond);
	tcase_add_test(tc, test_store_match_op);
	tcase_add_test(tc, test_parse_cmp);
	tcase_add_test(tc, test_parse_field_cmp);
	tcase_add_test(tc, test_scan);
	suite_add_tcase(s, tc);

	return s;
} /* core_store_lookup_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

