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
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(hosts); ++i) {
		int status = sdb_store_host(hosts[i], 1);
		fail_unless(status == 0,
				"sdb_store_host(%s, 1) = %d; expected: 0",
				hosts[i], status);
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
	sdb_store_base_t *obj;

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
		char buf[1024];
		int status;

		m = sdb_store_name_matcher(golden_data[i].type,
				golden_data[i].name, golden_data[i].re);
		fail_unless(m != NULL,
				"sdb_store_service_matcher(%d, %s, %d) = NULL; expected: <matcher>",
				golden_data[i].type, golden_data[i].name, golden_data[i].re);

		status = sdb_store_matcher_matches(m, obj);
		fail_unless(status == golden_data[i].expected,
				"sdb_store_matcher_matches(%s, <host a>) = %d; expected: %d",
				sdb_store_matcher_tostring(m, buf, sizeof(buf)),
				status, golden_data[i].expected);

		n = sdb_store_inv_matcher(m);
		fail_unless(n != NULL,
				"sdb_store_inv_matcher() = NULL; expected: <matcher>");
		sdb_object_deref(SDB_OBJ(m));

		/* now match the inverted set of objects */
		status = sdb_store_matcher_matches(n, obj);
		fail_unless(status == !golden_data[i].expected,
				"sdb_store_matcher_matches(%s, <host a>) = %d; expected: %d",
				sdb_store_matcher_tostring(n, buf, sizeof(buf)),
				status, !golden_data[i].expected);

		sdb_object_deref(SDB_OBJ(n));
	}

	sdb_object_deref(SDB_OBJ(obj));
}
END_TEST

START_TEST(test_store_match_attr)
{
	sdb_store_base_t *obj;

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
		char buf[1024];
		int status;

		m = sdb_store_attr_matcher(golden_data[i].attr_name,
				golden_data[i].attr_value, golden_data[i].re);
		fail_unless(m != NULL,
				"sdb_store_attr_matcher() = NULL; expected: <matcher>");

		status = sdb_store_matcher_matches(m, obj);
		fail_unless(status == golden_data[i].expected,
				"sdb_store_matcher_matches({%s, <host a>) = %d; expected: %d",
				sdb_store_matcher_tostring(m, buf, sizeof(buf)),
				status, golden_data[i].expected);

		n = sdb_store_inv_matcher(m);
		fail_unless(n != NULL,
				"sdb_store_inv_matcher() = NULL; expected: <matcher>");
		sdb_object_deref(SDB_OBJ(m));

		/* now match the inverted set of objects */
		status = sdb_store_matcher_matches(n, obj);
		fail_unless(status == !golden_data[i].expected,
				"sdb_store_matcher_matches({%s, <host a>) = %d; expected: %d",
				sdb_store_matcher_tostring(n, buf, sizeof(buf)),
				status, !golden_data[i].expected);

		sdb_object_deref(SDB_OBJ(n));
	}

	sdb_object_deref(SDB_OBJ(obj));
}
END_TEST

