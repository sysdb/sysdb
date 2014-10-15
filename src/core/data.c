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

#include <errno.h>

#include <inttypes.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

/*
 * private helper functions
 */

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
	unsigned char *s1, *s2;
	size_t len1, len2;

	if (d1->type != d2->type)
		return -1;

	if (d1->type == SDB_TYPE_STRING) {
		s1 = (unsigned char *)d1->data.string;
		s2 = (unsigned char *)d2->data.string;
		len1 = s1 ? strlen((char *)s1) : 0;
		len2 = s2 ? strlen((char *)s2) : 0;
	}
	else if (d1->type == SDB_TYPE_BINARY) {
		s1 = d1->data.binary.datum;
		s2 = d2->data.binary.datum;
		len1 = d1->data.binary.length;
		len2 = d2->data.binary.length;
	}
	else
		return -1;

	if (s1 || s2) {
		new = malloc(len1 + len2 + 1);
		if (! new)
			return -1;
	}
	else
		new = NULL;

	if (len1)
		memcpy(new, s1, len1);
	if (len2)
		memcpy(new + len1, s2, len2);
	if (new)
		new[len1 + len2] = '\0';

	res->type = d1->type;
	if (res->type == SDB_TYPE_STRING) {
		res->data.string = (char *)new;
	}
	else {
		res->data.binary.datum = new;
		res->data.binary.length = len1 + len2;
	}
	return 0;
} /* data_concat */

/*
 * public API
 */

