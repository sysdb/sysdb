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

#include <assert.h>

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

START_TEST(test_cmp_name)
{
	sdb_store_obj_t *host;

	struct {
		int type;
		char *name;
		_Bool re;

		int expected;
	} golden_data[] = {
		{ SDB_HOST,      "a",    0, 1 },
		{ SDB_HOST,      "a",    1, 1 },
		{ SDB_HOST,      "b",    0, 0 },
		{ SDB_HOST,      "b",    1, 0 },
		{ SDB_HOST,      "^a$",  1, 1 },
		{ SDB_HOST,      "^b$",  1, 0 },
		{ SDB_HOST,      "^a$",  0, 0 },
		{ SDB_HOST,      "^b$",  0, 0 },
		{ SDB_METRIC,    "m1",   0, 1 },
		{ SDB_METRIC,    "m1",   1, 1 },
		{ SDB_METRIC,    "^m1$", 1, 1 },
		{ SDB_METRIC,    "m",    1, 1 },
		{ SDB_METRIC,    "s",    1, 0 },
		{ SDB_METRIC,    "m2",   0, 0 },
		{ SDB_METRIC,    "x1",   0, 0 },
		{ SDB_METRIC,    "x1",   1, 0 },
		{ SDB_SERVICE,   "s1",   0, 1 },
		{ SDB_SERVICE,   "s2",   0, 1 },
		{ SDB_SERVICE,   "s3",   0, 0 },
		{ SDB_SERVICE,   "s4",   0, 0 },
		{ SDB_SERVICE,   "^s1$", 1, 1 },
		{ SDB_SERVICE,   "^s1$", 0, 0 },
		{ SDB_SERVICE,   "x1",   0, 0 },
		{ SDB_SERVICE,   "x1",   1, 0 },
		{ SDB_SERVICE,   "x",    1, 0 },
		{ SDB_ATTRIBUTE, "k1",   0, 1 },
		{ SDB_ATTRIBUTE, "k2",   0, 1 },
		{ SDB_ATTRIBUTE, "k3",   0, 0 },
		{ SDB_ATTRIBUTE, "k4",   0, 0 },
		{ SDB_ATTRIBUTE, "k",    1, 1 },
		{ SDB_ATTRIBUTE, "1",    1, 1 },
		{ SDB_ATTRIBUTE, "3",    1, 0 },
	};

	size_t i;

	host = sdb_store_get_host("a");
	fail_unless(host != NULL,
			"sdb_store_get_host(a) = NULL; expected: <host>");

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_data_t datum;
		sdb_store_expr_t *obj, *value;
		sdb_store_matcher_t *m, *n;
		int status;

		datum.type = SDB_TYPE_STRING;
		datum.data.string = golden_data[i].name;

		obj = sdb_store_expr_fieldvalue(SDB_FIELD_NAME);
		fail_unless(obj != NULL,
				"sdb_store_expr_fieldvalue(SDB_STORE_NAME) = NULL; "
				"expected: <expr>");
		value = sdb_store_expr_constvalue(&datum);
		fail_unless(value != NULL,
				"sdb_store_expr_constvalue(%s) = NULL; "
				"expected: <expr>", golden_data[i].name);

		if (golden_data[i].re)
			m = sdb_store_regex_matcher(obj, value);
		else
			m = sdb_store_eq_matcher(obj, value);
		if (golden_data[i].type != SDB_HOST) {
			sdb_store_matcher_t *tmp;
			tmp = sdb_store_any_matcher(golden_data[i].type, m);
			sdb_object_deref(SDB_OBJ(m));
			m = tmp;
		}
		sdb_object_deref(SDB_OBJ(obj));
		sdb_object_deref(SDB_OBJ(value));
		fail_unless(m != NULL,
				"sdb_store_%s_matcher(%s, %s) = NULL; "
				"expected: <matcher>",
				golden_data[i].re ? "regex" : "eq",
				SDB_STORE_TYPE_TO_NAME(golden_data[i].type),
				golden_data[i].name);

		status = sdb_store_matcher_matches(m, host, /* filter */ NULL);
		fail_unless(status == golden_data[i].expected,
				"sdb_store_matcher_matches(%s->%s, <host a>, NULL) = %d; "
				"expected: %d", SDB_STORE_TYPE_TO_NAME(golden_data[i].type),
				golden_data[i].name, status, golden_data[i].expected);

		n = sdb_store_inv_matcher(m);
		fail_unless(n != NULL,
				"sdb_store_inv_matcher() = NULL; expected: <matcher>");
		sdb_object_deref(SDB_OBJ(m));

		/* now match the inverted set of objects */
		status = sdb_store_matcher_matches(n, host, /* filter */ NULL);
		fail_unless(status == !golden_data[i].expected,
				"sdb_store_matcher_matches(%s->%s, <host a>, NULL) = %d; "
				"expected: %d", SDB_STORE_TYPE_TO_NAME(golden_data[i].type),
				golden_data[i].name, status, !golden_data[i].expected);

		sdb_object_deref(SDB_OBJ(n));
	}

	sdb_object_deref(SDB_OBJ(host));
}
END_TEST

