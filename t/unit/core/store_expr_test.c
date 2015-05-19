/*
 * SysDB - t/unit/core/store_expr_test.c
 * Copyright (C) 2015 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include "core/store.h"
#include "core/store-private.h"
#include "parser/parser.h"
#include "testutils.h"

#include <check.h>

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

	struct {
		const char *host;
		const char *service;
		const char *name;
		sdb_data_t  value;
	} svc_attrs[] = {
		{ "a", "s1", "k1", { SDB_TYPE_STRING, { .string = "v1" } } },
		{ "a", "s2", "k2", { SDB_TYPE_INTEGER, { .integer = 123 } } },
	};

	struct {
		const char *host;
		const char *metric;
		const char *name;
		sdb_data_t  value;
	} metric_attrs[] = {
		{ "b", "m2", "k1", { SDB_TYPE_STRING, { .string = "v1" } } },
		{ "b", "m2", "k2", { SDB_TYPE_INTEGER, { .integer = 123 } } },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(hosts); ++i) {
		int status = sdb_store_host(hosts[i], 1);
		ck_assert(status == 0);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(metrics); ++i) {
		int status = sdb_store_metric(metrics[i].host,
				metrics[i].metric, /* store */ NULL, 1);
		ck_assert(status == 0);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(services); ++i) {
		int status = sdb_store_service(services[i].host,
				services[i].service, 1);
		ck_assert(status == 0);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(attrs); ++i) {
		int status = sdb_store_attribute(attrs[i].host,
				attrs[i].name, &attrs[i].value, 1);
		ck_assert(status == 0);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(svc_attrs); ++i) {
		int status = sdb_store_service_attr(svc_attrs[i].host,
				svc_attrs[i].service, svc_attrs[i].name,
				&svc_attrs[i].value, 1);
		ck_assert(status == 0);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(metric_attrs); ++i) {
		int status = sdb_store_metric_attr(metric_attrs[i].host,
				metric_attrs[i].metric, metric_attrs[i].name,
				&metric_attrs[i].value, 1);
		ck_assert(status == 0);
	}
} /* populate */

#define NAME { SDB_TYPE_INTEGER, { .integer = SDB_FIELD_NAME } }
#define LAST_UPDATE { SDB_TYPE_INTEGER, { .integer = SDB_FIELD_LAST_UPDATE } }
#define AGE { SDB_TYPE_INTEGER, { .integer = SDB_FIELD_AGE } }
#define INTERVAL { SDB_TYPE_INTEGER, { .integer = SDB_FIELD_INTERVAL } }
#define BACKENDS { SDB_TYPE_INTEGER, { .integer = SDB_FIELD_BACKEND } }
#define HOSTS { SDB_TYPE_INTEGER, { .integer = SDB_HOST } }
#define SERVICES { SDB_TYPE_INTEGER, { .integer = SDB_SERVICE } }
#define METRICS { SDB_TYPE_INTEGER, { .integer = SDB_METRIC } }
#define ATTRS { SDB_TYPE_INTEGER, { .integer = SDB_ATTRIBUTE } }
static sdb_store_expr_t namer = {
	SDB_OBJECT_INIT, FIELD_VALUE, -1, NULL, NULL, NAME,
};
static int64_t int_values[] = { 1, 2, 3, 4, 5 };
static double dec_values[] = { 47.0, 11.0, 32.0, 64.0 };
static char *str_values[] = { "foo", "bar", "qux" };
static sdb_time_t dt_values[] = { 4711L, 1234567890L };
static struct {
	size_t length;
	unsigned char *datum;
} bin_values[] = { { 4, (unsigned char *)"\3\2\0\1" } };
struct {
	sdb_store_expr_t expr;
	bool iterable;

	char *host;
	int child_type;
	char *child;  /* optional */
	char *filter; /* optional */

