/*
 * SysDB - t/core/data_test.c
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

Suite *
core_data_suite(void)
{
	Suite *s = suite_create("core::data");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_test(tc, test_data);
	tcase_add_test(tc, test_format);
	suite_add_tcase(s, tc);

	return s;
} /* core_data_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

