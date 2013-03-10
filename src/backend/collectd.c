/*
 * SysDB - src/backend/collectd.c
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

#include "sysdb.h"
#include "core/plugin.h"
#include "core/store.h"
#include "utils/error.h"
#include "utils/string.h"
#include "utils/unixsock.h"

#include "liboconfig/utils.h"

#include <assert.h>

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SDB_PLUGIN_MAGIC;

/*
 * private data types
 */

typedef struct {
	char *current_host;
	sdb_time_t current_timestamp;
	int svc_updated;
	int svc_failed;
} sdb_collectd_state_t;
#define SDB_COLLECTD_STATE_INIT { NULL, 0, 0, 0 }

/*
 * private helper functions
 */

static int
sdb_collectd_add_host(const char *hostname, sdb_time_t last_update)
{
	sdb_host_t host = SDB_HOST_INIT;
	char name[strlen(hostname) + 1];

	int status;

	strncpy(name, hostname, sizeof(name));

	host.host_name = name;
	host.host_last_update = last_update;

	status = sdb_store_host(&host);

	if (status < 0) {
		sdb_log(SDB_LOG_ERR, "collectd backend: Failed to store/update "
				"host '%s'.\n", name);
		return -1;
	}
	else if (status > 0) /* value too old */
		return 0;

	sdb_log(SDB_LOG_DEBUG, "collectd backend: Added/updated host '%s' "
			"(last update timestamp = %"PRIscTIME").\n",
			name, last_update);
	return 0;
} /* sdb_collectd_add_host */

static int
sdb_collectd_add_svc(const char *hostname, const char *plugin,
		const char *type, sdb_time_t last_update)
{
	sdb_service_t svc = SDB_SVC_INIT;
	char host[strlen(hostname) + 1];
	char name[strlen(plugin) + strlen(type) + 2];

	int status;

	strncpy(host, hostname, sizeof(host));
	snprintf(name, sizeof(name), "%s/%s", plugin, type);

	svc.hostname = host;
	svc.svc_name = name;
	svc.svc_last_update = last_update;

	status = sdb_store_service(&svc);
	if (status < 0) {
		sdb_log(SDB_LOG_ERR, "collectd backend: Failed to store/update "
				"service '%s/%s'.\n", host, name);
		return -1;
	}
	return 0;
} /* sdb_collectd_add_svc */

static int
sdb_collectd_get_data(sdb_unixsock_client_t __attribute__((unused)) *client,
		size_t n, sdb_data_t *data, sdb_object_t *user_data)
{
	sdb_collectd_state_t *state;

	const char *hostname;
	const char *plugin;
	const char *type;
	sdb_time_t last_update;

	assert(user_data);

	assert(n == 4);
	assert((data[0].type == SDB_TYPE_DATETIME)
			&& (data[1].type == SDB_TYPE_STRING)
			&& (data[2].type == SDB_TYPE_STRING)
			&& (data[3].type == SDB_TYPE_STRING));

	last_update = data[0].data.datetime;
	hostname = data[1].data.string;
	plugin   = data[2].data.string;
	type     = data[3].data.string;

	state = SDB_OBJ_WRAPPER(user_data)->data;

	if (! state->current_host) {
		state->current_host = strdup(hostname);
		state->current_timestamp = last_update;
	}

	if (! state->current_host) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "collectd backend: Failed to allocate "
				"string buffer: %s\n",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return -1;
	}

	if (! sdb_store_get_host(hostname))
		sdb_collectd_add_host(hostname, last_update);

	if (sdb_collectd_add_svc(hostname, plugin, type, last_update))
		++state->svc_failed;
	else
		++state->svc_updated;

	if (! strcasecmp(state->current_host, hostname)) {
		if (last_update > state->current_timestamp)
			state->current_timestamp = last_update;
		return 0;
	}

	/* new host */
	sdb_collectd_add_host(hostname, last_update);

	sdb_log(SDB_LOG_DEBUG, "collectd backend: Added/updated "
			"%i service%s (%i failed) for host '%s'.\n",
			state->svc_updated, state->svc_updated == 1 ? "" : "s",
			state->svc_failed, state->current_host);
	state->svc_updated = state->svc_failed = 0;

	free(state->current_host);
	state->current_host = strdup(hostname);
	state->current_timestamp = last_update;
	return 0;
} /* sdb_collectd_get_data */

/*
 * plugin API
 */

static int
sdb_collectd_init(sdb_object_t *user_data)
{
	sdb_unixsock_client_t *client;

	if (! user_data)
		return -1;

	client = SDB_OBJ_WRAPPER(user_data)->data;
	if (sdb_unixsock_client_connect(client)) {
		sdb_log(SDB_LOG_ERR, "collectd backend: "
				"Failed to connect to collectd.\n");
		return -1;
	}

	sdb_log(SDB_LOG_INFO, "collectd backend: Successfully "
			"connected to collectd @ %s.\n",
			sdb_unixsock_client_path(client));
	return 0;
} /* sdb_collectd_init */

static int
sdb_collectd_shutdown(__attribute__((unused)) sdb_object_t *user_data)
{
	return 0;
} /* sdb_collectd_shutdown */

