/*
 * SysDB - t/utils/channel_test.c
 * Copyright (C) 2013 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include "utils/channel.h"
#include "libsysdb_test.h"

#include <check.h>
#include <limits.h>

#include <stdint.h>

static struct {
	int data;
	int expected_write;
	int expected_read;
} golden_data_int[] = {
	{ 5,       0, 0 },
	{ 15,      0, 0 },
	{ -3,      0, 0 },
	{ INT_MAX, 0, 0 },
	{ 27,      0, 0 },
	{ 42,      0, 0 },
	{ 6,       0, 0 },
	{ 2854,    0, 0 },
	{ 10562,   0, 0 },
	{ 0,       0, 0 },

	/* exceeding buffer size */
	{ 20, -1, -1 },
	{ 42, -1, -1 },
};

static struct {
	char *data;
	int   expected_write;
	int   expected_read;
} golden_data_string[] = {
	{ "c",      0, 0 },
	{ "",       0, 0 },
	{ "abc",    0, 0 },
	{ "foobar", 0, 0 },
	{ "qux",    0, 0 },
	{ "a b c",  0, 0 },
	{ "123",    0, 0 },
	{ "xyz",    0, 0 },
	{ "b",      0, 0 },
	{ "a",      0, 0 },

	/* exceeding buffer size */
	{ "err1", -1, -1 },
	{ "err2", -1, -1 },
};

static sdb_channel_t *chan;

static void
setup_int(void)
{
	chan = sdb_channel_create(10, sizeof(int));
	fail_unless(chan != NULL,
			"sdb_channel_create(10, sizeof(int)) = NULL; "
			"expected list object");
} /* setup_int */

static void
setup_string(void)
{
	chan = sdb_channel_create(10, sizeof(char *));
	fail_unless(chan != NULL,
			"sdb_chan_create(10, sizeof(char *))) = NULL; "
			"expected channel object");
} /* setup_string */

static void
teardown(void)
{
	sdb_channel_destroy(chan);
	chan = NULL;
} /* teardown */

START_TEST(test_create)
{
	chan = sdb_channel_create(0, 0);
	fail_unless(chan == NULL,
			"sdb_channel_create(0, 0) = %p; expected: NULL", chan);

	chan = sdb_channel_create(0, 1);
	fail_unless(chan != NULL,
			"sdb_channel_create(0, 1) = NULL; expected: channel object");
	sdb_channel_destroy(chan);

	chan = sdb_channel_create(42, 23);
	fail_unless(chan != NULL,
			"sdb_channel_create(32, 23) = NULL; expected: channel object");
	sdb_channel_destroy(chan);
}
END_TEST

START_TEST(test_write_read)
{
	uint32_t data;
	int check;

	chan = sdb_channel_create(0, 1);
	fail_unless(chan != NULL,
			"sdb_channel_create(0, 0) = NULL; expected: channel object");

	data = 0x00ffff00;
	check = sdb_channel_write(chan, &data);
	fail_unless(!check, "sdb_channel_write() = %i; expected: 0", check);
	check = sdb_channel_write(chan, &data);
	fail_unless(check, "sdb_channel_write() = 0; expected: <0");

	data = 0xffffffff;
	check = sdb_channel_read(chan, &data);
	/* result depends on endianess */
	fail_unless((data == 0xffffff00) || (data == 0x00ffffff),
			"sdb_channel_read() returned data %x; "
			"expected: 0xffffff00 || 0x00ffffff", data);

	sdb_channel_destroy(chan);
}
END_TEST

START_TEST(test_write_int)
{
	size_t i;
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data_int); ++i) {
		int data = golden_data_int[i].data;
		int expected = golden_data_int[i].expected_write;

		int check = sdb_channel_write(chan, &data);
		fail_unless(check == expected,
				"sdb_channel_write(chan, %i) = %i; expected: %i",
				data, check, expected);
	}
}
END_TEST

START_TEST(test_read_int)
{
	size_t i;

	/* populate */
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data_int); ++i) {
		int data = golden_data_int[i].data;
		int expected = golden_data_int[i].expected_write;

		int check = sdb_channel_write(chan, &data);
		fail_unless(check == expected,
				"sdb_channel_write(chan, %i) = %i; expected: %i",
				data, check, expected);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data_int); ++i) {
		int data = (int)i;
		int expected = golden_data_int[i].expected_read;

		int check = sdb_channel_read(chan, &data);
		fail_unless(check == expected,
				"sdb_channel_read(chan, %i) = %i; expected: %i",
				data, check, expected);
		if (check) {
			fail_unless(data == (int)i,
					"sdb_channel_read() modified data to '%i'; "
					"expected: no modification", data);
		}
		else {
			fail_unless(data == golden_data_int[i].data,
					"sdb_channel_read() returned data %i; expected: %i",
					data, golden_data_int[i].data);
		}
	}
}
END_TEST

