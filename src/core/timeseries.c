/*
 * SysDB - src/core/timeseries.c
 * Copyright (C) 2014-2016 Sebastian 'tokkee' Harl <sh@tokkee.org>
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
#include "core/timeseries.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

static int
copy_strings(char ***out, size_t *out_len,
		const char * const *in, size_t in_len)
{
	size_t i;

	*out = calloc(in_len, sizeof(**out));
	if (! *out)
		return -1;

	*out_len = in_len;
	for (i = 0; i < in_len; ++i) {
		(*out)[i] = strdup(in[i]);
		if (! (*out)[i])
			return -1;
	}
	return 0;
} /* copy_strings */

static void
free_strings(char ***strings, size_t *strings_len)
{
	size_t i;

	if (*strings) {
		for (i = 0; i < *strings_len; ++i) {
			if ((*strings)[i])
				free((*strings)[i]);
			(*strings)[i] = NULL;
		}
		free(*strings);
	}

	*strings = NULL;
	*strings_len = 0;
} /* free_strings */

/*
 * public API
 */

sdb_timeseries_info_t *
sdb_timeseries_info_create(size_t data_names_len, const char * const *data_names)
{
	sdb_timeseries_info_t *ts_info;

	ts_info = calloc(1, sizeof(*ts_info));
	if (! ts_info)
		return NULL;

	if (copy_strings(&ts_info->data_names, &ts_info->data_names_len,
				data_names, data_names_len)) {
		sdb_timeseries_info_destroy(ts_info);
		return NULL;
	}
	return ts_info;
} /* sdb_timeseries_info_create */

void
sdb_timeseries_info_destroy(sdb_timeseries_info_t *ts_info)
{
	if (! ts_info)
		return;

	free_strings(&ts_info->data_names, &ts_info->data_names_len);
	free(ts_info);
} /* sdb_timeseries_info_destroy */

sdb_timeseries_t *
sdb_timeseries_create(size_t data_names_len, const char * const *data_names,
		size_t data_len)
{
	sdb_timeseries_t *ts;
	size_t i;

	ts = calloc(1, sizeof(*ts));
	if (! ts)
		return NULL;

	if (copy_strings(&ts->data_names, &ts->data_names_len,
				data_names, data_names_len)) {
		sdb_timeseries_destroy(ts);
		return NULL;
	}

	ts->data = calloc(data_names_len, sizeof(*ts->data));
	if (! ts->data) {
		sdb_timeseries_destroy(ts);
		return NULL;
	}
	for (i = 0; i < data_names_len; ++i) {
		ts->data[i] = calloc(data_len, sizeof(**ts->data));
		if (! ts->data[i]) {
			sdb_timeseries_destroy(ts);
			return NULL;
		}
	}
	ts->data_len = data_len;
	return ts;
} /* sdb_timeseries_create */

void
sdb_timeseries_destroy(sdb_timeseries_t *ts)
{
	size_t i;

	if (! ts)
		return;

	if (ts->data) {
		for (i = 0; i < ts->data_names_len; ++i) {
			if (ts->data[i])
				free(ts->data[i]);
			ts->data[i] = NULL;
		}
		free(ts->data);
	}
	ts->data = NULL;
	ts->data_len = 0;

	free_strings(&ts->data_names, &ts->data_names_len);
	free(ts);
} /* sdb_timeseries_destroy */

int
sdb_timeseries_tojson(sdb_timeseries_t *ts, sdb_strbuf_t *buf)
{
	char start_str[64];
	char end_str[64];

	size_t i;

	if ((! ts) || (! buf))
		return -1;

	/* TODO: make time format configurable */
	if (! sdb_strftime(start_str, sizeof(start_str), ts->start))
		snprintf(start_str, sizeof(start_str), "<error>");
	start_str[sizeof(start_str) - 1] = '\0';
	if (! sdb_strftime(end_str, sizeof(end_str), ts->end))
		snprintf(end_str, sizeof(end_str), "<error>");
	end_str[sizeof(end_str) - 1] = '\0';

	sdb_strbuf_append(buf, "{\"start\": \"%s\", \"end\": \"%s\", \"data\": {",
			start_str, end_str);

	for (i = 0; i < ts->data_names_len; ++i) {
		size_t j;
		sdb_strbuf_append(buf, "\"%s\": [", ts->data_names[i]);

		for (j = 0; j < ts->data_len; ++j) {
			char time_str[64];

			if (! sdb_strftime(time_str, sizeof(time_str), ts->data[i][j].timestamp))
				snprintf(time_str, sizeof(time_str), "<error>");
			time_str[sizeof(time_str) - 1] = '\0';

			/* Some GNU libc versions may print '-nan' which we dont' want */
			if (isnan(ts->data[i][j].value))
				sdb_strbuf_append(buf, "{\"timestamp\": \"%s\", "
						"\"value\": \"nan\"}", time_str);
			else
				sdb_strbuf_append(buf, "{\"timestamp\": \"%s\", "
						"\"value\": \"%f\"}", time_str, ts->data[i][j].value);

			if (j < ts->data_len - 1)
				sdb_strbuf_append(buf, ",");
		}

		if (i < ts->data_names_len - 1)
			sdb_strbuf_append(buf, "],");
		else
			sdb_strbuf_append(buf, "]");
	}
	sdb_strbuf_append(buf, "}}");
	return 0;
} /* sdb_timeseries_tojson */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

