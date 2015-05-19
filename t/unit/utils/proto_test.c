/*
 * SysDB - t/unit/utils/proto_test.c
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
#include "utils/proto.h"
#include "testutils.h"

#include <check.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool
streq(const char *s1, const char *s2)
{
	if ((! s1) || (! s2))
		return (s1 == NULL) == (s2 == NULL);
	return strcmp(s1, s2) == 0;
} /* streq */

START_TEST(test_marshal_data)
{
#define INT_TYPE "\0\0\0\2"
#define DECIMAL_TYPE "\0\0\0\3"
#define STRING_TYPE "\0\0\0\4"
#define DATETIME_TYPE "\0\0\0\5"
#define BINARY_TYPE "\0\0\0\6"
#define REGEX_TYPE "\0\0\0\7"

#define NULL_ARRAY "\0\0\1\0"
#define INT_ARRAY "\0\0\1\2"
#define DECIMAL_ARRAY "\0\0\1\3"
#define STRING_ARRAY "\0\0\1\4"
#define DATETIME_ARRAY "\0\0\1\5"
#define BINARY_ARRAY "\0\0\1\6"
#define REGEX_ARRAY "\0\0\1\7"

	regex_t dummy_re;
	int64_t int_values[] = { 47, 11, 23 };
	double dec_values[] = { 47.11, .5 };
	char *string_values[] = { "foo", "abcd" };
	sdb_time_t datetime_values[] = { 4711, 1234567890123456789L };
	struct {
		size_t length;
		unsigned char *datum;
	} binary_values[] = {
		{ 3, (unsigned char *)"\x1\x2\x3" },
		{ 4, (unsigned char *)"\x42\x0\xa\x1b" },
	};
	struct {
		char *raw;
		regex_t regex;
	} regex_values[] = {
		{ "dummy regex", dummy_re },
	};

	struct {
		sdb_data_t datum;
		ssize_t expected_len;
		char *expected;
	} golden_data[] = {
		{
			{ SDB_TYPE_NULL, { .integer = 0 } },
			4, "\0\0\0\0",
		},
		{
			{ SDB_TYPE_INTEGER, { .integer = 4711 } },
			12, INT_TYPE "\0\0\0\0\0\0\x12\x67",
		},
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 3.141592653e130 } },
			12, DECIMAL_TYPE "\x5b\x6\xa9\x40\x66\x1e\x10\x4",
		},
		{
			{ SDB_TYPE_STRING, { .string = "some string" } },
			16, STRING_TYPE "some string\0",
		},
		{
			{ SDB_TYPE_DATETIME, { .datetime = 1418923804000000 } },
			12, DATETIME_TYPE "\x0\x5\xa\x80\xf1\x4c\xff\x0",
		},
		{
			{ SDB_TYPE_BINARY, { .binary = {
				4, (unsigned char *)"\x42\x0\xa\x1b" } } },
			12, BINARY_TYPE "\0\0\0\x4" "\x42\x0\xa\x1b",
		},
		{
			{ SDB_TYPE_REGEX, { .re = { "dummy", dummy_re } } },
			10, REGEX_TYPE "dummy\0",
		},
		{
			{ SDB_TYPE_INTEGER | SDB_TYPE_ARRAY, { .array = {
				3, int_values } } },
			32, INT_ARRAY "\0\0\0\x3" "\0\0\0\0\0\0\0\x2f"
				"\0\0\0\0\0\0\0\xb" "\0\0\0\0\0\0\0\x17"
		},
		{
			{ SDB_TYPE_DECIMAL | SDB_TYPE_ARRAY, { .array = {
				2, dec_values } } },
			24, DECIMAL_ARRAY "\0\0\0\x2" "\x40\x47\x8e\x14\x7a\xe1\x47\xae"
				"\x3f\xe0\0\0\0\0\0\0"
		},
		{
			{ SDB_TYPE_STRING | SDB_TYPE_ARRAY, { .array = {
				2, string_values } } },
			17, STRING_ARRAY "\0\0\0\x2" "foo\0" "abcd\0"
		},
		{
			{ SDB_TYPE_DATETIME | SDB_TYPE_ARRAY, { .array = {
				2, datetime_values } } },
			24, DATETIME_ARRAY "\0\0\0\x2" "\0\0\0\0\0\0\x12\x67"
				"\x11\x22\x10\xf4\x7d\xe9\x81\x15"
		},
		{
			{ SDB_TYPE_BINARY | SDB_TYPE_ARRAY, { .array = {
				2, binary_values } } },
			23, BINARY_ARRAY "\0\0\0\x2" "\0\0\0\x3" "\x1\x2\x3"
				"\0\0\0\4" "\x42\x0\xa\x1b"
		},
		{
			{ SDB_TYPE_REGEX | SDB_TYPE_ARRAY, { .array = {
				1, regex_values } } },
			20, REGEX_ARRAY "\0\0\0\1" "dummy regex\0"
		},
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		ssize_t len = sdb_proto_marshal_data(NULL, 0, &golden_data[i].datum);
		char buf[len > 0 ? len : 1];
		char v1[sdb_data_strlen(&golden_data[i].datum)];
		char v2[sdb_data_strlen(&golden_data[i].datum)];

		sdb_data_t datum = SDB_DATA_INIT;
		ssize_t check;

		if (! sdb_data_format(&golden_data[i].datum, v1, sizeof(v1), 0))
			snprintf(v1, sizeof(v1), "<ERR>");

		fail_unless(len == golden_data[i].expected_len,
				"<%zu> sdb_proto_marshal_data(NULL, 0, %s) = %zi; expected: %zi",
				i, v1, len, golden_data[i].expected_len);

		if (len < 0)
			continue;

		len = sdb_proto_marshal_data(buf, sizeof(buf), &golden_data[i].datum);
		fail_unless(len == golden_data[i].expected_len,
				"<%zu> sdb_proto_marshal_data(<buf>, %zu, %s) = %zi; expected: %zi",
				i, sizeof(buf), v1, len, golden_data[i].expected_len);
		if (memcmp(buf, golden_data[i].expected, len) != 0) {
			size_t pos;
			for (pos = 0; pos < (size_t)len; ++pos)
				if (buf[pos] != golden_data[i].expected[pos])
					break;
			fail("<%zu> sdb_proto_marshal_data(%s) -> \"%s\"; expected: \"%s\" "
					"(bytes %zu differ: '%x' != '%x')",
					i, v1, buf, golden_data[i].expected,
					pos, (int)buf[pos], (int)golden_data[i].expected[pos]);
		}

		check = sdb_proto_unmarshal_data(buf, len, &datum);
		if (! sdb_data_format(&datum, v2, sizeof(v2), 0))
			snprintf(v2, sizeof(v2), "<ERR>");

		if (sdb_data_isnull(&golden_data[i].datum))
			fail_unless(sdb_data_isnull(&datum),
					"<%zu> sdb_proto_unmarshal_data(buf<%s>) -> \"%s\"", i, v1, v2);
		else
			fail_unless(sdb_data_cmp(&golden_data[i].datum, &datum) == 0,
					"<%zu> sdb_proto_unmarshal_data(buf<%s>) -> \"%s\"", i, v1, v2);
		fail_unless(check == len,
				"<%zu> sdb_proto_unmarshal_data(buf<%s>) = %zi; expected: %zi",
				i, v1, check, len);

		sdb_data_free_datum(&datum);
	}
}
END_TEST