START_TEST(test_cmp_attr)
{
	sdb_store_obj_t *host;

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

	host = sdb_store_get_host("a");
	fail_unless(host != NULL,
			"sdb_store_get_host(a) = NULL; expected: <host>");

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_expr_t *attr;
		sdb_store_expr_t *value;
		char value_str[1024];
		size_t j;

		struct {
			sdb_store_matcher_t *(*matcher)(sdb_store_expr_t *,
					sdb_store_expr_t *);
			int expected;
		} tests[] = {
			{ sdb_store_lt_matcher, golden_data[i].expected_lt },
			{ sdb_store_le_matcher, golden_data[i].expected_le },
			{ sdb_store_eq_matcher, golden_data[i].expected_eq },
			{ sdb_store_ge_matcher, golden_data[i].expected_ge },
			{ sdb_store_gt_matcher, golden_data[i].expected_gt },
		};

		const char *op_str[] = { "<", "<=", "=", ">=", ">" };
		assert(SDB_STATIC_ARRAY_LEN(tests) == SDB_STATIC_ARRAY_LEN(op_str));

		sdb_data_format(&golden_data[i].value,
				value_str, sizeof(value_str), SDB_UNQUOTED);

		attr = sdb_store_expr_attrvalue(golden_data[i].attr);
		fail_unless(attr != NULL,
				"sdb_store_expr_attrvalue(%s) = NULL; expected: <expr>",
				golden_data[i].attr);

		value = sdb_store_expr_constvalue(&golden_data[i].value);
		fail_unless(value != NULL,
				"sdb_store_expr_constvalue(%s) = NULL; expected: <expr>",
				value_str);

		for (j = 0; j < SDB_STATIC_ARRAY_LEN(tests); ++j) {
			sdb_store_matcher_t *m;

			m = tests[j].matcher(attr, value);
			fail_unless(m != NULL,
					"sdb_store_<cond>_matcher() = NULL; expected: <matcher>");

			status = sdb_store_matcher_matches(m, host, /* filter */ NULL);
			fail_unless(status == tests[j].expected,
					"sdb_store_matcher_matches(<attr[%s] %s %s>, "
					"<host>, NULL) = %d; expected: %d",
					golden_data[i].attr, op_str[j], value_str,
					status, tests[j].expected);

			sdb_object_deref(SDB_OBJ(m));
		}

		sdb_object_deref(SDB_OBJ(attr));
		sdb_object_deref(SDB_OBJ(value));
	}

	sdb_object_deref(SDB_OBJ(host));
}
END_TEST

START_TEST(test_cmp_obj)
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
		sdb_store_obj_t *host;
		sdb_store_expr_t *field;
		sdb_store_expr_t *value;
		char value_str[1024];
		size_t j;

		struct {
			sdb_store_matcher_t *(*matcher)(sdb_store_expr_t *,
					sdb_store_expr_t *);
			int expected;
		} tests[] = {
			{ sdb_store_lt_matcher, golden_data[i].expected_lt },
			{ sdb_store_le_matcher, golden_data[i].expected_le },
			{ sdb_store_eq_matcher, golden_data[i].expected_eq },
			{ sdb_store_ge_matcher, golden_data[i].expected_ge },
			{ sdb_store_gt_matcher, golden_data[i].expected_gt },
		};
		char *op_str[] = { "<", "<=", "=", ">=", ">" };

		assert(SDB_STATIC_ARRAY_LEN(tests) == SDB_STATIC_ARRAY_LEN(op_str));

		host = sdb_store_get_host(golden_data[i].host);
		fail_unless(host != NULL,
				"sdb_store_get_host(%s) = NULL; expected: <host>",
				golden_data[i].host);

		sdb_data_format(&golden_data[i].value,
				value_str, sizeof(value_str), SDB_UNQUOTED);

		field = sdb_store_expr_fieldvalue(golden_data[i].field);
		fail_unless(field != NULL,
				"sdb_store_expr_fieldvalue(%d) = NULL; "
				"expected: <expr>", golden_data[i].field);

		value = sdb_store_expr_constvalue(&golden_data[i].value);
		fail_unless(value != NULL,
				"sdb_store_expr_constvalue(%s) = NULL; "
				"expected: <expr>", value_str);

		for (j = 0; j < SDB_STATIC_ARRAY_LEN(tests); ++j) {
			char m_str[1024];
			sdb_store_matcher_t *m;

			snprintf(m_str, sizeof(m_str), "%s %s %s",
					SDB_FIELD_TO_NAME(golden_data[i].field),
					op_str[j], value_str);

			m = tests[j].matcher(field, value);
			fail_unless(m != NULL,
					"sdb_store_<cond>_matcher() = NULL; expected: <matcher>");

			status = sdb_store_matcher_matches(m, host, /* filter */ NULL);
			fail_unless(status == tests[j].expected,
					"sdb_store_matcher_matches(<%s>, <host '%s'>, NULL) = %d; "
					"expected: %d", m_str, golden_data[i].host, status,
					tests[j].expected);

			sdb_object_deref(SDB_OBJ(m));
		}

		sdb_object_deref(SDB_OBJ(field));
		sdb_object_deref(SDB_OBJ(value));
		sdb_object_deref(SDB_OBJ(host));
	}
}
END_TEST

