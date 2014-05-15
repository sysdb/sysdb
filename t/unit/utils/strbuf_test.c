/*
 * SysDB - t/unit/utils/strbuf_test.c
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

START_TEST(test_null)
{
	sdb_strbuf_t *b = NULL;
	va_list ap;

	/* check that methods don't crash */
	sdb_strbuf_destroy(b);
	sdb_strbuf_skip(b, 0, 0);
	sdb_strbuf_skip(b, 0, 10);
	sdb_strbuf_skip(b, 10, 10);
	sdb_strbuf_clear(b);

	/* check that methods return an error */
	fail_unless(sdb_strbuf_vappend(b, "test", ap) < 0,
			"sdb_strbuf_vappend(NULL) didn't report failure");
	fail_unless(sdb_strbuf_append(b, "test") < 0,
			"sdb_strbuf_append(NULL) didn't report failure");
	fail_unless(sdb_strbuf_vsprintf(b, "test", ap) < 0,
			"sdb_strbuf_vsprintf(NULL) didn't report failure");
	fail_unless(sdb_strbuf_sprintf(b, "test") < 0,
			"sdb_strbuf_sprintf(NULL) didn't report failure");
	fail_unless(sdb_strbuf_memcpy(b, "test", 4) < 0,
			"sdb_strbuf_memcpy(NULL) didn't report failure");
	fail_unless(sdb_strbuf_memappend(b, "test", 4) < 0,
			"sdb_strbuf_memappend(NULL) didn't report failure");
	fail_unless(sdb_strbuf_read(b, 0, 32) < 0,
			"sdb_strbuf_read(NULL) didn't report failure");
	fail_unless(sdb_strbuf_chomp(b) < 0,
			"sdb_strbuf_chomp(NULL) didn't report failure");
}
END_TEST

START_TEST(test_empty)
{
	sdb_strbuf_t *b = sdb_strbuf_create(0);
	const char *data;
	size_t len;

	/* check that methods don't crash */
	sdb_strbuf_skip(b, 1, 1);
	sdb_strbuf_clear(b);
	sdb_strbuf_chomp(b);

	data = sdb_strbuf_string(b);
	fail_unless(data && (*data == '\0'),
			"sdb_strbuf_string(<empty>) = '%s'; expected: ''", data);
	len = sdb_strbuf_len(b);
	fail_unless(len == 0,
			"sdb_strbuf_len(<empty>) = %zu; expected: 0", len);

	sdb_strbuf_destroy(b);
}
END_TEST

START_TEST(test_create)
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

START_TEST(test_append)
{
	ssize_t n;
	size_t len, total = 0;
	const char *test;

	struct {
		const char *input;
		const char *result;
	} golden_data[] = {
		{ "1234567890", "1234567890" },
		{ "ABCDE",      "1234567890ABCDE" },
		{ "",           "1234567890ABCDE" },
		{ "-",          "1234567890ABCDE-" },
		/* when adding anything to this array, the last check has to be
		 * updated accordingly */
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		n = sdb_strbuf_append(buf, golden_data[i].input);
		fail_unless((size_t)n == strlen(golden_data[i].input),
				"sdb_strbuf_append() appended %zi bytes; expected: %zu",
				n, strlen(golden_data[i].input));
		total += n;
		len = sdb_strbuf_len(buf);
		fail_unless(len == total,
				"sdb_strbuf_append() left behind buffer with len = %zu; "
				"expected: %zu", len, total);

		test = sdb_strbuf_string(buf);
		fail_unless(test[len] == '\0',
				"sdb_strbuf_append() did not nil-terminate the string");

		test = sdb_strbuf_string(buf);
		fail_unless(!strcmp(test, golden_data[i].result),
				"sdb_strbuf_append() did not correctly concatenate "
				"the input; got: %s; expected: %s",
				test, golden_data[i].result);
	}

	n = sdb_strbuf_append(buf, "%zu; %5.4f", 42, 4.2);
	fail_unless(n == 10,
			"sdb_strbuf_append() appended %zi bytes; expected: 10", n);
	total += n;
	len = sdb_strbuf_len(buf);
	fail_unless(len == total,
			"sdb_strbuf_append() left behind buffer with len = %zu; "
			"expected: %zu", len, total);

	test = sdb_strbuf_string(buf);
	fail_unless(test[len] == '\0',
			"sdb_strbuf_append() did not nil-terminate the string");
	fail_unless(!strcmp(test, "1234567890ABCDE-42; 4.2000"),
			"sdb_strbuf_append() did not correctly concatenate the input; "
			"got: %s; expected: 1234567890ABCDE-42; 4.2000", test);
}
END_TEST