START_TEST(test_write_read_int)
{
	size_t i;
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data_int); ++i) {
		int data = golden_data_int[i].data;
		int check = sdb_channel_write(chan, &data);
		fail_unless(check == 0,
				"sdb_channel_write(chan, %i) = %i; expected: 0",
				data, check);

		data = (int)i;
		check = sdb_channel_read(chan, &data);
		fail_unless(check == 0,
				"sdb_channel_read(chan, %i) = %i; expected: 0",
				data, check);
		if (check) {
			fail_unless(data == (int)i,
					"sdb_channel_read() modified data to '%i'; "
					"expected: no modification", data);
		}
		else {
			fail_unless(data == golden_data_int[i].data,
					"sdb_channel_read() returned data %i; expected: %i",
					data, golden_data_int[i].data);
		}
	}
}
END_TEST

START_TEST(test_write_string)
{
	size_t i;
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data_string); ++i) {
		char *data = golden_data_string[i].data;
		int expected = golden_data_string[i].expected_write;

		int check = sdb_channel_write(chan, &data);
		fail_unless(check == expected,
				"sdb_channel_write(chan, '%s') = %i; expected: %i",
				data, check, expected);
	}
}
END_TEST

START_TEST(test_read_string)
{
	size_t i;

	/* populate */
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data_string); ++i) {
		char *data = golden_data_string[i].data;
		int expected = golden_data_string[i].expected_write;

		int check = sdb_channel_write(chan, &data);
		fail_unless(check == expected,
				"sdb_channel_write(chan, '%s') = %i; expected: %i",
				data, check, expected);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data_string); ++i) {
		char *data = NULL;
		int expected = golden_data_string[i].expected_read;

		int check = sdb_channel_read(chan, &data);
		fail_unless(check == expected,
				"sdb_channel_read(chan, '') = %i; expected: %i",
				check, expected);
		if (check) {
			fail_unless(data == NULL,
					"sdb_channel_read() modified data to '%s'; "
					"expected: no modification", data);
		}
		else {
			fail_unless(data != NULL,
					"sdb_channel_read() did not return any data");
			fail_unless(!strcmp(data, golden_data_string[i].data),
					"sdb_channel_read() returned data '%s'; expected: '%s'",
					data, golden_data_string[i].data);
		}
	}
}
END_TEST

START_TEST(test_write_read_string)
{
	size_t i;
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data_string); ++i) {
		char *data = golden_data_string[i].data;
		int check = sdb_channel_write(chan, &data);
		fail_unless(check == 0,
				"sdb_channel_write(chan, '%s') = %i; expected: 0",
				data, check);

		data = NULL;
		check = sdb_channel_read(chan, &data);
		fail_unless(check == 0,
				"sdb_channel_read(chan, '') = %i; expected: 0", check);
		if (check) {
			fail_unless(data == NULL,
					"sdb_channel_read() modified data to '%s'; "
					"expected: no modifications", data);
		}
		else {
			fail_unless(data != NULL,
					"sdb_channel_read() did not return any data");
			fail_unless(!strcmp(data, golden_data_string[i].data),
					"sdb_channel_read() returned data '%s'; expected: '%s'",
					data, golden_data_string[i].data);
		}
	}
}
END_TEST

Suite *
util_channel_suite(void)
{
	Suite *s = suite_create("utils::channel");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_test(tc, test_create);
	tcase_add_test(tc, test_write_read);
	suite_add_tcase(s, tc);

	tc = tcase_create("integer");
	tcase_add_checked_fixture(tc, setup_int, teardown);
	tcase_add_test(tc, test_write_int);
	tcase_add_test(tc, test_read_int);
	tcase_add_test(tc, test_write_read_int);
	suite_add_tcase(s, tc);

	tc = tcase_create("string");
	tcase_add_checked_fixture(tc, setup_string, teardown);
	tcase_add_test(tc, test_write_string);
	tcase_add_test(tc, test_read_string);
	tcase_add_test(tc, test_write_read_string);
	suite_add_tcase(s, tc);

	return s;
} /* util_llist_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