#define HOST_TYPE "\0\0\0\1"
#define SVC_TYPE "\0\0\0\2"
#define METRIC_TYPE "\0\0\0\3"
#define HOST_ATTR_TYPE "\0\0\0\x11"
#define SVC_ATTR_TYPE "\0\0\0\x12"
#define METRIC_ATTR_TYPE "\0\0\0\x13"

START_TEST(test_marshal_host)
{
	struct {
		sdb_proto_host_t host;
		ssize_t expected_len;
		char *expected;
	} golden_data[] = {
		{
			{ 4711, "hostA" },
			18, HOST_TYPE "\0\0\0\0\0\0\x12\x67" "hostA\0"
		},
		{
			{ 0, "hostA" },
			18, HOST_TYPE "\0\0\0\0\0\0\0\0" "hostA\0"
		},
		{ { 4711, NULL }, -1, NULL },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		ssize_t len = sdb_proto_marshal_host(NULL, 0, &golden_data[i].host);
		char buf[len > 0 ? len : 1];

		sdb_proto_host_t host;
		ssize_t check;

		fail_unless(len == golden_data[i].expected_len,
				"<%zu> sdb_proto_marshal_host(NULL, 0, %s) = %zi; expected: %zi",
				i, golden_data[i].host.name, len, golden_data[i].expected_len);

		if (len < 0)
			continue;

		len = sdb_proto_marshal_host(buf, sizeof(buf), &golden_data[i].host);
		fail_unless(len == golden_data[i].expected_len,
				"<%zu> sdb_proto_marshal_host(<buf>, %zu, %s) = %zi; expected: %zi",
				i, sizeof(buf), golden_data[i].host.name,
				len, golden_data[i].expected_len);
		if (memcmp(buf, golden_data[i].expected, len) != 0) {
			size_t pos;
			for (pos = 0; pos < (size_t)len; ++pos)
				if (buf[pos] != golden_data[i].expected[pos])
					break;
			fail("<%zu> sdb_proto_marshal_host(%s) -> \"%s\"; expected: \"%s\" "
					"(bytes %zu differ: '%x' != '%x')",
					i, golden_data[i].host.name, buf, golden_data[i].expected,
					pos, (int)buf[pos], (int)golden_data[i].expected[pos]);
		}

		check = sdb_proto_unmarshal_host(buf, len, &host);
		fail_unless(check == len,
				"<%zu> sdb_proto_unmarshal_host(buf<%s>) = %zi; expected: %zi",
				i, golden_data[i].host.name, check, len);
		fail_unless((host.last_update == golden_data[i].host.last_update)
				&& streq(host.name, golden_data[i].host.name),
				"<%zu> sdb_proto_unmarshal_host(buf<%s>) = { %"PRIsdbTIME", %s }; "
				"expected: { %"PRIsdbTIME", %s }", i, golden_data[i].host.name,
				host.last_update, host.name, golden_data[i].host.last_update,
				golden_data[i].host.name);
	}
}
END_TEST

