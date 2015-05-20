/*
 * SysDB - src/core/time.c
 * Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>
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
#endif /* HAVE_CONFIG_H */

#include "sysdb.h"
#include "core/time.h"

#include <time.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*
 * public API
 */

/* 1 second (in nano-seconds) */
#define SEC 1000000000L

const sdb_time_t SDB_INTERVAL_YEAR   = 3652425L   * 24L * 60L * 60L * 100000L;
const sdb_time_t SDB_INTERVAL_MONTH  =  30436875L * 24L * 60L * 60L * 1000L;
const sdb_time_t SDB_INTERVAL_DAY    =              24L * 60L * 60L * SEC;
const sdb_time_t SDB_INTERVAL_HOUR   =                    60L * 60L * SEC;
const sdb_time_t SDB_INTERVAL_MINUTE =                          60L * SEC;
const sdb_time_t SDB_INTERVAL_SECOND =                                SEC;

sdb_time_t
sdb_gettime(void)
{
	struct timespec ts_now = { 0, 0 };

	if (clock_gettime(CLOCK_REALTIME, &ts_now))
		return 0;
	return TIMESPEC_TO_SDB_TIME(ts_now);
} /* sdb_gettime */

int
sdb_sleep(sdb_time_t reg, sdb_time_t *rem)
{
	struct timespec ts_reg, ts_rem = { 0, 0 };
	int status;

	ts_reg.tv_sec  = (time_t)SDB_TIME_TO_SECS(reg);
	ts_reg.tv_nsec = (long int)(reg % (sdb_time_t)1000000000);

	status = nanosleep(&ts_reg, &ts_rem);
	if (rem)
		*rem = TIMESPEC_TO_SDB_TIME(ts_rem);
	return status;
} /* sdb_sleep */

size_t
sdb_strftime(char *s, size_t len, sdb_time_t t)
{
	char tmp[len];
	time_t tstamp;
	struct tm tm;
	long tz;

	memset(&tm, 0, sizeof(tm));
	tstamp = (time_t)SDB_TIME_TO_SECS(t);
	if (! localtime_r (&tstamp, &tm))
		return 0;

	if (! strftime(tmp, len, "%F %T", &tm))
		return 0;
	tmp[sizeof(tmp) - 1] = '\0';

	tz = -timezone / 36;
	if (tm.tm_isdst > 0)
		tz += 100;

	t %= SDB_INTERVAL_SECOND;
	if (! t)
		return snprintf(s, len, "%s %+05ld", tmp, tz);
	return snprintf(s, len, "%s.%09ld %+05ld", tmp, t, tz);
} /* sdb_strftime */

size_t
sdb_strfinterval(char *s, size_t len, sdb_time_t interval)
{
	size_t n = 0;
	size_t i;

	/* special case the optional fractional part for seconds */
	bool have_seconds = 0;

	struct {
		sdb_time_t  interval;
		const char *suffix;
	} specs[] = {
		{ SDB_INTERVAL_YEAR,   "Y" },
		{ SDB_INTERVAL_MONTH,  "M" },
		{ SDB_INTERVAL_DAY,    "D" },
		{ SDB_INTERVAL_HOUR,   "h" },
		{ SDB_INTERVAL_MINUTE, "m" },
		{ SDB_INTERVAL_SECOND, "" },
	};

#define LEN (len > n ? len - n : 0)
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(specs); ++i) {
		if (interval >= specs[i].interval) {
			n += snprintf(s + n, LEN, "%"PRIsdbTIME"%s",
					interval / specs[i].interval, specs[i].suffix);
			interval %= specs[i].interval;
			if (specs[i].interval == SDB_INTERVAL_SECOND)
				have_seconds = 1;
		}
	}

	if (interval) {
		n += snprintf(s + n, LEN, ".%09"PRIsdbTIME, interval);
		have_seconds = 1;

		/* removing trailing zeroes */
		if (n <= len)
			while (s[n - 1] == '0')
				s[n--] = '\0';
	}

	if (! n) {
		n = snprintf(s, len, "0");
		have_seconds = 1;
	}

	if (have_seconds)
		n += snprintf(s + n, LEN, "s");
#undef LEN

	if (len)
		s[len - 1] = '\0';
	return n;
} /* sdb_strfinterval */

sdb_time_t
sdb_strpunit(const char *s)
{
	struct {
		const char *s;
		sdb_time_t unit;
	} units[] = {
		{ "Y", SDB_INTERVAL_YEAR },
		{ "M", SDB_INTERVAL_MONTH },
		{ "D", SDB_INTERVAL_DAY },
		{ "h", SDB_INTERVAL_HOUR },
		{ "m", SDB_INTERVAL_MINUTE },
		{ "s", SDB_INTERVAL_SECOND },
		{ "ms", SDB_INTERVAL_SECOND / 1000L },
		{ "us", SDB_INTERVAL_SECOND / 1000000L },
		{ "ns", 1 },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(units); ++i)
		if (! strcmp(s, units[i].s))
			return units[i].unit;
	return 0;
} /* sdb_strpunit */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

