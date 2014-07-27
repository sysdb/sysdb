/*
 * SysDB - src/include/core/data.h
 * Copyright (C) 2012-2014 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#define SDB_TYPE_TO_STRING(t) \
	(((t) == SDB_TYPE_INTEGER) \
		? "INTEGER" \
		: ((t) == SDB_TYPE_DECIMAL) \
			? "DECIMAL" \
			: ((t) == SDB_TYPE_STRING) \
				? "STRING" \
				: ((t) == SDB_TYPE_DATETIME) \
					? "DATETIME" \
					: ((t) == SDB_TYPE_BINARY) \
						? "BINARY" \
						: "UNKNOWN")

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
#define SDB_DATA_INIT { 0, { .integer = 0 } }

/*
 * sdb_data_copy:
 * Copy the datum stored in 'src' to the memory location pointed to by 'dst'.
 * Any dynamic data (strings, binary data) is copied to newly allocated
 * memory. Use, for example, sdb_data_free_datum() to free any dynamic memory
 * stored in a datum. On error, 'dst' is unchanged. Else, any dynamic memory
 * in 'dst' will be freed.
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
 * the memory allocated by the sdb_data_t object itself. This function must
 * not be used if any static or stack memory is referenced from the data
 * object.
 */
void
sdb_data_free_datum(sdb_data_t *datum);

/*
 * sdb_data_cmp:
 * Compare two data points. A NULL datum is considered less than any non-NULL
 * datum. On data-type mismatch, the function always returns a negative value.
 *
 * Returns:
 *  - a value less than zero if d1 compares less than d2
 *  - zero if d1 compares equal to d2
 *  - a value greater than zero if d1 compares greater than d2
 */
int
sdb_data_cmp(const sdb_data_t *d1, const sdb_data_t *d2);

/*
 * Operators supported by sdb_data_eval_expr.
 */
enum {
	SDB_DATA_ADD = 1, /* addition */
	SDB_DATA_SUB,     /* substraction */
	SDB_DATA_MUL,     /* multiplication */
	SDB_DATA_DIV,     /* division */
	SDB_DATA_MOD,     /* modulo */
	SDB_DATA_CONCAT,  /* string / binary data concatenation */
};

#define SDB_DATA_OP_TO_STRING(op) \
	(((op) == SDB_DATA_ADD) \
		? "+" \
		: ((op) == SDB_DATA_SUB) \
			? "-" \
			: ((op) == SDB_DATA_MUL) \
				? "*" \
				: ((op) == SDB_DATA_DIV) \
					? "/" \
					: ((op) == SDB_DATA_MOD) \
						? "%" \
						: ((op) == SDB_DATA_CONCAT) \
							? "||" : "UNKNOWN")

/*
 * sdb_data_expr_eval:
 * Evaluate a simple arithmetic expression on two data points. The data-type
 * of d1 and d2 have to be the same. String and binary data only support
 * concatenation and all other data types only support the other operators.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_data_expr_eval(int op, const sdb_data_t *d1, const sdb_data_t *d2,
		sdb_data_t *res);

/*
 * sdb_data_strlen:
 * Returns a (worst-case) estimate for the number of bytes required to format
 * the datum as a string. Does not take the terminating null byte into
 * account.
 */
size_t
sdb_data_strlen(const sdb_data_t *datum);

enum {
	SDB_UNQUOTED = 0,
	SDB_SINGLE_QUOTED,
	SDB_DOUBLE_QUOTED,
};

/*
 * sdb_data_format:
 * Output the specified datum to the specified string using a default format.
 * The value of 'quoted' determines whether and how non-integer and
 * non-decimal values are quoted. If the buffer size is less than the return
 * value of sdb_data_strlen, the datum may be truncated. The buffer will
 * always be nul-terminated after calling this function.
 *
 * Returns:
 *  - the number of characters written to the buffer (excluding the terminated
 *    null byte) or the number of characters which would have been written in
 *    case the output was truncated
 *  - a negative value else
 */
int
sdb_data_format(const sdb_data_t *datum, char *buf, size_t buflen, int quoted);

/*
 * sdb_data_parse:
 * Parse the specified string into a datum using the specified type. The
 * string value is expected to be a raw value of the specified type. Integer
 * and decimal numbers may be signed or unsigned octal (base 8, if the first
 * character of the string is "0"), sedecimal (base 16, if the string includes
 * the "0x" prefix), or decimal. Decimal numbers may also be "infinity" or
 * "NaN" or may use a decimal exponent. Date-time values are expected to be
 * specified as (floating point) number of seconds since the epoch. For string
 * and binary data, the input string is passed to the datum. The function does
 * not allocate new memory for that purpose. Use sdb_data_copy() if you want
 * to do that.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_data_parse(char *str, int type, sdb_data_t *data);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_CORE_DATA_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

