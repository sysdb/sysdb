/*
 * SysDB - src/plugins/backend/collectd/unixsock.c
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

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "sysdb.h"
#include "core/plugin.h"
#include "core/store.h"
#include "utils/error.h"
#include "utils/unixsock.h"

#include "liboconfig/utils.h"

#include <assert.h>

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

SDB_PLUGIN_MAGIC;

/*
 * private data types
 */

typedef struct {
	sdb_unixsock_client_t *client;

	char *ts_type;
	char *ts_base;
} user_data_t;
#define UD(obj) ((user_data_t *)(obj))

typedef struct {
	char *current_host;
	sdb_time_t current_timestamp;
	int metrics_updated;
	int metrics_failed;

	user_data_t *ud;
} state_t;
#define STATE_INIT { NULL, 0, 0, 0, NULL }

/*
 * private helper functions
 */

static void
user_data_destroy(void *obj)
{
	user_data_t *ud = UD(obj);

	if (! obj)
		return;

	if (ud->client)
		sdb_unixsock_client_destroy(ud->client);
	ud->client = NULL;

	if (ud->ts_type)
		free(ud->ts_type);
	if (ud->ts_base)
		free(ud->ts_base);
	ud->ts_type = ud->ts_base = NULL;

	free(ud);
} /* user_data_destroy */

/* store the specified host-name (once per iteration) */
static int
store_host(state_t *state, const char *hostname, sdb_time_t last_update)
{
	int status;

	if (last_update > state->current_timestamp)
		state->current_timestamp = last_update;

	if (state->current_host && (! strcasecmp(state->current_host, hostname)))
		return 0;
	/* else: first/new host */

	if (state->current_host) {
		sdb_log(SDB_LOG_DEBUG, "collectd::unixsock backend: Added/updated "
				"%i metric%s (%i failed) for host '%s'.",
				state->metrics_updated, state->metrics_updated == 1 ? "" : "s",
				state->metrics_failed, state->current_host);
		state->metrics_updated = state->metrics_failed = 0;
		free(state->current_host);
	}

	state->current_host = strdup(hostname);
	if (! state->current_host) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: Failed to allocate "
				"string buffer: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return -1;
	}

	status = sdb_plugin_store_host(hostname, last_update);

	if (status < 0) {
		sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: Failed to "
				"store/update host '%s'.", hostname);
		return -1;
	}
	else if (status > 0) /* value too old */
		return 0;

	sdb_log(SDB_LOG_DEBUG, "collectd::unixsock backend: Added/updated "
			"host '%s' (last update timestamp = %"PRIsdbTIME").",
			hostname, last_update);
	return 0;
} /* store_host */

static int
add_metrics(const char *hostname, char *plugin, char *type,
		sdb_time_t last_update, user_data_t *ud)
{
	char  name[strlen(plugin) + strlen(type) + 2];
	char *plugin_instance, *type_instance;

	char metric_id[(ud->ts_base ? strlen(ud->ts_base) : 0)
		+ strlen(hostname) + sizeof(name) + 7];
	sdb_metric_store_t store = { ud->ts_type, metric_id, NULL, last_update };

	sdb_data_t data = { SDB_TYPE_STRING, { .string = NULL } };

	int status;

	snprintf(name, sizeof(name), "%s/%s", plugin, type);

	if (ud->ts_base) {
		snprintf(metric_id, sizeof(metric_id), "%s/%s/%s.rrd",
				ud->ts_base, hostname, name);
		status = sdb_plugin_store_metric(hostname, name, &store, last_update);
	}
	else
		status = sdb_plugin_store_metric(hostname, name, NULL, last_update);
	if (status < 0) {
		sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: Failed to "
				"store/update metric '%s/%s'.", hostname, name);
		return -1;
	}

	plugin_instance = strchr(plugin, '-');
	if (plugin_instance) {
		*plugin_instance = '\0';
		++plugin_instance;

		data.data.string = plugin_instance;
		sdb_plugin_store_metric_attribute(hostname, name,
				"plugin_instance", &data, last_update);
	}

	type_instance = strchr(type, '-');
	if (type_instance) {
		*type_instance = '\0';
		++type_instance;

		data.data.string = type_instance;
		sdb_plugin_store_metric_attribute(hostname, name,
				"type_instance", &data, last_update);
	}

	data.data.string = plugin;
	sdb_plugin_store_metric_attribute(hostname, name, "plugin", &data, last_update);
	data.data.string = type;
	sdb_plugin_store_metric_attribute(hostname, name, "type", &data, last_update);
	return 0;
} /* add_metrics */

