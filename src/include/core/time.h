/*
 * SysDB - src/include/core/time.h
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

#ifndef SDB_CORE_TIME_H
#define SDB_CORE_TIME_H 1

#include <inttypes.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sdb_time_t:
 * The time, in nano-seconds, since the epoch.
 */
typedef uint64_t sdb_time_t;
#define PRIscTIME PRIu64

#define SECS_TO_SDB_TIME(s) ((sdb_time_t)(s) * (sdb_time_t)1000000000)
#define SDB_TIME_TO_SECS(t) ((t) / (sdb_time_t)1000000000)

#define NSECS_TO_SDB_TIME(ns) ((sdb_time_t)ns)

#define DOUBLE_TO_SDB_TIME(d) ((sdb_time_t)((d) * 1000000000.0))
#define SDB_TIME_TO_DOUBLE(t) ((double)(t) / 1000000000.0)

#define TIMESPEC_TO_SDB_TIME(ts) (SECS_TO_SDB_TIME((ts).tv_sec) \
		+ NSECS_TO_SDB_TIME((ts).tv_nsec))

/*
 * Interval constants:
 * Each constant specifies the time interval, in nano-seconds, of the named
 * time-frame. Year, month, and day are approximations which do not work well
 * for very large time intervals.
 */
extern const sdb_time_t SDB_INTERVAL_YEAR;
extern const sdb_time_t SDB_INTERVAL_MONTH;
extern const sdb_time_t SDB_INTERVAL_DAY;
extern const sdb_time_t SDB_INTERVAL_HOUR;
extern const sdb_time_t SDB_INTERVAL_MINUTE;
extern const sdb_time_t SDB_INTERVAL_SECOND;

sdb_time_t
sdb_gettime(void);

int
sdb_sleep(sdb_time_t reg, sdb_time_t *rem);

size_t
sdb_strftime(char *s, size_t len, const char *format, sdb_time_t);

size_t
sdb_strfinterval(char *s, size_t len, sdb_time_t interval);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_CORE_TIME_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

