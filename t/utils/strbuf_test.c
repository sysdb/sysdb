/*
 * SysDB - t/utils/strbuf_test.c
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

#include "utils/strbuf.h"
#include "libsysdb_test.h"

#include <check.h>

/*
 * private data types
 */

static sdb_strbuf_t *buf;

static void
setup(void)
{
	buf = sdb_strbuf_create(0);
	fail_unless(buf != NULL,
			"sdb_strbuf_create() = NULL; expected strbuf object");
} /* setup */

static void
teardown(void)
{
	sdb_strbuf_destroy(buf);
	buf = NULL;
} /* teardown */

/*
 * tests
 */

START_TEST(test_sdb_strbuf_create)
{
	sdb_strbuf_t *s;
	size_t len;

	s = sdb_strbuf_create(0);
	fail_unless(s != NULL,
			"sdb_strbuf_create() = NULL; expected strbuf object");
	len = sdb_strbuf_len(s);
	fail_unless(len == 0,
			"sdb_strbuf_create() created buffer with len = %zu; "
			"expected: 0", len);
	sdb_strbuf_destroy(s);

	s = sdb_strbuf_create(128);
	fail_unless(s != NULL,
			"sdb_strbuf_create() = NULL; expected strbuf object");
	len = sdb_strbuf_len(s);
	/* len still has to be 0 -- there's no content */
	fail_unless(len == 0,
			"sdb_strbuf_create() created buffer with len = %zu; "
			"expected: 0", len);
	sdb_strbuf_destroy(s);
}
END_TEST

START_TEST(test_sdb_strbuf_append)
{
	ssize_t n, expected;
	size_t len;
	const char *test;

	n = sdb_strbuf_append(buf, "1234567890");
	fail_unless(n == 10,
			"sdb_strbuf_append() appended %zi bytes; expected: 10", n);
	len = sdb_strbuf_len(buf);
	fail_unless(len == 10,
			"sdb_strbuf_append() left behind buffer with len = %zu; "
			"expected: 10", len);

	n = sdb_strbuf_append(buf, "ABCDE");
	fail_unless(n == 5,
			"sdb_strbuf_append() appended %zi bytes; expected: 5", n);
	len = sdb_strbuf_len(buf);
	fail_unless(len == 15,
			"sdb_strbuf_append() left behind buffer with len = %zu; "
			"expected: 15", len);

	test = sdb_strbuf_string(buf);
	fail_unless(test[len] == '\0',
			"sdb_strbuf_append() did not nil-terminate the string");
	fail_unless(!strcmp(test, "1234567890ABCDE"),
			"sdb_strbuf_append() did not correctly concatenate two string; "
			"got: %s; expected: 1234567890ABCDE", test);

	n = sdb_strbuf_append(buf, "%zu; %5.4f", len, (double)len / 10.0);
	expected = /* len */ 2 + /* "; " */ 2 + /* decimal len/10 */ 6;
	fail_unless(n == expected,
			"sdb_strbuf_append() appended %zi bytes; expected: %zi",
			n, expected);
	len = sdb_strbuf_len(buf);
	fail_unless(len == 15 + (size_t)expected,
			"sdb_strbuf_append() left behind buffer with len = %zu; "
			"expected: %zu", len, 15 + (size_t)expected);

	test = sdb_strbuf_string(buf);
	fail_unless(test[len] == '\0',
			"sdb_strbuf_append() did not nil-terminate the string");
	fail_unless(!strcmp(test, "1234567890ABCDE15; 1.5000"),
			"sdb_strbuf_append() did not correctly concatenate two string; "
			"got: %s; expected: 1234567890ABCDE15; 1.5000", test);
}
END_TEST

