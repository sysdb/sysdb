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

#include <assert.h>
#include <check.h>

static regex_t empty_re;

START_TEST(test_data)
{
	sdb_data_t d1, d2;
	int check;

	int64_t int_values[] = { 47, 11, 23 };
	char *string_values[] = { "foo", "bar", "qux" "baz" };
	size_t i;

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

	d1.type = 0;
	d2.type = SDB_TYPE_STRING;
	d2.data.string = NULL;
	check = sdb_data_copy(&d1, &d2);
	fail_unless(!check, "sdb_data_copy() = %i; expected: 0", check);
	fail_unless(d1.type == d2.type,
			"sdb_data_copy() didn't copy type; got: %i; expected: %i",
			d1.type, d2.type);
	fail_unless(d1.data.string == d2.data.string,
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
			d1.data.binary.datum, d2.data.binary.datum);

	sdb_data_free_datum(&d1);
	fail_unless(d1.data.binary.length == 0,
			"sdb_data_free_datum() didn't reset binary datum length");
	fail_unless(d1.data.binary.datum == NULL,
			"sdb_data_free_datum() didn't free binary datum");

	d1.type = 0;
	d2.type = SDB_TYPE_BINARY;
	d2.data.binary.datum = NULL;
	d2.data.binary.length = 0;
	check = sdb_data_copy(&d1, &d2);
	fail_unless(!check, "sdb_data_copy() = %i; expected: 0", check);
	fail_unless(d1.type == d2.type,
			"sdb_data_copy() didn't copy type; got: %i; expected: %i",
			d1.type, d2.type);
	fail_unless(d1.data.binary.length == d2.data.binary.length,
			"sdb_data_copy() didn't copy length; got: %d; expected: 5d",
			d1.data.binary.length, d2.data.binary.length);
	fail_unless(d1.data.binary.datum == d2.data.binary.datum,
			"sdb_data_copy() didn't copy binary data: got: %s; expected: %s",
			d1.data.binary.datum, d2.data.binary.datum);

	sdb_data_free_datum(&d1);
	fail_unless(d1.data.binary.length == 0,
			"sdb_data_free_datum() didn't reset binary datum length");
	fail_unless(d1.data.binary.datum == NULL,
			"sdb_data_free_datum() didn't free binary datum");

	check = sdb_data_parse(".", SDB_TYPE_REGEX, &d2);
	fail_unless(check == 0,
			"INTERNAL ERROR: Failed to parse regex '.'");
	assert(d2.type == SDB_TYPE_REGEX);
	check = sdb_data_copy(&d1, &d2);
	fail_unless(!check, "sdb_data_copy() = %i; expected: 0", check);
	fail_unless(d1.type == d2.type,
			"sdb_data_copy() didn't copy type; got: %i; expected: %i",
			d1.type, d2.type);
	fail_unless(d1.data.re.raw != d2.data.re.raw,
			"sdb_data_copy() copy string pointer");
	fail_unless(!strcmp(d1.data.re.raw, d2.data.re.raw),
			"sdb_data_copy() didn't copy raw regex: got: %s; expected: %s",
			d1.data.re.raw, d2.data.re.raw);
	sdb_data_free_datum(&d2);

	sdb_data_free_datum(&d1);
	fail_unless(d1.data.re.raw == NULL,
			"sdb_data_free_datum() didn't reset raw regex");

	d2.type = SDB_TYPE_REGEX;
	d2.data.re.raw = NULL;
	check = sdb_data_copy(&d1, &d2);
	fail_unless(!check, "sdb_data_copy() = %i; expected: 0", check);
	fail_unless(d1.type == d2.type,
			"sdb_data_copy() didn't copy type; got: %i; expected: %i",
			d1.type, d2.type);

	d2.type = SDB_TYPE_ARRAY | SDB_TYPE_INTEGER;
	d2.data.array.length = SDB_STATIC_ARRAY_LEN(int_values);
	d2.data.array.values = int_values;
	check = sdb_data_copy(&d1, &d2);
	fail_unless(!check, "sdb_data_copy() = %i; expected: 0", check);
	fail_unless(d1.type == d2.type,
			"sdb_data_copy() didn't copy type; got: %i; expected: %i",
			d1.type, d2.type);
	fail_unless(d1.data.array.values != d2.data.array.values,
			"sdb_data_copy() didn't copy values: got: %p; expected: %p",
			d1.data.array.values, d2.data.array.values);
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(int_values); ++i) {
		int *i1 = d1.data.array.values;
		int *i2 = d2.data.array.values;
		fail_unless(i1[i] == i2[i],
				"sdb_data_copy() modified integer value %d: "
				"got: %d; expected: %d", i, i1[i], i2[i]);
	}
	sdb_data_free_datum(&d1);

	d2.type = SDB_TYPE_ARRAY | SDB_TYPE_STRING;
	d2.data.array.length = SDB_STATIC_ARRAY_LEN(string_values);
	d2.data.array.values = string_values;
	check = sdb_data_copy(&d1, &d2);
	fail_unless(!check, "sdb_data_copy() = %i; expected: 0", check);
	fail_unless(d1.type == d2.type,
			"sdb_data_copy() didn't copy type; got: %i; expected: %i",
			d1.type, d2.type);
	fail_unless(d1.data.array.values != d2.data.array.values,
			"sdb_data_copy() didn't copy values: got: %p; expected: %p",
			d1.data.array.values, d2.data.array.values);
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(string_values); ++i) {
		char **s1 = d1.data.array.values;
		char **s2 = d2.data.array.values;
		fail_unless(s1[i] != s2[i],
				"sdb_data_copy() didn't copy string value %d", i);
		fail_unless(!strcmp(s1[i], s2[i]),
				"sdb_data_copy() modified string value %d: "
				"got: %s; expected: %s", i, s1[i], s2[i]);
	}
	sdb_data_free_datum(&d1);
}
END_TEST

