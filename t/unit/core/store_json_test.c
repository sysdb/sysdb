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

#if HAVE_CONFIG_H
#	include "config.h"
#endif

#include "core/store.h"
#include "testutils.h"

#include <check.h>
#include <stdlib.h>

/* Make SDB_INTERVAL_SECOND a constant initializer. */
#undef SDB_INTERVAL_SECOND
#define SDB_INTERVAL_SECOND 1000000000L

static sdb_memstore_t *store;

static void
populate(void)
{
	sdb_data_t datum;

	store = sdb_memstore_create();
	ck_assert(store != NULL);

	sdb_memstore_host(store, "h1", 1 * SDB_INTERVAL_SECOND);
	sdb_memstore_host(store, "h2", 3 * SDB_INTERVAL_SECOND);

	datum.type = SDB_TYPE_STRING;
	datum.data.string = "v1";
	sdb_memstore_attribute(store, "h1", "k1", &datum, 1 * SDB_INTERVAL_SECOND);
	datum.data.string = "v2";
	sdb_memstore_attribute(store, "h1", "k2", &datum, 2 * SDB_INTERVAL_SECOND);
	datum.data.string = "v3";
	sdb_memstore_attribute(store, "h1", "k3", &datum, 2 * SDB_INTERVAL_SECOND);

	/* make sure that older updates don't overwrite existing values */
	datum.data.string = "fail";
	sdb_memstore_attribute(store, "h1", "k2", &datum, 1 * SDB_INTERVAL_SECOND);
	sdb_memstore_attribute(store, "h1", "k3", &datum, 2 * SDB_INTERVAL_SECOND);

	sdb_memstore_metric(store, "h1", "m1", /* store */ NULL, 2 * SDB_INTERVAL_SECOND);
	sdb_memstore_metric(store, "h1", "m2", /* store */ NULL, 1 * SDB_INTERVAL_SECOND);
	sdb_memstore_metric(store, "h2", "m1", /* store */ NULL, 1 * SDB_INTERVAL_SECOND);

	sdb_memstore_service(store, "h2", "s1", 1 * SDB_INTERVAL_SECOND);
	sdb_memstore_service(store, "h2", "s2", 2 * SDB_INTERVAL_SECOND);

	datum.type = SDB_TYPE_INTEGER;
	datum.data.integer = 42;
	sdb_memstore_metric_attr(store, "h1", "m1", "k3",
			&datum, 2 * SDB_INTERVAL_SECOND);

	datum.data.integer = 123;
	sdb_memstore_service_attr(store, "h2", "s2", "k1",
			&datum, 2 * SDB_INTERVAL_SECOND);
	datum.data.integer = 4711;
	sdb_memstore_service_attr(store, "h2", "s2", "k2",
			&datum, 1 * SDB_INTERVAL_SECOND);

	/* don't overwrite k1 */
	datum.data.integer = 666;
	sdb_memstore_service_attr(store, "h2", "s2", "k1",
			&datum, 2 * SDB_INTERVAL_SECOND);
} /* populate */

static void
turndown(void)
{
	sdb_object_deref(SDB_OBJ(store));
	store = NULL;
} /* turndown */

static int
scan_tojson(sdb_memstore_obj_t *obj,
		sdb_memstore_matcher_t __attribute__((unused)) *filter,
		void *user_data)
{
	return sdb_memstore_emit(obj, &sdb_store_json_writer, user_data);
} /* scan_tojson */