static int
get_data(sdb_unixsock_client_t __attribute__((unused)) *client,
		size_t n, sdb_data_t *data, sdb_object_t *user_data)
{
	state_t *state;
	sdb_data_t last_update;

	char *hostname;
	char *plugin;
	char *type;

	assert(user_data);

	/* 0: <last_update> <hostname>
	 * 1: <plugin>
	 * 2: <type> */
	assert(n == 3);
	assert((data[0].type == SDB_TYPE_STRING)
			&& (data[1].type == SDB_TYPE_STRING)
			&& (data[2].type == SDB_TYPE_STRING));

	hostname = data[0].data.string;
	plugin   = data[1].data.string;
	type     = data[2].data.string;

	hostname = strchr(hostname, ' ');
	if (! hostname) {
		sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: Expected to find "
				"a space character in the LISTVAL response");
		return -1;
	}
	*hostname = '\0';
	++hostname;

	if (sdb_data_parse(data[0].data.string, SDB_TYPE_DATETIME, &last_update)) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: Failed to parse "
				"timestamp '%s' returned by LISTVAL: %s", data[0].data.string,
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return -1;
	}

	state = SDB_OBJ_WRAPPER(user_data)->data;
	if (store_host(state, hostname, last_update.data.datetime))
		return -1;

	if (add_metrics(hostname, plugin, type,
				last_update.data.datetime, state->ud))
		++state->metrics_failed;
	else
		++state->metrics_updated;
	return 0;
} /* get_data */

/*
 * plugin API
 */

static int
sdb_collectd_init(sdb_object_t *user_data)
{
	user_data_t *ud;

	if (! user_data)
		return -1;

	ud = SDB_OBJ_WRAPPER(user_data)->data;
	if (sdb_unixsock_client_connect(ud->client)) {
		sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: "
				"Failed to connect to collectd.");
		return -1;
	}

	sdb_log(SDB_LOG_INFO, "collectd::unixsock backend: Successfully "
			"connected to collectd @ %s.",
			sdb_unixsock_client_path(ud->client));
	return 0;
} /* sdb_collectd_init */

static int
sdb_collectd_collect(sdb_object_t *user_data)
{
	user_data_t *ud;

	char  buffer[1024];
	char *line;
	char *msg;

	char *endptr = NULL;
	long int count;

	state_t state = STATE_INIT;
	sdb_object_wrapper_t state_obj = SDB_OBJECT_WRAPPER_STATIC(&state);

	if (! user_data)
		return -1;

	ud = SDB_OBJ_WRAPPER(user_data)->data;
	state.ud = ud;

	if (sdb_unixsock_client_send(ud->client, "LISTVAL") <= 0) {
		sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: Failed to send "
				"LISTVAL command to collectd @ %s.",
				sdb_unixsock_client_path(ud->client));
		return -1;
	}

	line = sdb_unixsock_client_recv(ud->client, buffer, sizeof(buffer));
	if (! line) {
		sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: Failed to read "
				"status of LISTVAL command from collectd @ %s.",
				sdb_unixsock_client_path(ud->client));
		return -1;
	}

	msg = strchr(line, ' ');
	if (msg) {
		*msg = '\0';
		++msg;
	}

	errno = 0;
	count = strtol(line, &endptr, /* base */ 0);
	if (errno || (line == endptr)) {
		sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: Failed to parse "
				"status of LISTVAL command from collectd @ %s.",
				sdb_unixsock_client_path(ud->client));
		return -1;
	}

	if (count < 0) {
		sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: Failed to get "
				"value list from collectd @ %s: %s",
				sdb_unixsock_client_path(ud->client),
				msg ? msg : line);
		return -1;
	}

	if (sdb_unixsock_client_process_lines(ud->client, get_data,
				SDB_OBJ(&state_obj), count, /* delim */ "/",
				/* column count = */ 3,
				SDB_TYPE_STRING, SDB_TYPE_STRING, SDB_TYPE_STRING)) {
		sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: Failed "
				"to read response from collectd @ %s.",
				sdb_unixsock_client_path(ud->client));
		return -1;
	}

	if (state.current_host) {
		sdb_log(SDB_LOG_DEBUG, "collectd::unixsock backend: Added/updated "
				"%i metric%s (%i failed) for host '%s'.",
				state.metrics_updated, state.metrics_updated == 1 ? "" : "s",
				state.metrics_failed, state.current_host);
		free(state.current_host);
	}
	return 0;
} /* collect */

