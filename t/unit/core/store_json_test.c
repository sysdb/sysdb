/*
 * SysDB - t/unit/core/store_json_test.c
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
#include "libsysdb_test.h"

#include <assert.h>

#include <check.h>
#include <stdlib.h>

static void
populate(void)
{
	sdb_data_t datum;

	sdb_store_host("h1", 1);
	sdb_store_host("h2", 3);

	datum.type = SDB_TYPE_STRING;
	datum.data.string = "v1";
	sdb_store_attribute("h1", "k1", &datum, 1);
	datum.data.string = "v2";
	sdb_store_attribute("h1", "k2", &datum, 2);
	datum.data.string = "v3";
	sdb_store_attribute("h1", "k3", &datum, 2);

	/* make sure that older updates don't overwrite existing values */
	datum.data.string = "fail";
	sdb_store_attribute("h1", "k2", &datum, 1);
	sdb_store_attribute("h1", "k3", &datum, 2);

	sdb_store_metric("h1", "m1", /* store */ NULL, 2);
	sdb_store_metric("h1", "m2", /* store */ NULL, 1);
	sdb_store_metric("h2", "m1", /* store */ NULL, 1);

	sdb_store_service("h2", "s1", 1);
	sdb_store_service("h2", "s2", 2);

	datum.type = SDB_TYPE_INTEGER;
	datum.data.integer = 42;
	sdb_store_metric_attr("h1", "m1", "k3", &datum, 2);

	datum.data.integer = 123;
	sdb_store_service_attr("h2", "s2", "k1", &datum, 2);
	datum.data.integer = 4711;
	sdb_store_service_attr("h2", "s2", "k2", &datum, 1);

	/* don't overwrite k1 */
	datum.data.integer = 666;
	sdb_store_service_attr("h2", "s2", "k1", &datum, 2);
} /* populate */

static int
scan_tojson(sdb_store_obj_t *obj,
		sdb_store_matcher_t __attribute__((unused)) *filter,
		void *user_data)
{
	sdb_store_json_formatter_t *f = user_data;
	return sdb_store_json_emit(f, obj);
} /* scan_tojson */

static int
scan_tojson_full(sdb_store_obj_t *obj, sdb_store_matcher_t *filter,
		void *user_data)
{
	sdb_store_json_formatter_t *f = user_data;
	return sdb_store_json_emit_full(f, obj, filter);
} /* scan_tojson_full */

static void
verify_json_output(sdb_strbuf_t *buf, const char *expected)
{
	const char *got = sdb_strbuf_string(buf);
	size_t len1 = strlen(got);
	size_t len2 = strlen(expected);

	size_t i;
	int pos = -1;

	if (len1 != len2)
		pos = (int)SDB_MIN(len1, len2);

	for (i = 0; i < SDB_MIN(len1, len2); ++i) {
		if (got[i] != expected[i]) {
			pos = (int)i;
			break;
		}
	}

	fail_unless(pos == -1,
			"Serializing hosts to JSON returned unexpected result\n"
			"         got: %s\n              %*s\n    expected: %s",
			got, pos + 1, "^", expected);
} /* verify_json_output */