static int
scan_tojson_full(sdb_memstore_obj_t *obj, sdb_memstore_matcher_t *filter,
		void *user_data)
{
	return sdb_memstore_emit_full(obj, filter, &sdb_store_json_writer, user_data);
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

struct {
	struct {
		sdb_memstore_matcher_t *(*m)(sdb_memstore_expr_t *, sdb_memstore_expr_t *);
		int field;
		sdb_data_t value;
	} filter;
	int type;
	int (*f)(sdb_memstore_obj_t *, sdb_memstore_matcher_t *, void *);
	const char *expected;
} store_tojson_data[] = {
	{ { NULL, 0, SDB_DATA_INIT },
		SDB_HOST, scan_tojson_full,
		"["
			"{\"name\": \"h1\", \"last_update\": \"1970-01-01 00:00:01 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": [], "
				"\"attributes\": ["
					"{\"name\": \"k1\", \"value\": \"v1\", "
						"\"last_update\": \"1970-01-01 00:00:01 +0000\", "
						"\"update_interval\": \"0s\", \"backends\": []},"
					"{\"name\": \"k2\", \"value\": \"v2\", "
						"\"last_update\": \"1970-01-01 00:00:02 +0000\", "
						"\"update_interval\": \"0s\", \"backends\": []},"
					"{\"name\": \"k3\", \"value\": \"v3\", "
						"\"last_update\": \"1970-01-01 00:00:02 +0000\", "
						"\"update_interval\": \"0s\", \"backends\": []}"
				"], "
				"\"metrics\": ["
					"{\"name\": \"m1\", "
						"\"timeseries\": false, "
						"\"last_update\": \"1970-01-01 00:00:02 +0000\", "
						"\"update_interval\": \"0s\", \"backends\": [], "
						"\"attributes\": ["
							"{\"name\": \"k3\", \"value\": 42, "
								"\"last_update\": \"1970-01-01 00:00:02 +0000\", "
								"\"update_interval\": \"0s\", \"backends\": []}"
						"]},"
					"{\"name\": \"m2\", "
						"\"timeseries\": false, "
						"\"last_update\": \"1970-01-01 00:00:01 +0000\", "
						"\"update_interval\": \"0s\", \"backends\": []}"
				"]},"
			"{\"name\": \"h2\", \"last_update\": \"1970-01-01 00:00:03 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": [], "
				"\"metrics\": ["
					"{\"name\": \"m1\", "
						"\"timeseries\": false, "
						"\"last_update\": \"1970-01-01 00:00:01 +0000\", "
						"\"update_interval\": \"0s\", \"backends\": []}"
				"], "
				"\"services\": ["
					"{\"name\": \"s1\", "
						"\"last_update\": \"1970-01-01 00:00:01 +0000\", "
						"\"update_interval\": \"0s\", \"backends\": []},"
					"{\"name\": \"s2\", "
						"\"last_update\": \"1970-01-01 00:00:02 +0000\", "
						"\"update_interval\": \"0s\", \"backends\": [], "
						"\"attributes\": ["
							"{\"name\": \"k1\", \"value\": 123, "
								"\"last_update\": \"1970-01-01 00:00:02 +0000\", "
								"\"update_interval\": \"0s\", \"backends\": []},"
							"{\"name\": \"k2\", \"value\": 4711, "
								"\"last_update\": \"1970-01-01 00:00:01 +0000\", "
								"\"update_interval\": \"0s\", \"backends\": []}"
						"]}"
				"]}"
		"]" },
	{ { NULL, 0, SDB_DATA_INIT },
		SDB_HOST, scan_tojson,
		"["
			"{\"name\": \"h1\", \"last_update\": \"1970-01-01 00:00:01 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": []},"
			"{\"name\": \"h2\", \"last_update\": \"1970-01-01 00:00:03 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": []}"
		"]" },
	{ { sdb_memstore_eq_matcher, SDB_FIELD_NAME,
			{ SDB_TYPE_STRING, { .string = "h1" } } },
		SDB_HOST, scan_tojson_full,
		"["
			"{\"name\": \"h1\", \"last_update\": \"1970-01-01 00:00:01 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": []}"
		"]" },
	{ { sdb_memstore_gt_matcher, SDB_FIELD_LAST_UPDATE,
			{ SDB_TYPE_DATETIME, { .datetime = 1 * SDB_INTERVAL_SECOND } } },
		SDB_HOST, scan_tojson_full,
		"["
			"{\"name\": \"h2\", \"last_update\": \"1970-01-01 00:00:03 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": [], "
				"\"services\": ["
					"{\"name\": \"s2\", "
						"\"last_update\": \"1970-01-01 00:00:02 +0000\", "
						"\"update_interval\": \"0s\", \"backends\": [], "
						"\"attributes\": ["
							"{\"name\": \"k1\", \"value\": 123, "
								"\"last_update\": \"1970-01-01 00:00:02 +0000\", "
								"\"update_interval\": \"0s\", \"backends\": []}"
						"]}"
				"]}"
		"]" },
	{ { sdb_memstore_le_matcher, SDB_FIELD_LAST_UPDATE,
			{ SDB_TYPE_DATETIME, { .datetime = 1 * SDB_INTERVAL_SECOND } } },
		SDB_HOST, scan_tojson_full,
		"["
			"{\"name\": \"h1\", \"last_update\": \"1970-01-01 00:00:01 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": [], "
				"\"attributes\": ["
					"{\"name\": \"k1\", \"value\": \"v1\", "
						"\"last_update\": \"1970-01-01 00:00:01 +0000\", "
						"\"update_interval\": \"0s\", \"backends\": []}"
				"], "
				"\"metrics\": ["
					"{\"name\": \"m2\", "
						"\"timeseries\": false, "
						"\"last_update\": \"1970-01-01 00:00:01 +0000\", "
						"\"update_interval\": \"0s\", \"backends\": []}"
				"]}"
		"]" },
	{ { sdb_memstore_ge_matcher, SDB_FIELD_LAST_UPDATE,
			{ SDB_TYPE_DATETIME, { .datetime = 3 * SDB_INTERVAL_SECOND } } },
		SDB_HOST, scan_tojson_full,
		"["
			"{\"name\": \"h2\", \"last_update\": \"1970-01-01 00:00:03 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": []}"
		"]" },
	{ { sdb_memstore_lt_matcher, SDB_FIELD_LAST_UPDATE,
			{ SDB_TYPE_DATETIME, { .datetime = 0 } } },
		SDB_HOST, scan_tojson_full,
		"[]" },

	{ { NULL, 0, SDB_DATA_INIT },
		SDB_SERVICE, scan_tojson_full,
		"["
			"{\"name\": \"s1\", "
				"\"last_update\": \"1970-01-01 00:00:01 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": []},"
			"{\"name\": \"s2\", "
				"\"last_update\": \"1970-01-01 00:00:02 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": [], "
				"\"attributes\": ["
					"{\"name\": \"k1\", \"value\": 123, "
						"\"last_update\": \"1970-01-01 00:00:02 +0000\", "
						"\"update_interval\": \"0s\", \"backends\": []},"
					"{\"name\": \"k2\", \"value\": 4711, "
						"\"last_update\": \"1970-01-01 00:00:01 +0000\", "
						"\"update_interval\": \"0s\", \"backends\": []}"
				"]}"
		"]" },
	{ { NULL, 0, SDB_DATA_INIT },
		SDB_SERVICE, scan_tojson,
		"["
			"{\"name\": \"s1\", "
				"\"last_update\": \"1970-01-01 00:00:01 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": []},"
			"{\"name\": \"s2\", "
				"\"last_update\": \"1970-01-01 00:00:02 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": []}"
		"]" },
	{ { sdb_memstore_gt_matcher, SDB_FIELD_LAST_UPDATE,
			{ SDB_TYPE_DATETIME, { .datetime = 1 * SDB_INTERVAL_SECOND } } },
		SDB_SERVICE, scan_tojson_full,
		"["
			"{\"name\": \"s2\", "
				"\"last_update\": \"1970-01-01 00:00:02 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": [], "
				"\"attributes\": ["
					"{\"name\": \"k1\", \"value\": 123, "
						"\"last_update\": \"1970-01-01 00:00:02 +0000\", "
						"\"update_interval\": \"0s\", \"backends\": []}"
				"]}"
		"]" },
	{ { sdb_memstore_lt_matcher, SDB_FIELD_LAST_UPDATE,
			{ SDB_TYPE_DATETIME, { .datetime = 0 } } },
		SDB_SERVICE, scan_tojson_full,
		"[]" },
	{ { NULL, 0, SDB_DATA_INIT },
		SDB_METRIC, scan_tojson_full,
		"["
			"{\"name\": \"m1\", "
				"\"timeseries\": false, "
				"\"last_update\": \"1970-01-01 00:00:02 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": [], "
				"\"attributes\": ["
					"{\"name\": \"k3\", \"value\": 42, "
						"\"last_update\": \"1970-01-01 00:00:02 +0000\", "
						"\"update_interval\": \"0s\", \"backends\": []}"
				"]},"
			"{\"name\": \"m2\", "
				"\"timeseries\": false, "
				"\"last_update\": \"1970-01-01 00:00:01 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": []},"
			"{\"name\": \"m1\", "
				"\"timeseries\": false, "
				"\"last_update\": \"1970-01-01 00:00:01 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": []}"
		"]" },
	{ { NULL, 0, SDB_DATA_INIT },
		SDB_METRIC, scan_tojson,
		"["
			"{\"name\": \"m1\", "
				"\"timeseries\": false, "
				"\"last_update\": \"1970-01-01 00:00:02 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": []},"
			"{\"name\": \"m2\", "
				"\"timeseries\": false, "
				"\"last_update\": \"1970-01-01 00:00:01 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": []},"
			"{\"name\": \"m1\", "
				"\"timeseries\": false, "
				"\"last_update\": \"1970-01-01 00:00:01 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": []}"
		"]" },
	{ { sdb_memstore_le_matcher, SDB_FIELD_LAST_UPDATE,
			{ SDB_TYPE_DATETIME, { .datetime = 1 * SDB_INTERVAL_SECOND } } },
		SDB_METRIC, scan_tojson_full,
		"["
			"{\"name\": \"m2\", "
				"\"timeseries\": false, "
				"\"last_update\": \"1970-01-01 00:00:01 +0000\", "
				"\"update_interval\": \"0s\", \"backends\": []}"
		"]" },
	{ { sdb_memstore_lt_matcher, SDB_FIELD_LAST_UPDATE,
			{ SDB_TYPE_DATETIME, { .datetime = 0 } } },
		SDB_METRIC, scan_tojson_full,
		"[]" },
};