static int
sdb_collectd_config_instance(oconfig_item_t *ci)
{
	char *name = NULL;
	char *socket_path = NULL;

	sdb_object_t *user_data;
	user_data_t *ud;

	int i;

	if (oconfig_get_string(ci, &name)) {
		sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: Instance requires "
				"a single string argument\n\tUsage: <Instance NAME>");
		return -1;
	}

	ud = calloc(1, sizeof(*ud));
	if (! ud) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: Failed to "
				"allocate user-data object: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return -1;
	}

	for (i = 0; i < ci->children_num; ++i) {
		oconfig_item_t *child = ci->children + i;

		if (! strcasecmp(child->key, "Socket"))
			oconfig_get_string(child, &socket_path);
		else if (! strcasecmp(child->key, "TimeseriesBackend"))
			oconfig_get_string(child, &ud->ts_type);
		else if (! strcasecmp(child->key, "TimeseriesBaseURL"))
			oconfig_get_string(child, &ud->ts_base);
		else
			sdb_log(SDB_LOG_WARNING, "collectd::unixsock backend: Ignoring "
					"unknown config option '%s' inside <Instance %s>.",
					child->key, name);
	}

	if ((ud->ts_type == NULL) != (ud->ts_base == NULL)) {
		sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: Both options, "
				"TimeseriesBackend and TimeseriesBaseURL, have to be "
				"specified.");
		ud->ts_type = ud->ts_base = NULL;
		user_data_destroy(ud);
		return -1;
	}

	if (ud->ts_type) {
		/* TODO: add support for other backend types
		 * -> will require different ID generation */
		if (strcasecmp(ud->ts_type, "rrdtool")
				&& strcasecmp(ud->ts_type, "rrdcached")) {
			sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: "
					"TimeseriesBackend '%s' is not supported - "
					"use 'rrdtool' instead.", ud->ts_type);
			ud->ts_type = ud->ts_base = NULL;
			user_data_destroy(ud);
			return -1;
		}

		ud->ts_type = strdup(ud->ts_type);
		ud->ts_base = strdup(ud->ts_base);
		if ((! ud->ts_type) || (! ud->ts_base)) {
			sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: Failed "
					"to duplicate a string");
			user_data_destroy(ud);
			return -1;
		}
	}

	if (! socket_path) {
		sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: Instance '%s' "
				"missing the 'Socket' option.", name);
		user_data_destroy(ud);
		return -1;
	}

	ud->client = sdb_unixsock_client_create(socket_path);
	if (! ud->client) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: Failed to create "
				"unixsock client: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		user_data_destroy(ud);
		return -1;
	}

	user_data = sdb_object_create_wrapper("collectd-userdata", ud,
			user_data_destroy);
	if (! user_data) {
		sdb_log(SDB_LOG_ERR, "collectd::unixsock backend: Failed to allocate "
				"user-data wrapper object");
		user_data_destroy(ud);
		return -1;
	}

	sdb_plugin_register_init(name, sdb_collectd_init, user_data);
	sdb_plugin_register_collector(name, sdb_collectd_collect,
			/* interval */ NULL, user_data);

	/* pass control to the list */
	sdb_object_deref(user_data);
	return 0;
} /* sdb_collectd_config_instance */

static int
sdb_collectd_config(oconfig_item_t *ci)
{
	int i;

	if (! ci) /* nothing to do to deconfigure this plugin */
		return 0;

	for (i = 0; i < ci->children_num; ++i) {
		oconfig_item_t *child = ci->children + i;

		if (! strcasecmp(child->key, "Instance"))
			sdb_collectd_config_instance(child);
		else
			sdb_log(SDB_LOG_WARNING, "collectd::unixsock backend: Ignoring "
					"unknown config option '%s'.", child->key);
	}
	return 0;
} /* sdb_collectd_config */

int
sdb_module_init(sdb_plugin_info_t *info)
{
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_DESC,
			"backend accessing the system statistics collection daemon "
			"throught the UNIXSOCK interface");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_COPYRIGHT,
			"Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_LICENSE, "BSD");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_VERSION, SDB_VERSION);
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_PLUGIN_VERSION, SDB_VERSION);

	sdb_plugin_register_config(sdb_collectd_config);
	return 0;
} /* sdb_version_extra */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

