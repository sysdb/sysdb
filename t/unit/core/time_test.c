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

#if HAVE_CONFIG_H
#	include "config.h"
#endif

#include "core/time.h"
#include "testutils.h"

#include <check.h>

#define YEAR  3652425L   * 24L * 3600L * 100000L
#define MONTH  30436875L * 24L * 3600L * 1000L
#define DAY                24L * 3600L * 1000000000L
#define HOUR                     3600L * 1000000000L
#define MINUTE                     60L * 1000000000L
#define SECOND                           1000000000L
#define MS                                  1000000L
#define US                                     1000L
#define NS                                        1L

struct {
	sdb_time_t t;
	const char *tz;
	const char *expected;
} strftime_data[] = {
	{ 0,                    "UTC",           "1970-01-01 00:00:00 +0000" },
	{ 1428066243000000000L, "Europe/Berlin", "2015-04-03 15:04:03 +0200" },
	{ 1420113661000000000L, "Europe/Berlin", "2015-01-01 13:01:01 +0100" },
	{ 1428066243000000000L, "US/Pacific",    "2015-04-03 06:04:03 -0700" },
	{ 1420113661000000000L, "US/Pacific",    "2015-01-01 04:01:01 -0800" },
	{ 1146747723000000123L, "UTC", "2006-05-04 13:02:03.000000123 +0000" },
	{ 1146747723123456789L, "UTC", "2006-05-04 13:02:03.123456789 +0000" },
};

START_TEST(test_strftime)
{
	char buf[1024], tz[64];
	size_t check;

	/* strftime does not provide the number of bytes that would have been
	 * written. Only check that it does not segfault. */
	sdb_strftime(NULL, 0, strftime_data[_i].t);

	snprintf(tz, sizeof(tz), "TZ=%s", strftime_data[_i].tz);
	putenv(tz);
	tzset();

	check = sdb_strftime(buf, sizeof(buf), strftime_data[_i].t);
	fail_unless(check > 0,
			"%s; sdb_strftime(<buf>, <size>, %"PRIsdbTIME") = %zu; "
			"expected: >0", tz, strftime_data[_i].t, check);
	fail_unless(!strcmp(buf, strftime_data[_i].expected),
			"%s; sdb_strftime(<buf>, <size>, %"PRIsdbTIME") did not "
			"format time correctly; got: '%s'; expected: '%s'",
			tz, strftime_data[_i].t, buf, strftime_data[_i].expected);
	fail_unless(check == strlen(strftime_data[_i].expected),
			"%s; sdb_strftime(<buf>, <size>, %"PRIsdbTIME") = %zu; "
			"expected: %zu", tz, strftime_data[_i].t, check,
			strlen(strftime_data[_i].expected));

	putenv("TZ=UTC");
	tzset();
}
END_TEST

struct {
	sdb_time_t  interval;
	const char *expected;
} strfinterval_data[] = {
	{ 0,                    "0s" },
	{ 4711,                 ".000004711s" },
	{ 1000123400,           "1.0001234s" },
	{ 47940228000000000L,   "1Y6M7D" },
	{ YEAR,                 "1Y" },
	{ MONTH,                "1M" },
	{ DAY,                  "1D" },
	{ HOUR,                 "1h" },
	{ MINUTE,               "1m" },
	{ SECOND,               "1s" },
	{ YEAR
	  + MONTH
	  + DAY
	  + HOUR
	  + MINUTE
	  + SECOND
	  + 1234,               "1Y1M1D1h1m1.000001234s" },
};

START_TEST(test_strfinterval)
{
	char buf[1024];
	size_t check;

	/* this should return the number of bytes which would have been written;
	 * in fact, it might return a bit more because it cannot detect trailing
	 * zeroes; even more importantly, it should not segfault ;-) */
	check = sdb_strfinterval(NULL, 0, strfinterval_data[_i].interval);
	fail_unless(check >= strlen(strfinterval_data[_i].expected),
			"sdb_strfinterval(NULL, 0, %"PRIsdbTIME") = %zu; expected: %zu",
			strfinterval_data[_i].interval, check,
			strlen(strfinterval_data[_i].expected));

	check = sdb_strfinterval(buf, sizeof(buf), strfinterval_data[_i].interval);
	fail_unless(check > 0,
			"sdb_strfinterval(<buf>, <size>, %"PRIsdbTIME") = %zu; "
			"expected: >0", strfinterval_data[_i].interval, check);
	fail_unless(!strcmp(buf, strfinterval_data[_i].expected),
			"sdb_strfinterval(<buf>, <size>, %"PRIsdbTIME") did not "
			"format interval correctly; got: '%s'; expected: '%s'",
			strfinterval_data[_i].interval, buf, strfinterval_data[_i].expected);
	fail_unless(check == strlen(strfinterval_data[_i].expected),
			"sdb_strfinterval(<buf>, <size>, %"PRIsdbTIME") = %zu; "
			"expected: %zu", strfinterval_data[_i].interval, check,
			strlen(strfinterval_data[_i].expected));
}
END_TEST

struct {
	const char *s;
	sdb_time_t expected;
} strpunit_data[] = {
	{ "Y",  YEAR },
	{ "M",  MONTH },
	{ "D",  DAY },
	{ "h",  HOUR },
	{ "m",  MINUTE },
	{ "s",  SECOND },
	{ "ms", MS },
	{ "us", US },
	{ "ns", NS },
	/* invalid units */
	{ "y",  0 },
	{ "d",  0 },
	{ "H",  0 },
	{ "S",  0 },
	{ "ps", 0 },
};

START_TEST(test_strpunit)
{
	sdb_time_t check = sdb_strpunit(strpunit_data[_i].s);

	fail_unless(check == strpunit_data[_i].expected,
			"sdb_strpunit(%s) = %"PRIsdbTIME"; expected: %"PRIsdbTIME,
			strpunit_data[_i].s, check, strpunit_data[_i].expected);
}
END_TEST

TEST_MAIN("core::time")
{
	TCase *tc = tcase_create("core");
	TC_ADD_LOOP_TEST(tc, strftime);
	TC_ADD_LOOP_TEST(tc, strfinterval);
	TC_ADD_LOOP_TEST(tc, strpunit);
	ADD_TCASE(tc);
}
TEST_MAIN_END

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

