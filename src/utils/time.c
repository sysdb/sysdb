/*
 * SysDB - src/utils/time.c
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

#include "utils/time.h"
#include "utils/string.h"

#include <time.h>

#include <string.h>

/*
 * public API
 */

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
sdb_strftime(char *s, size_t len, const char *format, sdb_time_t t)
{
	time_t tstamp;
	struct tm tm;

	memset(&tm, 0, sizeof(tm));

	tstamp = (time_t)SDB_TIME_TO_SECS(t);
	if (! localtime_r (&tstamp, &tm))
		return 0;

	return strftime(s, len, format, &tm);
} /* sdb_strftime */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