START_TEST(test_sprintf)
{
	ssize_t n;
	size_t len;
	const char *test;

	const char *golden_data[] = {
		"1234567890",
		"ABCDE",
		"",
		"-",
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		n = sdb_strbuf_sprintf(buf, golden_data[i]);
		fail_unless((size_t)n == strlen(golden_data[i]),
				"sdb_strbuf_sprintf() wrote %zi bytes; expected: %zu",
				n, strlen(golden_data[i]));
		len = sdb_strbuf_len(buf);
		fail_unless(len == (size_t)n,
				"sdb_strbuf_sprintf() left behind buffer with len = %zu; "
				"expected: %zi", len, n);

		test = sdb_strbuf_string(buf);
		fail_unless(test[len] == '\0',
				"sdb_strbuf_sprintf() did not nil-terminate the string");
		fail_unless(!strcmp(test, golden_data[i]),
				"sdb_strbuf_sprintf() did not format string correctly; "
				"got: %s; expected: %s", test, golden_data[i]);
	}

	n = sdb_strbuf_sprintf(buf, "%zu; %5.4f", 42, 4.2);
	fail_unless(n == 10,
			"sdb_strbuf_sprintf() wrote %zi bytes; expected: 10", n);
	len = sdb_strbuf_len(buf);
	fail_unless(len == 10,
			"sdb_strbuf_sprintf() left behind buffer with len = %zu; "
			"expected: 10", len);

	test = sdb_strbuf_string(buf);
	fail_unless(test[len] == '\0',
			"sdb_strbuf_sprintf() did not nil-terminate the string");
	fail_unless(!strcmp(test, "42; 4.2000"),
			"sdb_strbuf_sprintf() did not format string correctly; "
			"got: %s; expected: 42; 4.2000", test);
}
END_TEST

START_TEST(test_incremental)
{
	const char *data;

	ssize_t n;
	size_t i;

	sdb_strbuf_destroy(buf);
	buf = sdb_strbuf_create(1024);

	/* fill buffer one by one; leave room for nul-byte */
	for (i = 0; i < 1023; ++i) {
		n = sdb_strbuf_append(buf, ".");
		fail_unless(n == 1, "sdb_strbuf_append() = %zi; expected: 1", n);
	}

	/* write another byte; this has to trigger a resize */
	n = sdb_strbuf_append(buf, ".");
	fail_unless(n == 1, "sdb_strbuf_append() = %zi; expected: 1", n);

	/* write more bytes; this should trigger at least one more resize but
	 * that's an implementation detail */
	for (i = 0; i < 1024; ++i) {
		n = sdb_strbuf_append(buf, ".");
		fail_unless(n == 1, "sdb_strbuf_append() = %zi; expected: 1", n);
	}

	n = (ssize_t)sdb_strbuf_len(buf);
	fail_unless(n == 2048, "sdb_strbuf_len() = %zi; expectd: 2048", n);

	data = sdb_strbuf_string(buf);
	for (i = 0; i < 2048; ++i)
		fail_unless(data[i] == '.',
				"After sdb_strbuf_append(), found character %x "
				"at position %zi; expected %x (.)",
				(int)data[i], i, '.');
	fail_unless(data[i] == '\0',
			"After sdb_strbuf_append(), found character %x at end of string; "
			"expected '\\0'", (int)data[i]);
}
END_TEST

/* used by test_memcpy and test_memappend */
static struct {
	const char *input;
	size_t size;
} mem_golden_data[] = {
	{ "abc\0\x10\x42", 6 },
	{ "\0\1\2\3\4", 5 },
	{ "\n\n\0\n\n", 5 },
	{ "", 0 },
};

START_TEST(test_memcpy)
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

START_TEST(test_memappend)
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

START_TEST(test_chomp)
{
	struct {
		const char *input;
		ssize_t expected;
		const char *expected_string;
	} golden_data[] = {
		{ NULL, 0, "" },
		{ "\n", 1, "" },
		{ "\n\n", 2, "" },
		{ "12345\n\n\n", 3, "12345" },
		{ "abcd", 0, "abcd" },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		ssize_t n;
		const char *check;

		if (golden_data[i].input)
			sdb_strbuf_sprintf(buf, golden_data[i].input);

		/* empty buffer */
		n = sdb_strbuf_chomp(buf);
		fail_unless(n == golden_data[i].expected,
				"sdb_strbuf_chomp() = %zi; expected: %zi", n,
				golden_data[i].expected);

		check = sdb_strbuf_string(buf);
		fail_unless(!strcmp(check, golden_data[i].expected_string),
				"sdb_strbuf_chomp() did not correctly remove newlines; "
				"got string '%s'; expected: '%s'", check,
				golden_data[i].expected_string);
	}
}
END_TEST