START_TEST(test_marshal_service)
{
	struct {
		sdb_proto_service_t svc;
		ssize_t expected_len;
		char *expected;
	} golden_data[] = {
		{
			{ 4711, "hostA", "serviceX" },
			27, SVC_TYPE "\0\0\0\0\0\0\x12\x67" "hostA\0serviceX\0"
		},
		{
			{ 0, "hostA", "serviceX" },
			27, SVC_TYPE "\0\0\0\0\0\0\0\0" "hostA\0serviceX\0"
		},
		{ { 4711, "hostA", NULL }, -1, NULL },
		{ { 4711, NULL, "serviceX" }, -1, NULL },
		{ { 4711, NULL, NULL }, -1, NULL },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		ssize_t len = sdb_proto_marshal_service(NULL, 0, &golden_data[i].svc);
		char buf[len > 0 ? len : 1];

		sdb_proto_service_t svc;
		ssize_t check;

		fail_unless(len == golden_data[i].expected_len,
				"<%zu> sdb_proto_marshal_service(NULL, 0, %s) = %zi; expected: %zi",
				i, golden_data[i].svc.name, len, golden_data[i].expected_len);

		if (len < 0)
			continue;

		len = sdb_proto_marshal_service(buf, sizeof(buf), &golden_data[i].svc);
		fail_unless(len == golden_data[i].expected_len,
				"<%zu> sdb_proto_marshal_service(<buf>, %zu, %s) = %zi; expected: %zi",
				i, sizeof(buf), golden_data[i].svc.name,
				len, golden_data[i].expected_len);
		if (memcmp(buf, golden_data[i].expected, len) != 0) {
			size_t pos;
			for (pos = 0; pos < (size_t)len; ++pos)
				if (buf[pos] != golden_data[i].expected[pos])
					break;
			fail("<%zu> sdb_proto_marshal_service(%s) -> \"%s\"; expected: \"%s\" "
					"(bytes %zu differ: '%x' != '%x')",
					i, golden_data[i].svc.name, buf, golden_data[i].expected,
					pos, (int)buf[pos], (int)golden_data[i].expected[pos]);
		}

		check = sdb_proto_unmarshal_service(buf, len, &svc);
		fail_unless(check == len,
				"<%zu> sdb_proto_unmarshal_service(buf<%s>) = %zi; expected: %zi",
				i, golden_data[i].svc.name, check, len);
		fail_unless((svc.last_update == golden_data[i].svc.last_update)
				&& streq(svc.hostname, golden_data[i].svc.hostname)
				&& streq(svc.name, golden_data[i].svc.name),
				"<%zu> sdb_proto_unmarshal_service(buf<%s>) = { %"PRIsdbTIME", %s, %s }; "
				"expected: { %"PRIsdbTIME", %s, %s }", i, golden_data[i].svc.name,
				svc.last_update, svc.hostname, svc.name, golden_data[i].svc.last_update,
				golden_data[i].svc.hostname, golden_data[i].svc.name);
	}
}
END_TEST