	sdb_data_t expected[5];
	size_t expected_len;
} expr_iter_data[] = {
	/* iterate host children */
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, SERVICES }, true,
		"a", -1, NULL, NULL,
		{
			{ SDB_TYPE_STRING, { .string = "s1" } },
			{ SDB_TYPE_STRING, { .string = "s2" } },
			{ 0 },
			{ 0 },
			{ 0 },
		}, 2,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, SERVICES }, true,
		"b", -1, NULL, NULL,
		{
			{ SDB_TYPE_STRING, { .string = "s1" } },
			{ SDB_TYPE_STRING, { .string = "s3" } },
			{ 0 },
			{ 0 },
			{ 0 },
		}, 2,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, SERVICES }, true,
		"a", -1, NULL, "name = 'a' OR name = 's1'",
		{
			{ SDB_TYPE_STRING, { .string = "s1" } },
			{ 0 },
			{ 0 },
			{ 0 },
			{ 0 },
		}, 1,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, SERVICES }, true,
		"a", -1, NULL, "name = 'a' OR name = 's2'",
		{
			{ SDB_TYPE_STRING, { .string = "s2" } },
			{ 0 },
			{ 0 },
			{ 0 },
			{ 0 },
		}, 1,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, SERVICES }, true,
		"a", -1, NULL, "name = 'a'",
		{
			{ 0 },
			{ 0 },
			{ 0 },
			{ 0 },
			{ 0 },
		}, 0,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, METRICS }, true,
		"a", -1, NULL, NULL,
		{
			{ SDB_TYPE_STRING, { .string = "m1" } },
			{ 0 },
			{ 0 },
			{ 0 },
			{ 0 },
		}, 1,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, METRICS }, true,
		"a", -1, NULL, "name = 'a' OR name = 'm1'",
		{
			{ SDB_TYPE_STRING, { .string = "m1" } },
			{ 0 },
			{ 0 },
			{ 0 },
			{ 0 },
		}, 1,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, METRICS }, true,
		"a", -1, NULL, "name = 'a' OR name = 'm2'",
		{
			{ 0 },
			{ 0 },
			{ 0 },
			{ 0 },
			{ 0 },
		}, 0,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, METRICS }, true,
		"a", -1, NULL, "name = 'a'",
		{
			{ 0 },
			{ 0 },
			{ 0 },
			{ 0 },
			{ 0 },
		}, 0,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, ATTRS }, true,
		"a", -1, NULL, NULL,
		{
			{ SDB_TYPE_STRING, { .string = "k1" } },
			{ SDB_TYPE_STRING, { .string = "k2" } },
			{ 0 },
			{ 0 },
			{ 0 },
		}, 2,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, HOSTS }, false,
		"a", -1, NULL, NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, }, 0,
	},
	/* host fields */
	{
		{ SDB_OBJECT_INIT, FIELD_VALUE, -1, NULL, NULL, BACKENDS }, true,
		"a", -1, NULL, NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, }, 0,
	},
	{
		{ SDB_OBJECT_INIT, FIELD_VALUE, -1, NULL, NULL, NAME }, false,
		"a", -1, NULL, NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, }, 0,
	},
	{
		{ SDB_OBJECT_INIT, FIELD_VALUE, -1, NULL, NULL, LAST_UPDATE }, false,
		"a", -1, NULL, NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, }, 0,
	},
	{
		{ SDB_OBJECT_INIT, FIELD_VALUE, -1, NULL, NULL, AGE }, false,
		"a", -1, NULL, NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, }, 0,
	},
	{
		{ SDB_OBJECT_INIT, FIELD_VALUE, -1, NULL, NULL, INTERVAL }, false,
		"a", -1, NULL, NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, }, 0,
	},
	/* service children */
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, ATTRS }, true,
		"a", SDB_SERVICE, "s1", NULL,
		{
			{ SDB_TYPE_STRING, { .string = "hostname" } },
			{ SDB_TYPE_STRING, { .string = "k1" } },
			{ 0 },
			{ 0 },
			{ 0 },
		}, 2,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, ATTRS }, true,
		"a", SDB_SERVICE, "s1", "age >= 0s",
		{
			{ SDB_TYPE_STRING, { .string = "hostname" } },
			{ SDB_TYPE_STRING, { .string = "k1" } },
			{ 0 },
			{ 0 },
			{ 0 },
		}, 2,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, ATTRS }, true,
		"a", SDB_SERVICE, "s1", "age < 0s",
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 } }, 0,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, HOSTS }, false,
		"a", SDB_SERVICE, "s1", NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 } }, 0,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, SERVICES }, false,
		"a", SDB_SERVICE, "s1", NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 } }, 0,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, METRICS }, false,
		"a", SDB_SERVICE, "s1", NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 } }, 0,
	},
	/* service fields */
	{
		{ SDB_OBJECT_INIT, FIELD_VALUE, -1, NULL, NULL, BACKENDS }, true,
		"a", SDB_SERVICE, "s1", NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, }, 0,
	},
	{
		{ SDB_OBJECT_INIT, FIELD_VALUE, -1, NULL, NULL, NAME }, false,
		"a", SDB_SERVICE, "s1", NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, }, 0,
	},
	{
		{ SDB_OBJECT_INIT, FIELD_VALUE, -1, NULL, NULL, LAST_UPDATE }, false,
		"a", SDB_SERVICE, "s1", NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, }, 0,
	},
	{
		{ SDB_OBJECT_INIT, FIELD_VALUE, -1, NULL, NULL, AGE }, false,
		"a", SDB_SERVICE, "s1", NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, }, 0,
	},
	{
		{ SDB_OBJECT_INIT, FIELD_VALUE, -1, NULL, NULL, INTERVAL }, false,
		"a", SDB_SERVICE, "s1", NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, }, 0,
	},
	/* metric children */
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, ATTRS }, true,
		"b", SDB_METRIC, "m2", NULL,
		{
			{ SDB_TYPE_STRING, { .string = "hostname" } },
			{ SDB_TYPE_STRING, { .string = "k1" } },
			{ SDB_TYPE_STRING, { .string = "k2" } },
			{ 0 },
			{ 0 },
		}, 3,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, ATTRS }, true,
		"b", SDB_METRIC, "m2", "age >= 0s",
		{
			{ SDB_TYPE_STRING, { .string = "hostname" } },
			{ SDB_TYPE_STRING, { .string = "k1" } },
			{ SDB_TYPE_STRING, { .string = "k2" } },
			{ 0 },
			{ 0 },
		}, 3,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, ATTRS }, true,
		"b", SDB_METRIC, "m2", "name = 'b' OR name = 'm2' OR name = 'k2'",
		{
			{ SDB_TYPE_STRING, { .string = "k2" } },
			{ 0 },
			{ 0 },
			{ 0 },
			{ 0 },
		}, 1,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, ATTRS }, true,
		"b", SDB_METRIC, "m2", "age < 0s",
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 } }, 0,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, HOSTS }, false,
		"b", SDB_METRIC, "m2", NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 } }, 0,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, SERVICES }, false,
		"b", SDB_METRIC, "m2", NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 } }, 0,
	},
	{
		{ SDB_OBJECT_INIT, TYPED_EXPR, -1, &namer, NULL, METRICS }, false,
		"b", SDB_METRIC, "m2", NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 } }, 0,
	},
	/* metric fields */
	{
		{ SDB_OBJECT_INIT, FIELD_VALUE, -1, NULL, NULL, BACKENDS }, true,
		"b", SDB_METRIC, "m2", NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, }, 0,
	},
	{
		{ SDB_OBJECT_INIT, FIELD_VALUE, -1, NULL, NULL, NAME }, false,
		"b", SDB_METRIC, "m2", NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, }, 0,
	},
	{
		{ SDB_OBJECT_INIT, FIELD_VALUE, -1, NULL, NULL, LAST_UPDATE }, false,
		"b", SDB_METRIC, "m2", NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, }, 0,
	},
	{
		{ SDB_OBJECT_INIT, FIELD_VALUE, -1, NULL, NULL, AGE }, false,
		"b", SDB_METRIC, "m2", NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, }, 0,
	},
	{
		{ SDB_OBJECT_INIT, FIELD_VALUE, -1, NULL, NULL, INTERVAL }, false,
		"b", SDB_METRIC, "m2", NULL,
		{ { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, }, 0,
	},
	/* arrays */
	{
		{
			SDB_OBJECT_INIT, 0, -1, NULL, NULL,
			{
				SDB_TYPE_INTEGER | SDB_TYPE_ARRAY,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values), int_values } },
			},
		}, true,
		NULL, -1, NULL, NULL,
		{
			{ SDB_TYPE_INTEGER, { .integer = 1 } },
			{ SDB_TYPE_INTEGER, { .integer = 2 } },
			{ SDB_TYPE_INTEGER, { .integer = 3 } },
			{ SDB_TYPE_INTEGER, { .integer = 4 } },
			{ SDB_TYPE_INTEGER, { .integer = 5 } },
		}, 5,
	},
	{
		{
			SDB_OBJECT_INIT, 0, -1, NULL, NULL,
			{
				SDB_TYPE_DECIMAL | SDB_TYPE_ARRAY,
				{ .array = { SDB_STATIC_ARRAY_LEN(dec_values), dec_values } },
			},
		}, true,
		NULL, -1, NULL, NULL,
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 47.0 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 11.0 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 32.0 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 64.0 } },
			{ 0 },
		}, 4,
	},
	{
		{
			SDB_OBJECT_INIT, 0, -1, NULL, NULL,
			{
				SDB_TYPE_STRING | SDB_TYPE_ARRAY,
				{ .array = { SDB_STATIC_ARRAY_LEN(str_values), str_values } },
			},
		}, true,
		NULL, -1, NULL, NULL,
		{
			{ SDB_TYPE_STRING, { .string = "foo" } },
			{ SDB_TYPE_STRING, { .string = "bar" } },
			{ SDB_TYPE_STRING, { .string = "qux" } },
			{ 0 },
			{ 0 },
		}, 3,
	},
	{
		{
			SDB_OBJECT_INIT, 0, -1, NULL, NULL,
			{
				SDB_TYPE_DATETIME | SDB_TYPE_ARRAY,
				{ .array = { SDB_STATIC_ARRAY_LEN(dt_values), dt_values } },
			},
		}, true,
		NULL, -1, NULL, NULL,
		{
			{ SDB_TYPE_DATETIME, { .datetime = 4711L } },
			{ SDB_TYPE_DATETIME, { .datetime = 1234567890L } },
			{ 0 },
			{ 0 },
			{ 0 },
		}, 2,
	},
	{
		{
			SDB_OBJECT_INIT, 0, -1, NULL, NULL,
			{
				SDB_TYPE_BINARY  | SDB_TYPE_ARRAY,
				{ .array = { SDB_STATIC_ARRAY_LEN(bin_values), bin_values } },
			},
		}, true,
		NULL, -1, NULL, NULL,
		{
			{ SDB_TYPE_BINARY, { .binary = { 4, (unsigned char *)"\3\2\0\1" } } },
			{ 0 },
			{ 0 },
			{ 0 },
			{ 0 },
		}, 1,
	},
};