int
sdb_data_copy(sdb_data_t *dst, const sdb_data_t *src)
{
	sdb_data_t tmp;

	if ((! dst) || (! src))
		return -1;

	tmp = *src;
	switch (src->type) {
		case SDB_TYPE_STRING:
			if (src->data.string) {
				tmp.data.string = strdup(src->data.string);
				if (! tmp.data.string)
					return -1;
			}
			break;
		case SDB_TYPE_BINARY:
			if (src->data.binary.datum) {
				tmp.data.binary.datum = malloc(src->data.binary.length);
				if (! tmp.data.binary.datum)
					return -1;
				memcpy(tmp.data.binary.datum, src->data.binary.datum,
						src->data.binary.length);
			}
			break;
		case SDB_TYPE_REGEX:
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
			break;
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

	switch (datum->type) {
		case SDB_TYPE_STRING:
			if (datum->data.string)
				free(datum->data.string);
			datum->data.string = NULL;
			break;
		case SDB_TYPE_BINARY:
			if (datum->data.binary.datum)
				free(datum->data.binary.datum);
			datum->data.binary.datum = NULL;
			datum->data.binary.length = 0;
			break;
		case SDB_TYPE_REGEX:
			if (datum->data.re.raw) {
				free(datum->data.re.raw);
				regfree(&datum->data.re.regex);
			}
			datum->data.re.raw = NULL;
			memset(&datum->data.re.regex, 0, sizeof(datum->data.re.regex));
			break;
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

	switch (d1->type) {
		case SDB_TYPE_INTEGER:
			return SDB_CMP(d1->data.integer, d2->data.integer);
		case SDB_TYPE_DECIMAL:
			return SDB_CMP(d1->data.decimal, d2->data.decimal);
		case SDB_TYPE_STRING:
			CMP_NULL(d1->data.string, d2->data.string);
			return strcasecmp(d1->data.string, d2->data.string);
		case SDB_TYPE_DATETIME:
			return SDB_CMP(d1->data.datetime, d2->data.datetime);
		case SDB_TYPE_BINARY:
		{
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
		case SDB_TYPE_REGEX:
			CMP_NULL(d1->data.re.raw, d2->data.re.raw);
			return strcmp(d1->data.re.raw, d2->data.re.raw);
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

	if (sdb_data_format(d1, d1_str, sizeof(d1_str), SDB_UNQUOTED) < 0)
		return SDB_CMP(sizeof(d1_str), sizeof(d2_str));
	if (sdb_data_format(d2, d2_str, sizeof(d2_str), SDB_UNQUOTED) < 0)
		return SDB_CMP(sizeof(d1_str), sizeof(d2_str));

	return strcasecmp(d1_str, d2_str);
#undef CMP_NULL
} /* sdb_data_strcmp */

_Bool
sdb_data_isnull(const sdb_data_t *datum)
{
	if (! datum)
		return 1;
	if ((datum->type == SDB_TYPE_STRING) && (! datum->data.string))
		return 1;
	if ((datum->type == SDB_TYPE_BINARY) && (! datum->data.binary.datum))
		return 1;
	if ((datum->type == SDB_TYPE_REGEX) && (! datum->data.re.raw))
		return 1;
	return 0;
} /* sdb_data_isnull */

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
	switch (op) {
		case SDB_DATA_CONCAT:
			return data_concat(d1, d2, res);
		case SDB_DATA_ADD:
			return data_lin(d1, 1, d2, res);
		case SDB_DATA_SUB:
			return data_lin(d1, -1, d2, res);
		case SDB_DATA_MUL:
			return data_mul(d1, d2, res);
		case SDB_DATA_DIV:
			return data_div(d1, d2, res, NULL);
		case SDB_DATA_MOD:
			return data_div(d1, d2, NULL, res);
	}
	return -1;
} /* sdb_data_expr_eval */

size_t
sdb_data_strlen(const sdb_data_t *datum)
{
	if (! datum)
		return 0;

	switch (datum->type) {
		case SDB_TYPE_INTEGER:
			/* log(64) */
			return 20;
		case SDB_TYPE_DECIMAL:
			/* XXX: -d.dddddde+dd or -ddddd.dddddd */
			return 42;
		case SDB_TYPE_STRING:
			if (! datum->data.string)
				return 8; /* "<NULL>" */
			/* in the worst case, each character needs to be escaped */
			return 2 * strlen(datum->data.string) + 2;
		case SDB_TYPE_DATETIME:
			/* "YYYY-MM-DD HH:MM:SS +zzzz" */
			return 27;
		case SDB_TYPE_BINARY:
			if (! datum->data.binary.datum)
				return 8; /* "<NULL>" */
			/* "\xNN" */
			return 4 * datum->data.binary.length + 2;
		case SDB_TYPE_REGEX:
			if (! datum->data.re.raw)
				return 8; /* "<NULL>" */
			/* "/.../" */
			return strlen(datum->data.re.raw) + 4;
	}
	return 0;
} /* sdb_data_strlen */

int
sdb_data_format(const sdb_data_t *datum, char *buf, size_t buflen, int quoted)
{
	char tmp[sdb_data_strlen(datum) + 1];
	char *data = NULL;
	int ret = -1;

	size_t i, pos;

	if ((! datum) || (! buf))
		return -1;

	switch (datum->type) {
		case SDB_TYPE_INTEGER:
			ret = snprintf(buf, buflen, "%"PRIi64, datum->data.integer);
			break;
		case SDB_TYPE_DECIMAL:
			ret = snprintf(buf, buflen, "%g", datum->data.decimal);
			break;
		case SDB_TYPE_STRING:
			if (! datum->data.string)
				data = "<NULL>";
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
			break;
		case SDB_TYPE_DATETIME:
			if (! sdb_strftime(tmp, sizeof(tmp), "%F %T %z",
						datum->data.datetime))
				return -1;
			tmp[sizeof(tmp) - 1] = '\0';
			data = tmp;
			break;
		case SDB_TYPE_BINARY:
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
				data = "<NULL>";
			break;
		case SDB_TYPE_REGEX:
			if (! datum->data.re.raw)
				data = "<NULL>";
			else {
				snprintf(tmp, sizeof(tmp), "/%s/", datum->data.re.raw);
				data = tmp;
			}
			break;
	}

	if (data) {
		if (quoted == SDB_UNQUOTED)
			ret = snprintf(buf, buflen, "%s", data);
		else if (quoted == SDB_SINGLE_QUOTED)
			ret = snprintf(buf, buflen, "'%s'", data);
		else
			ret = snprintf(buf, buflen, "\"%s\"", data);
	}
	buf[buflen - 1] = '\0';
	return ret;
} /* sdb_data_format */

int
sdb_data_parse(char *str, int type, sdb_data_t *data)
{
	sdb_data_t tmp;

	char *endptr = NULL;

	errno = 0;
	switch (type) {
		case SDB_TYPE_INTEGER:
			tmp.data.integer = strtoll(str, &endptr, 0);
			break;
		case SDB_TYPE_DECIMAL:
			tmp.data.decimal = strtod(str, &endptr);
			break;
		case SDB_TYPE_STRING:
			tmp.data.string = str;
			break;
		case SDB_TYPE_DATETIME:
			{
				double datetime = strtod(str, &endptr);
				tmp.data.datetime = DOUBLE_TO_SDB_TIME(datetime);
			}
			break;
		case SDB_TYPE_BINARY:
			/* we don't support any binary information containing 0-bytes */
			tmp.data.binary.length = strlen(str);
			tmp.data.binary.datum = (unsigned char *)str;
			break;
		case SDB_TYPE_REGEX:
			tmp.data.re.raw = strdup(str);
			if (! tmp.data.re.raw)
				return -1;
			if (regcomp(&tmp.data.re.regex, tmp.data.re.raw,
						REG_EXTENDED | REG_ICASE | REG_NOSUB)) {
				free(tmp.data.re.raw);
				sdb_log(SDB_LOG_ERR, "core: Failed to compile regular "
						"expression '%s'", tmp.data.re.raw);
				return -1;
			}
			if (! data) {
				tmp.type = SDB_TYPE_REGEX;
				sdb_data_free_datum(&tmp);
			}
			break;
		default:
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

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