static int
sdb_collectd_collect(sdb_object_t *user_data)
{
	sdb_unixsock_client_t *client;

	char  buffer[1024];
	char *line;
	char *msg;

	char *endptr = NULL;
	long int count;

	sdb_collectd_state_t state = SDB_COLLECTD_STATE_INIT;
	sdb_object_wrapper_t state_obj = SDB_OBJECT_WRAPPER_STATIC(&state,
			/* destructor = */ NULL);

	if (! user_data)
		return -1;

	client = SDB_OBJ_WRAPPER(user_data)->data;

	if (sdb_unixsock_client_send(client, "LISTVAL") <= 0) {
		sdb_log(SDB_LOG_ERR, "collectd backend: Failed to send LISTVAL "
				"command to collectd @ %s.\n",
				sdb_unixsock_client_path(client));
		return -1;
	}

	line = sdb_unixsock_client_recv(client, buffer, sizeof(buffer));
	if (! line) {
		sdb_log(SDB_LOG_ERR, "collectd backend: Failed to read status "
				"of LISTVAL command from collectd @ %s.\n",
				sdb_unixsock_client_path(client));
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
		sdb_log(SDB_LOG_ERR, "collectd backend: Failed to parse status "
				"of LISTVAL command from collectd @ %s.\n",
				sdb_unixsock_client_path(client));
		return -1;
	}

	if (count < 0) {
		sdb_log(SDB_LOG_ERR, "collectd backend: Failed to get value "
				"list from collectd @ %s: %s\n",
				sdb_unixsock_client_path(client),
				msg ? msg : line);
		return -1;
	}

	if (sdb_unixsock_client_process_lines(client, sdb_collectd_get_data,
				SDB_OBJ(&state_obj), count, /* delim */ " /",
				/* column count = */ 4,
				SDB_TYPE_DATETIME, SDB_TYPE_STRING,
				SDB_TYPE_STRING, SDB_TYPE_STRING)) {
		sdb_log(SDB_LOG_ERR, "collectd backend: Failed to read response "
				"from collectd @ %s.\n", sdb_unixsock_client_path(client));
		return -1;
	}

	if (state.current_host) {
		sdb_collectd_add_host(state.current_host, state.current_timestamp);
		sdb_log(SDB_LOG_DEBUG, "collectd backend: Added/updated "
				"%i service%s (%i failed) for host '%s'.\n",
				state.svc_updated, state.svc_updated == 1 ? "" : "s",
				state.svc_failed, state.current_host);
	}
	return 0;
} /* sdb_collectd_collect */

static int
sdb_collectd_config_instance(oconfig_item_t *ci)
{
	char *name = NULL;
	char *socket_path = NULL;

	char cb_name[1024];

	sdb_object_t *user_data;
	sdb_unixsock_client_t *client;

	int i;

	if (oconfig_get_string(ci, &name)) {
		sdb_log(SDB_LOG_ERR, "collectd backend: Instance requires a "
				"single string argument\n\tUsage: <Instance NAME>\n");
		return -1;
	}

	for (i = 0; i < ci->children_num; ++i) {
		oconfig_item_t *child = ci->children + i;

		if (! strcasecmp(child->key, "Socket"))
			oconfig_get_string(child, &socket_path);
		else
			sdb_log(SDB_LOG_WARNING, "collectd backend: Ignoring "
					"unknown config option '%s' inside <Instance %s>.\n",
					child->key, name);
	}

	if (! socket_path) {
		sdb_log(SDB_LOG_ERR, "collectd backend: Instance '%s' missing "
				"the 'Socket' option.\n", name);
		return -1;
	}

	snprintf(cb_name, sizeof(cb_name), "collectd-%s", name);
	cb_name[sizeof(cb_name) - 1] = '\0';

	client = sdb_unixsock_client_create(socket_path);
	if (! client) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "collectd backend: Failed to create "
				"unixsock client: %s\n",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return -1;
	}

	user_data = sdb_object_create_wrapper(client,
			(void (*)(void *))sdb_unixsock_client_destroy);
	if (! user_data) {
		sdb_unixsock_client_destroy(client);
		sdb_log(SDB_LOG_ERR, "collectd backend: Failed to allocate "
				"sdb_object_t\n");
		return -1;
	}

	sdb_plugin_register_init(cb_name, sdb_collectd_init, user_data);
	sdb_plugin_register_shutdown(cb_name, sdb_collectd_shutdown, user_data);

	sdb_plugin_register_collector(cb_name, sdb_collectd_collect,
			/* interval */ NULL, user_data);

	/* pass control to the list */
	sdb_object_deref(user_data);
	return 0;
} /* sdb_collectd_config_instance */

static int
sdb_collectd_config(oconfig_item_t *ci)
{
	int i;

	for (i = 0; i < ci->children_num; ++i) {
		oconfig_item_t *child = ci->children + i;

		if (! strcasecmp(child->key, "Instance"))
			sdb_collectd_config_instance(child);
		else
			sdb_log(SDB_LOG_WARNING, "collectd backend: Ignoring "
					"unknown config option '%s'.\n", child->key);
	}
	return 0;
} /* sdb_collectd_config */

int
sdb_module_init(sdb_plugin_info_t *info)
{
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_NAME, "collectd");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_DESC,
			"backend accessing the system statistics collection daemon");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_COPYRIGHT,
			"Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_LICENSE, "BSD");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_VERSION, SDB_VERSION);
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_PLUGIN_VERSION, SDB_VERSION);

	sdb_plugin_register_config("collectd", sdb_collectd_config);
	return 0;
} /* sdb_version_extra */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