START_TEST(test_store_tojson)
{
	sdb_strbuf_t *buf;
	size_t i;

	struct {
		struct {
			sdb_store_matcher_t *(*m)(sdb_store_expr_t *,
					sdb_store_expr_t *);
			int field;
			sdb_data_t value;
		} filter;
		int (*f)(sdb_store_obj_t *, sdb_store_matcher_t *, void *);
		const char *expected;
	} golden_data[] = {
		{ { NULL, 0, SDB_DATA_INIT }, scan_tojson_full,
			"["
				"{\"name\": \"h1\", \"last_update\": \"1970-01-01 00:00:00 +0000\", "
					"\"update_interval\": \"0s\", \"backends\": [], "
					"\"attributes\": ["
						"{\"name\": \"k1\", \"value\": \"v1\", "
							"\"last_update\": \"1970-01-01 00:00:00 +0000\", "
							"\"update_interval\": \"0s\", \"backends\": []},"
						"{\"name\": \"k2\", \"value\": \"v2\", "
							"\"last_update\": \"1970-01-01 00:00:00 +0000\", "
							"\"update_interval\": \"0s\", \"backends\": []},"
						"{\"name\": \"k3\", \"value\": \"v3\", "
							"\"last_update\": \"1970-01-01 00:00:00 +0000\", "
							"\"update_interval\": \"0s\", \"backends\": []}"
					"], "
					"\"metrics\": ["
						"{\"name\": \"m1\", "
							"\"last_update\": \"1970-01-01 00:00:00 +0000\", "
							"\"update_interval\": \"0s\", \"backends\": [], "
							"\"attributes\": ["
								"{\"name\": \"k3\", \"value\": 42, "
									"\"last_update\": \"1970-01-01 00:00:00 +0000\", "
									"\"update_interval\": \"0s\", \"backends\": []}"
							"]},"
						"{\"name\": \"m2\", "
							"\"last_update\": \"1970-01-01 00:00:00 +0000\", "
							"\"update_interval\": \"0s\", \"backends\": []}"
					"]},"
				"{\"name\": \"h2\", \"last_update\": \"1970-01-01 00:00:00 +0000\", "
					"\"update_interval\": \"0s\", \"backends\": [], "
					"\"metrics\": ["
						"{\"name\": \"m1\", "
							"\"last_update\": \"1970-01-01 00:00:00 +0000\", "
							"\"update_interval\": \"0s\", \"backends\": []}"
					"], "
					"\"services\": ["
						"{\"name\": \"s1\", "
							"\"last_update\": \"1970-01-01 00:00:00 +0000\", "
							"\"update_interval\": \"0s\", \"backends\": []},"
						"{\"name\": \"s2\", "
							"\"last_update\": \"1970-01-01 00:00:00 +0000\", "
							"\"update_interval\": \"0s\", \"backends\": [], "
							"\"attributes\": ["
								"{\"name\": \"k1\", \"value\": 123, "
									"\"last_update\": \"1970-01-01 00:00:00 +0000\", "
									"\"update_interval\": \"0s\", \"backends\": []},"
								"{\"name\": \"k2\", \"value\": 4711, "
									"\"last_update\": \"1970-01-01 00:00:00 +0000\", "
									"\"update_interval\": \"0s\", \"backends\": []}"
							"]}"
					"]}"
			"]" },
		{ { NULL, 0, SDB_DATA_INIT }, scan_tojson,
			"["
				"{\"name\": \"h1\", \"last_update\": \"1970-01-01 00:00:00 +0000\", "
					"\"update_interval\": \"0s\", \"backends\": []},"
				"{\"name\": \"h2\", \"last_update\": \"1970-01-01 00:00:00 +0000\", "
					"\"update_interval\": \"0s\", \"backends\": []}"
			"]" },
		{ { sdb_store_eq_matcher, SDB_FIELD_NAME,
				{ SDB_TYPE_STRING, { .string = "h1" } } }, scan_tojson_full,
			"["
				"{\"name\": \"h1\", \"last_update\": \"1970-01-01 00:00:00 +0000\", "
					"\"update_interval\": \"0s\", \"backends\": []}"
			"]" },
		{ { sdb_store_gt_matcher, SDB_FIELD_LAST_UPDATE,
				{ SDB_TYPE_DATETIME, { .datetime = 1 } } }, scan_tojson_full,
			"["
				"{\"name\": \"h2\", \"last_update\": \"1970-01-01 00:00:00 +0000\", "
					"\"update_interval\": \"0s\", \"backends\": [], "
					"\"services\": ["
						"{\"name\": \"s2\", "
							"\"last_update\": \"1970-01-01 00:00:00 +0000\", "
							"\"update_interval\": \"0s\", \"backends\": [], "
							"\"attributes\": ["
								"{\"name\": \"k1\", \"value\": 123, "
									"\"last_update\": \"1970-01-01 00:00:00 +0000\", "
									"\"update_interval\": \"0s\", \"backends\": []}"
							"]}"
					"]}"
			"]" },
		{ { sdb_store_le_matcher, SDB_FIELD_LAST_UPDATE,
				{ SDB_TYPE_DATETIME, { .datetime = 1 } } }, scan_tojson_full,
			"["
				"{\"name\": \"h1\", \"last_update\": \"1970-01-01 00:00:00 +0000\", "
					"\"update_interval\": \"0s\", \"backends\": [], "
					"\"attributes\": ["
						"{\"name\": \"k1\", \"value\": \"v1\", "
							"\"last_update\": \"1970-01-01 00:00:00 +0000\", "
							"\"update_interval\": \"0s\", \"backends\": []}"
					"], "
					"\"metrics\": ["
						"{\"name\": \"m2\", "
							"\"last_update\": \"1970-01-01 00:00:00 +0000\", "
							"\"update_interval\": \"0s\", \"backends\": []}"
					"]}"
			"]" },
		{ { sdb_store_ge_matcher, SDB_FIELD_LAST_UPDATE,
				{ SDB_TYPE_DATETIME, { .datetime = 3 } } }, scan_tojson_full,
			"["
				"{\"name\": \"h2\", \"last_update\": \"1970-01-01 00:00:00 +0000\", "
					"\"update_interval\": \"0s\", \"backends\": []}"
			"]" },
	};

	buf = sdb_strbuf_create(0);
	fail_unless(buf != NULL, "INTERNAL ERROR: failed to create string buffer");
	populate();

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_matcher_t *filter = NULL;
		sdb_store_json_formatter_t *f;
		int status;

		sdb_strbuf_clear(buf);

		if (golden_data[i].filter.m) {
			sdb_store_expr_t *field;
			sdb_store_expr_t *value;

			field = sdb_store_expr_fieldvalue(golden_data[i].filter.field);
			fail_unless(field != NULL,
					"INTERNAL ERROR: sdb_store_expr_fieldvalue() = NULL");
			value = sdb_store_expr_constvalue(&golden_data[i].filter.value);
			fail_unless(value != NULL,
					"INTERNAL ERROR: sdb_store_expr_constvalue() = NULL");

			filter = golden_data[i].filter.m(field, value);
			fail_unless(filter != NULL,
					"INTERNAL ERROR: sdb_store_*_matcher() = NULL");

			sdb_object_deref(SDB_OBJ(field));
			sdb_object_deref(SDB_OBJ(value));
		}

		sdb_strbuf_clear(buf);
		f = sdb_store_json_formatter(buf);
		assert(f);

		sdb_strbuf_append(buf, "[");
		status = sdb_store_scan(SDB_HOST, /* m = */ NULL, filter,
				golden_data[i].f, f);
		fail_unless(status == 0,
				"sdb_store_scan(HOST, ..., tojson) = %d; expected: 0",
				status);
		sdb_store_json_finish(f);
		sdb_strbuf_append(buf, "]");

		verify_json_output(buf, golden_data[i].expected);
		free(f);
		sdb_object_deref(SDB_OBJ(filter));
	}
	sdb_strbuf_destroy(buf);
}
END_TEST

Suite *
core_store_json_suite(void)
{
	Suite *s = suite_create("core::store_json");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_test(tc, test_store_tojson);
	tcase_add_unchecked_fixture(tc, NULL, sdb_store_clear);
	suite_add_tcase(s, tc);

	return s;
} /* core_store_json_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