START_TEST(test_sdb_strbuf_sprintf)
{
	ssize_t n, expected;
	size_t len;
	const char *test;

	n = sdb_strbuf_sprintf(buf, "1234567890");
	fail_unless(n == 10,
			"sdb_strbuf_sprintf() wrote %zi bytes; expected: 10", n);
	len = sdb_strbuf_len(buf);
	fail_unless(len == 10,
			"sdb_strbuf_sprintf() left behind buffer with len = %zu; "
			"expected: 10", len);

	n = sdb_strbuf_sprintf(buf, "ABCDE");
	fail_unless(n == 5,
			"sdb_strbuf_sprintf() wrote %zi bytes; expected: 5", n);
	len = sdb_strbuf_len(buf);
	fail_unless(len == 5,
			"sdb_strbuf_sprintf() left behind buffer with len = %zu; "
			"expected: 5", len);

	test = sdb_strbuf_string(buf);
	fail_unless(test[len] == '\0',
			"sdb_strbuf_sprintf() did not nil-terminate the string");
	fail_unless(!strcmp(test, "ABCDE"),
			"sdb_strbuf_sprintf() did not format string correctly; "
			"got: %s; expected: ABCDE", test);

	n = sdb_strbuf_sprintf(buf, "%zu; %5.4f", len, (double)len / 10.0);
	expected = /* len */ 1 + /* "; " */ 2 + /* decimal len/10 */ 6;
	fail_unless(n == expected,
			"sdb_strbuf_sprintf() wrote %zi bytes; expected: %zi",
			n, expected);
	len = sdb_strbuf_len(buf);
	fail_unless(len == (size_t)expected,
			"sdb_strbuf_sprintf() left behind buffer with len = %zu; "
			"expected: %zu", len, (size_t)expected);

	test = sdb_strbuf_string(buf);
	fail_unless(test[len] == '\0',
			"sdb_strbuf_sprintf() did not nil-terminate the string");
	fail_unless(!strcmp(test, "5; 0.5000"),
			"sdb_strbuf_sprintf() did not format string correctly; "
			"got: %s; expected: 5; 0.5000", test);
}
END_TEST

static struct {
	const char *input;
	size_t size;
} mem_golden_data[] = {
	{ "abc\0\x10\x42", 6 },
	{ "\0\1\2\3\4", 5 },
	{ "\n\n\0\n\n", 5 },
	{ "", 0 },
};

START_TEST(test_sdb_strbuf_memcpy)
{
	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(mem_golden_data); ++i) {
		ssize_t n;
		const char *check;

		n = sdb_strbuf_memcpy(buf, mem_golden_data[i].input,
				mem_golden_data[i].size);
		fail_unless(n >= 0,
				"sdb_strbuf_memcpy() = %zi; expected: >=0", n);
		fail_unless((size_t)n == mem_golden_data[i].size,
				"sdb_strbuf_memcpy() = %zi; expected: %zu",
				n, mem_golden_data[i].size);

		n = (ssize_t)sdb_strbuf_len(buf);
		fail_unless((size_t)n == mem_golden_data[i].size,
				"sdb_strbuf_len() = %zu (after memcpy); expected: %zu",
				n, mem_golden_data[i].size);

		check = sdb_strbuf_string(buf);
		fail_unless(check != NULL,
				"sdb_strbuf_string() = NULL (after memcpy); expected: data");
		fail_unless(check[mem_golden_data[i].size] == '\0',
				"sdb_strbuf_memcpy() did not nil-terminate the data");
		fail_unless(!memcmp(check, mem_golden_data[i].input,
					mem_golden_data[i].size),
				"sdb_strbuf_memcpy() did not set the buffer correctly");
	}
}
END_TEST

START_TEST(test_sdb_strbuf_memappend)
{
	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(mem_golden_data); ++i) {
		ssize_t n;
		const char *check;

		size_t total, j;

		n = sdb_strbuf_memappend(buf, mem_golden_data[i].input,
				mem_golden_data[i].size);
		fail_unless(n >= 0,
				"sdb_strbuf_memappend() = %zi; expected: >=0", n);
		fail_unless((size_t)n == mem_golden_data[i].size,
				"sdb_strbuf_memappend() = %zi; expected: %zu",
				n, mem_golden_data[i].size);

		check = sdb_strbuf_string(buf);
		fail_unless(check != NULL,
				"sdb_strbuf_string() = NULL (after memappend); "
				"expected: data");

		n = (ssize_t)sdb_strbuf_len(buf);
		total = 0;
		for (j = 0; j <= i; ++j) {
			fail_unless(total + mem_golden_data[j].size <= (size_t)n,
					"sdb_strbuf_len() = %zu (after memappend); "
					"expected: >=%zu", n, total + mem_golden_data[j].size);

			fail_unless(!memcmp(check + total, mem_golden_data[j].input,
						mem_golden_data[j].size),
					"sdb_strbuf_memappend() did not "
					"set the buffer correctly");
			total += mem_golden_data[j].size;
		}
		fail_unless((size_t)n == total,
				"sdb_strbuf_len() = %zu (after memappend); expected: %zu",
				n, total);

		fail_unless(check[total] == '\0',
				"sdb_strbuf_memappend() did not nil-terminate the data");
	}
}
END_TEST

