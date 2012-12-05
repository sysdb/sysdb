/*
 * syscollector - src/include/utils/time.h
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

#ifndef SC_UTILS_TIME_H
#define SC_UTILS_TIME_H 1

#include <inttypes.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sc_time_t:
 * The time, in nano-seconds, since the epoch.
 */
typedef uint64_t sc_time_t;
#define PRIscTIME PRIu64

#define SECS_TO_SC_TIME(s) ((sc_time_t)(s) * (sc_time_t)1000000000)
#define SC_TIME_TO_SECS(t) ((t) / (sc_time_t)1000000000)

#define NSECS_TO_SC_TIME(ns) ((sc_time_t)ns)

#define DOUBLE_TO_SC_TIME(d) ((sc_time_t)((d) * 1000000000.0))
#define SC_TIME_TO_DOUBLE(t) ((double)(t) / 1000000000.0)

#define TIMESPEC_TO_SC_TIME(ts) (SECS_TO_SC_TIME((ts).tv_sec) \
		+ NSECS_TO_SC_TIME((ts).tv_nsec))

sc_time_t
sc_gettime(void);

int
sc_sleep(sc_time_t reg, sc_time_t *rem);

size_t
sc_strftime(char *s, size_t len, const char *format, sc_time_t);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SC_UTILS_TIME_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

