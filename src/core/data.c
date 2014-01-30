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

int
sdb_data_format(sdb_data_t *datum, sdb_strbuf_t *buf)
{
	if ((! datum) || (! buf))
		return -1;

	switch (datum->type) {
		case SDB_TYPE_INTEGER:
			sdb_strbuf_append(buf, "%"PRIi64, datum->data.integer);
			break;
		case SDB_TYPE_DECIMAL:
			sdb_strbuf_append(buf, "%a", datum->data.decimal);
			break;
		case SDB_TYPE_STRING:
			/* TODO: escape special characters */
			sdb_strbuf_append(buf, "\"%s\"", datum->data.string);
			break;
		case SDB_TYPE_DATETIME:
			{
				char tmp[64];
				if (! sdb_strftime(tmp, sizeof(tmp), "%F %T %z",
							datum->data.datetime))
					return -1;
				tmp[sizeof(tmp) - 1] = '\0';
				sdb_strbuf_append(buf, "%s", tmp);
			}
			break;
		case SDB_TYPE_BINARY:
			{
				size_t i;
				/* TODO: improve this! */
				sdb_strbuf_append(buf, "%s", "\"");
				for (i = 0; i < datum->data.binary.length; ++i)
					sdb_strbuf_append(buf, "\\%x",
							(int)datum->data.binary.datum[i]);
				sdb_strbuf_append(buf, "%s", "\"");
			}
			break;
		default:
			return -1;
	}
	return 0;
} /* sdb_data_format */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

