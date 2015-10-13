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

#if HAVE_CONFIG_H
#	include "config.h"
#endif

#include "core/plugin.h"
#include "core/store.h"
#include "core/memstore-private.h"
#include "parser/parser.h"
#include "testutils.h"

#include <check.h>
#include <string.h>

static sdb_memstore_t *store;

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

	store = sdb_memstore_create();
	ck_assert(store != NULL);

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(hosts); ++i) {
		int status = sdb_memstore_host(store, hosts[i], 1, 0);
		fail_unless(status == 0,
				"sdb_memstore_host(%s, 1, 0) = %d; expected: 0",
				hosts[i], status);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(metrics); ++i) {
		int status = sdb_memstore_metric(store, metrics[i].host,
				metrics[i].metric, /* store */ NULL, 1, 0);
		fail_unless(status == 0,
				"sdb_memstore_metric(%s, %s, NULL, 1, 0) = %d; expected: 0",
				metrics[i].host, metrics[i].metric, status);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(services); ++i) {
		int status = sdb_memstore_service(store, services[i].host,
				services[i].service, 1, 0);
		fail_unless(status == 0,
				"sdb_memstore_service(%s, %s, 1, 0) = %d; expected: 0",
				services[i].host, services[i].service, status);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(attrs); ++i) {
		int status = sdb_memstore_attribute(store, attrs[i].host,
				attrs[i].name, &attrs[i].value, 1, 0);
		fail_unless(status == 0,
				"sdb_memstore_attribute(%s, %s, <val>, 1, 0) = %d; expected: 0",
				attrs[i].host, attrs[i].name, status);
	}
} /* populate */

static void
turndown(void)
{
	sdb_object_deref(SDB_OBJ(store));
	store = NULL;
} /* turndown */

struct {
	int type;
	char *name;
	_Bool re;

