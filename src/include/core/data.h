/*
 * SysDB - src/include/core/data.h
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

#ifndef SDB_CORE_DATA_H
#define SDB_CORE_DATA_H 1

#include "core/time.h"
#include "utils/strbuf.h"

#include <inttypes.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	SDB_TYPE_INTEGER = 1,
	SDB_TYPE_DECIMAL,
	SDB_TYPE_STRING,
	SDB_TYPE_DATETIME,
	SDB_TYPE_BINARY,
};

/*
 * sdb_data_t:
 * A datum retrieved from an arbitrary data source.
 */
typedef struct {
	int type;
	union {
		int64_t     integer;  /* SDB_TYPE_INTEGER */
		double      decimal;  /* SDB_TYPE_DECIMAL */
		char       *string;   /* SDB_TYPE_STRING  */
		sdb_time_t  datetime; /* SDB_TYPE_DATETIME */
		struct {
			size_t length;
			unsigned char *datum;
		} binary;             /* SDB_TYPE_BINARY */
	} data;
} sdb_data_t;

/*
 * sdb_data_copy:
 * Copy the datum stored in 'src' to the memory location pointed to by 'dst'.
 * Any dynamic data (strings, binary data) is copied to newly allocated
 * memory. Use, for example, sdb_data_free_datum() to free any dynamic memory
 * stored in a datum.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_data_copy(sdb_data_t *dst, const sdb_data_t *src);

/*
 * sdb_data_free_datum:
 * Free any dynamic memory referenced by the specified datum. Does not free
 * the memory allocated for the sdb_data_t object itself. This function must
 * not be used if any static or stack memory is referenced from the data
 * object.
 */
void
sdb_data_free_datum(sdb_data_t *datum);

/*
 * sdb_data_format:
 * Append the specified datum to the specified string buffer using a default
 * format.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_data_format(sdb_data_t *datum, sdb_strbuf_t *buf);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_CORE_DATA_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