START_TEST(test_marshal_metric)
{
	struct {
		sdb_proto_metric_t metric;
		ssize_t expected_len;
		char *expected;
	} golden_data[] = {
		{
			{ 4711, "hostA", "metricX", NULL, NULL },
			26, METRIC_TYPE "\0\0\0\0\0\0\x12\x67" "hostA\0metricX\0"
		},
		{
			{ 0, "hostA", "metricX", NULL, NULL },
			26, METRIC_TYPE "\0\0\0\0\0\0\0\0" "hostA\0metricX\0"
		},
		{
			{ 0, "hostA", "metricX", "type", NULL },
			26, METRIC_TYPE "\0\0\0\0\0\0\0\0" "hostA\0metricX\0"
		},
		{
			{ 0, "hostA", "metricX", NULL, "id" },
			26, METRIC_TYPE "\0\0\0\0\0\0\0\0" "hostA\0metricX\0"
		},
		{
			{ 4711, "hostA", "metricX", "type", "id" },
			34, METRIC_TYPE "\0\0\0\0\0\0\x12\x67" "hostA\0metricX\0type\0id\0"
		},
		{ { 4711, "hostA", NULL, NULL, NULL }, -1, NULL },
		{ { 4711, NULL, "metricX", NULL, NULL }, -1, NULL },
		{ { 4711, NULL, NULL, NULL, NULL }, -1, NULL },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		ssize_t len = sdb_proto_marshal_metric(NULL, 0, &golden_data[i].metric);
		char buf[len > 0 ? len : 1];

		sdb_proto_metric_t metric;
		ssize_t check;

		fail_unless(len == golden_data[i].expected_len,
				"<%zu> sdb_proto_marshal_metric(NULL, 0, %s) = %zi; expected: %zi",
				i, golden_data[i].metric.name, len, golden_data[i].expected_len);

		if (len < 0)
			continue;

		len = sdb_proto_marshal_metric(buf, sizeof(buf), &golden_data[i].metric);
		fail_unless(len == golden_data[i].expected_len,
				"<%zu> sdb_proto_marshal_metric(<buf>, %zu, %s) = %zi; expected: %zi",
				i, sizeof(buf), golden_data[i].metric.name,
				len, golden_data[i].expected_len);
		if (memcmp(buf, golden_data[i].expected, len) != 0) {
			size_t pos;
			for (pos = 0; pos < (size_t)len; ++pos)
				if (buf[pos] != golden_data[i].expected[pos])
					break;
			fail("<%zu> sdb_proto_marshal_metric(%s) -> \"%s\"; expected: \"%s\" "
					"(bytes %zu differ: '%x' != '%x')",
					i, golden_data[i].metric.name, buf, golden_data[i].expected,
					pos, (int)buf[pos], (int)golden_data[i].expected[pos]);
		}

		if ((! golden_data[i].metric.store_type)
				|| (! golden_data[i].metric.store_id)) {
			/* if any of these is NULL, we expect both to be NULL */
			golden_data[i].metric.store_type = NULL;
			golden_data[i].metric.store_id = NULL;
		}

		check = sdb_proto_unmarshal_metric(buf, len, &metric);
		fail_unless(check == len,
				"<%zu> sdb_proto_unmarshal_metric(buf<%s>) = %zi; expected: %zi",
				i, golden_data[i].metric.name, check, len);
		fail_unless((metric.last_update == golden_data[i].metric.last_update)
				&& streq(metric.hostname, golden_data[i].metric.hostname)
				&& streq(metric.name, golden_data[i].metric.name)
				&& streq(metric.store_type, golden_data[i].metric.store_type)
				&& streq(metric.store_id, golden_data[i].metric.store_id),
				"<%zu> sdb_proto_unmarshal_metric(buf<%s>) = "
				"{ %"PRIsdbTIME", %s, %s, %s, %s }; expected: "
				"{ %"PRIsdbTIME", %s, %s, %s, %s }", i, golden_data[i].metric.name,
				metric.last_update, metric.hostname, metric.name,
				metric.store_type, metric.store_id,
				golden_data[i].metric.last_update,
				golden_data[i].metric.hostname, golden_data[i].metric.name,
				golden_data[i].metric.store_type, golden_data[i].metric.store_id);
	}
}
END_TEST