START_TEST(test_cmp)
{
	regex_t dummy_re;
	int64_t int_values1[] = { 1, 2, 3 };
	int64_t int_values2[] = { 1, 3, 2 };
	double dec_values1[] = { 12.34, 47.11 };
	double dec_values2[] = { 47.11, 12.34 };
	char *string_values1[] = { "a", "b", "c" };
	char *string_values2[] = { "a", "c", "b" };
	sdb_time_t dt_values1[] = { 4711, 1234567890123456789L };
	sdb_time_t dt_values2[] = { 1234567890123456789L, 4711 };
	struct {
		size_t length;
		unsigned char *datum;
	} bin_values1[] = {
		{ 3, (unsigned char *)"\x1\x2\x3" },
		{ 4, (unsigned char *)"\x42\x0\xa\x1b" },
	};
	struct {
		size_t length;
		unsigned char *datum;
	} bin_values2[] = {
		{ 4, (unsigned char *)"\x42\x0\xa\x1b" },
		{ 3, (unsigned char *)"\x1\x2\x3" },
	};
	struct {
		char *raw;
		regex_t regex;
	} re_values1[] = {
		{ "dummy regex A", dummy_re },
	};
	struct {
		char *raw;
		regex_t regex;
	} re_values2[] = {
		{ "dummy regex B", dummy_re },
	};

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
		{
			{ SDB_TYPE_REGEX, { .re = { "a", empty_re } } },
			{ SDB_TYPE_REGEX, { .re = { "a", empty_re } } },
			0,
		},
		{
			{ SDB_TYPE_REGEX, { .re = { "a", empty_re } } },
			{ SDB_TYPE_REGEX, { .re = { "b", empty_re } } },
			-1,
		},
		{
			{ SDB_TYPE_REGEX, { .re = { "b", empty_re } } },
			{ SDB_TYPE_REGEX, { .re = { "a", empty_re } } },
			1,
		},
		{
			{ SDB_TYPE_ARRAY | SDB_TYPE_INTEGER, { .array = { 0, NULL } } },
			{ SDB_TYPE_ARRAY | SDB_TYPE_INTEGER, { .array = { 0, NULL } } },
			0,
		},
		{
			{ SDB_TYPE_ARRAY | SDB_TYPE_INTEGER, { .array = { 0, NULL } } },
			{
				SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values1), int_values1 } },
			},
			-1,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values1), int_values1 } },
			},
			{ SDB_TYPE_ARRAY | SDB_TYPE_INTEGER, { .array = { 0, NULL } } },
			1,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values1), int_values1 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values1), int_values1 } },
			},
			0,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values1), int_values1 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values2), int_values2 } },
			},
			-1,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values2), int_values2 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values1), int_values1 } },
			},
			1,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_DECIMAL,
				{ .array = { SDB_STATIC_ARRAY_LEN(dec_values1), dec_values1 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_DECIMAL,
				{ .array = { SDB_STATIC_ARRAY_LEN(dec_values1), dec_values1 } },
			},
			0,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_DECIMAL,
				{ .array = { SDB_STATIC_ARRAY_LEN(dec_values1), dec_values1 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_DECIMAL,
				{ .array = { SDB_STATIC_ARRAY_LEN(dec_values2), dec_values2 } },
			},
			-1,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_DECIMAL,
				{ .array = { SDB_STATIC_ARRAY_LEN(dec_values2), dec_values2 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_DECIMAL,
				{ .array = { SDB_STATIC_ARRAY_LEN(dec_values1), dec_values1 } },
			},
			1,
		},
		{
			{ SDB_TYPE_ARRAY | SDB_TYPE_STRING, { .array = { 0, NULL } } },
			{ SDB_TYPE_ARRAY | SDB_TYPE_STRING, { .array = { 0, NULL } } },
			0,
		},
		{
			{ SDB_TYPE_ARRAY | SDB_TYPE_STRING, { .array = { 0, NULL } } },
			{
				SDB_TYPE_ARRAY | SDB_TYPE_STRING,
				{ .array = { SDB_STATIC_ARRAY_LEN(string_values1), string_values1 } },
			},
			-1,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_STRING,
				{ .array = { SDB_STATIC_ARRAY_LEN(string_values1), string_values1 } },
			},
			{ SDB_TYPE_ARRAY | SDB_TYPE_STRING, { .array = { 0, NULL } } },
			1,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_STRING,
				{ .array = { SDB_STATIC_ARRAY_LEN(string_values1), string_values1 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_STRING,
				{ .array = { SDB_STATIC_ARRAY_LEN(string_values1), string_values1 } },
			},
			0,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_STRING,
				{ .array = { SDB_STATIC_ARRAY_LEN(string_values1), string_values1 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_STRING,
				{ .array = { SDB_STATIC_ARRAY_LEN(string_values2), string_values2 } },
			},
			-1,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_STRING,
				{ .array = { SDB_STATIC_ARRAY_LEN(string_values2), string_values2 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_STRING,
				{ .array = { SDB_STATIC_ARRAY_LEN(string_values1), string_values1 } },
			},
			1,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_DATETIME,
				{ .array = { SDB_STATIC_ARRAY_LEN(dt_values1), dt_values1 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_DATETIME,
				{ .array = { SDB_STATIC_ARRAY_LEN(dt_values1), dt_values1 } },
			},
			0,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_DATETIME,
				{ .array = { SDB_STATIC_ARRAY_LEN(dt_values1), dt_values1 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_DATETIME,
				{ .array = { SDB_STATIC_ARRAY_LEN(dt_values2), dt_values2 } },
			},
			-1,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_DATETIME,
				{ .array = { SDB_STATIC_ARRAY_LEN(dt_values2), dt_values2 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_DATETIME,
				{ .array = { SDB_STATIC_ARRAY_LEN(dt_values1), dt_values1 } },
			},
			1,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_BINARY,
				{ .array = { SDB_STATIC_ARRAY_LEN(bin_values1), bin_values1 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_BINARY,
				{ .array = { SDB_STATIC_ARRAY_LEN(bin_values1), bin_values1 } },
			},
			0,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_BINARY,
				{ .array = { SDB_STATIC_ARRAY_LEN(bin_values1), bin_values1 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_BINARY,
				{ .array = { SDB_STATIC_ARRAY_LEN(bin_values2), bin_values2 } },
			},
			-1,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_BINARY,
				{ .array = { SDB_STATIC_ARRAY_LEN(bin_values2), bin_values2 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_BINARY,
				{ .array = { SDB_STATIC_ARRAY_LEN(bin_values1), bin_values1 } },
			},
			1,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_REGEX,
				{ .array = { SDB_STATIC_ARRAY_LEN(re_values1), re_values1 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_REGEX,
				{ .array = { SDB_STATIC_ARRAY_LEN(re_values1), re_values1 } },
			},
			0,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_REGEX,
				{ .array = { SDB_STATIC_ARRAY_LEN(re_values1), re_values1 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_REGEX,
				{ .array = { SDB_STATIC_ARRAY_LEN(re_values2), re_values2 } },
			},
			-1,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_REGEX,
				{ .array = { SDB_STATIC_ARRAY_LEN(re_values2), re_values2 } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_REGEX,
				{ .array = { SDB_STATIC_ARRAY_LEN(re_values1), re_values1 } },
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

START_TEST(test_strcmp)
{
	struct {
		sdb_data_t d1;
		sdb_data_t d2;
		int expected;
	} golden_data[] = {
		/* same data as for the sdb_data_cmp test; in case the types match,
		 * both functions should behave the same (except for some loss in
		 * precision, e.g. when formatting datetime values) */
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
			{ SDB_TYPE_DATETIME, { .datetime = 471047114711471100 } },
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
			{ SDB_TYPE_DATETIME, { .datetime = 471047114711471100 } },
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
		{
			{ SDB_TYPE_REGEX, { .re = { "a", empty_re } } },
			{ SDB_TYPE_REGEX, { .re = { "a", empty_re } } },
			0,
		},
		{
			{ SDB_TYPE_REGEX, { .re = { "a", empty_re } } },
			{ SDB_TYPE_REGEX, { .re = { "b", empty_re } } },
			-1,
		},
		{
			{ SDB_TYPE_REGEX, { .re = { "b", empty_re } } },
			{ SDB_TYPE_REGEX, { .re = { "a", empty_re } } },
			1,
		},
		/* type mismatches */
		{
			{ SDB_TYPE_INTEGER, { .integer = 123 } },
			{ SDB_TYPE_STRING, { .string = "123" } },
			0,
		},
		{
			{ SDB_TYPE_INTEGER, { .integer = 120 } },
			{ SDB_TYPE_STRING, { .string = "123" } },
			-1,
		},
		{
			{ SDB_TYPE_STRING, { .string = "123" } },
			{ SDB_TYPE_INTEGER, { .integer = 120 } },
			1,
		},
		{
			{ SDB_TYPE_STRING, { .string = "12.3" } },
			{ SDB_TYPE_DECIMAL, { .decimal = 12.3 } },
			0,
		},
		{
			{ SDB_TYPE_STRING, { .string = "12.0" } },
			{ SDB_TYPE_DECIMAL, { .decimal = 12.3 } },
			-1,
		},
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 12.3 } },
			{ SDB_TYPE_STRING, { .string = "12.0" } },
			1,
		},
		{
			{ SDB_TYPE_REGEX, { .re = { "regex", empty_re } } },
			{ SDB_TYPE_STRING, { .string = "/regex/" } },
			0,
		},
		/* TODO: add support for arrays */
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		int check = sdb_data_strcmp(&golden_data[i].d1, &golden_data[i].d2);
		check = check < 0 ? -1 : check > 0 ? 1 : 0;
		if (check != golden_data[i].expected) {
			char d1_str[64] = "", d2_str[64] = "";
			sdb_data_format(&golden_data[i].d1, d1_str, sizeof(d1_str),
					SDB_DOUBLE_QUOTED);
			sdb_data_format(&golden_data[i].d2, d2_str, sizeof(d2_str),
					SDB_DOUBLE_QUOTED);
			fail("sdb_data_strcmp(%s, %s) = %d; expected: %d",
					d1_str, d2_str, check, golden_data[i].expected);
		}
	}
}
END_TEST

START_TEST(test_inarray)
{
	int64_t int_values[] = { 47, 11, 64 };
	int64_t int_values2[] = { 64, 11 };
	int64_t int_values3[] = { 47, 11, 42 };
	double dec_values[] = { 12.3, 47.11, 64.0 };
	double dec_values2[] = { 12.3, 47.11 };
	double dec_values3[] = { 2.3, 47.11 };
	char *string_values[] = { "foo", "bar", "qux", "baz" };
	char *string_values2[] = { "qux", "bar" };
	char *string_values3[] = { "foo", "bar", "qux", "baz", "bay" };

	sdb_data_t int_array = {
		SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
		{ .array = { SDB_STATIC_ARRAY_LEN(int_values), int_values } }
	};
	sdb_data_t int_array2 = {
		SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
		{ .array = { SDB_STATIC_ARRAY_LEN(int_values2), int_values2 } }
	};
	sdb_data_t int_array3 = {
		SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
		{ .array = { SDB_STATIC_ARRAY_LEN(int_values3), int_values3 } }
	};
	sdb_data_t dec_array = {
		SDB_TYPE_ARRAY | SDB_TYPE_DECIMAL,
		{ .array = { SDB_STATIC_ARRAY_LEN(dec_values), dec_values } }
	};
	sdb_data_t dec_array2 = {
		SDB_TYPE_ARRAY | SDB_TYPE_DECIMAL,
		{ .array = { SDB_STATIC_ARRAY_LEN(dec_values2), dec_values2 } }
	};
	sdb_data_t dec_array3 = {
		SDB_TYPE_ARRAY | SDB_TYPE_DECIMAL,
		{ .array = { SDB_STATIC_ARRAY_LEN(dec_values3), dec_values3 } }
	};
	sdb_data_t string_array = {
		SDB_TYPE_ARRAY | SDB_TYPE_STRING,
		{ .array = { SDB_STATIC_ARRAY_LEN(string_values), string_values } }
	};
	sdb_data_t string_array2 = {
		SDB_TYPE_ARRAY | SDB_TYPE_STRING,
		{ .array = { SDB_STATIC_ARRAY_LEN(string_values2), string_values2 } }
	};
	sdb_data_t string_array3 = {
		SDB_TYPE_ARRAY | SDB_TYPE_STRING,
		{ .array = { SDB_STATIC_ARRAY_LEN(string_values3), string_values3 } }
	};

	struct {
		sdb_data_t value;
		sdb_data_t array;
		_Bool expected;
	} golden_data[] = {
		{ { SDB_TYPE_INTEGER, { .integer = 47    } }, int_array,    1 },
		{ { SDB_TYPE_INTEGER, { .integer = 11    } }, int_array,    1 },
		{ { SDB_TYPE_INTEGER, { .integer = 64    } }, int_array,    1 },
		{ { SDB_TYPE_INTEGER, { .integer = 65    } }, int_array,    0 },
		{ { SDB_TYPE_NULL,    { .integer = 0     } }, int_array,    0 },
		{ { SDB_TYPE_DECIMAL, { .decimal = 12.3  } }, dec_array,    1 },
		{ { SDB_TYPE_DECIMAL, { .decimal = 47.11 } }, dec_array,    1 },
		{ { SDB_TYPE_DECIMAL, { .decimal = 64.0  } }, dec_array,    1 },
		{ { SDB_TYPE_DECIMAL, { .decimal = 60.0  } }, dec_array,    0 },
		{ { SDB_TYPE_INTEGER, { .integer = 64    } }, dec_array,    0 },
		{ { SDB_TYPE_NULL,    { .integer = 0     } }, dec_array,    0 },
		{ { SDB_TYPE_STRING,  { .string  = "Foo" } }, string_array, 1 },
		{ { SDB_TYPE_STRING,  { .string  = "FOO" } }, string_array, 1 },
		{ { SDB_TYPE_STRING,  { .string  = "foo" } }, string_array, 1 },
		{ { SDB_TYPE_STRING,  { .string  = "bar" } }, string_array, 1 },
		{ { SDB_TYPE_STRING,  { .string  = "qux" } }, string_array, 1 },
		{ { SDB_TYPE_STRING,  { .string  = "baz" } }, string_array, 1 },
		{ { SDB_TYPE_STRING,  { .string  = "ba"  } }, string_array, 0 },
		{ { SDB_TYPE_STRING,  { .string  = "abc" } }, string_array, 0 },
		{ { SDB_TYPE_NULL,    { .integer = 0     } }, string_array, 0 },
		{ int_array, { SDB_TYPE_INTEGER, { .integer = 47 } },       0 },
		{ int_array,     int_array,    1 },
		{ int_array2,    int_array,    1 },
		{ int_array3,    int_array,    0 },
		{ dec_array2,    int_array,    0 },
		{ string_array2, int_array,    0 },
		{ dec_array,     dec_array,    1 },
		{ dec_array2,    dec_array,    1 },
		{ dec_array3,    dec_array,    0 },
		{ int_array2,    dec_array,    0 },
		{ string_array2, dec_array,    0 },
		{ string_array,  string_array, 1 },
		{ string_array2, string_array, 1 },
		{ string_array3, string_array, 0 },
		{ int_array2,    string_array, 0 },
		{ dec_array2,    string_array, 0 },
		{
			{ SDB_TYPE_INTEGER | SDB_TYPE_ARRAY, { .array = { 0, NULL } } },
			int_array, 1,
		},
		{
			{ SDB_TYPE_INTEGER | SDB_TYPE_ARRAY, { .array = { 0, NULL } } },
			dec_array, 0,
		},
		{
			{ SDB_TYPE_DECIMAL | SDB_TYPE_ARRAY, { .array = { 0, NULL } } },
			dec_array, 1,
		},
		{
			{ SDB_TYPE_DECIMAL | SDB_TYPE_ARRAY, { .array = { 0, NULL } } },
			int_array, 0,
		},
		{
			{ SDB_TYPE_STRING | SDB_TYPE_ARRAY, { .array = { 0, NULL } } },
			string_array, 1,
		},
		{
			{ SDB_TYPE_STRING | SDB_TYPE_ARRAY, { .array = { 0, NULL } } },
			dec_array, 0,
		},
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		char v_str[1024] = "", a_str[1024] = "";
		_Bool check;

		sdb_data_format(&golden_data[i].value,
				v_str, sizeof(v_str), SDB_UNQUOTED);
		sdb_data_format(&golden_data[i].array,
				a_str, sizeof(a_str), SDB_UNQUOTED);

		check = sdb_data_inarray(&golden_data[i].value, &golden_data[i].array);
		fail_unless(check == golden_data[i].expected,
				"sdb_data_inarray(%s, %s) = %d; expected: %d",
				v_str, a_str, check, golden_data[i].expected);
	}
}
END_TEST

START_TEST(test_array_get)
{
	int64_t int_values[] = { 47, 11, 64 };
	double dec_values[] = { 12.3, 47.11, 64.0 };
	char *string_values[] = { "foo", "bar", "qux", "baz" };

	sdb_data_t int_array = {
		SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
		{ .array = { SDB_STATIC_ARRAY_LEN(int_values), int_values } }
	};
	sdb_data_t dec_array = {
		SDB_TYPE_ARRAY | SDB_TYPE_DECIMAL,
		{ .array = { SDB_STATIC_ARRAY_LEN(dec_values), dec_values } }
	};
	sdb_data_t string_array = {
		SDB_TYPE_ARRAY | SDB_TYPE_STRING,
		{ .array = { SDB_STATIC_ARRAY_LEN(string_values), string_values } }
	};

	struct {
		sdb_data_t array;
		size_t i;
		sdb_data_t expected;
	} golden_data[] = {
		{ int_array, 0, { SDB_TYPE_INTEGER, { .integer = 47 } } },
		{ int_array, 1, { SDB_TYPE_INTEGER, { .integer = 11 } } },
		{ int_array, 2, { SDB_TYPE_INTEGER, { .integer = 64 } } },
		{ int_array, 3, { -1, { .integer = 0 } } },
		{ dec_array, 0, { SDB_TYPE_DECIMAL, { .decimal = 12.3 } } },
		{ dec_array, 1, { SDB_TYPE_DECIMAL, { .decimal = 47.11 } } },
		{ dec_array, 2, { SDB_TYPE_DECIMAL, { .decimal = 64.0 } } },
		{ dec_array, 3, { -1, { .integer = 0 } } },
		{ string_array, 0, { SDB_TYPE_STRING, { .string = "foo" } } },
		{ string_array, 1, { SDB_TYPE_STRING, { .string = "bar" } } },
		{ string_array, 2, { SDB_TYPE_STRING, { .string = "qux" } } },
		{ string_array, 3, { SDB_TYPE_STRING, { .string = "baz" } } },
		{ string_array, 4, { -1, { .integer = 0 } } },
		{ { SDB_TYPE_INTEGER, { .integer = 666 } }, 0, { -1, { .integer = 0 } } },
		{ { SDB_TYPE_INTEGER, { .integer = 666 } }, 1, { -1, { .integer = 0 } } },
		{
			{ SDB_TYPE_ARRAY | SDB_TYPE_INTEGER, { .array = { 0, NULL } } },
			0, { -1, { .integer = 0 } },
		},
		{
			{ SDB_TYPE_ARRAY | SDB_TYPE_DECIMAL, { .array = { 0, NULL } } },
			0, { -1, { .integer = 0 } },
		},
		{
			{ SDB_TYPE_ARRAY | SDB_TYPE_STRING, { .array = { 0, NULL } } },
			0, { -1, { .integer = 0 } },
		},
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		char a_str[1024] = "", v_str[1024] = "", exp_str[1024] = "";
		sdb_data_t value = SDB_DATA_INIT;
		int check;

		sdb_data_format(&golden_data[i].array,
				a_str, sizeof(a_str), SDB_UNQUOTED);
		sdb_data_format(&golden_data[i].expected,
				exp_str, sizeof(exp_str), SDB_UNQUOTED);

		check = sdb_data_array_get(&golden_data[i].array,
				golden_data[i].i, &value);

		sdb_data_format(&value, v_str, sizeof(v_str), SDB_UNQUOTED);

		if (golden_data[i].expected.type < 0) {
			fail_unless(check < 0,
					"sdb_data_array_get(%s, %zu) = %d (%s); expected: <0",
					a_str, golden_data[i].i, check, v_str);
			continue;
		}

		fail_unless(check == 0,
				"sdb_data_array_get(%s, %zu) = %d; expected: 0",
				a_str, golden_data[i].i, check);
		fail_unless(! sdb_data_cmp(&value, &golden_data[i].expected),
				"sdb_data_array_get(%s, %zu) -> '%s'; expected: '%s'",
				a_str, golden_data[i].i, v_str, exp_str);
	}
}
END_TEST

START_TEST(test_parse_op)
{
	struct {
		const char *op;
		int id;
	} golden_data[] = {
		{ "+",  SDB_DATA_ADD },
		{ "-",  SDB_DATA_SUB },
		{ "*",  SDB_DATA_MUL },
		{ "/",  SDB_DATA_DIV },
		{ "%",  SDB_DATA_MOD },
		{ "||", SDB_DATA_CONCAT },
		{ "&&", -1 },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		const char *op;
		int id;

		id = sdb_data_parse_op(golden_data[i].op);
		fail_unless(id == golden_data[i].id,
				"sdb_data_parse_op(%s) = %d; expected: %d",
				golden_data[i].op, id, golden_data[i].id);

		if (id <= 0)
			continue;

		op = SDB_DATA_OP_TO_STRING(id);
		fail_unless(!strcmp(op, golden_data[i].op),
				"SDB_DATA_OP_TO_STRING(%d) = '%s'; expected: '%s'",
				id, op, golden_data[i].op);
	}
}
END_TEST

START_TEST(test_expr_eval)
{
	sdb_data_t err = { -1, { .integer = 0 } };

	int64_t int_values[] = { 47, 11, 23 };
	int64_t expected_int_append[] = { 47, 11, 23, 42 };
	int64_t expected_int_prepend[] = { 42, 47, 11, 23 };
	int64_t expected_int_concat[] = { 47, 11, 23, 47, 11, 23 };
	char *string_values[] = { "foo", "bar", "qux" "baz" };
	char *expected_string_append[] = { "foo", "bar", "qux" "baz", "bay" };
	char *expected_string_prepend[] = { "bay", "foo", "bar", "qux" "baz" };
	char *expected_string_concat[] =
		{ "foo", "bar", "qux" "baz", "foo", "bar", "qux" "baz" };

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
			err,
		},
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 35.0 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 17.5 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 52.5 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 17.5 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 612.5 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 2.0 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 0.0 } },
			err,
		},
		{
			{ SDB_TYPE_STRING, { .string = NULL } },
			{ SDB_TYPE_STRING, { .string = "" } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		{
			{ SDB_TYPE_STRING, { .string = NULL } },
			{ SDB_TYPE_STRING, { .string = NULL } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		{
			{ SDB_TYPE_STRING, { .string = "" } },
			{ SDB_TYPE_STRING, { .string = NULL } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		{
			{ SDB_TYPE_STRING, { .string = "a" } },
			{ SDB_TYPE_STRING, { .string = "b" } },
			err,
			err,
			err,
			err,
			err,
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
			err,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 0, NULL } } },
			{ SDB_TYPE_BINARY, { .binary = { 1, (unsigned char *)"a" } } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 0, NULL } } },
			{ SDB_TYPE_BINARY, { .binary = { 0, NULL } } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 1, (unsigned char *)"a" } } },
			{ SDB_TYPE_BINARY, { .binary = { 0, NULL } } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
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
			err,
			err,
			err,
			err,
			err,
			{
				SDB_TYPE_BINARY,
				{ .binary = { 6, (unsigned char *)"a\0ab\0b" } },
			},
		},
		{
			{ SDB_TYPE_REGEX, { .re = { ".", empty_re } } },
			{ SDB_TYPE_REGEX, { .re = { ".", empty_re } } },
			err,
			err,
			err,
			err,
			err,
			err,
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values), int_values } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values), int_values } },
			},
			err,
			err,
			err,
			err,
			err,
			{
				SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
				{ .array = {
						SDB_STATIC_ARRAY_LEN(expected_int_concat),
						expected_int_concat
				} },
			},
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values), int_values } },
			},
			{ SDB_TYPE_INTEGER, { .integer = 42 }, },
			err,
			err,
			err,
			err,
			err,
			{
				SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
				{ .array = {
						SDB_STATIC_ARRAY_LEN(expected_int_append),
						expected_int_append
				} },
			},
		},
		{
			{ SDB_TYPE_INTEGER, { .integer = 42 }, },
			{
				SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values), int_values } },
			},
			err,
			err,
			err,
			err,
			err,
			{
				SDB_TYPE_ARRAY | SDB_TYPE_INTEGER,
				{ .array = {
						SDB_STATIC_ARRAY_LEN(expected_int_prepend),
						expected_int_prepend
				} },
			},
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_STRING,
				{ .array = { SDB_STATIC_ARRAY_LEN(string_values), string_values } },
			},
			{
				SDB_TYPE_ARRAY | SDB_TYPE_STRING,
				{ .array = { SDB_STATIC_ARRAY_LEN(string_values), string_values } },
			},
			err,
			err,
			err,
			err,
			err,
			{
				SDB_TYPE_ARRAY | SDB_TYPE_STRING,
				{ .array = {
						SDB_STATIC_ARRAY_LEN(expected_string_concat),
						expected_string_concat
				} },
			},
		},
		{
			{
				SDB_TYPE_ARRAY | SDB_TYPE_STRING,
				{ .array = { SDB_STATIC_ARRAY_LEN(string_values), string_values } },
			},
			{ SDB_TYPE_STRING, { .string = "bay" } },
			err,
			err,
			err,
			err,
			err,
			{
				SDB_TYPE_ARRAY | SDB_TYPE_STRING,
				{ .array = {
						SDB_STATIC_ARRAY_LEN(expected_string_append),
						expected_string_append
				} },
			},
		},
		{
			{ SDB_TYPE_STRING, { .string = "bay" } },
			{
				SDB_TYPE_ARRAY | SDB_TYPE_STRING,
				{ .array = { SDB_STATIC_ARRAY_LEN(string_values), string_values } },
			},
			err,
			err,
			err,
			err,
			err,
			{
				SDB_TYPE_ARRAY | SDB_TYPE_STRING,
				{ .array = {
						SDB_STATIC_ARRAY_LEN(expected_string_prepend),
						expected_string_prepend
				} },
			},
		},
		{
			{ SDB_TYPE_INTEGER | SDB_TYPE_ARRAY, { .array = { 0, NULL } } },
			{
				SDB_TYPE_INTEGER | SDB_TYPE_ARRAY,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values), int_values } },
			},
			err,
			err,
			err,
			err,
			err,
			{
				SDB_TYPE_INTEGER | SDB_TYPE_ARRAY,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values), int_values } },
			},
		},
		{
			{
				SDB_TYPE_INTEGER | SDB_TYPE_ARRAY,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values), int_values } },
			},
			{ SDB_TYPE_INTEGER | SDB_TYPE_ARRAY, { .array = { 0, NULL } } },
			err,
			err,
			err,
			err,
			err,
			{
				SDB_TYPE_INTEGER | SDB_TYPE_ARRAY,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values), int_values } },
			},
		},
		{
			{ SDB_TYPE_STRING | SDB_TYPE_ARRAY, { .array = { 0, NULL } } },
			{
				SDB_TYPE_STRING | SDB_TYPE_ARRAY,
				{ .array = { SDB_STATIC_ARRAY_LEN(string_values), string_values } },
			},
			err,
			err,
			err,
			err,
			err,
			{
				SDB_TYPE_STRING | SDB_TYPE_ARRAY,
				{ .array = { SDB_STATIC_ARRAY_LEN(string_values), string_values } },
			},
		},
		{
			{
				SDB_TYPE_STRING | SDB_TYPE_ARRAY,
				{ .array = { SDB_STATIC_ARRAY_LEN(string_values), string_values } },
			},
			{ SDB_TYPE_STRING | SDB_TYPE_ARRAY, { .array = { 0, NULL } } },
			err,
			err,
			err,
			err,
			err,
			{
				SDB_TYPE_STRING | SDB_TYPE_ARRAY,
				{ .array = { SDB_STATIC_ARRAY_LEN(string_values), string_values } },
			},
		},
		{
			{ SDB_TYPE_NULL, { .integer = 0 } },
			{ SDB_TYPE_NULL, { .integer = 0 } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		{
			{ SDB_TYPE_NULL, { .integer = 0 } },
			{ SDB_TYPE_INTEGER, { .integer = 42 } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		{
			{ SDB_TYPE_INTEGER, { .integer = 42 } },
			{ SDB_TYPE_NULL, { .integer = 0 } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		{
			{ SDB_TYPE_NULL, { .integer = 0 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 47.11 } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 47.11 } },
			{ SDB_TYPE_NULL, { .integer = 0 } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		{
			{ SDB_TYPE_NULL, { .integer = 0 } },
			{ SDB_TYPE_STRING, { .string = "47.11" } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		{
			{ SDB_TYPE_STRING, { .string = "47.11" } },
			{ SDB_TYPE_NULL, { .integer = 0 } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		{
			{ SDB_TYPE_NULL, { .integer = 0 } },
			{ SDB_TYPE_DATETIME, { .datetime = 4711 } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		{
			{ SDB_TYPE_DATETIME, { .datetime = 4711 } },
			{ SDB_TYPE_NULL, { .integer = 0 } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		{
			{ SDB_TYPE_NULL, { .integer = 0 } },
			{ SDB_TYPE_BINARY, { .binary = { 1, (unsigned char *)"a" } } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 1, (unsigned char *)"a" } } },
			{ SDB_TYPE_NULL, { .integer = 0 } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		{
			{ SDB_TYPE_NULL, { .integer = 0 } },
			{ SDB_TYPE_REGEX, { .re = { ".", empty_re } } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		{
			{ SDB_TYPE_REGEX, { .re = { ".", empty_re } } },
			{ SDB_TYPE_NULL, { .integer = 0 } },
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
			SDB_DATA_NULL,
		},
		/* supported type-mismatches */
		{
			/* int * datetime */
			{ SDB_TYPE_INTEGER,  { .integer  = 20 } },
			{ SDB_TYPE_DATETIME, { .datetime = 2 } },
			err,
			err,
			{ SDB_TYPE_DATETIME, { .datetime = 40 } },
			err,
			err,
			err,
		},
		{
			/* datetime * int, datetime / int, datetime % int */
			{ SDB_TYPE_DATETIME, { .datetime = 20 } },
			{ SDB_TYPE_INTEGER,  { .integer  = 2 } },
			err,
			err,
			{ SDB_TYPE_DATETIME, { .datetime = 40 } },
			{ SDB_TYPE_DATETIME, { .datetime = 10 } },
			{ SDB_TYPE_DATETIME, { .datetime = 0 } },
			err,
		},
		{
			/* float * datetime */
			{ SDB_TYPE_DECIMAL,  { .decimal  = 20.0 } },
			{ SDB_TYPE_DATETIME, { .datetime = 2 } },
			err,
			err,
			{ SDB_TYPE_DATETIME, { .datetime = 40 } },
			err,
			err,
			err,
		},
		{
			/* datetime * float, datetime / float, datetime % float */
			{ SDB_TYPE_DATETIME, { .datetime = 20 } },
			{ SDB_TYPE_DECIMAL,  { .decimal  = 2.0 } },
			err,
			err,
			{ SDB_TYPE_DATETIME, { .datetime = 40 } },
			{ SDB_TYPE_DATETIME, { .datetime = 10 } },
			{ SDB_TYPE_DATETIME, { .datetime = 0 } },
			err,
		},
		/* unsupported type-mismatches */
		{
			{ SDB_TYPE_INTEGER, { .integer = 20 } },
			{ SDB_TYPE_DECIMAL, { .decimal = 20.0 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_INTEGER, { .integer = 20 } },
			{ SDB_TYPE_STRING, { .string = "20" } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_INTEGER, { .integer = 20 } },
			{ SDB_TYPE_BINARY, { .binary = { 2, (unsigned char *)"20" } } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_INTEGER, { .integer = 20 } },
			{ SDB_TYPE_BINARY, { .binary = { 3, (unsigned char *)"20" } } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_INTEGER, { .integer = 20 } },
			{ SDB_TYPE_REGEX, { .re = { ".", empty_re } } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_INTEGER, { .integer = 20 } },
			{ SDB_TYPE_REGEX + 1, { .integer = 0 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 20.0 } },
			{ SDB_TYPE_INTEGER, { .integer = 20 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 20.0 } },
			{ SDB_TYPE_STRING, { .string = "20.0" } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 20.0 } },
			{ SDB_TYPE_BINARY, { .binary = { 2, (unsigned char *)"20" } } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 20.0 } },
			{ SDB_TYPE_BINARY, { .binary = { 3, (unsigned char *)"20" } } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 20.0 } },
			{ SDB_TYPE_BINARY, { .binary = { 4, (unsigned char *)"20.0" } } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 20.0 } },
			{ SDB_TYPE_REGEX, { .re = { ".", empty_re } } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 20.0 } },
			{ SDB_TYPE_REGEX + 1, { .integer = 0 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_STRING, { .string = "20" } },
			{ SDB_TYPE_INTEGER, { .integer = 20 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_STRING, { .string = "20" } },
			{ SDB_TYPE_DECIMAL, { .decimal = 20.0 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_STRING, { .string = "20" } },
			{ SDB_TYPE_DATETIME, { .datetime = 20 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_STRING, { .string = "20" } },
			{ SDB_TYPE_BINARY, { .binary = { 2, (unsigned char *)"20" } } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_STRING, { .string = "20" } },
			{ SDB_TYPE_BINARY, { .binary = { 3, (unsigned char *)"20" } } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_STRING, { .string = "20" } },
			{ SDB_TYPE_REGEX, { .re = { ".", empty_re } } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_STRING, { .string = "20" } },
			{ SDB_TYPE_REGEX + 1, { .integer = 0 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_DATETIME, { .datetime = 20 } },
			{ SDB_TYPE_STRING, { .string = "20" } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_DATETIME, { .datetime = 20 } },
			{ SDB_TYPE_BINARY, { .binary = { 2, (unsigned char *)"20" } } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_DATETIME, { .datetime = 20 } },
			{ SDB_TYPE_BINARY, { .binary = { 3, (unsigned char *)"20" } } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_DATETIME, { .datetime = 20 } },
			{ SDB_TYPE_REGEX, { .re = { ".", empty_re } } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_DATETIME, { .datetime = 20 } },
			{ SDB_TYPE_REGEX + 1, { .integer = 0 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 2, (unsigned char *)"20" } } },
			{ SDB_TYPE_INTEGER, { .integer = 20 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 3, (unsigned char *)"20" } } },
			{ SDB_TYPE_INTEGER, { .integer = 20 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 2, (unsigned char *)"20" } } },
			{ SDB_TYPE_DECIMAL, { .decimal = 20.0 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 3, (unsigned char *)"20" } } },
			{ SDB_TYPE_DECIMAL, { .decimal = 20.0 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 2, (unsigned char *)"20" } } },
			{ SDB_TYPE_DATETIME, { .datetime = 20 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 3, (unsigned char *)"20" } } },
			{ SDB_TYPE_DATETIME, { .datetime = 20 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 2, (unsigned char *)"20" } } },
			{ SDB_TYPE_STRING, { .string = "20" } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 3, (unsigned char *)"20" } } },
			{ SDB_TYPE_STRING, { .string = "20" } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 2, (unsigned char *)"20" } } },
			{ SDB_TYPE_STRING, { .string = "20" } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 3, (unsigned char *)"20" } } },
			{ SDB_TYPE_STRING, { .string = "20" } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 2, (unsigned char *)"20" } } },
			{ SDB_TYPE_REGEX, { .re = { ".", empty_re } } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 3, (unsigned char *)"20" } } },
			{ SDB_TYPE_REGEX, { .re = { ".", empty_re } } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 2, (unsigned char *)"20" } } },
			{ SDB_TYPE_REGEX + 1, { .integer = 0 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_BINARY, { .binary = { 3, (unsigned char *)"20" } } },
			{ SDB_TYPE_REGEX + 1, { .integer = 0 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_REGEX, { .re = { ".", empty_re } } },
			{ SDB_TYPE_INTEGER, { .integer = 20 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_REGEX, { .re = { ".", empty_re } } },
			{ SDB_TYPE_DECIMAL, { .decimal = 20.0 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_REGEX, { .re = { ".", empty_re } } },
			{ SDB_TYPE_STRING, { .string = "20" } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_REGEX, { .re = { ".", empty_re } } },
			{ SDB_TYPE_DATETIME, { .datetime = 20 } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_REGEX, { .re = { ".", empty_re } } },
			{ SDB_TYPE_BINARY, { .binary = { 2, (unsigned char *)"20" } } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_REGEX, { .re = { ".", empty_re } } },
			{ SDB_TYPE_BINARY, { .binary = { 3, (unsigned char *)"20" } } },
			err, err, err, err, err, err,
		},
		{
			{ SDB_TYPE_REGEX, { .re = { ".", empty_re } } },
			{ SDB_TYPE_REGEX + 1, { .integer = 0 } },
			err, err, err, err, err, err,
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
			int type1, type2, type;

			char d1_str[64] = "", d2_str[64] = "";
			sdb_data_format(&golden_data[i].d1, d1_str, sizeof(d1_str),
					SDB_DOUBLE_QUOTED);
			sdb_data_format(&golden_data[i].d2, d2_str, sizeof(d2_str),
					SDB_DOUBLE_QUOTED);

			type1 = golden_data[i].d1.type;
			type2 = golden_data[i].d2.type;
			if (sdb_data_isnull(&golden_data[i].d1))
				type1 = SDB_TYPE_NULL;
			if (sdb_data_isnull(&golden_data[i].d2))
				type2 = SDB_TYPE_NULL;
			type = sdb_data_expr_type(tests[j].op, type1, type2);

			check = sdb_data_expr_eval(tests[j].op,
					&golden_data[i].d1, &golden_data[i].d2, &res);
			fail_unless((check == 0) == (tests[j].expected.type != -1),
					"sdb_data_expr_eval(%s, %s, %s) = %d; expected: %d",
					SDB_DATA_OP_TO_STRING(tests[j].op), d1_str, d2_str, check,
					tests[j].expected.type == -1 ? -1 : 0);

			fail_unless(tests[j].expected.type == type,
					"sdb_data_expr_eval(%s, %s, %s) expected to evaluate "
					"to type %d while sdb_data_expr_type(%d, %d, %d) "
					"predicted type %d", SDB_DATA_OP_TO_STRING(tests[j].op),
					d1_str, d2_str, tests[j].expected.type,
					tests[j].op, golden_data[i].d1.type,
					golden_data[i].d2.type, type);

			if (tests[j].expected.type == -1)
				continue;

			if (tests[j].expected.type == SDB_TYPE_NULL) {
				fail_unless(res.type == SDB_TYPE_NULL,
						"sdb_data_expr_eval(%s, %s, %s) evaluated to "
						"type %d; expected: SDB_TYPE_NULL",
						SDB_DATA_OP_TO_STRING(tests[j].op),
						d1_str, d2_str, res.type);
				continue;
			}

			check = sdb_data_cmp(&res, &tests[j].expected);
			if (check != 0) {
				char res_str[64] = "", expected_str[64] = "";
				sdb_data_format(&res, res_str, sizeof(res_str),
						SDB_DOUBLE_QUOTED);
				sdb_data_format(&tests[j].expected, expected_str,
						sizeof(expected_str), SDB_DOUBLE_QUOTED);
				fail("sdb_data_expr_eval(%s, %s, %s) evaluated to %s "
						"(type %d); expected: %s (type %d)",
						SDB_DATA_OP_TO_STRING(tests[j].op),
						d1_str, d2_str, res_str, res.type,
						expected_str, tests[j].expected.type);
			}

			sdb_data_free_datum(&res);
		}
	}
}
END_TEST

START_TEST(test_format)
{
	int64_t int_values[] = { 47, 11, 23 };
	char *string_values[] = { "foo", "bar", "qux", "baz" };

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
			"65536",
		},
		{
			{ SDB_TYPE_DECIMAL, { .decimal = 12.3 } },
			"12.3",
		},
		{
			{ SDB_TYPE_STRING, { .string = NULL } },
			"NULL",
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
			"NULL",
		},
		{
			{
				SDB_TYPE_BINARY,
				{ .binary = { 12, (unsigned char *)"binary\0crap\x42" } },
			},
			"\"\\x62\\x69\\x6e\\x61\\x72\\x79\\x0\\x63\\x72\\x61\\x70\\x42\"",
		},
		{
			{ SDB_TYPE_REGEX, { .re = { "some regex", empty_re } } },
			"\"/some regex/\"",
		},
		{
			{ SDB_TYPE_INTEGER | SDB_TYPE_ARRAY, { .array = { 0, NULL } } },
			"[]",
		},
		{
			{
				SDB_TYPE_INTEGER | SDB_TYPE_ARRAY,
				{ .array = { SDB_STATIC_ARRAY_LEN(int_values), int_values } },
			},
			"[47, 11, 23]",
		},
		{
			{
				SDB_TYPE_STRING | SDB_TYPE_ARRAY,
				{ .array = { SDB_STATIC_ARRAY_LEN(string_values), string_values } },
			},
			"[\"foo\", \"bar\", \"qux\", \"baz\"]",
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
		{ "4711",    { SDB_TYPE_INTEGER,  { .integer  = 4711 } },          0 },
		{ "0x10",    { SDB_TYPE_INTEGER,  { .integer  = 16 } },            0 },
		{ "010",     { SDB_TYPE_INTEGER,  { .integer  = 8 } },             0 },
		{ "abc",     { SDB_TYPE_INTEGER,  { .integer  = 0 } },            -1 },
		{ "1.2",     { SDB_TYPE_DECIMAL,  { .decimal  = 1.2 } },           0 },
		{ "0x1p+16", { SDB_TYPE_DECIMAL,  { .decimal  = 65536.0 } },       0 },
		{ "abc",     { SDB_TYPE_DECIMAL,  { .decimal  = 0.0 } },          -1 },
		{ "abc",     { SDB_TYPE_STRING,   { .string   = "abc" } },         0 },
		{ ".4",      { SDB_TYPE_DATETIME, { .datetime = 400000000 } },     0 },
		{ "abc",     { SDB_TYPE_DATETIME, { .datetime = 0 } },            -1 },
		{ "abc",     { SDB_TYPE_BINARY,
					 { .binary = { 3, (unsigned char *)"abc" } } }, 0 },
		{ "abc",     { SDB_TYPE_REGEX,    { .re = { "abc", empty_re } } }, 0 },
		{ "(|",      { SDB_TYPE_REGEX,    { .re = { "", empty_re } } },   -1 },
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
			fail_unless(golden_data[i].input != result.data.string,
					"sdb_data_parse(%s, %d, <d>) copied input string",
					golden_data[i].input, type);
		if (type == SDB_TYPE_BINARY)
			fail_unless(golden_data[i].input != (char *)result.data.binary.datum,
					"sdb_data_parse(%s, %d, <d>) copied input string",
					golden_data[i].input, type);
		if (type == SDB_TYPE_REGEX)
			fail_unless(golden_data[i].input != result.data.re.raw,
					"sdb_data_parse(%s, %d, <d>) copied input string",
					golden_data[i].input, type);
		sdb_data_free_datum(&result);
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
	tcase_add_test(tc, test_strcmp);
	tcase_add_test(tc, test_inarray);
	tcase_add_test(tc, test_array_get);
	tcase_add_test(tc, test_parse_op);
	tcase_add_test(tc, test_expr_eval);
	tcase_add_test(tc, test_format);
	tcase_add_test(tc, test_parse);
	suite_add_tcase(s, tc);

	return s;
} /* core_data_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

