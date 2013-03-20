/*
 * SysDB - src/include/utils/error.h
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

/*
 * SysDB error handling:
 * Error handling in SysDB is done on a by-thread basis, that is, each thread
 * will use its own memory region to store information about the last reported
 * error.
 * Once the error message has been passed to SysDB, it will log the entire
 * message at once. XXX: currently, SysDB only supports printing the error to
 * the standard error channel; support for other logging backends will be
 * added later in a modular fashion.
 */

#ifndef SDB_UTILS_ERROR_H
#define SDB_UTILS_ERROR_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* max length of any error message */
#ifndef SDB_MAX_ERROR
#	define SDB_MAX_ERROR 4096
#endif /* ! SDB_MAX_ERROR */

/* On Linux systems and possibly others, this should be the same as the LOG_
 * constants defined by syslog. */
enum {
	SDB_LOG_EMERG   = 0,
	SDB_LOG_ERR     = 3,
	SDB_LOG_WARNING = 4,
	SDB_LOG_NOTICE  = 5,
	SDB_LOG_INFO    = 6,
	SDB_LOG_DEBUG   = 7,
};
#define SDB_LOG_PRIO_TO_STRING(prio) \
	(((prio) == SDB_LOG_EMERG) ? "EMERG" \
		: ((prio) == SDB_LOG_ERR) ? "ERROR" \
		: ((prio) == SDB_LOG_WARNING) ? "WARNING" \
		: ((prio) == SDB_LOG_NOTICE) ? "NOTICE" \
		: ((prio) == SDB_LOG_INFO) ? "INFO" \
		: ((prio) == SDB_LOG_DEBUG) ? "DEBUG" : "UNKNOWN")

/*
 * sdb_log:
 * Log the specified message. The string will be formatted in printf-style
 * using the specified format and arguments and logged with the specified
 * priority. In addition, the error message will be stored as the current
 * error message. This function is basically the same as calling sdb_error_set
 * and sdb_error_log. XXX: SDB_LOG_EMERG might, at some point and/or depending
 * on configuration, try a clean shut-down of the process.
 */
int
sdb_log(int prio, const char *fmt, ...);

/*
 * sdb_error_set, sdb_error_append:
 * Compose the current error message. The string will be formatted in printf-
 * style using the specified format and arguments. No automatic logging will
 * be done.
 */
int
sdb_error_set(const char *fmt, ...);
int
sdb_error_append(const char *fmt, ...);

/*
 * sdb_error_chomp:
 * Remove all consecutive newline characters at the end of the error message.
 */
int
sdb_error_chomp(void);

/*
 * sdb_error_log:
 * Log the current error message with the specified priority. See sdb_log for
 * more information.
 */
int
sdb_error_log(int prio);

/*
 * sdb_error_get:
 * Get the current error message. The string returned by this function is
 * owned by SysDB and might point to static memory -- do not modify or free
 * it.
 */
const char *
sdb_error_get(void);

/*
 * sdb_error_get_prio:
 * Get the priority of the last logged error message -- see the SDB_LOG_
 * constants for details.
 */
int
sdb_error_get_prio(void);

/*
 * sdb_strerror:
 * This is a wrapper around the system's strerror function which ensures that
 * a pointer to the formatted error message is returned.
 */
char *
sdb_strerror(int errnum, char *strerrbuf, size_t buflen);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_UTILS_ERROR_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

