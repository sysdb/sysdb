/*
 * SysDB - t/unit/core/time_test.c
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

#include "core/time.h"
#include "libsysdb_test.h"

#include <check.h>

START_TEST(test_strfinterval)
{
	char buf[1024];
	size_t check;

	struct {
		sdb_time_t  interval;
		const char *expected;
	} golden_data[] = {
		{ 0,                    "0s" },
		{ 4711,                 ".000004711s" },
		{ 1000123400,           "1.0001234s" },
		{ 47940228000000000L,   "1Y6M7D" },
		{ SDB_INTERVAL_YEAR,    "1Y" },
		{ SDB_INTERVAL_MONTH,   "1M" },
		{ SDB_INTERVAL_DAY,     "1D" },
		{ SDB_INTERVAL_HOUR,    "1h" },
		{ SDB_INTERVAL_MINUTE,  "1m" },
		{ SDB_INTERVAL_SECOND,  "1s" },
		{ SDB_INTERVAL_YEAR
		  + SDB_INTERVAL_MONTH
		  + SDB_INTERVAL_DAY
		  + SDB_INTERVAL_HOUR
		  + SDB_INTERVAL_MINUTE
		  + SDB_INTERVAL_SECOND
		  + 1234,               "1Y1M1D1h1m1.000001234s" },
	};

	size_t i;

	/* this should return the number of bytes which would have been written;
	 * most notably, it should not segfault ;-) */
	check = sdb_strfinterval(NULL, 0, 4711); /* expected: .000004711s */
	fail_unless(check == 11,
			"sdb_strfinterval(NULL, 0, 4711) = %zu; expected: 11", check);

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		check = sdb_strfinterval(buf, sizeof(buf), golden_data[i].interval);
		fail_unless(check > 0,
				"sdb_strfinterval(<buf>, <size>, %"PRIsdbTIME") = %zu; "
				"expected: >0", golden_data[i].interval, check);
		fail_unless(!strcmp(buf, golden_data[i].expected),
				"sdb_strfinterval(<buf>, <size>, %"PRIsdbTIME") did not "
				"format interval correctly; got: '%s'; expected: '%s'",
				golden_data[i].interval, buf, golden_data[i].expected);
		fail_unless(check == strlen(golden_data[i].expected),
				"sdb_strfinterval(<buf>, <size>, %"PRIsdbTIME") = %zu; "
				"expected: %zu", golden_data[i].interval, check,
				strlen(golden_data[i].expected));
	}
}
END_TEST

START_TEST(test_strpunit)
{
	struct {
		const char *s;
		sdb_time_t expected;
	} golden_data[] = {
		{ "Y",  SDB_INTERVAL_YEAR },
		{ "M",  SDB_INTERVAL_MONTH },
		{ "D",  SDB_INTERVAL_DAY },
		{ "h",  SDB_INTERVAL_HOUR },
		{ "m",  SDB_INTERVAL_MINUTE },
		{ "s",  SDB_INTERVAL_SECOND },
		{ "ms", 1000000L },
		{ "us", 1000L },
		{ "ns", 1L },
		/* invalid units */
		{ "y",  0 },
		{ "d",  0 },
		{ "H",  0 },
		{ "S",  0 },
		{ "ps", 0 },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_time_t check = sdb_strpunit(golden_data[i].s);

		fail_unless(check == golden_data[i].expected,
				"sdb_strpunit(%s) = %"PRIsdbTIME"; expected: %"PRIsdbTIME,
				golden_data[i].s, check, golden_data[i].expected);
	}
}
END_TEST

Suite *
core_time_suite(void)
{
	Suite *s = suite_create("core::time");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_test(tc, test_strfinterval);
	tcase_add_test(tc, test_strpunit);
	suite_add_tcase(s, tc);

	return s;
} /* core_time_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

