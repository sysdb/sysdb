/*
 * SysDB - src/core/data.c
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
#endif /* HAVE_CONFIG_H */

#include "sysdb.h"

#include "core/data.h"
#include "utils/error.h"

#include <assert.h>

#include <errno.h>

#include <inttypes.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

/*
 * Operator support maxtrix.
 * <type1> <op> <type2> -> op_matrix[<op>][<type1>][<type2>]
 */

/* add, sub, mul, div, mod, concat */

/* integer, decimal, string, datetime, binary, regex */

static int op_matrix[6][7][7] = {
	/* SDB_DATA_ADD */
	{
		{ -1, -1, -1, -1, -1, -1, -1, },
		{ -1, SDB_TYPE_INTEGER, -1, -1, -1, -1, -1 },
		{ -1, -1, SDB_TYPE_DECIMAL, -1, -1, -1, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
		{ -1, -1, -1, -1, SDB_TYPE_DATETIME, -1, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
	},

	/* SDB_DATA_SUB */
	{
		{ -1, -1, -1, -1, -1, -1, -1, },
		{ -1, SDB_TYPE_INTEGER, -1, -1, -1, -1, -1 },
		{ -1, -1, SDB_TYPE_DECIMAL, -1, -1, -1, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
		{ -1, -1, -1, -1, SDB_TYPE_DATETIME, -1, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
	},

	/* SDB_DATA_MUL */
	{
		{ -1, -1, -1, -1, -1, -1, -1, },
		{ -1, SDB_TYPE_INTEGER, -1, -1, SDB_TYPE_DATETIME, -1, -1 },
		{ -1, -1, SDB_TYPE_DECIMAL, -1, SDB_TYPE_DATETIME, -1, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
		{ -1, SDB_TYPE_DATETIME, SDB_TYPE_DATETIME, -1, SDB_TYPE_DATETIME, -1, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
	},

	/* SDB_DATA_DIV */
	{
		{ -1, -1, -1, -1, -1, -1, -1, },
		{ -1, SDB_TYPE_INTEGER, -1, -1, -1, -1, -1 },
		{ -1, -1, SDB_TYPE_DECIMAL, -1, -1, -1, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
		{ -1, SDB_TYPE_DATETIME, SDB_TYPE_DATETIME, -1, SDB_TYPE_DATETIME, -1, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
	},

	/* SDB_DATA_MOD */
	{
		{ -1, -1, -1, -1, -1, -1, -1, },
		{ -1, SDB_TYPE_INTEGER, -1, -1, -1, -1, -1 },
		{ -1, -1, SDB_TYPE_DECIMAL, -1, -1, -1, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
		{ -1, SDB_TYPE_DATETIME, SDB_TYPE_DATETIME, -1, SDB_TYPE_DATETIME, -1, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
	},

	/* SDB_DATA_CONCAT */
	{
		{ -1, -1, -1, -1, -1, -1, -1, },
		{ -1, -1, -1, -1, -1, -1, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
		{ -1, -1, -1, SDB_TYPE_STRING, -1, -1, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
		{ -1, -1, -1, -1, -1, SDB_TYPE_BINARY, -1 },
		{ -1, -1, -1, -1, -1, -1, -1 },
	},
};

/*
 * private helper functions
 */

/* this function supports in-place copies */
static int
copy_array_values(sdb_data_t *dst, const sdb_data_t *src, size_t elem_size)
{
	int type = src->type & 0xff;

	if ((type == SDB_TYPE_BOOLEAN) || (type == SDB_TYPE_INTEGER)
			|| (type == SDB_TYPE_DECIMAL)) {
		if (dst != src)
			memcpy(dst->data.array.values, src->data.array.values,
					src->data.array.length * elem_size);
	}
	else if (type == SDB_TYPE_STRING) {
		char **s = src->data.array.values;
		char **d = dst->data.array.values;
		size_t i;

		for (i = 0; i < src->data.array.length; ++i) {
			d[i] = strdup(s[i]);
			if (! d[i])
				return -1;
		}
	}
	else {
		/* TODO */
		errno = ENOTSUP;
		return -1;
	}
	return 0;
} /* copy_array_values */

static void
free_array_values(sdb_data_t *datum)
{
	int type = datum->type & 0xff;

	if (type == SDB_TYPE_STRING) {
		char **v = datum->data.array.values;
		size_t i;

		for (i = 0; i < datum->data.array.length; ++i) {
			if (v[i])
				free(v[i]);
			v[i] = NULL;
		}
	}
	else if (type == SDB_TYPE_BINARY) {
		struct {
			size_t length;
			unsigned char *datum;
		} *v = datum->data.array.values;
		size_t i;

		for (i = 0; i < datum->data.array.length; ++i) {
			if (v[i].datum)
				free(v[i].datum);
			v[i].datum = NULL;
		}
	}
	else if (type == SDB_TYPE_REGEX) {
		struct {
			char *raw;
			regex_t regex;
		} *v = datum->data.array.values;
		size_t i;

		for (i = 0; i < datum->data.array.length; ++i) {
			if (v[i].raw) {
				free(v[i].raw);
				regfree(&v[i].regex);
			}
			v[i].raw = NULL;
		}
	}
} /* free_array_values */

/* compare two arrays element-by-element returning how the first non-equal
 * elements compare to each other */
static int
array_cmp(const sdb_data_t *a1, const sdb_data_t *a2)
{
	int type = a1->type & 0xff;
	size_t len, i;

	assert((a1->type == a2->type) && (a1->type & SDB_TYPE_ARRAY));

	len = SDB_MIN(a1->data.array.length, a2->data.array.length);

	if (type == SDB_TYPE_BOOLEAN) {
		bool *v1 = a1->data.array.values;
		bool *v2 = a2->data.array.values;

		for (i = 0; i < len; ++i)
			if (v1[i] != v2[i])
				return SDB_CMP(v1[i], v2[i]);
	}
	else if (type == SDB_TYPE_INTEGER) {
		int64_t *v1 = a1->data.array.values;
		int64_t *v2 = a2->data.array.values;

		for (i = 0; i < len; ++i)
			if (v1[i] != v2[i])
				return SDB_CMP(v1[i], v2[i]);
	}
	else if (type == SDB_TYPE_DECIMAL) {
		double *v1 = a1->data.array.values;
		double *v2 = a2->data.array.values;

		for (i = 0; i < len; ++i)
			if (v1[i] != v2[i])
				return SDB_CMP(v1[i], v2[i]);
	}
	else if (type == SDB_TYPE_STRING) {
		char **v1 = a1->data.array.values;
		char **v2 = a2->data.array.values;

		for (i = 0; i < len; ++i) {
			int diff = strcasecmp(v1[i], v2[i]);
			if (diff)
				return diff;
		}
	}
	else if (type == SDB_TYPE_DATETIME) {
		sdb_time_t *v1 = a1->data.array.values;
		sdb_time_t *v2 = a2->data.array.values;

		for (i = 0; i < len; ++i)
			if (v1[i] != v2[i])
				return SDB_CMP(v1[i], v2[i]);
	}
	else if (type == SDB_TYPE_BINARY) {
		struct {
			size_t length;
			unsigned char *datum;
		} *v1 = a1->data.array.values;
		struct {
			size_t length;
			unsigned char *datum;
		} *v2 = a2->data.array.values;

		for (i = 0; i < len; ++i) {
			int diff;

			/* on a common prefix, the shorter datum sorts less */
			if (v1[i].length < v2[i].length) {
				diff = memcmp(v1[i].datum, v2[i].datum, v1[i].length);
				diff = diff ? diff : -1;
			}
			else if (v1[i].length > v2[i].length) {
				diff = memcmp(v1[i].datum, v2[i].datum, v2[i].length);
				diff = diff ? diff : 1;
			}
			else
				diff = memcmp(v1[i].datum, v2[i].datum, v1[i].length);

			if (diff)
				return diff;
		}
	}
	else if (type == SDB_TYPE_REGEX) {
		struct {
			char *raw;
			regex_t regex;
		} *v1 = a1->data.array.values;
		struct {
			char *raw;
			regex_t regex;
		} *v2 = a2->data.array.values;

		for (i = 0; i < len; ++i) {
			int diff = strcasecmp(v1[i].raw, v2[i].raw);
			if (diff)
				return diff;
		}
	}
	else {
		errno = EINVAL;
		/* but fall through to ensure stable sorting: */
	}
	return SDB_CMP(a1->data.array.length, a2->data.array.length);
} /* array_cmp */

/* Calculate the linear function 'd1 + n * d2'. */
static int
data_lin(const sdb_data_t *d1, int n, const sdb_data_t *d2, sdb_data_t *res)
{
	if (d1->type != d2->type)
		return -1;

	if (d1->type == SDB_TYPE_INTEGER)
		res->data.integer = d1->data.integer + (int64_t)n * d2->data.integer;
	else if (d1->type == SDB_TYPE_DECIMAL)
		res->data.decimal = d1->data.decimal + (double)n * d2->data.decimal;
	else if (d1->type ==  SDB_TYPE_DATETIME)
		res->data.datetime = d1->data.datetime + (sdb_time_t)n * d2->data.datetime;
	else
		return -1;
	res->type = d1->type;
	return 0;
} /* data_lin */

/* Multiply d1 with d2. */
static int
data_mul(const sdb_data_t *d1, const sdb_data_t *d2, sdb_data_t *res)
{
	if (d1->type == SDB_TYPE_INTEGER) {
		if (d2->type == SDB_TYPE_INTEGER)
			res->data.integer = d1->data.integer * d2->data.integer;
		else if (d2->type == SDB_TYPE_DATETIME) {
			res->data.datetime = (sdb_time_t)d1->data.integer
				* d2->data.datetime;
			res->type = SDB_TYPE_DATETIME;
			return 0;
		}
		else
			return -1;
	}
	else if (d1->type == SDB_TYPE_DECIMAL) {
		if (d2->type == SDB_TYPE_DECIMAL)
			res->data.decimal = d1->data.decimal * d2->data.decimal;
		else if (d2->type == SDB_TYPE_DATETIME) {
			res->data.datetime = (sdb_time_t)(d1->data.decimal
					* (double)d2->data.datetime);
			res->type = SDB_TYPE_DATETIME;
			return 0;
		}
		else
			return -1;
	}
	else if (d1->type == SDB_TYPE_DATETIME) {
		if (d2->type == SDB_TYPE_DATETIME)
			res->data.datetime = d1->data.datetime
				* d2->data.datetime;
		else if (d2->type == SDB_TYPE_INTEGER)
			res->data.datetime = d1->data.datetime
				* (sdb_time_t)d2->data.integer;
		else if (d2->type == SDB_TYPE_DECIMAL)
			res->data.datetime = (sdb_time_t)((double)d1->data.datetime
					* d2->data.decimal);
		else
			return -1;
	}
	else
		return -1;

	res->type = d1->type;
	return 0;
} /* data_mul */

/* Device d1 by d2 and return the result and the remainder. */
static int
data_div(const sdb_data_t *d1, const sdb_data_t *d2,
		sdb_data_t *res, sdb_data_t *rem)
{
	if (d1->type == SDB_TYPE_INTEGER) {
		if (d2->type != SDB_TYPE_INTEGER)
			return -1;
		if (res)
			res->data.integer = d1->data.integer / d2->data.integer;
		if (rem)
			rem->data.integer = d1->data.integer % d2->data.integer;
	}
	else if (d1->type == SDB_TYPE_DECIMAL) {
		if (d2->type != SDB_TYPE_DECIMAL)
			return -1;
		if (res)
			res->data.decimal = d1->data.decimal / d2->data.decimal;
		if (rem)
			rem->data.decimal = fmod(d1->data.decimal, d2->data.decimal);
	}
	else if (d1->type == SDB_TYPE_DATETIME) {
		if (d2->type == SDB_TYPE_DECIMAL) {
			if (res)
				res->data.datetime = (sdb_time_t)((double)d1->data.datetime
						/ d2->data.decimal);
			if (rem) {
				double tmp = fmod((double)d1->data.datetime, d2->data.decimal);
				rem->data.datetime = (sdb_time_t)tmp;
			}
		}
		else {
			sdb_time_t a, b;
			if (d2->type == SDB_TYPE_DATETIME) {
				a = d1->data.datetime;
				b = d2->data.datetime;
			}
			else if (d2->type == SDB_TYPE_INTEGER) {
				a = d1->data.datetime;
				b = (sdb_time_t)d2->data.integer;
			}
			else
				return -1;
			if (res)
				res->data.datetime = a / b;
			if (rem)
				rem->data.datetime = a % b;
		}
	}
	else
		return -1;

	if (res)
		res->type = d1->type;
	if (rem)
		rem->type = d1->type;
	return 0;
} /* data_div */

/* Concatenate d1 and d2. */
static int
data_concat(const sdb_data_t *d1, const sdb_data_t *d2, sdb_data_t *res)
{
	unsigned char *new;
	const unsigned char *s1, *s2;
	size_t len1, len2, array1_len = 0, array2_len = 0;

	if ((d1->type & 0xff) != (d2->type & 0xff))
		return -1;

	if ((d1->type & SDB_TYPE_ARRAY) || (d2->type & SDB_TYPE_ARRAY)) {
		size_t elem_size = sdb_data_sizeof(d1->type & 0xff);
		if (d1->type & SDB_TYPE_ARRAY) {
			s1 = (const unsigned char *)d1->data.array.values;
			array1_len = d1->data.array.length;
		}
		else {
			/* As per C99, section 6.7.2.1, paragraph 14:
			 * "A pointer to a union object, suitably converted, points to
			 * each of its members" */
			s1 = (const unsigned char *)&d1->data;
			array1_len = 1;
		}
		if (d2->type & SDB_TYPE_ARRAY) {
			s2 = (const unsigned char *)d2->data.array.values;
			array2_len = d2->data.array.length;
		}
		else {
			s2 = (const unsigned char *)&d2->data;
			array2_len = 1;
		}
		len1 = array1_len * elem_size;
		len2 = array2_len * elem_size;
	}
	else if (d1->type == SDB_TYPE_STRING) {
		s1 = (unsigned char *)d1->data.string;
		s2 = (unsigned char *)d2->data.string;
		len1 = s1 ? strlen((const char *)s1) : 0;
		len2 = s2 ? strlen((const char *)s2) : 0;
	}
	else if (d1->type == SDB_TYPE_BINARY) {
		s1 = d1->data.binary.datum;
		s2 = d2->data.binary.datum;
		len1 = d1->data.binary.length;
		len2 = d2->data.binary.length;
	}
	else
		return -1;

	new = malloc(len1 + len2 + 1);
	if (! new)
		return -1;

	if (len1)
		memcpy(new, s1, len1);
	if (len2)
		memcpy(new + len1, s2, len2);
	new[len1 + len2] = '\0';

	/* element types match and if either datum is an array,
	 * the result is an array as well */
	res->type = d1->type | d2->type;
	if (res->type == SDB_TYPE_STRING) {
		res->data.string = (char *)new;
	}
	else if (res->type == SDB_TYPE_BINARY) {
		res->data.binary.datum = new;
		res->data.binary.length = len1 + len2;
	}
	else if (res->type & SDB_TYPE_ARRAY) {
		res->data.array.values = new;
		res->data.array.length = array1_len + array2_len;
		if (copy_array_values(res, res, sdb_data_sizeof(res->type & 0xff))) {
			/* this leaks already copied values but there's not much we can
			 * do and this should only happen if we're in trouble anyway */
			free(new);
			res->data.array.values = NULL;
			res->data.array.length = 0;
			return -1;
		}
	}
	return 0;
} /* data_concat */

/*
 * public API
 */

const sdb_data_t SDB_DATA_NULL = SDB_DATA_INIT;

int
sdb_data_copy(sdb_data_t *dst, const sdb_data_t *src)
{
	sdb_data_t tmp;

	if ((! dst) || (! src))
		return -1;

	tmp = *src;
	if (src->type == SDB_TYPE_STRING) {
		if (src->data.string) {
			tmp.data.string = strdup(src->data.string);
			if (! tmp.data.string)
				return -1;
		}
	}
	else if (src->type == SDB_TYPE_BINARY) {
		if (src->data.binary.datum) {
			tmp.data.binary.datum = malloc(src->data.binary.length);
			if (! tmp.data.binary.datum)
				return -1;
			memcpy(tmp.data.binary.datum, src->data.binary.datum,
					src->data.binary.length);
		}
	}
	else if (src->type == SDB_TYPE_REGEX) {
		if (src->data.re.raw) {
			tmp.data.re.raw = strdup(src->data.re.raw);
			if (! tmp.data.re.raw)
				return -1;
			/* we need to recompile because the regex might point to
			 * dynamically allocated memory */
			if (regcomp(&tmp.data.re.regex, tmp.data.re.raw,
						REG_EXTENDED | REG_ICASE | REG_NOSUB)) {
				free(tmp.data.re.raw);
				return -1;
			}
		}
		else
			memset(&tmp.data.re.regex, 0, sizeof(tmp.data.re.regex));
	}
	else if (src->type & SDB_TYPE_ARRAY) {
		if (src->data.array.values) {
			size_t elem_size = sdb_data_sizeof(src->type & 0xff);
			tmp.data.array.values = calloc(src->data.array.length, elem_size);
			if (! tmp.data.array.values)
				return -1;
			if (copy_array_values(&tmp, src, elem_size)) {
				sdb_data_free_datum(&tmp);
				return -1;
			}
		}
	}

	sdb_data_free_datum(dst);
	*dst = tmp;
	return 0;
} /* sdb_data_copy */

void
sdb_data_free_datum(sdb_data_t *datum)
{
	if (! datum)
		return;

	if (datum->type == SDB_TYPE_STRING) {
		if (datum->data.string)
			free(datum->data.string);
		datum->data.string = NULL;
	}
	else if (datum->type == SDB_TYPE_BINARY) {
		if (datum->data.binary.datum)
			free(datum->data.binary.datum);
		datum->data.binary.datum = NULL;
		datum->data.binary.length = 0;
	}
	else if (datum->type == SDB_TYPE_REGEX) {
		if (datum->data.re.raw) {
			free(datum->data.re.raw);
			regfree(&datum->data.re.regex);
		}
		datum->data.re.raw = NULL;
		memset(&datum->data.re.regex, 0, sizeof(datum->data.re.regex));
	}
	else if (datum->type & SDB_TYPE_ARRAY) {
		free_array_values(datum);
		if (datum->data.array.values)
			free(datum->data.array.values);
		datum->data.array.values = NULL;
		datum->data.array.length = 0;
	}
} /* sdb_data_free_datum */

int
sdb_data_cmp(const sdb_data_t *d1, const sdb_data_t *d2)
{
#define CMP_NULL(a, b) \
	do { \
		if (!(a) && !(b)) return 0; \
		if (!(a)) return -1; \
		if (!(b)) return 1; \
	} while (0)

	CMP_NULL(d1, d2);

	if (d1->type != d2->type)
		return SDB_CMP(d1->type, d2->type);

	if (d1->type == SDB_TYPE_BOOLEAN)
		return SDB_CMP(d1->data.boolean, d2->data.boolean);
	else if (d1->type == SDB_TYPE_INTEGER)
		return SDB_CMP(d1->data.integer, d2->data.integer);
	else if (d1->type == SDB_TYPE_DECIMAL)
		return SDB_CMP(d1->data.decimal, d2->data.decimal);
	else if (d1->type == SDB_TYPE_STRING) {
		CMP_NULL(d1->data.string, d2->data.string);
		return strcasecmp(d1->data.string, d2->data.string);
	}
	else if (d1->type == SDB_TYPE_DATETIME)
		return SDB_CMP(d1->data.datetime, d2->data.datetime);
	else if (d1->type == SDB_TYPE_BINARY) {
		int diff;

		CMP_NULL(d1->data.binary.datum, d2->data.binary.datum);

		/* on a common prefix, the shorter datum sorts less */
		if (d1->data.binary.length < d2->data.binary.length) {
			diff = memcmp(d1->data.binary.datum, d2->data.binary.datum,
					d1->data.binary.length);
			diff = diff ? diff : -1;
		}
		else if (d1->data.binary.length > d2->data.binary.length) {
			diff = memcmp(d1->data.binary.datum, d2->data.binary.datum,
					d2->data.binary.length);
			diff = diff ? diff : 1;
		}
		else
			diff = memcmp(d1->data.binary.datum, d2->data.binary.datum,
					d1->data.binary.length);

		return diff;
	}
	else if (d1->type == SDB_TYPE_REGEX) {
		CMP_NULL(d1->data.re.raw, d2->data.re.raw);
		return strcmp(d1->data.re.raw, d2->data.re.raw);
	}
	else if (d1->type & SDB_TYPE_ARRAY) {
		CMP_NULL(d1->data.array.values, d2->data.array.values);
		return array_cmp(d1, d2);
	}
	return -1;
} /* sdb_data_cmp */

int
sdb_data_strcmp(const sdb_data_t *d1, const sdb_data_t *d2)
{
	char d1_str[sdb_data_strlen(d1) + 1];
	char d2_str[sdb_data_strlen(d2) + 1];

	if (sdb_data_isnull(d1))
		d1 = NULL;
	if (sdb_data_isnull(d2))
		d2 = NULL;

	CMP_NULL(d1, d2);

	if (! sdb_data_format(d1, d1_str, sizeof(d1_str), SDB_UNQUOTED))
		return SDB_CMP(sizeof(d1_str), sizeof(d2_str));
	if (! sdb_data_format(d2, d2_str, sizeof(d2_str), SDB_UNQUOTED))
		return SDB_CMP(sizeof(d1_str), sizeof(d2_str));

	return strcasecmp(d1_str, d2_str);
#undef CMP_NULL
} /* sdb_data_strcmp */

bool
sdb_data_isnull(const sdb_data_t *datum)
{
	if (! datum)
		return 1;
	if (datum->type == SDB_TYPE_NULL)
		return 1;
	if ((datum->type == SDB_TYPE_STRING) && (! datum->data.string))
		return 1;
	if ((datum->type == SDB_TYPE_BINARY) && (! datum->data.binary.datum))
		return 1;
	if ((datum->type == SDB_TYPE_REGEX) && (! datum->data.re.raw))
		return 1;
	return 0;
} /* sdb_data_isnull */

bool
sdb_data_inarray(const sdb_data_t *value, const sdb_data_t *array)
{
	const void *values;
	size_t length, i;
	int type = value->type & 0xff;

	if (sdb_data_isnull(value) || sdb_data_isnull(array))
		return 0;
	if (! (array->type & SDB_TYPE_ARRAY))
		return 0;
	if ((value->type & 0xff) != (array->type & 0xff))
		return 0;

	if (value->type & SDB_TYPE_ARRAY) {
		values = value->data.array.values;
		length = value->data.array.length;
	}
	else {
		values = &value->data;
		length = 1;
	}

	for (i = 0; i < length; ++i) {
		size_t j;

		if (type == SDB_TYPE_BOOLEAN) {
			bool *v = array->data.array.values;
			for (j = 0; j < array->data.array.length; ++j)
				if (((const bool *)values)[i] == v[j])
					break;
		}
		else if (type == SDB_TYPE_INTEGER) {
			int64_t *v = array->data.array.values;
			for (j = 0; j < array->data.array.length; ++j)
				if (((const int64_t *)values)[i] == v[j])
					break;
		}
		else if (type == SDB_TYPE_DECIMAL) {
			double *v = array->data.array.values;
			for (j = 0; j < array->data.array.length; ++j)
				if (((const double *)values)[i] == v[j])
					break;
		}
		else if (type == SDB_TYPE_STRING) {
			char **v = array->data.array.values;
			for (j = 0; j < array->data.array.length; ++j)
				if (!strcasecmp(((const char * const*)values)[i], v[j]))
					break;
		}
		else {
			/* TODO */
			errno = ENOTSUP;
			return 0;
		}

		if (j >= array->data.array.length)
			/* value not found */
			return 0;
	}
	return 1;
} /* sdb_data_inarray */

int
sdb_data_array_get(const sdb_data_t *array, size_t i, sdb_data_t *value)
{
	sdb_data_t tmp = SDB_DATA_INIT;
	int type;

	if ((! array) || (! (array->type & SDB_TYPE_ARRAY)))
		return -1;
	if (i >= array->data.array.length)
		return -1;

	type = array->type & 0xff;
	if (type == SDB_TYPE_BOOLEAN) {
		bool *v = array->data.array.values;
		tmp.data.boolean = v[i];
	}
	else if (type == SDB_TYPE_INTEGER) {
		int64_t *v = array->data.array.values;
		tmp.data.integer = v[i];
	}
	else if (type == SDB_TYPE_DECIMAL) {
		double *v = array->data.array.values;
		tmp.data.decimal = v[i];
	}
	else if (type == SDB_TYPE_STRING) {
		char **v = array->data.array.values;
		tmp.data.string = v[i];
	}
	else if (type == SDB_TYPE_DATETIME) {
		sdb_time_t *v = array->data.array.values;
		tmp.data.datetime = v[i];
	}
	else if (type == SDB_TYPE_BINARY) {
		struct {
			size_t length;
			unsigned char *datum;
		} *v = array->data.array.values;
		assert(sizeof(tmp.data.binary) == sizeof(v[i]));
		memcpy(&tmp.data.binary, &v[i], sizeof(v[i]));
	}
	else if (type == SDB_TYPE_REGEX) {
		struct {
			char *raw;
			regex_t regex;
		} *v = array->data.array.values;
		assert(sizeof(tmp.data.re) == sizeof(v[i]));
		memcpy(&tmp.data.re, &v[i], sizeof(v[i]));
	}
	else {
		errno = EINVAL;
		return -1;
	}

	if (value) {
		*value = tmp;
		value->type = type;
	}
	return 0;
} /* sdb_data_array_get */

int
sdb_data_parse_op(const char *op)
{
	if (! strcmp(op, "+"))
		return SDB_DATA_ADD;
	else if (! strcmp(op, "-"))
		return SDB_DATA_SUB;
	else if (! strcmp(op, "*"))
		return SDB_DATA_MUL;
	else if (! strcmp(op, "/"))
		return SDB_DATA_DIV;
	else if (! strcmp(op, "%"))
		return SDB_DATA_MOD;
	else if (! strcmp(op, "||"))
		return SDB_DATA_CONCAT;
	return -1;
} /* sdb_data_parse_op */

int
sdb_data_expr_eval(int op, const sdb_data_t *d1, const sdb_data_t *d2,
		sdb_data_t *res)
{
	if ((! d1) || (! d2) || (! res))
		return -1;
	if (sdb_data_isnull(d1) || sdb_data_isnull(d2)) {
		*res = SDB_DATA_NULL;
		return 0;
	}
	switch (op) {
		case SDB_DATA_CONCAT: return data_concat(d1, d2, res);
		case SDB_DATA_ADD: return data_lin(d1, 1, d2, res);
		case SDB_DATA_SUB: return data_lin(d1, -1, d2, res);
		case SDB_DATA_MUL: return data_mul(d1, d2, res);
		case SDB_DATA_DIV: return data_div(d1, d2, res, NULL);
		case SDB_DATA_MOD: return data_div(d1, d2, NULL, res);
	}
	return -1;
} /* sdb_data_expr_eval */

int
sdb_data_expr_type(int op, int type1, int type2)
{
	int types_num = (int)SDB_STATIC_ARRAY_LEN(op_matrix[0]);

	assert(SDB_STATIC_ARRAY_LEN(op_matrix[0])
			== SDB_STATIC_ARRAY_LEN(op_matrix[0][0]));

	if ((op <= 0) || (SDB_STATIC_ARRAY_LEN(op_matrix) < (size_t)op))
		return -1;

	/* arrays only support concat; element type has to match */
	if ((type1 & SDB_TYPE_ARRAY) || (type2 & SDB_TYPE_ARRAY)) {
		if (((type1 & 0xff) != (type2 & 0xff)) || (op != SDB_DATA_CONCAT))
			return -1;
		return type1 | SDB_TYPE_ARRAY;
	}
	if ((type1 < 0) || (types_num < type1)
			|| (type2 < 0) || (types_num < type2))
		return -1;

	if ((type1 == SDB_TYPE_NULL) || (type2 == SDB_TYPE_NULL))
		return SDB_TYPE_NULL;
	return op_matrix[op - 1][type1 - 1][type2 - 1];
} /* sdb_data_expr_type */

size_t
sdb_data_strlen(const sdb_data_t *datum)
{
	if (! datum)
		return 0;

	if (sdb_data_isnull(datum)) {
		/* NULL */
		return 4;
	}
	switch (datum->type) {
	case SDB_TYPE_BOOLEAN:
		/* true | false */
		return 5;
	case SDB_TYPE_INTEGER:
		/* log(64) */
		return 20;
	case SDB_TYPE_DECIMAL:
		/* XXX: -d.dddddde+dd or -ddddd.dddddd */
		return 42;
	case SDB_TYPE_STRING:
		if (! datum->data.string)
			return 6; /* NULL */
		/* in the worst case, each character needs to be escaped */
		return 2 * strlen(datum->data.string) + 2;
	case SDB_TYPE_DATETIME:
		/* "YYYY-MM-DD HH:MM:SS +zzzz" */
		return 27;
	case SDB_TYPE_BINARY:
		if (! datum->data.binary.datum)
			return 6; /* NULL */
		/* "\xNN" */
		return 4 * datum->data.binary.length + 2;
	case SDB_TYPE_REGEX:
		if (! datum->data.re.raw)
			return 6; /* NULL */
		/* "/.../" */
		return strlen(datum->data.re.raw) + 4;
	}
	if (datum->type & SDB_TYPE_ARRAY) {
		size_t len = 2; /* [] */
		size_t i;
		for (i = 0; i < datum->data.array.length; ++i) {
			sdb_data_t v = SDB_DATA_INIT;
			sdb_data_array_get(datum, i, &v);
			len += sdb_data_strlen(&v) + 1;
		}
		return len;
	}
	return 0;
} /* sdb_data_strlen */

size_t
sdb_data_format(const sdb_data_t *datum, char *buf, size_t buflen, int quoted)
{
	char tmp[sdb_data_strlen(datum) + 1];
	char *data = NULL;
	bool is_null = 0;
	size_t ret = 0;

	size_t i, pos;

	if (! datum)
		return 0;

	if (datum->type == SDB_TYPE_NULL) {
		strncpy(buf, "NULL", buflen);
		ret = 4;
	}
	else if (datum->type == SDB_TYPE_BOOLEAN) {
		if (datum->data.boolean) {
			strncpy(buf, "true", buflen);
			ret = 4;
		}
		else {
			strncpy(buf, "false", buflen);
			ret = 5;
		}
	}
	else if (datum->type == SDB_TYPE_INTEGER) {
		ret = snprintf(buf, buflen, "%"PRIi64, datum->data.integer);
	}
	else if (datum->type == SDB_TYPE_DECIMAL) {
		if (isnan(datum->data.decimal))
			ret = snprintf(buf, buflen, "nan");
		else
			ret = snprintf(buf, buflen, "%g", datum->data.decimal);
	}
	else if (datum->type == SDB_TYPE_STRING) {
		if (! datum->data.string)
			is_null = 1;
		else {
			pos = 0;
			for (i = 0; i < strlen(datum->data.string); ++i) {
				char byte = datum->data.string[i];

				if ((byte == '\\') || (byte == '"')) {
					tmp[pos] = '\\';
					++pos;
				}
				tmp[pos] = byte;
				++pos;
			}
			tmp[pos] = '\0';
			data = tmp;
		}
	}
	else if (datum->type == SDB_TYPE_DATETIME) {
		if (! sdb_strftime(tmp, sizeof(tmp), "%F %T %z",
					datum->data.datetime))
			return -1;
		tmp[sizeof(tmp) - 1] = '\0';
		data = tmp;
	}
	else if (datum->type == SDB_TYPE_BINARY) {
		pos = 0;
		for (i = 0; i < datum->data.binary.length; ++i) {
			int byte = (int)datum->data.binary.datum[i];
			char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
				'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

			tmp[pos] = '\\';
			tmp[pos + 1] = 'x';
			pos += 2;

			if (byte > 0xf) {
				tmp[pos] = hex[byte >> 4];
				++pos;
			}
			tmp[pos] = hex[byte & 0xf];
			++pos;
		}
		if (datum->data.binary.datum) {
			tmp[pos] = '\0';
			data = tmp;
		}
		else
			is_null = 1;
	}
	else if (datum->type == SDB_TYPE_REGEX) {
		if (! datum->data.re.raw)
			is_null = 1;
		else {
			snprintf(tmp, sizeof(tmp), "/%s/", datum->data.re.raw);
			data = tmp;
		}
	}
	else if (datum->type & SDB_TYPE_ARRAY) {
		ret = 1;
		if (buflen > 0)
			buf[0] = '[';
		for (i = 0; i < datum->data.array.length; ++i) {
			sdb_data_t v = SDB_DATA_INIT;
			size_t n;

			if (ret > 1) {
				if (buflen > ret + 1) {
					buf[ret] = ',';
					buf[ret + 1] = ' ';
				}
				ret += 2;
			}

			sdb_data_array_get(datum, i, &v);
			if (buflen > ret)
				n = sdb_data_format(&v, buf + ret, buflen - ret, quoted);
			else
				n = sdb_data_format(&v, NULL, 0, quoted);
			if (n > 0)
				ret += n;
			else
				break;
		}
		if (buflen > ret + 1) {
			buf[ret] = ']';
			buf[ret + 1] = '\0';
		}
		++ret;
	}

	if (is_null) {
		/* never quote NULL */
		strncpy(buf, "NULL", buflen);
		ret = 4;
	}
	else if (data) {
		if (quoted == SDB_UNQUOTED)
			ret = snprintf(buf, buflen, "%s", data);
		else if (quoted == SDB_SINGLE_QUOTED)
			ret = snprintf(buf, buflen, "'%s'", data);
		else
			ret = snprintf(buf, buflen, "\"%s\"", data);
	}
	if (buflen > 0)
		buf[buflen - 1] = '\0';
	return ret;
} /* sdb_data_format */

int
sdb_data_parse(const char *str, int type, sdb_data_t *data)
{
	sdb_data_t tmp;

	char *endptr = NULL;

	if (! str) {
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	if (type == SDB_TYPE_BOOLEAN) {
		if (! strcasecmp(str, "true"))
			tmp.data.boolean = true;
		else if (! strcasecmp(str, "false"))
			tmp.data.boolean = false;
		else
			return -1;
	}
	else if (type == SDB_TYPE_INTEGER) {
		tmp.data.integer = strtoll(str, &endptr, 0);
	}
	else if (type == SDB_TYPE_DECIMAL) {
		tmp.data.decimal = strtod(str, &endptr);
	}
	else if (type == SDB_TYPE_STRING) {
		tmp.data.string = strdup(str);
		if (! tmp.data.string)
			return -1;
	}
	else if (type == SDB_TYPE_DATETIME) {
		double datetime = strtod(str, &endptr);
		tmp.data.datetime = DOUBLE_TO_SDB_TIME(datetime);
	}
	else if (type == SDB_TYPE_BINARY) {
		/* we don't support any binary information containing 0-bytes here */
		tmp.data.binary.datum = (unsigned char *)strdup(str);
		if (! tmp.data.binary.datum)
			return -1;
		tmp.data.binary.length = strlen(str);
	}
	else if (type == SDB_TYPE_REGEX) {
		tmp.data.re.raw = strdup(str);
		if (! tmp.data.re.raw)
			return -1;
		if (regcomp(&tmp.data.re.regex, tmp.data.re.raw,
					REG_EXTENDED | REG_ICASE | REG_NOSUB)) {
			sdb_log(SDB_LOG_ERR, "core: Failed to compile regular "
					"expression '%s'", tmp.data.re.raw);
			free(tmp.data.re.raw);
			return -1;
		}
		if (! data) {
			tmp.type = SDB_TYPE_REGEX;
			sdb_data_free_datum(&tmp);
		}
	}
	else if (type & SDB_TYPE_ARRAY) {
		/* TODO */
		errno = ENOTSUP;
		return -1;
	}
	else {
		errno = EINVAL;
		return -1;
	}

	if ((type == SDB_TYPE_INTEGER) || (type == SDB_TYPE_DECIMAL)
			|| (type == SDB_TYPE_DATETIME)) {
		if (errno || (str == endptr)) {
			char errbuf[1024];
			sdb_log(SDB_LOG_ERR, "core: Failed to parse string "
					"'%s' as numeric value (type %i): %s", str, type,
					sdb_strerror(errno, errbuf, sizeof(errbuf)));
			return -1;
		}
		else if (endptr && (*endptr != '\0'))
			sdb_log(SDB_LOG_WARNING, "core: Ignoring garbage after "
					"number while parsing numeric value (type %i): %s.",
					type, endptr);
	}

	if (data) {
		*data = tmp;
		data->type = type;
	}
	return 0;
} /* sdb_data_parse */

size_t
sdb_data_sizeof(int type)
{
	sdb_data_t v;
	if (type == SDB_TYPE_BOOLEAN)
		return sizeof(v.data.boolean);
	else if (type == SDB_TYPE_INTEGER)
		return sizeof(v.data.integer);
	else if (type == SDB_TYPE_DECIMAL)
		return sizeof(v.data.decimal);
	else if (type == SDB_TYPE_STRING)
		return sizeof(v.data.string);
	else if (type == SDB_TYPE_DATETIME)
		return sizeof(v.data.datetime);
	else if (type == SDB_TYPE_BINARY)
		return sizeof(v.data.binary);
	else if (type == SDB_TYPE_REGEX)
		return sizeof(v.data.re);
	return 0;
} /* sdb_data_sizeof */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

