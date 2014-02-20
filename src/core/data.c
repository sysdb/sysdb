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

#include "core/data.h"

#include <inttypes.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
			tmp.data.string = strdup(src->data.string);
			if (! tmp.data.string)
				return -1;
			break;
		case SDB_TYPE_BINARY:
			tmp.data.binary.datum = malloc(src->data.binary.length);
			if (! tmp.data.binary.datum)
				return -1;
			memcpy(tmp.data.binary.datum, src->data.binary.datum,
					src->data.binary.length);
			break;
	}

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
	}
} /* sdb_data_free_datum */

size_t
sdb_data_strlen(sdb_data_t *datum)
{
	if (! datum)
		return 0;

	switch (datum->type) {
		case SDB_TYPE_INTEGER:
			/* log(64) */
			return 20;
		case SDB_TYPE_DECIMAL:
			/* XXX: -0xN.NNNNNNp+NNN */
			return 42;
		case SDB_TYPE_STRING:
			if (! datum->data.string)
				return 6; /* "NULL" */
			/* in the worst case, each character needs to be escaped */
			return 2 * strlen(datum->data.string) + 2;
		case SDB_TYPE_DATETIME:
			/* "YYYY-MM-DD HH:MM:SS +zzzz" */
			return 27;
		case SDB_TYPE_BINARY:
			/* "\xNN" */
			return 4 * datum->data.binary.length + 2;
	}
	return 0;
} /* sdb_data_strlen */

int
sdb_data_format(sdb_data_t *datum, char *buf, size_t buflen, int quoted)
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
			ret = snprintf(buf, buflen, "%a", datum->data.decimal);
			break;
		case SDB_TYPE_STRING:
			if (! datum->data.string)
				data = "NULL";
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
			tmp[pos] = '\0';
			data = tmp;
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

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

