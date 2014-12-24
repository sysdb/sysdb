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

#include "utils/proto.h"
#include "libsysdb_test.h"

#include <check.h>
#include <stdio.h>
#include <string.h>

START_TEST(test_marshal_data)
{
#define INT_TYPE "\0\0\0\1"
#define DECIMAL_TYPE "\0\0\0\2"
#define STRING_TYPE "\0\0\0\3"
#define DATETIME_TYPE "\0\0\0\4"
#define BINARY_TYPE "\0\0\0\5"
#define REGEX_TYPE "\0\0\0\6"

#define NULL_ARRAY "\0\0\1\0"
#define INT_ARRAY "\0\0\1\1"
#define DECIMAL_ARRAY "\0\0\1\2"
#define STRING_ARRAY "\0\0\1\3"
#define DATETIME_ARRAY "\0\0\1\4"
#define BINARY_ARRAY "\0\0\1\5"
#define REGEX_ARRAY "\0\0\1\6"

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
		char v[sdb_data_strlen(&golden_data[i].datum)];

		if (sdb_data_format(&golden_data[i].datum, v, sizeof(v), 0) < 0)
			snprintf(v, sizeof(v), "<ERR>");

		fail_unless(len == golden_data[i].expected_len,
				"sdb_proto_marshal_data(NULL, 0, %s) = %zi; expected: %zi",
				v, len, golden_data[i].expected_len);

		if (len < 0)
			continue;

		len = sdb_proto_marshal_data(buf, sizeof(buf), &golden_data[i].datum);
		fail_unless(len == golden_data[i].expected_len,
				"sdb_proto_marshal_data(<buf>, %zu, %s) = %zi; expected: %zi",
				sizeof(buf), v, len, golden_data[i].expected_len);
		if (memcmp(buf, golden_data[i].expected, len) != 0) {
			size_t pos;
			for (pos = 0; pos < (size_t)len; ++pos)
				if (buf[pos] != golden_data[i].expected[pos])
					break;
			fail("sdb_proto_marshal_data(%s) -> \"%s\"; expected: \"%s\" "
					"(bytes %zu differ: '%x' != '%x')",
					v, buf, golden_data[i].expected,
					pos, (int)buf[pos], (int)golden_data[i].expected[pos]);
		}
	}
}
END_TEST

Suite *
util_proto_suite(void)
{
	Suite *s = suite_create("utils::proto");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_test(tc, test_marshal_data);
	suite_add_tcase(s, tc);

	return s;
} /* util_proto_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