static struct {
	const char *input;
	ssize_t expected;
	const char *expected_string;
} chomp_golden_data[] = {
	{ NULL, 0, "" },
	{ "\n", 1, "" },
	{ "\n\n", 2, "" },
	{ "12345\n\n\n", 3, "12345" },
	{ "abcd", 0, "abcd" },
};

START_TEST(test_sdb_strbuf_chomp)
{
	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(chomp_golden_data); ++i) {
		ssize_t n;
		const char *check;

		if (chomp_golden_data[i].input)
			sdb_strbuf_sprintf(buf, chomp_golden_data[i].input);

		/* empty buffer */
		n = sdb_strbuf_chomp(buf);
		fail_unless(n == chomp_golden_data[i].expected,
				"sdb_strbuf_chomp() = %zi; expected: %zi", n,
				chomp_golden_data[i].expected);

		check = sdb_strbuf_string(buf);
		fail_unless(!strcmp(check, chomp_golden_data[i].expected_string),
				"sdb_strbuf_chomp() did not correctly remove newlines; "
				"got string '%s'; expected: '%s'", check,
				chomp_golden_data[i].expected_string);
	}
}
END_TEST

/* input is "1234567890" */
static struct {
	size_t n;
	const char *expected;
} skip_golden_data[] = {
	{ 0, "1234567890" },
	{ 1, "234567890" },
	{ 2, "34567890" },
	{ 9, "0" },
	{ 10, "" },
	{ 11, "" },
	{ 100, "" },
};

START_TEST(test_sdb_strbuf_skip)
{
	const char *input = "1234567890";
	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(skip_golden_data); ++i) {
		const char *check;

		sdb_strbuf_sprintf(buf, input);
		sdb_strbuf_skip(buf, skip_golden_data[i].n);

		check = sdb_strbuf_string(buf);
		fail_unless(!!check,
				"sdb_strbuf_string() = NULL (after skip); expected: string");

		fail_unless(! strcmp(skip_golden_data[i].expected, check),
				"sdb_strbuf_skip('%s', %zu) did not skip correctly; "
				"got string '%s'; expected: '%s'", input,
				skip_golden_data[i].n, check, skip_golden_data[i].expected);
	}
}
END_TEST

static struct {
	const char *input;
	const char *expected;
} string_golden_data[] = {
	{ NULL, "" },
	{ "a", "a" },
	{ "abcdef", "abcdef" },
};

START_TEST(test_sdb_strbuf_string)
{
	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(string_golden_data); ++i) {
		const char *check;

		if (string_golden_data[i].input)
			sdb_strbuf_sprintf(buf, string_golden_data[i].input);
		check = sdb_strbuf_string(buf);
		fail_unless(!strcmp(check, string_golden_data[i].expected),
				"sdb_strbuf_string() = '%s'; expected: '%s'",
				check, string_golden_data[i].expected);
	}
}
END_TEST

static struct {
	const char *input;
	size_t expected;
} len_golden_data[] = {
	{ NULL, 0 },
	{ "a", 1 },
	{ "12345", 5 },
};

START_TEST(test_sdb_strbuf_len)
{
	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(len_golden_data); ++i) {
		size_t check;

		if (len_golden_data[i].input)
			sdb_strbuf_sprintf(buf, len_golden_data[i].input);
		check = sdb_strbuf_len(buf);
		fail_unless(check == len_golden_data[i].expected,
				"sdb_strbuf_len() = %zu; expected: %zu",
				check, len_golden_data[i].expected);
	}
}
END_TEST

Suite *
util_strbuf_suite(void)
{
	Suite *s = suite_create("utils::strbuf");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_sdb_strbuf_create);
	tcase_add_test(tc, test_sdb_strbuf_append);
	tcase_add_test(tc, test_sdb_strbuf_sprintf);
	tcase_add_test(tc, test_sdb_strbuf_memcpy);
	tcase_add_test(tc, test_sdb_strbuf_memappend);
	tcase_add_test(tc, test_sdb_strbuf_chomp);
	tcase_add_test(tc, test_sdb_strbuf_skip);
	tcase_add_test(tc, test_sdb_strbuf_string);
	tcase_add_test(tc, test_sdb_strbuf_len);
	suite_add_tcase(s, tc);

	return s;
} /* util_strbuf_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

