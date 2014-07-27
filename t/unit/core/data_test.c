/*
 * SysDB - t/unit/core/data_test.c
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

#include "core/data.h"
#include "libsysdb_test.h"

#include <check.h>

START_TEST(test_data)
{
	sdb_data_t d1, d2;
	int check;

	d2.type = SDB_TYPE_INTEGER;
	d2.data.integer = 4711;
	memset(&d1, 0, sizeof(d1));
	check = sdb_data_copy(&d1, &d2);
	fail_unless(!check, "sdb_data_copy() = %i; expected: 0", check);
	fail_unless(d1.type == d2.type,
			"sdb_data_copy() didn't copy type; got: %i; expected: %i",
			d1.type, d2.type);
	fail_unless(d1.data.integer == d2.data.integer,
			"sdb_data_copy() didn't copy integer data: got: %d; expected: %d",
			d1.data.integer, d2.data.integer);

	d2.type = SDB_TYPE_DECIMAL;
	d2.data.decimal = 47.11;
	check = sdb_data_copy(&d1, &d2);
	fail_unless(!check, "sdb_data_copy() = %i; expected: 0", check);
	fail_unless(d1.type == d2.type,
			"sdb_data_copy() didn't copy type; got: %i; expected: %i",
			d1.type, d2.type);
	fail_unless(d1.data.decimal == d2.data.decimal,
			"sdb_data_copy() didn't copy decimal data: got: %f; expected: %f",
			d1.data.decimal, d2.data.decimal);

	d2.type = SDB_TYPE_STRING;
	d2.data.string = "some string";
	check = sdb_data_copy(&d1, &d2);
	fail_unless(!check, "sdb_data_copy() = %i; expected: 0", check);
	fail_unless(d1.type == d2.type,
			"sdb_data_copy() didn't copy type; got: %i; expected: %i",
			d1.type, d2.type);
	fail_unless(!strcmp(d1.data.string, d2.data.string),
			"sdb_data_copy() didn't copy string data: got: %s; expected: %s",
			d1.data.string, d2.data.string);

	sdb_data_free_datum(&d1);
	fail_unless(d1.data.string == NULL,
			"sdb_data_free_datum() didn't free string data");

	d2.type = SDB_TYPE_DATETIME;
	d2.data.datetime = 4711;
	check = sdb_data_copy(&d1, &d2);
	fail_unless(!check, "sdb_data_copy() = %i; expected: 0", check);
	fail_unless(d1.type == d2.type,
			"sdb_data_copy() didn't copy type; got: %i; expected: %i",
			d1.type, d2.type);
	fail_unless(d1.data.datetime == d2.data.datetime,
			"sdb_data_copy() didn't copy datetime data: got: %d; expected: %d",
			d1.data.datetime, d2.data.datetime);

	d2.type = SDB_TYPE_BINARY;
	d2.data.binary.datum = (unsigned char *)"some string";
	d2.data.binary.length = strlen((const char *)d2.data.binary.datum);
	check = sdb_data_copy(&d1, &d2);
	fail_unless(!check, "sdb_data_copy() = %i; expected: 0", check);
	fail_unless(d1.type == d2.type,
			"sdb_data_copy() didn't copy type; got: %i; expected: %i",
			d1.type, d2.type);
	fail_unless(d1.data.binary.length == d2.data.binary.length,
			"sdb_data_copy() didn't copy length; got: %d; expected: 5d",
			d1.data.binary.length, d2.data.binary.length);
	fail_unless(!memcmp(d1.data.binary.datum, d2.data.binary.datum,
				d2.data.binary.length),
			"sdb_data_copy() didn't copy binary data: got: %s; expected: %s",
			d1.data.string, d2.data.string);

	sdb_data_free_datum(&d1);
	fail_unless(d1.data.binary.length == 0,
			"sdb_data_free_datum() didn't reset binary datum length");
	fail_unless(d1.data.binary.datum == NULL,
			"sdb_data_free_datum() didn't free binary datum");
}
END_TEST

START_TEST(test_cmp)
{
	struct {
		sdb_data_t d1;
		sdb_data_t d2;
		int expected;
	} golden_data[] = {
		{
			{ SDB_TYPE_INTEGER, { .integer = 47 } },
			{ SDB_TYPE_INTEGER, { .integer = 4711 } },
			-1,
		},
		{
			{ SDB_TYPE_INTEGER, { .integer = 4711 } },
			{ SDB_TYPE_INTEGER, { .integer = 4711 } },
			0,
		},
		{
			{ SDB_TYPE_INTEGER, { .integer = 4711 } },
			{ SDB_TYPE_INTEGER, { .integer = 47 } },
			1,
		},
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 65535.9 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 65536.0 } },
			-1,
		},
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 65536.0 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 65536.0 } },
			0,
		},
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 65536.0 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 65535.9 } },
			1,
		},
		{
			{ SDB_TYPE_STRING, { .string = NULL } },
			{ SDB_TYPE_STRING, { .string = "" } },
			-1,
		},
		{
			{ SDB_TYPE_STRING, { .string = NULL } },
			{ SDB_TYPE_STRING, { .string = NULL } },
			0,
		},
		{
			{ SDB_TYPE_STRING, { .string = "" } },
			{ SDB_TYPE_STRING, { .string = NULL } },
			1,
		},
		{
			{ SDB_TYPE_STRING, { .string = "a" } },
			{ SDB_TYPE_STRING, { .string = "b" } },
			-1,
		},
		{
			{ SDB_TYPE_STRING, { .string = "a" } },
			{ SDB_TYPE_STRING, { .string = "ab" } },
			-1,
		},
		{
			{ SDB_TYPE_STRING, { .string = "a" } },
			{ SDB_TYPE_STRING, { .string = "a" } },
			0,
		},
		{
			{ SDB_TYPE_STRING, { .string = "b" } },
			{ SDB_TYPE_STRING, { .string = "a" } },
			1,
		},
		{
			{ SDB_TYPE_STRING, { .string = "ab" } },
			{ SDB_TYPE_STRING, { .string = "a" } },
			1,
		},
		{
			{ SDB_TYPE_DATETIME, { .datetime = 471147114711471000 } },
			{ SDB_TYPE_DATETIME, { .datetime = 471147114711471100 } },
			-1,
		},
		{
			{ SDB_TYPE_DATETIME, { .datetime = 471147114711471100 } },
			{ SDB_TYPE_DATETIME, { .datetime = 471147114711471100 } },
			0,
		},
		{
			{ SDB_TYPE_DATETIME, { .datetime = 471147114711471100 } },
			{ SDB_TYPE_DATETIME, { .datetime = 471147114711471000 } },
			1,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 0, NULL } } },
			{ SDB_TYPE_BINARY, { .binary = { 1, (unsigned char *)"a" } } },
			-1,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 0, NULL } } },
			{ SDB_TYPE_BINARY, { .binary = { 0, NULL } } },
			0,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 1, (unsigned char *)"a" } } },
			{ SDB_TYPE_BINARY, { .binary = { 0, NULL } } },
			1,
		},
		{
			{
				SDB_TYPE_BINARY,
				{ .binary = { 3, (unsigned char *)"a\0a" } },
			},
			{
				SDB_TYPE_BINARY,
				{ .binary = { 3, (unsigned char *)"a\0b" } },
			},
			-1,
		},
		{
			{
				SDB_TYPE_BINARY,
				{ .binary = { 1, (unsigned char *)"a" } },
			},
			{
				SDB_TYPE_BINARY,
				{ .binary = { 3, (unsigned char *)"a\0\0" } },
			},
			-1,
		},
		{
			{
				SDB_TYPE_BINARY,
				{ .binary = { 3, (unsigned char *)"a\0a" } },
			},
			{
				SDB_TYPE_BINARY,
				{ .binary = { 3, (unsigned char *)"a\0a" } },
			},
			0,
		},
		{
			{
				SDB_TYPE_BINARY,
				{ .binary = { 3, (unsigned char *)"a\0b" } },
			},
			{
				SDB_TYPE_BINARY,
				{ .binary = { 3, (unsigned char *)"a\0a" } },
			},
			1,
		},
		{
			{
				SDB_TYPE_BINARY,
				{ .binary = { 3, (unsigned char *)"a\0\0" } },
			},
			{
				SDB_TYPE_BINARY,
				{ .binary = { 1, (unsigned char *)"a" } },
			},
			1,
		},
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		int check = sdb_data_cmp(&golden_data[i].d1, &golden_data[i].d2);
		check = check < 0 ? -1 : check > 0 ? 1 : 0;
		if (check != golden_data[i].expected) {
			char d1_str[64] = "", d2_str[64] = "";
			sdb_data_format(&golden_data[i].d1, d1_str, sizeof(d1_str),
					SDB_DOUBLE_QUOTED);
			sdb_data_format(&golden_data[i].d2, d2_str, sizeof(d2_str),
					SDB_DOUBLE_QUOTED);
			fail("sdb_data_cmp(%s, %s) = %d; expected: %d",
					d1_str, d2_str, check, golden_data[i].expected);
		}
	}
}
END_TEST

START_TEST(test_expr_eval)
{
	struct {
		sdb_data_t d1;
		sdb_data_t d2;
		sdb_data_t expected_add;
		sdb_data_t expected_sub;
		sdb_data_t expected_mul;
		sdb_data_t expected_div;
		sdb_data_t expected_mod;
		sdb_data_t expected_concat;
	} golden_data[] = {
		{
			{ SDB_TYPE_INTEGER, { .integer = 4711 } },
			{ SDB_TYPE_INTEGER, { .integer = 47 } },
			{ SDB_TYPE_INTEGER, { .integer = 4758 } },
			{ SDB_TYPE_INTEGER, { .integer = 4664 } },
			{ SDB_TYPE_INTEGER, { .integer = 221417 } },
			{ SDB_TYPE_INTEGER, { .integer = 100 } },
			{ SDB_TYPE_INTEGER, { .integer = 11 } },
			SDB_DATA_INIT,
		},
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 35.0 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 17.5 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 52.5 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 17.5 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 612.5 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 2.0 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 0.0 } },
			SDB_DATA_INIT,
		},
		{
			{ SDB_TYPE_STRING, { .string = NULL } },
			{ SDB_TYPE_STRING, { .string = "" } },
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			{ SDB_TYPE_STRING, { .string = "" } },
		},
		{
			{ SDB_TYPE_STRING, { .string = NULL } },
			{ SDB_TYPE_STRING, { .string = NULL } },
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			{ SDB_TYPE_STRING, { .string = NULL } },
		},
		{
			{ SDB_TYPE_STRING, { .string = "" } },
			{ SDB_TYPE_STRING, { .string = NULL } },
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			{ SDB_TYPE_STRING, { .string = "" } },
		},
		{
			{ SDB_TYPE_STRING, { .string = "a" } },
			{ SDB_TYPE_STRING, { .string = "b" } },
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			{ SDB_TYPE_STRING, { .string = "ab" } },
		},
		{
			{ SDB_TYPE_DATETIME, { .datetime = 47114711 } },
			{ SDB_TYPE_DATETIME, { .datetime = 4711 } },
			{ SDB_TYPE_DATETIME, { .datetime = 47119422 } },
			{ SDB_TYPE_DATETIME, { .datetime = 47110000 } },
			{ SDB_TYPE_DATETIME, { .datetime = 221957403521 } },
			{ SDB_TYPE_DATETIME, { .datetime = 10001 } },
			{ SDB_TYPE_DATETIME, { .datetime = 0 } },
			SDB_DATA_INIT,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 0, NULL } } },
			{ SDB_TYPE_BINARY, { .binary = { 1, (unsigned char *)"a" } } },
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			{ SDB_TYPE_BINARY, { .binary = { 1, (unsigned char *)"a" } } },
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 0, NULL } } },
			{ SDB_TYPE_BINARY, { .binary = { 0, NULL } } },
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			{ SDB_TYPE_BINARY, { .binary = { 0, NULL } } },
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 1, (unsigned char *)"a" } } },
			{ SDB_TYPE_BINARY, { .binary = { 0, NULL } } },
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			{ SDB_TYPE_BINARY, { .binary = { 1, (unsigned char *)"a" } } },
		},
		{
			{
				SDB_TYPE_BINARY,
				{ .binary = { 3, (unsigned char *)"a\0a" } },
			},
			{
				SDB_TYPE_BINARY,
				{ .binary = { 3, (unsigned char *)"b\0b" } },
			},
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			SDB_DATA_INIT,
			{
				SDB_TYPE_BINARY,
				{ .binary = { 6, (unsigned char *)"a\0ab\0b" } },
			},
		},
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		struct {
			int op;
			sdb_data_t expected;
		} tests[] = {
			{ SDB_DATA_ADD, golden_data[i].expected_add },
			{ SDB_DATA_SUB, golden_data[i].expected_sub },
			{ SDB_DATA_MUL, golden_data[i].expected_mul },
			{ SDB_DATA_DIV, golden_data[i].expected_div },
			{ SDB_DATA_MOD, golden_data[i].expected_mod },
			{ SDB_DATA_CONCAT, golden_data[i].expected_concat },
		};

		size_t j;
		for (j = 0; j < SDB_STATIC_ARRAY_LEN(tests); ++j) {
			sdb_data_t res;
			int check;

			char d1_str[64] = "", d2_str[64] = "";
			sdb_data_format(&golden_data[i].d1, d1_str, sizeof(d1_str),
					SDB_DOUBLE_QUOTED);
			sdb_data_format(&golden_data[i].d2, d2_str, sizeof(d2_str),
					SDB_DOUBLE_QUOTED);

			check = sdb_data_expr_eval(tests[j].op,
					&golden_data[i].d1, &golden_data[i].d2, &res);
			fail_unless((check == 0) == (tests[j].expected.type != 0),
					"sdb_data_expr_eval(%s, %s, %s) = %d; expected: %d",
					SDB_DATA_OP_TO_STRING(tests[j].op), d1_str, d2_str, check,
					tests[j].expected.type == 0 ? -1 : 0);
			if (tests[j].expected.type == 0)
				continue;

			check = sdb_data_cmp(&res, &tests[j].expected);
			if (check != 0) {
				char res_str[64] = "", expected_str[64] = "";
				sdb_data_format(&res, res_str, sizeof(res_str),
						SDB_DOUBLE_QUOTED);
				sdb_data_format(&tests[j].expected, expected_str,
						sizeof(expected_str), SDB_DOUBLE_QUOTED);
				fail("sdb_data_expr_eval(%s, %s, %s) evaluated to %s; "
						"expected: %s", SDB_DATA_OP_TO_STRING(tests[j].op),
						d1_str, d2_str, res_str, expected_str);
			}

			sdb_data_free_datum(&res);
		}
	}
}
END_TEST

START_TEST(test_format)
{
	struct {
		sdb_data_t datum;
		const char *expected;
	} golden_data[] = {
		{
			{ SDB_TYPE_INTEGER, { .integer = 4711 } },
			"4711",
		},
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 65536.0 } },
			"0x1p+16",
		},
		{
			{ SDB_TYPE_STRING, { .string = NULL } },
			"\"NULL\"",
		},
		{
			{ SDB_TYPE_STRING, { .string = "this is a test" } },
			"\"this is a test\"",
		},
		{
			{ SDB_TYPE_STRING, { .string = "special \\ \" characters" } },
			"\"special \\\\ \\\" characters\"",
		},
		{
			{ SDB_TYPE_DATETIME, { .datetime= 471147114711471100 } },
			"\"1984-12-06 02:11:54 +0000\"",
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 0, NULL } } },
			"\"\"",
		},
		{
			{
				SDB_TYPE_BINARY,
				{ .binary = { 12, (unsigned char *)"binary\0crap\x42" } },
			},
			"\"\\x62\\x69\\x6e\\x61\\x72\\x79\\x0\\x63\\x72\\x61\\x70\\x42\"",
		},
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_data_t *datum = &golden_data[i].datum;
		char buf[sdb_data_strlen(datum) + 2];
		int check;

		memset(buf, (int)'A', sizeof(buf));

		check = sdb_data_format(datum, buf, sizeof(buf) - 1,
				SDB_DOUBLE_QUOTED);
		fail_unless(check > 0,
				"sdb_data_format(type=%s) = %d; expected: >0",
				SDB_TYPE_TO_STRING(datum->type), check);
		fail_unless(! strcmp(buf, golden_data[i].expected),
				"sdb_data_format(type=%s) used wrong format: %s; expected: %s",
				SDB_TYPE_TO_STRING(datum->type), buf, golden_data[i].expected);

		fail_unless((size_t)check <= sizeof(buf) - 2,
				"sdb_data_format(type=%s) wrote %d bytes; "
				"expected <= %zu based on sdb_data_strlen()",
				SDB_TYPE_TO_STRING(datum->type), check, sizeof(buf) - 2);

		fail_unless(buf[sizeof(buf) - 2] == '\0',
				"sdb_data_format(type=%s) did not nul-terminate the buffer",
				SDB_TYPE_TO_STRING(datum->type));
		fail_unless(buf[sizeof(buf) - 1] == 'A',
				"sdb_data_format(type=%s) wrote past the end of the buffer",
				SDB_TYPE_TO_STRING(datum->type));
	}
}
END_TEST

START_TEST(test_parse)
{
	struct {
		char *input;
		sdb_data_t result;
		int expected;
	} golden_data[] = {
		{ "4711",    { SDB_TYPE_INTEGER,  { .integer  = 4711 } },       0 },
		{ "0x10",    { SDB_TYPE_INTEGER,  { .integer  = 16 } },         0 },
		{ "010",     { SDB_TYPE_INTEGER,  { .integer  = 8 } },          0 },
		{ "abc",     { SDB_TYPE_INTEGER,  { .integer  = 0 } },         -1 },
		{ "1.2",     { SDB_TYPE_DECIMAL,  { .decimal  = 1.2 } },        0 },
		{ "0x1p+16", { SDB_TYPE_DECIMAL,  { .decimal  = 65536.0 } },    0 },
		{ "abc",     { SDB_TYPE_DECIMAL,  { .decimal  = 0.0 } },       -1 },
		{ "abc",     { SDB_TYPE_STRING,   { .string   = "abc" } },      0 },
		{ ".4",      { SDB_TYPE_DATETIME, { .datetime = 400000000 } },  0 },
		{ "abc",     { SDB_TYPE_DATETIME, { .datetime = 0 } },         -1 },
		{ "abc",     { SDB_TYPE_BINARY,
					 { .binary = { 3, (unsigned char *)"abc" } } }, 0 },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_data_t result;
		int type, check;

		memset(&result, 0, sizeof(result));
		type = golden_data[i].result.type;
		check = sdb_data_parse(golden_data[i].input, type, &result);
		fail_unless(check == golden_data[i].expected,
				"sdb_data_parse(%s, %d, <d>) = %d; expected: %d",
				golden_data[i].input, type, check, golden_data[i].expected);

		if (check)
			continue;

		fail_unless(sdb_data_cmp(&result, &golden_data[i].result) == 0,
				"sdb_data_parse(%s, %d, <d>) did not create expected result",
				golden_data[i].input, type);

		if (type == SDB_TYPE_STRING)
			fail_unless(golden_data[i].input == result.data.string,
					"sdb_data_parse(%s, %d, <d>) modified input string",
					golden_data[i].input, type);
		if (type == SDB_TYPE_BINARY)
			fail_unless(golden_data[i].input == (char *)result.data.binary.datum,
					"sdb_data_parse(%s, %d, <d>) modified input string",
					golden_data[i].input, type);
	}
}
END_TEST

Suite *
core_data_suite(void)
{
	Suite *s = suite_create("core::data");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_test(tc, test_data);
	tcase_add_test(tc, test_cmp);
	tcase_add_test(tc, test_expr_eval);
	tcase_add_test(tc, test_format);
	tcase_add_test(tc, test_parse);
	suite_add_tcase(s, tc);

	return s;
} /* core_data_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

