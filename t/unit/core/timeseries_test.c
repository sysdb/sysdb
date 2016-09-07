/*
 * SysDB - t/unit/core/timeseries_test.c
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

#include "core/timeseries.h"
#include "testutils.h"

#include <check.h>

#define TS "1970-01-01 00:00:00 +0000"
#define V "0.000000"

START_TEST(timeseries_info)
{
	const char * const data_names[] = {"abc", "xyz"};
	sdb_timeseries_info_t *ts_info = sdb_timeseries_info_create(2, data_names);

	fail_unless(ts_info != NULL,
			"sdb_timeseries_info_create(2, {\"abc\", \"xyz\"}) = NULL; expected: <ts_info>");
	sdb_timeseries_info_destroy(ts_info);
}
END_TEST

START_TEST(timeseries)
{
	const char * const data_names[] = {"abc", "xyz"};
	sdb_timeseries_t *ts = sdb_timeseries_create(2, data_names, 2);
	sdb_strbuf_t *buf = sdb_strbuf_create(0);
	int test;

	const char *expected =
		"{\"start\": \""TS"\", \"end\": \""TS"\", \"data\": {"
			"\"abc\": [{\"timestamp\": \""TS"\", \"value\": \""V"\"},"
				"{\"timestamp\": \""TS"\", \"value\": \""V"\"}],"
			"\"xyz\": [{\"timestamp\": \""TS"\", \"value\": \""V"\"},"
				"{\"timestamp\": \""TS"\", \"value\": \""V"\"}]"
		"}}";

	fail_unless(ts != NULL,
			"sdb_timeseries_create(2, {\"abc\", \"xyz\"}, 2) = NULL; expected: <ts>");

	test = sdb_timeseries_tojson(ts, buf);
	fail_unless(test == 0,
			"sdb_timeseries_tojson(<ts>, <buf>) = %d; expected: 0", test);
	sdb_diff_strings("sdb_timeseries_tojson(<ts>, <buf>) returned unexpected JSON",
			sdb_strbuf_string(buf), expected);

	sdb_timeseries_destroy(ts);
	sdb_strbuf_destroy(buf);
}
END_TEST

TEST_MAIN("core::timeseries")
{
	TCase *tc = tcase_create("core");
	tcase_add_test(tc, timeseries_info);
	tcase_add_test(tc, timeseries);
	ADD_TCASE(tc);
}
TEST_MAIN_END

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