START_TEST(test_skip)
{
	const char *input = "1234567890";
	struct {
		size_t offset;
		size_t n;
		const char *expected;
		size_t expected_len;
	} golden_data[] = {
		{ 0, 0, "1234567890", 10 },
		{ 0, 1, "234567890", 9 },
		{ 0, 2, "34567890", 8 },
		{ 0, 9, "0", 1 },
		{ 0, 10, "", 0 },
		{ 0, 11, "", 0 },
		{ 0, 100, "", 0 },
		{ 1, 0, "1234567890", 10 },
		{ 1, 1, "134567890", 9 },
		{ 1, 2, "14567890", 8 },
		{ 2, 0, "1234567890", 10 },
		{ 2, 1, "124567890", 9 },
		{ 2, 2, "12567890", 8 },
		{ 2, 3, "1267890", 7 },
		{ 2, 4, "127890", 6 },
		{ 2, 5, "12890", 5 },
		{ 2, 6, "1290", 4 },
		{ 2, 7, "120", 3 },
		{ 2, 8, "12", 2 },
		{ 2, 9, "12", 2 },
		{ 2, 10, "12", 2 },
		{ 8, 1, "123456780", 9 },
		{ 8, 2, "12345678", 8 },
		{ 8, 3, "12345678", 8 },
		{ 9, 1, "123456789", 9 },
		{ 9, 2, "123456789", 9 },
		{ 10, 1, "1234567890", 10 },
		{ 10, 2, "1234567890", 10 },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		const char *check;
		size_t n;

		sdb_strbuf_sprintf(buf, input);
		sdb_strbuf_skip(buf, golden_data[i].offset,
				golden_data[i].n);

		n = sdb_strbuf_len(buf);
		fail_unless(n == golden_data[i].expected_len,
				"sdb_strbuf_len() = %zu (after skip); expected: %zu",
				n, golden_data[i].expected_len);

		check = sdb_strbuf_string(buf);
		fail_unless(!!check,
				"sdb_strbuf_string() = NULL (after skip); expected: string");

		fail_unless(check[n] == '\0',
				"sdb_strbuf_skip() did not nil-terminate the string");

		fail_unless(! strcmp(golden_data[i].expected, check),
				"sdb_strbuf_skip('%s', %zu) did not skip correctly; "
				"got string '%s'; expected: '%s'", input,
				golden_data[i].n, check, golden_data[i].expected);
	}
}
END_TEST

START_TEST(test_clear)
{
	const char *data;
	size_t len;

	sdb_strbuf_append(buf, "abc");
	len = sdb_strbuf_len(buf);
	fail_unless(len != 0,
			"sdb_strbuf_len() = %zu; expected: != 0", len);

	sdb_strbuf_clear(buf);
	len = sdb_strbuf_len(buf);
	fail_unless(len == 0,
			"sdb_strbuf_len() = %zu (after clear); expected: 0", len);

	data = sdb_strbuf_string(buf);
	fail_unless(*data == '\0',
			"sdb_strbuf_string() = '%s' (after clear); expected: ''", data);
}
END_TEST

START_TEST(test_string)
{
	struct {
		const char *input;
		const char *expected;
	} golden_data[] = {
		{ NULL, "" },
		{ "a", "a" },
		{ "abcdef", "abcdef" },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		const char *check;

		if (golden_data[i].input)
			sdb_strbuf_sprintf(buf, golden_data[i].input);
		check = sdb_strbuf_string(buf);
		fail_unless(!strcmp(check, golden_data[i].expected),
				"sdb_strbuf_string() = '%s'; expected: '%s'",
				check, golden_data[i].expected);
	}
}
END_TEST

START_TEST(test_len)
{
	struct {
		const char *input;
		size_t expected;
	} golden_data[] = {
		{ NULL, 0 },
		{ "a", 1 },
		{ "12345", 5 },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		size_t check;

		if (golden_data[i].input)
			sdb_strbuf_sprintf(buf, golden_data[i].input);
		check = sdb_strbuf_len(buf);
		fail_unless(check == golden_data[i].expected,
				"sdb_strbuf_len() = %zu; expected: %zu",
				check, golden_data[i].expected);
	}
}
END_TEST

Suite *
util_strbuf_suite(void)
{
	Suite *s = suite_create("utils::strbuf");
	TCase *tc;

	tc = tcase_create("empty");
	tcase_add_test(tc, test_null);
	tcase_add_test(tc, test_empty);
	suite_add_tcase(s, tc);

	tc = tcase_create("core");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_create);
	tcase_add_test(tc, test_append);
	tcase_add_test(tc, test_sprintf);
	tcase_add_test(tc, test_incremental);
	tcase_add_test(tc, test_memcpy);
	tcase_add_test(tc, test_memappend);
	tcase_add_test(tc, test_chomp);
	tcase_add_test(tc, test_skip);
	tcase_add_test(tc, test_clear);
	tcase_add_test(tc, test_string);
	tcase_add_test(tc, test_len);
	suite_add_tcase(s, tc);

	return s;
} /* util_strbuf_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

