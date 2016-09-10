/*
 * SysDB - t/unit/utils/strings_test.c
 * Copyright (C) 2016 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include "core/data.h"
#include "utils/strings.h"
#include "testutils.h"

#include <check.h>

/*
 * tests
 */

START_TEST(test_stringv)
{
	char **dst = NULL;
	size_t dst_len = 0;

	char *src[] = { "a", "b", "c" };
	size_t src_len = SDB_STATIC_ARRAY_LEN(src);
	size_t i;

	int check;

	/* Test no-op, empty operations. */
	check = stringv_copy(&dst, &dst_len, NULL, 0);
	fail_unless(check == 0,
			"stringv_copy(&<null>, &<0>, <null>, 0) = %d; expected: 0",
			check);
	fail_unless((dst == NULL) && (dst_len == 0),
			"stringv_copy(&<null>, &<0>, <null>, 0) produced %p, %d; "
			"expected <null>, 0", dst, dst_len);
	stringv_free(&dst, &dst_len);
	fail_unless((dst == NULL) && (dst_len == 0),
			"stringv_free(&<null>, &<0>) produced %p, %d; expected <null>, 0",
			dst, dst_len);

	/* Now, append some content. */
	for (i = 0; i < src_len; i++) {
		sdb_data_t d1 = {
			SDB_TYPE_STRING | SDB_TYPE_ARRAY,
			{ .array = { 0, NULL } },
		};
		sdb_data_t d2 = d1;

		char buf1[256], buf2[256];
		size_t j;

		check = stringv_append(&dst, &dst_len, src[i]);
		fail_unless(check == 0,
				"stringv_append(<s>, <len>, '%s') = %d; expected: 0",
				src[i], check);
		fail_unless((dst != NULL) && (dst_len == i + 1),
				"stringv_append(<s>, <len>, '%s') produced s=%p, len=%zu; "
				"expected: s=<v>, len=%zu", src[i], dst, dst_len, i + 1);

		for (j = 0; j <= i; j++)
			if ((! dst[j]) || (strcmp(dst[j], src[j]) != 0))
				break;

		if (j <= i) {
			d1.data.array.values = dst;
			d1.data.array.length = dst_len;
			sdb_data_format(&d1, buf1, sizeof(buf1), 0);

			d2.data.array.values = src;
			d2.data.array.length = dst_len;
			sdb_data_format(&d2, buf2, sizeof(buf2), 0);

			fail("stringv_append(<s>, <len>, '%s') produced unexpected result: "
					"vectors differ at position %zu: '%s' <-> '%s'",
					src[i], j, buf1, buf2);
		}
	}
	stringv_free(&dst, &dst_len);

	/* Copy all in one go. */
	for (i = 0; i < src_len; i++) {
		sdb_data_t d1 = {
			SDB_TYPE_STRING | SDB_TYPE_ARRAY,
			{ .array = { 0, NULL } },
		};
		sdb_data_t d2 = d1;

		char buf1[256], buf2[256];
		size_t j;

		/* stringv_copy is expected to free memory as needed, so simply copy
		 * over the old values from the previous iteration. */
		check = stringv_copy(&dst, &dst_len, (const char * const *)src, i + 1);
		fail_unless(check == 0,
				"stringv_copy(<s>, <len>, <src>, %zu) = %d; expected: 0",
				i, check);
		fail_unless((dst != NULL) && (dst_len == i + 1),
				"stringv_copy(<s>, <len>, <src>, %zu) produced s=%p, len=%zu; "
				"expected: s=<v>, len=%zu", i, dst, dst_len, i + 1);

		for (j = 0; j <= i; j++)
			if ((! dst[j]) || (strcmp(dst[j], src[j]) != 0))
				break;

		if (j <= i) {
			d1.data.array.values = dst;
			d1.data.array.length = dst_len;
			sdb_data_format(&d1, buf1, sizeof(buf1), 0);

			d2.data.array.values = src;
			d2.data.array.length = dst_len;
			sdb_data_format(&d2, buf2, sizeof(buf2), 0);

			fail("stringv_copy(<s>, <len>, <src>, %zu) produced unexpected result: "
					"vectors differ at position %zu: '%s' <-> '%s'",
					i, j, buf1, buf2);
		}
	}
	stringv_free(&dst, &dst_len);
}
END_TEST

TEST_MAIN("utils::strings")
{
	TCase *tc = tcase_create("core");
	tcase_add_test(tc, test_stringv);
	ADD_TCASE(tc);
}
TEST_MAIN_END

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