START_TEST(test_store_tojson)
{
	sdb_strbuf_t *buf = sdb_strbuf_create(0);
	sdb_memstore_matcher_t *filter = NULL;
	sdb_store_json_formatter_t *f;
	int status;

	if (store_tojson_data[_i].filter.m) {
		sdb_memstore_expr_t *field;
		sdb_memstore_expr_t *value;

		field = sdb_memstore_expr_fieldvalue(store_tojson_data[_i].filter.field);
		fail_unless(field != NULL,
				"INTERNAL ERROR: sdb_memstore_expr_fieldvalue() = NULL");
		value = sdb_memstore_expr_constvalue(&store_tojson_data[_i].filter.value);
		fail_unless(value != NULL,
				"INTERNAL ERROR: sdb_memstore_expr_constvalue() = NULL");

		filter = store_tojson_data[_i].filter.m(field, value);
		fail_unless(filter != NULL,
				"INTERNAL ERROR: sdb_memstore_*_matcher() = NULL");

		sdb_object_deref(SDB_OBJ(field));
		sdb_object_deref(SDB_OBJ(value));
	}

	sdb_strbuf_clear(buf);
	f = sdb_store_json_formatter(buf, store_tojson_data[_i].type, SDB_WANT_ARRAY);
	ck_assert(f != NULL);

	status = sdb_memstore_scan(store, store_tojson_data[_i].type,
			/* m = */ NULL, filter, store_tojson_data[_i].f, f);
	fail_unless(status == 0,
			"sdb_memstore_scan(HOST, ..., tojson) = %d; expected: 0",
			status);
	sdb_store_json_finish(f);

	verify_json_output(buf, store_tojson_data[_i].expected);

	sdb_object_deref(SDB_OBJ(filter));
	sdb_object_deref(SDB_OBJ(f));
	sdb_strbuf_destroy(buf);
}
END_TEST

TEST_MAIN("core::store_json")
{
	TCase *tc = tcase_create("core");
	tcase_add_unchecked_fixture(tc, populate, turndown);
	TC_ADD_LOOP_TEST(tc, store_tojson);
	ADD_TCASE(tc);
}
TEST_MAIN_END

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