START_TEST(test_marshal_attribute)
{
	sdb_data_t v = { SDB_TYPE_INTEGER, { .integer = 4711 } };
#define VAL "\0\0\0\2" "\0\0\0\0\0\0\x12\x67"
	struct {
		sdb_proto_attribute_t attr;
		ssize_t expected_len;
		char *expected;
	} golden_data[] = {
		{
			{ 4711, SDB_HOST, NULL, "hostA", "k1", v },
			33, HOST_ATTR_TYPE "\0\0\0\0\0\0\x12\x67" "hostA\0" "k1\0" VAL
		},
		{
			{ 4711, SDB_SERVICE, "hostA", "svc1", "k1", v },
			38, SVC_ATTR_TYPE "\0\0\0\0\0\0\x12\x67" "hostA\0" "svc1\0" "k1\0" VAL
		},
		{
			{ 4711, SDB_METRIC, "hostA", "m1", "k1", v },
			36, METRIC_ATTR_TYPE "\0\0\0\0\0\0\x12\x67" "hostA\0" "m1\0" "k1\0" VAL
		},
		{ { 4711, SDB_HOST, NULL, NULL, "k1", v }, -1, NULL },
		{ { 4711, SDB_HOST, NULL, "hostA", NULL, v }, -1, NULL },
		{ { 4711, SDB_SERVICE, NULL, "svc1", "k1", v }, -1, NULL },
		{ { 4711, SDB_METRIC, NULL, "m1", "k1", v }, -1, NULL },
		{ { 4711, 0, "hostA", "svc1", "k1", v }, -1, NULL },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		ssize_t len = sdb_proto_marshal_attribute(NULL, 0, &golden_data[i].attr);
		char buf[len > 0 ? len : 1];

		sdb_proto_attribute_t attr;
		ssize_t check;
		char v1[sdb_data_strlen(&golden_data[i].attr.value) + 1];
		char v2[sdb_data_strlen(&golden_data[i].attr.value) + 1];

		fail_unless(len == golden_data[i].expected_len,
				"<%zu> sdb_proto_marshal_attribute(NULL, 0, %s) = %zi; expected: %zi",
				i, golden_data[i].attr.key, len, golden_data[i].expected_len);

		if (len < 0)
			continue;

		len = sdb_proto_marshal_attribute(buf, sizeof(buf), &golden_data[i].attr);
		fail_unless(len == golden_data[i].expected_len,
				"<%zu> sdb_proto_marshal_attribute(<buf>, %zu, %s) = %zi; expected: %zi",
				i, sizeof(buf), golden_data[i].attr.key,
				len, golden_data[i].expected_len);
		if (memcmp(buf, golden_data[i].expected, len) != 0) {
			size_t pos;
			for (pos = 0; pos < (size_t)len; ++pos)
				if (buf[pos] != golden_data[i].expected[pos])
					break;
			fail("<%zu> sdb_proto_marshal_attribute(%s) -> \"%s\"; expected: \"%s\" "
					"(bytes %zu differ: '%x' != '%x')",
					i, golden_data[i].attr.key, buf, golden_data[i].expected,
					pos, (int)buf[pos], (int)golden_data[i].expected[pos]);
		}

		if (! sdb_data_format(&golden_data[i].attr.value, v1, sizeof(v1), 0))
			snprintf(v1, sizeof(v1), "<ERR>");

		check = sdb_proto_unmarshal_attribute(buf, len, &attr);
		fail_unless(check == len,
				"<%zu> sdb_proto_unmarshal_attribute(buf<%s>) = %zi; expected: %zi",
				i, golden_data[i].attr.key, check, len);

		if (! sdb_data_format(&attr.value, v2, sizeof(v2), 0))
			snprintf(v2, sizeof(v2), "<ERR>");
		fail_unless((attr.last_update == golden_data[i].attr.last_update)
				&& (attr.parent_type == golden_data[i].attr.parent_type)
				&& streq(attr.hostname, golden_data[i].attr.hostname)
				&& streq(attr.parent, golden_data[i].attr.parent)
				&& streq(attr.key, golden_data[i].attr.key)
				&& (sdb_data_cmp(&attr.value, &golden_data[i].attr.value) == 0),
				"<%zu> sdb_proto_unmarshal_attribute(buf<%s>) = "
				"{ %"PRIsdbTIME", %s, %s, %s, %s, %s }; expected: "
				"{ %"PRIsdbTIME", %s, %s, %s, %s, %s }", i, golden_data[i].attr.key,
				attr.last_update, SDB_STORE_TYPE_TO_NAME(attr.parent_type),
				attr.hostname, attr.parent, attr.key, v1,
				golden_data[i].attr.last_update,
				SDB_STORE_TYPE_TO_NAME(golden_data[i].attr.parent_type),
				golden_data[i].attr.hostname, golden_data[i].attr.parent,
				golden_data[i].attr.key, v2);
	}
}
END_TEST

TEST_MAIN("utils::proto")
{
	TCase *tc = tcase_create("core");
	tcase_add_test(tc, test_marshal_data);
	tcase_add_test(tc, test_marshal_host);
	tcase_add_test(tc, test_marshal_service);
	tcase_add_test(tc, test_marshal_metric);
	tcase_add_test(tc, test_marshal_attribute);
	ADD_TCASE(tc);
}
TEST_MAIN_END

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