START_TEST(test_expr_iter)
{
	sdb_store_obj_t *obj = NULL;
	sdb_store_matcher_t *filter = NULL;
	int context = SDB_HOST;

	bool iterable;
	sdb_store_expr_iter_t *iter;
	size_t i;

	if (expr_iter_data[_i].host) {
		obj = sdb_store_get_host(expr_iter_data[_i].host);
		ck_assert(obj != NULL);

		if (expr_iter_data[_i].child) {
			sdb_store_obj_t *child = sdb_store_get_child(obj,
					expr_iter_data[_i].child_type, expr_iter_data[_i].child);
			ck_assert(child != NULL);
			sdb_object_deref(SDB_OBJ(obj));
			obj = child;
			context = expr_iter_data[_i].child_type;
		}
		ck_assert(obj->type == context);
	}

	if (expr_iter_data[_i].filter) {
		sdb_ast_node_t *ast;
		ast = sdb_parser_parse_conditional(expr_iter_data[_i].filter, -1, NULL);
		filter = sdb_store_query_prepare_matcher(ast);
		sdb_object_deref(SDB_OBJ(ast));
		ck_assert(filter != NULL);
	}

	iterable = sdb_store_expr_iterable(&expr_iter_data[_i].expr, context);
	fail_unless(iterable == expr_iter_data[_i].iterable,
			"%s expression not iterable in %s context",
			EXPR_TO_STRING(&expr_iter_data[_i].expr),
			SDB_STORE_TYPE_TO_NAME(context));

	iter = sdb_store_expr_iter(&expr_iter_data[_i].expr, obj, filter);
	fail_unless((iter != NULL) == iterable,
			"sdb_store_expr_iter(%s expression, %s, %s) = %s; expected: %s",
			EXPR_TO_STRING(&expr_iter_data[_i].expr),
			obj ? SDB_STORE_TYPE_TO_NAME(obj->type) : "<array>",
			expr_iter_data[_i].filter, iter ? "<iter>" : "NULL",
			iterable ? "<iter>" : "NULL");

	/* the iterator will keep a reference */
	sdb_object_deref(SDB_OBJ(obj)); obj = NULL;
	sdb_object_deref(SDB_OBJ(filter)); filter = NULL;

	i = 0;
	while (sdb_store_expr_iter_has_next(iter)) {
		char v_str[64], expected_str[64];
		sdb_data_t v;

		fail_unless(i < expr_iter_data[_i].expected_len,
				"iter<%s expression, %s, %s> returned >= %zu elements; "
				"expected: %zu", EXPR_TO_STRING(&expr_iter_data[_i].expr),
				SDB_STORE_TYPE_TO_NAME(context), expr_iter_data[_i].filter,
				i + 1, expr_iter_data[_i].expected_len);

		v = sdb_store_expr_iter_get_next(iter);
		sdb_data_format(&v, v_str, sizeof(v_str), SDB_DOUBLE_QUOTED);
		sdb_data_format(&expr_iter_data[_i].expected[i],
				expected_str, sizeof(expected_str), SDB_DOUBLE_QUOTED);
		fail_unless(sdb_data_cmp(&v, &expr_iter_data[_i].expected[i]) == 0,
				"iter<%s expression, %s, %s>, elem %zu = %s; expected: %s",
				EXPR_TO_STRING(&expr_iter_data[_i].expr),
				SDB_STORE_TYPE_TO_NAME(context), expr_iter_data[_i].filter,
				i, v_str, expected_str);

		sdb_data_free_datum(&v);
		++i;
	}

	fail_unless(i == expr_iter_data[_i].expected_len,
			"iter<%s expression, %s, %s> returned %zu elements; "
			"expected: %zu", EXPR_TO_STRING(&expr_iter_data[_i].expr),
			SDB_STORE_TYPE_TO_NAME(context), expr_iter_data[_i].filter,
			i, expr_iter_data[_i].expected_len);
	fail_unless(sdb_store_expr_iter_get_next(iter).type == SDB_TYPE_NULL,
			"iter<%s expression, %s, %s> returned further elements "
			"passed the end", EXPR_TO_STRING(&expr_iter_data[_i].expr),
			SDB_STORE_TYPE_TO_NAME(context), expr_iter_data[_i].filter);

	sdb_store_expr_iter_destroy(iter);
}
END_TEST

TEST_MAIN("core::store_expr")
{
	TCase *tc = tcase_create("core");
	tcase_add_checked_fixture(tc, populate, sdb_store_clear);
	TC_ADD_LOOP_TEST(tc, expr_iter);
	ADD_TCASE(tc);
}
TEST_MAIN_END

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

