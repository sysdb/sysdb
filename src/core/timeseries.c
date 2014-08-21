/*
 * SysDB - src/core/timeseries.c
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
#include "core/timeseries.h"

#include <stdlib.h>
#include <string.h>

/*
 * public API
 */

sdb_timeseries_t *
sdb_timeseries_create(size_t data_names_len, const char * const *data_names,
		size_t data_len)
{
	sdb_timeseries_t *ts;
	size_t i;

	ts = calloc(1, sizeof(*ts));
	if (! ts)
		return NULL;

	ts->data = calloc(data_names_len, sizeof(*ts->data));
	if (! ts->data) {
		sdb_timeseries_destroy(ts);
		return NULL;
	}
	ts->data_names_len = data_names_len;
	for (i = 0; i < data_names_len; ++i) {
		ts->data[i] = calloc(data_len, sizeof(**ts->data));
		if (! ts->data[i]) {
			sdb_timeseries_destroy(ts);
			return NULL;
		}
	}
	ts->data_len = data_len;

	ts->data_names = calloc(data_names_len, sizeof(*ts->data_names));
	if (! ts->data_names) {
		sdb_timeseries_destroy(ts);
		return NULL;
	}
	for (i = 0; i < data_names_len; ++i) {
		ts->data_names[i] = strdup(data_names[i]);
		if (! ts->data_names[i]) {
			sdb_timeseries_destroy(ts);
			return NULL;
		}
	}
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

	if (ts->data_names) {
		for (i = 0; i < ts->data_names_len; ++i) {
			if (ts->data_names[i])
				free(ts->data_names[i]);
			ts->data_names[i] = NULL;
		}
		free(ts->data_names);
	}
	ts->data_names = NULL;
	ts->data_names_len = 0;
} /* sdb_timeseries_destroy */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