START_TEST(test_store_match_op)
{
	sdb_store_obj_t *obj;

	sdb_data_t d = { SDB_TYPE_STRING, { .string = "a" } };
	sdb_store_expr_t *e = sdb_store_expr_constvalue(&d);

	sdb_store_matcher_t *always = sdb_store_isnnull_matcher(e);
	sdb_store_matcher_t *never = sdb_store_isnull_matcher(e);

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
	sdb_object_deref(SDB_OBJ(e));

	sdb_object_deref(SDB_OBJ(obj));
}
END_TEST

static int
scan_cb(sdb_store_obj_t *obj, sdb_store_matcher_t *filter, void *user_data)
{
	int *i = user_data;

	if (! sdb_store_matcher_matches(filter, obj, NULL))
		return 0;

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
		/* TODO: check the name of the expected hosts */
		{ "host = 'a'", NULL,                  1 },
		{ "host = 'a'", "host = 'x'",          0 }, /* filter never matches */
		{ "host = 'a'",
			"NOT attribute['x'] = ''",         1 }, /* filter always matches */
		{ "host =~ 'a|b'", NULL,               2 },
		{ "host =~ 'host'", NULL,              0 },
		{ "host =~ '.'", NULL,                 3 },
		{ "ANY metric = 'm1'", NULL,           2 },
		{ "ANY metric= 'm1'", "host = 'x'",    0 }, /* filter never matches */
		{ "ANY metric = 'm1'",
			"NOT attribute['x'] = ''",         2 }, /* filter always matches */
		{ "ANY metric =~ 'm'", NULL,           2 },
		{ "ALL metric =~ 'm'", NULL,           3 },
		{ "ALL metric =~ '1'", NULL,           2 },
		{ "ALL metric =~ '2'", NULL,           1 },
		{ "ANY metric !~ 'm'", NULL,           0 },
		{ "ALL metric !~ 'm'", NULL,           1 },
		{ "ANY metric =~ 'x'", NULL,           0 },
		{ "ANY service = 's1'", NULL,          2 },
		{ "ANY service = 's1'", "host = 'x'",  0 }, /* filter never matches */
		{ "ANY service = 's1'",
			"NOT attribute['x'] = ''",         2 }, /* filter always matches */
		{ "ANY service =~ 's'", NULL,          2 },
		{ "ANY service !~ 's'", NULL,          0 },
		{ "ANY attribute = 'k1'", NULL,        2 },
		{ "ANY attribute = 'k1'", "host = 'x'",0 }, /* filter never matches */
		{ "ANY attribute = 'k1'",
			"NOT attribute['x'] = ''",         2 }, /* filter always matches */
		{ "ANY attribute =~ 'k'", NULL,        2 },
		{ "ANY attribute =~ '1'", NULL,        2 },
		{ "ANY attribute =~ '2'", NULL,        1 },
		{ "ANY attribute = 'x'", NULL,         0 },
		{ "ANY attribute =~ 'x'", NULL,        0 },
		{ "ALL attribute = 'k1'", NULL,        2 },
		{ "attribute['k1'] = 'v1'", NULL,      1 },
		{ "attribute['k1'] =~ 'v1'", NULL,     1 },
		{ "attribute['k1'] =~ '^v1$'", NULL,   1 },
		{ "attribute['k1'] =~ 'v'", NULL,      2 },
		{ "attribute['k1'] =~ '1'", NULL,      1 },
		{ "attribute['k1'] !~ 'v'", NULL,      1 },
		{ "attribute['k1'] = 'v2'", NULL,      1 },
		{ "attribute['k1'] =~ 'v2'", NULL,     1 },
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
		{ "ANY attribute != 'x' "
		  "AND attribute['y'] !~ 'x'", NULL,   2 },
	};

	int check, n;
	size_t i;

	n = 0;
	check = sdb_store_scan(SDB_HOST, /* matcher */ NULL, /* filter */ NULL,
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
		sdb_store_scan(SDB_HOST, m, filter, scan_cb, &n);
		fail_unless(n == golden_data[i].expected,
				"sdb_store_scan(HOST, matcher{%s}, filter{%s}) "
				"found %d hosts; expected: %d", golden_data[i].query,
				golden_data[i].filter, n, golden_data[i].expected);
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
	tcase_add_test(tc, test_cmp_name);
	tcase_add_test(tc, test_cmp_attr);
	tcase_add_test(tc, test_cmp_obj);
	tcase_add_test(tc, test_store_match_op);
	tcase_add_test(tc, test_scan);
	suite_add_tcase(s, tc);

	return s;
} /* core_store_lookup_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