	int expected;
} cmp_name_data[] = {
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

START_TEST(test_cmp_name)
{
	sdb_memstore_obj_t *host;
	sdb_data_t datum;
	sdb_memstore_expr_t *obj = NULL, *value;
	sdb_memstore_matcher_t *m, *n;
	int status;

	host = sdb_memstore_get_host(store, "a");
	fail_unless(host != NULL,
			"sdb_memstore_get_host(a) = NULL; expected: <host>");

	datum.type = SDB_TYPE_STRING;
	datum.data.string = cmp_name_data[_i].name;

	if (cmp_name_data[_i].type == SDB_HOST) {
		obj = sdb_memstore_expr_fieldvalue(SDB_FIELD_NAME);
		fail_unless(obj != NULL,
				"sdb_memstore_expr_fieldvalue(SDB_STORE_NAME) = NULL; "
				"expected: <expr>");
	}
	value = sdb_memstore_expr_constvalue(&datum);
	fail_unless(value != NULL,
			"sdb_memstore_expr_constvalue(%s) = NULL; "
			"expected: <expr>", cmp_name_data[_i].name);

	if (cmp_name_data[_i].re)
		m = sdb_memstore_regex_matcher(obj, value);
	else
		m = sdb_memstore_eq_matcher(obj, value);

	if (cmp_name_data[_i].type != SDB_HOST) {
		sdb_memstore_expr_t *iter;
		sdb_memstore_matcher_t *tmp;
		obj = sdb_memstore_expr_fieldvalue(SDB_FIELD_NAME);
		iter = sdb_memstore_expr_typed(cmp_name_data[_i].type, obj);
		tmp = sdb_memstore_any_matcher(iter, m);
		ck_assert(iter && m);
		sdb_object_deref(SDB_OBJ(iter));
		sdb_object_deref(SDB_OBJ(m));
		m = tmp;
	}
	sdb_object_deref(SDB_OBJ(obj));
	sdb_object_deref(SDB_OBJ(value));
	fail_unless(m != NULL,
			"sdb_memstore_%s_matcher(%s, %s) = NULL; "
			"expected: <matcher>",
			cmp_name_data[_i].re ? "regex" : "eq",
			SDB_STORE_TYPE_TO_NAME(cmp_name_data[_i].type),
			cmp_name_data[_i].name);

	status = sdb_memstore_matcher_matches(m, host, /* filter */ NULL);
	fail_unless(status == cmp_name_data[_i].expected,
			"sdb_memstore_matcher_matches(%s->%s, <host a>, NULL) = %d; "
			"expected: %d", SDB_STORE_TYPE_TO_NAME(cmp_name_data[_i].type),
			cmp_name_data[_i].name, status, cmp_name_data[_i].expected);

	n = sdb_memstore_inv_matcher(m);
	fail_unless(n != NULL,
			"sdb_memstore_inv_matcher() = NULL; expected: <matcher>");
	sdb_object_deref(SDB_OBJ(m));

	/* now match the inverted set of objects */
	status = sdb_memstore_matcher_matches(n, host, /* filter */ NULL);
	fail_unless(status == !cmp_name_data[_i].expected,
			"sdb_memstore_matcher_matches(%s->%s, <host a>, NULL) = %d; "
			"expected: %d", SDB_STORE_TYPE_TO_NAME(cmp_name_data[_i].type),
			cmp_name_data[_i].name, status, !cmp_name_data[_i].expected);

	sdb_object_deref(SDB_OBJ(n));
	sdb_object_deref(SDB_OBJ(host));
}
END_TEST

struct {
	const char *attr;
	const sdb_data_t value;
	int expected_lt, expected_le, expected_eq, expected_ge, expected_gt;
} cmp_attr_data[] = {
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

START_TEST(test_cmp_attr)
{
	sdb_memstore_obj_t *host;
	sdb_memstore_expr_t *attr;
	sdb_memstore_expr_t *value;
	char value_str[1024];
	int status;
	size_t j;

	struct {
		sdb_memstore_matcher_t *(*matcher)(sdb_memstore_expr_t *,
				sdb_memstore_expr_t *);
		int expected;
	} tests[] = {
		{ sdb_memstore_lt_matcher, cmp_attr_data[_i].expected_lt },
		{ sdb_memstore_le_matcher, cmp_attr_data[_i].expected_le },
		{ sdb_memstore_eq_matcher, cmp_attr_data[_i].expected_eq },
		{ sdb_memstore_ge_matcher, cmp_attr_data[_i].expected_ge },
		{ sdb_memstore_gt_matcher, cmp_attr_data[_i].expected_gt },
	};

	const char *op_str[] = { "<", "<=", "=", ">=", ">" };
	ck_assert(SDB_STATIC_ARRAY_LEN(tests) == SDB_STATIC_ARRAY_LEN(op_str));

	host = sdb_memstore_get_host(store, "a");
	fail_unless(host != NULL,
			"sdb_memstore_get_host(a) = NULL; expected: <host>");

	sdb_data_format(&cmp_attr_data[_i].value,
			value_str, sizeof(value_str), SDB_UNQUOTED);

	attr = sdb_memstore_expr_attrvalue(cmp_attr_data[_i].attr);
	fail_unless(attr != NULL,
			"sdb_memstore_expr_attrvalue(%s) = NULL; expected: <expr>",
			cmp_attr_data[_i].attr);

	value = sdb_memstore_expr_constvalue(&cmp_attr_data[_i].value);
	fail_unless(value != NULL,
			"sdb_memstore_expr_constvalue(%s) = NULL; expected: <expr>",
			value_str);

	for (j = 0; j < SDB_STATIC_ARRAY_LEN(tests); ++j) {
		sdb_memstore_matcher_t *m;

		m = tests[j].matcher(attr, value);
		fail_unless(m != NULL,
				"sdb_memstore_<cond>_matcher() = NULL; expected: <matcher>");

		status = sdb_memstore_matcher_matches(m, host, /* filter */ NULL);
		fail_unless(status == tests[j].expected,
				"sdb_memstore_matcher_matches(<attr[%s] %s %s>, "
				"<host>, NULL) = %d; expected: %d",
				cmp_attr_data[_i].attr, op_str[j], value_str,
				status, tests[j].expected);

		sdb_object_deref(SDB_OBJ(m));
	}

	sdb_object_deref(SDB_OBJ(attr));
	sdb_object_deref(SDB_OBJ(value));
	sdb_object_deref(SDB_OBJ(host));
}
END_TEST

struct {
	const char *host;
	int field;
	const sdb_data_t value;
	int expected_lt, expected_le, expected_eq, expected_ge, expected_gt;
} cmp_obj_data[] = {
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

START_TEST(test_cmp_obj)
{
	sdb_memstore_obj_t *host;
	sdb_memstore_expr_t *field;
	sdb_memstore_expr_t *value;
	char value_str[1024];
	int status;
	size_t j;

	struct {
		sdb_memstore_matcher_t *(*matcher)(sdb_memstore_expr_t *,
				sdb_memstore_expr_t *);
		int expected;
	} tests[] = {
		{ sdb_memstore_lt_matcher, cmp_obj_data[_i].expected_lt },
		{ sdb_memstore_le_matcher, cmp_obj_data[_i].expected_le },
		{ sdb_memstore_eq_matcher, cmp_obj_data[_i].expected_eq },
		{ sdb_memstore_ge_matcher, cmp_obj_data[_i].expected_ge },
		{ sdb_memstore_gt_matcher, cmp_obj_data[_i].expected_gt },
	};
	char *op_str[] = { "<", "<=", "=", ">=", ">" };

	ck_assert(SDB_STATIC_ARRAY_LEN(tests) == SDB_STATIC_ARRAY_LEN(op_str));

	host = sdb_memstore_get_host(store, cmp_obj_data[_i].host);
	fail_unless(host != NULL,
			"sdb_memstore_get_host(%s) = NULL; expected: <host>",
			cmp_obj_data[_i].host);

	sdb_data_format(&cmp_obj_data[_i].value,
			value_str, sizeof(value_str), SDB_UNQUOTED);

	field = sdb_memstore_expr_fieldvalue(cmp_obj_data[_i].field);
	fail_unless(field != NULL,
			"sdb_memstore_expr_fieldvalue(%d) = NULL; "
			"expected: <expr>", cmp_obj_data[_i].field);

	value = sdb_memstore_expr_constvalue(&cmp_obj_data[_i].value);
	fail_unless(value != NULL,
			"sdb_memstore_expr_constvalue(%s) = NULL; "
			"expected: <expr>", value_str);

	for (j = 0; j < SDB_STATIC_ARRAY_LEN(tests); ++j) {
		char m_str[1024];
		sdb_memstore_matcher_t *m;

		snprintf(m_str, sizeof(m_str), "%s %s %s",
				SDB_FIELD_TO_NAME(cmp_obj_data[_i].field),
				op_str[j], value_str);

		m = tests[j].matcher(field, value);
		fail_unless(m != NULL,
				"sdb_memstore_<cond>_matcher() = NULL; expected: <matcher>");

		status = sdb_memstore_matcher_matches(m, host, /* filter */ NULL);
		fail_unless(status == tests[j].expected,
				"sdb_memstore_matcher_matches(<%s>, <host '%s'>, NULL) = %d; "
				"expected: %d", m_str, cmp_obj_data[_i].host, status,
				tests[j].expected);

		sdb_object_deref(SDB_OBJ(m));
	}

	sdb_object_deref(SDB_OBJ(field));
	sdb_object_deref(SDB_OBJ(value));
	sdb_object_deref(SDB_OBJ(host));
}
END_TEST

START_TEST(test_store_match_op)
{
	sdb_memstore_obj_t *obj;

	sdb_data_t d = { SDB_TYPE_STRING, { .string = "a" } };
	sdb_memstore_expr_t *e = sdb_memstore_expr_constvalue(&d);

	sdb_memstore_matcher_t *never = sdb_memstore_isnull_matcher(e);
	sdb_memstore_matcher_t *always = sdb_memstore_inv_matcher(never);

	struct {
		const char *op;
		sdb_memstore_matcher_t *left;
		sdb_memstore_matcher_t *right;
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

	obj = sdb_memstore_get_host(store, "a");

	status = sdb_memstore_matcher_matches(always, obj, /* filter */ NULL);
	fail_unless(status == 1,
			"INTERNAL ERROR: 'always' did not match host");
	status = sdb_memstore_matcher_matches(never, obj, /* filter */ NULL);
	fail_unless(status == 0,
			"INTERNAL ERROR: 'never' matches host");

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_memstore_matcher_t *m;

		if (! strcmp(golden_data[i].op, "OR"))
			m = sdb_memstore_dis_matcher(golden_data[i].left,
					golden_data[i].right);
		else if (! strcmp(golden_data[i].op, "AND"))
			m = sdb_memstore_con_matcher(golden_data[i].left,
					golden_data[i].right);
		else {
			fail("INTERNAL ERROR: unexpected operator %s", golden_data[i].op);
			continue;
		}

#define TO_NAME(v) (((v) == always) ? "always" \
		: ((v) == never) ? "never" : "<unknown>")

		status = sdb_memstore_matcher_matches(m, obj, /* filter */ NULL);
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
scan_cb(sdb_memstore_obj_t *obj, sdb_memstore_matcher_t *filter, void *user_data)
{
	int *i = user_data;

	if (! sdb_memstore_matcher_matches(filter, obj, NULL))
		return 0;

	fail_unless(obj != NULL,
			"sdb_memstore_scan callback received NULL obj; expected: "
			"<store base obj>");
	fail_unless(i != NULL,
			"sdb_memstore_scan callback received NULL user_data; "
			"expected: <pointer to data>");

	++(*i);
	return 0;
} /* scan_cb */

struct {
	const char *query;
	const char *filter;
	int expected;
} scan_data[] = {
	/* TODO: check the name of the expected hosts */
	{ "name = 'a'", NULL,                        1 },
	{ "name = 'a'", "name = 'x'",                0 }, /* filter never matches */
	{ "name = 'a'",
		"NOT attribute['x'] = ''",               1 }, /* filter always matches */
	{ "name =~ 'a|b'", NULL,                     2 },
	{ "name =~ 'host'", NULL,                    0 },
	{ "name =~ '.'", NULL,                       3 },
	{ "ANY backend = 'backend'", NULL,           0 },
	{ "ALL backend = ''", NULL,                  3 }, /* backend is empty */
	{ "backend = ['backend']", NULL,             0 },
	{ "backend != ['backend']", NULL,            3 },
	{ "backend < ['backend']", NULL,             3 },
	{ "backend <= ['backend']", NULL,            3 },
	{ "backend >= ['backend']", NULL,            0 },
	{ "backend > ['backend']", NULL,             0 },
	{ "ANY metric.name = 'm1'", NULL,            2 },
	{ "ANY metric.name = 'm1'", "name = 'x'",    0 }, /* filter never matches */
	{ "ANY metric.name = 'm1'",
		"NOT attribute['x'] = ''",               2 }, /* filter always matches */
	{ "ANY metric.name =~ 'm'", NULL,            2 },
	{ "ALL metric.name =~ 'm'", NULL,            3 },
	{ "ANY metric.name =~ 'm'", "name !~ '1'",   1 },
	{ "ANY metric.name =~ 'm'", "name !~ 'm'",   0 },
	{ "ALL metric.name =~ '1'", NULL,            2 },
	{ "ALL metric.name =~ '2'", NULL,            1 },
	{ "ANY metric.name !~ 'm'", NULL,            0 },
	{ "ALL metric.name !~ 'm'", NULL,            1 },
	{ "ANY metric.name =~ 'x'", NULL,            0 },
	{ "ANY service.name = 's1'", NULL,           2 },
	{ "ANY service.name = 's1'", "name = 'x'",   0 }, /* filter never matches */
	{ "ANY service.name = 's1'",
		"NOT attribute['x'] = ''",               2 }, /* filter always matches */
	{ "ANY service.name =~ 's'", NULL,           2 },
	{ "ANY service.name =~ 's'", "name !~ 's'",  0 },
	{ "ANY service.name =~ 's'", "name !~ '1'",  2 },
	{ "ANY service.name !~ 's'", NULL,           0 },
	{ "ANY attribute.name = 'k1'", NULL,         2 },
	{ "ANY attribute.name = 'k1'", "name = 'x'", 0 }, /* filter never matches */
	{ "ANY attribute.name = 'k1'",
		"NOT attribute['x'] = ''",         2 }, /* filter always matches */
	{ "ANY attribute.name =~ 'k'", NULL,   2 },
	{ "ANY attribute.name =~ 'k'",
		"name !~ '1'",                     1 },
	{ "ANY attribute.name =~ 'k'",
		"name !~ 'k'",                     0 },
	{ "ANY attribute.name =~ '1'", NULL,   2 },
	{ "ANY attribute.name =~ '2'", NULL,   1 },
	{ "ANY attribute.name = 'x'", NULL,    0 },
	{ "ANY attribute.name =~ 'x'", NULL,   0 },
	{ "ALL attribute.name = 'k1'", NULL,   2 },
	{ "ANY attribute.value = 'v1'", NULL,  1 },
	{ "ANY attribute.value =~ 'v'", NULL,  2 },
	{ "ANY attribute.value = 123", NULL,   1 },
	{ "host.name = 'a'", NULL,             1 },
	{ "host.attribute['k1'] =~ 'v1'",
		NULL,                              1 },
	{ "host.attribute['x1'] IS NULL",
		NULL,                              3 },
	/* not a boolean so neither TRUE nor FALSE: */
	{ "attribute['k1'] IS TRUE", NULL,     0 },
	{ "attribute['k1'] IS FALSE", NULL,    0 },
	{ "attribute['k1'] = 'v1'", NULL,      1 },
	{ "attribute['k1'] = 'v1'",
		"name != 'k1'",                    0 },
	{ "attribute['k1'] =~ 'v1'", NULL,     1 },
	{ "attribute['k1'] =~ '^v1$'", NULL,   1 },
	{ "attribute['k1'] =~ 'v'", NULL,      2 },
	{ "attribute['k1'] =~ '1'", NULL,      1 },
	{ "attribute['k1'] !~ 'v'", NULL,      0 },
	{ "attribute['k1'] = 'v2'", NULL,      1 },
	{ "attribute['k1'] =~ 'v2'", NULL,     1 },
	{ "attribute['x1'] =~ 'v'", NULL,      0 },
	{ "attribute['x1'] =~ 'NULL'", NULL,   0 },
	{ "attribute['x1'] !~ 'v'", NULL,      0 },
	{ "attribute['k1'] IS NULL", NULL,     1 },
	{ "attribute['x1'] IS NULL", NULL,     3 },
	{ "attribute['k1'] IS TRUE", NULL,     0 },
	{ "attribute['x1'] IS TRUE", NULL,     0 },
	{ "attribute['k1'] IS FALSE", NULL,    0 },
	{ "attribute['x1'] IS FALSE", NULL,    0 },
	{ "attribute['k1'] IS NOT NULL", NULL, 2 },
	{ "attribute['x1'] IS NOT NULL", NULL, 0 },
	{ "attribute['x1'] IS NOT TRUE", NULL, 3 },
	{ "attribute['k2'] < 123", NULL,       0 },
	{ "attribute['k2'] <= 123", NULL,      1 },
	{ "attribute['k2'] >= 123", NULL,      1 },
	{ "attribute['k2'] > 123", NULL,       0 },
	{ "attribute['k2'] = 123", NULL,       1 },
	{ "attribute['k2'] != 123", NULL,      0 },
	{ "attribute['k1'] != 'v1'", NULL,     1 },
	{ "attribute['k1'] != 'v2'", NULL,     1 },
	{ "ANY attribute.name != 'x' "
	  "AND attribute['k1'] !~ 'x'", NULL,  2 },
};

START_TEST(test_scan)
{
	sdb_strbuf_t *errbuf = sdb_strbuf_create(64);
	sdb_memstore_matcher_t *m, *filter = NULL;
	sdb_ast_node_t *ast;
	int check, n;

	n = 0;
	check = sdb_memstore_scan(store, SDB_HOST,
			/* matcher */ NULL, /* filter */ NULL,
			scan_cb, &n);
	fail_unless(check == 0,
			"sdb_memstore_scan() = %d; expected: 0", check);
	fail_unless(n == 3,
			"sdb_memstore_scan called callback %d times; expected: 3", (int)n);

	ast = sdb_parser_parse_conditional(SDB_HOST, scan_data[_i].query, -1, errbuf);
	m = sdb_memstore_query_prepare_matcher(ast);
	sdb_object_deref(SDB_OBJ(ast));
	fail_unless(m != NULL,
			"sdb_parser_parse_conditional(HOST, %s, -1) = NULL; expected: <ast> "
			"(parser error: %s)", scan_data[_i].query,
			sdb_strbuf_string(errbuf));

	if (scan_data[_i].filter) {
		ast = sdb_parser_parse_conditional(SDB_HOST, scan_data[_i].filter, -1, errbuf);
		filter = sdb_memstore_query_prepare_matcher(ast);
		sdb_object_deref(SDB_OBJ(ast));
		fail_unless(filter != NULL,
				"sdb_parser_parse_conditional(HOST, %s, -1) = NULL; "
				"expected: <ast> (parser error: %s)",
				scan_data[_i].filter, sdb_strbuf_string(errbuf));
	}

	n = 0;
	sdb_memstore_scan(store, SDB_HOST, m, filter, scan_cb, &n);
	fail_unless(n == scan_data[_i].expected,
			"sdb_memstore_scan(HOST, matcher{%s}, filter{%s}) "
			"found %d hosts; expected: %d", scan_data[_i].query,
			scan_data[_i].filter, n, scan_data[_i].expected);

	sdb_object_deref(SDB_OBJ(filter));
	sdb_object_deref(SDB_OBJ(m));
	sdb_strbuf_destroy(errbuf);
}
END_TEST

TEST_MAIN("core::store_lookup")
{
	TCase *tc = tcase_create("core");
	tcase_add_checked_fixture(tc, populate, turndown);
	TC_ADD_LOOP_TEST(tc, cmp_name);
	TC_ADD_LOOP_TEST(tc, cmp_attr);
	TC_ADD_LOOP_TEST(tc, cmp_obj);
	TC_ADD_LOOP_TEST(tc, scan);
	tcase_add_test(tc, test_store_match_op);
	ADD_TCASE(tc);
}
TEST_MAIN_END

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