START_TEST(test_store_cond)
{
	sdb_store_base_t *obj;

	struct {
		const char *attr;
		const sdb_data_t value;
		int expected_lt, expected_le, expected_eq, expected_ge, expected_gt;
	} golden_data[] = {
		{ "k1", { SDB_TYPE_STRING,  { .string  = "v1" } },  0, 1, 1, 1, 0 },
		{ "k1", { SDB_TYPE_STRING,  { .string  = "v2" } },  1, 1, 0, 0, 0 },
		{ "k1", { SDB_TYPE_STRING,  { .string  = "v0" } },  0, 0, 0, 1, 1 },
		{ "k2", { SDB_TYPE_INTEGER, { .integer = 123 } },   0, 1, 1, 1, 0 },
		{ "k2", { SDB_TYPE_INTEGER, { .integer = 124 } },   1, 1, 0, 0, 0 },
		{ "k2", { SDB_TYPE_INTEGER, { .integer = 122 } },   0, 0, 0, 1, 1 },
		/* key does not exist */
		{ "k3", { SDB_TYPE_STRING,  { .string  = "v1" } },  0, 0, 0, 0, 0 },
		{ "k3", { SDB_TYPE_STRING,  { .string  = "123" } }, 0, 0, 0, 0, 0 },
		{ "k3", { SDB_TYPE_INTEGER, { .integer = 123 } },   0, 0, 0, 0, 0 },
		/* type mismatch */
		{ "k1", { SDB_TYPE_INTEGER, { .integer = 0 } },     0, 0, 0, 0, 0 },
		{ "k2", { SDB_TYPE_STRING,  { .string  = "123" } }, 0, 0, 0, 0, 0 },
	};

	int status;
	size_t i;

	obj = sdb_store_get_host("a");
	fail_unless(obj != NULL,
			"sdb_store_get_host(a) = NULL; expected: <host>");

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_cond_t *c;
		char buf[1024];
		size_t j;

		struct {
			sdb_store_matcher_t *(*matcher)(sdb_store_cond_t *);
			int *expected;
		} tests[] = {
			{ sdb_store_lt_matcher, &golden_data[i].expected_lt },
			{ sdb_store_le_matcher, &golden_data[i].expected_le },
			{ sdb_store_eq_matcher, &golden_data[i].expected_eq },
			{ sdb_store_ge_matcher, &golden_data[i].expected_ge },
			{ sdb_store_gt_matcher, &golden_data[i].expected_gt },
		};

		sdb_data_format(&golden_data[i].value,
				buf, sizeof(buf), SDB_UNQUOTED);

		c = sdb_store_attr_cond(golden_data[i].attr,
				&golden_data[i].value);
		fail_unless(c != NULL,
				"sdb_store_attr_cond(%s, %s) = NULL; expected: <cond>",
				golden_data[i].attr, buf);

		for (j = 0; j < SDB_STATIC_ARRAY_LEN(tests); ++j) {
			sdb_store_matcher_t *m;
			char m_str[1024];

			m = tests[j].matcher(c);
			fail_unless(m != NULL,
					"sdb_store_<cond>_matcher() = NULL; expected: <matcher>");

			status = sdb_store_matcher_matches(m, obj);
			fail_unless(status == *tests[j].expected,
					"sdb_store_matcher_matches(%s) = %d; expected: %d",
					sdb_store_matcher_tostring(m, m_str, sizeof(m_str)),
					status, *tests[j].expected);

			sdb_object_deref(SDB_OBJ(m));
		}

		sdb_object_deref(SDB_OBJ(c));
	}

	sdb_object_deref(SDB_OBJ(obj));
}
END_TEST

