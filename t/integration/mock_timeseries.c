/*
 * SysDB - t/integration/mock_timeseries.c
 * Copyright (C) 2015 Sebastian 'tokkee' Harl <sh@tokkee.org>
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
#include "core/plugin.h"
#include "core/store.h"
#include "utils/error.h"

#include <stdlib.h>
#include <string.h>

#define MAGIC_DATA (void *)0x4711

SDB_PLUGIN_MAGIC;

/*
 * plugin API
 */

static sdb_timeseries_t *
mock_fetch_ts(const char *id, sdb_timeseries_opts_t *opts,
		sdb_object_t *user_data)
{
	sdb_timeseries_t *ts;
	const char *names[] = { "nameA", "nameB" };
	size_t i, j;

	if (*id != '/') {
		sdb_log(SDB_LOG_ERR, "mock::timeseries: Invalid time-series %s", id);
		exit(1);
	}

	if (SDB_OBJ_WRAPPER(user_data)->data != MAGIC_DATA) {
		sdb_log(SDB_LOG_ERR, "mock::timeseries: Invalid user data %p "
				"passed to collect", SDB_OBJ_WRAPPER(user_data)->data);
		exit(1);
	}

	ts = sdb_timeseries_create(SDB_STATIC_ARRAY_LEN(names), names, 10);
	if (! ts)
		return NULL;

	ts->start = opts->start;
	ts->end = opts->end;

	for (i = 0; i < 10; ++i) {
		for (j = 0; j < SDB_STATIC_ARRAY_LEN(names); ++j) {
			ts->data[j][i].timestamp = ts->start
				+ i * (ts->end - ts->start) / 10;
			ts->data[j][i].value = (double)(i + j);
		}
	}
	return ts;
} /* mock_fetch_ts */

int
sdb_module_init(sdb_plugin_info_t *info)
{
	sdb_object_t *user_data;

	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_DESC, "a mock timeseries fetcher");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_COPYRIGHT,
			"Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_LICENSE, "BSD");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_VERSION, SDB_VERSION);
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_PLUGIN_VERSION, SDB_VERSION);

	user_data = sdb_object_create_wrapper("mock_data", MAGIC_DATA, NULL);
	if (! user_data) {
		sdb_log(SDB_LOG_ERR, "mock::plugin: Failed to allocate user data");
		exit(1);
	}
	sdb_plugin_register_ts_fetcher("mock", mock_fetch_ts, user_data);
	sdb_object_deref(user_data);
	return 0;
} /* sdb_module_init */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