START_TEST(test_store_match_op)
{
	sdb_store_base_t *obj;

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

	status = sdb_store_matcher_matches(always, obj);
	fail_unless(status == 1,
			"INTERNAL ERROR: 'always' did not match host");
	status = sdb_store_matcher_matches(never, obj);
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

		status = sdb_store_matcher_matches(m, obj);
		fail_unless(status == golden_data[i].expected,
				"%s(%s, %s) = %d; expected: %d", golden_data[i].op,
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
	sdb_data_t hostname = { SDB_TYPE_STRING, { .string = "hostname" } };
	sdb_data_t srvname  = { SDB_TYPE_STRING, { .string = "srvname" } };
	sdb_data_t attrname = { SDB_TYPE_STRING, { .string = "attrname" } };

	sdb_store_matcher_t *check;

	size_t i;

	struct {
		const char *obj_type;
		const char *attr;
		const char *op;
		const sdb_data_t value;
		int expected;
	} golden_data[] = {
		{ "host",      "name", "=",  hostname, MATCHER_NAME },
		{ "host",      "name", "!=", hostname, MATCHER_NOT },
		{ "host",      "name", "=~", hostname, MATCHER_NAME },
		{ "host",      "name", "!~", hostname, MATCHER_NOT },
		{ "host",      "attr", "=",  hostname, -1 },
		{ "host",      "attr", "!=", hostname, -1 },
		{ "host",      "name", "&^", hostname, -1 },
		{ "host",      "name", "<",  hostname, -1 },
		{ "host",      "name", "<=", hostname, -1 },
		{ "host",      "name", ">=", hostname, -1 },
		{ "host",      "name", ">",  hostname, -1 },
		{ "service",   "name", "=",  srvname,  MATCHER_NAME },
		{ "service",   "name", "!=", srvname,  MATCHER_NOT },
		{ "service",   "name", "=~", srvname,  MATCHER_NAME },
		{ "service",   "name", "!~", srvname,  MATCHER_NOT },
		{ "service",   "attr", "=",  srvname,  -1 },
		{ "service",   "attr", "!=", srvname,  -1 },
		{ "service",   "name", "&^", srvname,  -1 },
		{ "service",   "name", "<",  srvname,  -1 },
		{ "service",   "name", "<=", srvname,  -1 },
		{ "service",   "name", ">=", srvname,  -1 },
		{ "service",   "name", ">",  srvname,  -1 },
		{ "attribute", "name", "=",  attrname, MATCHER_NAME },
		{ "attribute", "name", "!=", attrname, MATCHER_NOT },
		{ "attribute", "name", "=~", attrname, MATCHER_NAME },
		{ "attribute", "name", "!~", attrname, MATCHER_NOT },
		{ "attribute", "name", "<",  attrname, -1 },
		{ "attribute", "name", "<=", attrname, -1 },
		{ "attribute", "name", ">=", attrname, -1 },
		{ "attribute", "name", ">",  attrname, -1 },
		{ "attribute", "attr", "=",  attrname, MATCHER_ATTR },
		{ "attribute", "attr", "!=", attrname, MATCHER_NOT },
		{ "attribute", "attr", "=~", attrname, MATCHER_ATTR },
		{ "attribute", "attr", "!~", attrname, MATCHER_NOT },
		{ "attribute", "attr", "&^", attrname, -1 },
		{ "attribute", "attr", "<",  attrname, MATCHER_LT },
		{ "attribute", "attr", "<=", attrname, MATCHER_LE },
/*		{ "attribute", "attr", "=",  attrname, MATCHER_EQ }, */
		{ "attribute", "attr", ">=", attrname, MATCHER_GE },
		{ "attribute", "attr", ">",  attrname, MATCHER_GT },
		{ "foo",       "name", "=",  attrname, -1 },
		{ "foo",       "attr", "=",  attrname, -1 },
	};

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		char buf[1024];

		check = sdb_store_matcher_parse_cmp(golden_data[i].obj_type,
				golden_data[i].attr, golden_data[i].op,
				&golden_data[i].value);

		if (sdb_data_format(&golden_data[i].value,
					buf, sizeof(buf), SDB_UNQUOTED) < 0)
			snprintf(buf, sizeof(buf), "ERR");

		if (golden_data[i].expected == -1) {
			fail_unless(check == NULL,
					"sdb_store_matcher_parse_cmp(%s, %s, %s, %s) = %p; "
					"expected: NULL", golden_data[i].obj_type,
					golden_data[i].attr, golden_data[i].op, buf, check);
			continue;
		}

		fail_unless(check != NULL,
				"sdb_store_matcher_parse_cmp(%s, %s, %s, %s) = %p; "
				"expected: NULL", golden_data[i].obj_type,
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

static int
lookup_cb(sdb_store_base_t *obj, void *user_data)
{
	int *i = user_data;

	fail_unless(obj != NULL,
			"sdb_store_lookup callback received NULL obj; expected: "
			"<store base obj>");
	fail_unless(i != NULL,
			"sdb_store_lookup callback received NULL user_data; "
			"expected: <pointer to data>");

	++(*i);
	return 0;
} /* lookup_cb */

START_TEST(test_lookup)
{
#define PTR_RE "0x[0-9a-f]+"
	struct {
		const char *query;
		int expected;
		const char *tostring_re;
	} golden_data[] = {
		{ "host.name = 'a'",       1,
			"OBJ\\[host\\]\\{ NAME\\{ 'a', \\(nil\\) \\} \\}" },
		{ "host.name =~ 'a|b'",    2,
			"OBJ\\[host\\]\\{ NAME\\{ NULL, "PTR_RE" \\} \\}" },
		{ "host.name =~ 'host'",   0,
			"OBJ\\[host\\]\\{ NAME\\{ NULL, "PTR_RE" \\} \\}" },
		{ "host.name =~ '.'",      3,
			"OBJ\\[host\\]\\{ NAME\\{ NULL, "PTR_RE" \\} \\}" },
		{ "service.name = 's1'",   2,
			"OBJ\\[service\\]\\{ NAME\\{ 's1', \\(nil\\) } \\}" },
		{ "service.name =~ 's'",   2,
			"OBJ\\[service\\]\\{ NAME\\{ NULL, "PTR_RE" } \\}" },
		{ "service.name !~ 's'",   1,
			"\\(NOT, OBJ\\[service\\]\\{ NAME\\{ NULL, "PTR_RE" } \\}\\)" },
		{ "attribute.name = 'k1'", 1,
			"OBJ\\[attribute\\]\\{ NAME\\{ 'k1', \\(nil\\) \\} " },
		{ "attribute.name = 'x'",  0,
			"OBJ\\[attribute\\]\\{ NAME\\{ 'x', \\(nil\\) \\}" },
		{ "attribute.k1 = 'v1'",   1,
			"ATTR\\[k1\\]\\{ VALUE\\{ 'v1', \\(nil\\) \\} \\}" },
		{ "attribute.k2 < 123",    0,
			"ATTR\\[k2\\]\\{ < 123 \\}" },
		{ "attribute.k2 <= 123",   1,
			"ATTR\\[k2\\]\\{ <= 123 \\}" },
		{ "attribute.k2 >= 123",   1,
			"ATTR\\[k2\\]\\{ >= 123 \\}" },
		{ "attribute.k2 > 123",    0,
			"ATTR\\[k2\\]\\{ > 123 \\}" },
		{ "attribute.k2 = 123",    1,
			"ATTR\\[k2\\]\\{ = 123 \\}" },
		{ "attribute.k2 != 123",   2,
			"\\(NOT, ATTR\\[k2\\]\\{ = 123 \\}\\)" },
		{ "attribute.k1 != 'v1'",  2,
			"\\(NOT, ATTR\\[k1\\]\\{ VALUE\\{ 'v1', \\(nil\\) \\} \\}\\)" },
		{ "attribute.k1 != 'v2'",  3,
			"\\(NOT, ATTR\\[k1\\]\\{ VALUE\\{ 'v2', \\(nil\\) \\} \\}\\)" },
		{ "attribute.name != 'x' "
		  "AND attribute.y !~ 'x'", 3,
			"\\(AND, "
				"\\(NOT, OBJ\\[attribute\\]\\{ NAME\\{ 'x', \\(nil\\) \\} \\}\\), "
				"\\(NOT, ATTR\\[y\\]\\{ VALUE\\{ NULL, "PTR_RE" \\} \\}\\)\\)" },
	};

	int check, n;
	size_t i;

	n = 0;
	check = sdb_store_lookup(NULL, lookup_cb, &n);
	fail_unless(check == 0,
			"sdb_store_lookup() = %d; expected: 0", check);
	fail_unless(n == 3,
			"sdb_store_lookup called callback %d times; expected: 3", (int)n);

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_matcher_t *m;
		char buf[4096];

		m = sdb_fe_parse_matcher(golden_data[i].query, -1);
		fail_unless(m != NULL,
				"sdb_fe_parse_matcher(%s, -1) = NULL; expected: <matcher>",
				golden_data[i].query);
		fail_unless(sdb_regmatches(golden_data[i].tostring_re,
					sdb_store_matcher_tostring(m, buf, sizeof(buf))) == 0,
				"sdb_fe_parse_matcher(%s, -1) = %s; expected: %s",
				golden_data[i].query,
				sdb_store_matcher_tostring(m, buf, sizeof(buf)),
				golden_data[i].tostring_re);

		n = 0;
		sdb_store_lookup(m, lookup_cb, &n);
		fail_unless(n == golden_data[i].expected,
				"sdb_store_lookup(matcher{%s}) found %d hosts; expected: %d",
				golden_data[i].query, n, golden_data[i].expected);
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
	tcase_add_test(tc, test_store_cond);
	tcase_add_test(tc, test_store_match_op);
	tcase_add_test(tc, test_parse_cmp);
	tcase_add_test(tc, test_lookup);
	suite_add_tcase(s, tc);

	return s;
} /* core_store_lookup_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

