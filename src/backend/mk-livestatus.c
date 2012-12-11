/*
 * syscollector - src/backend/mk-livestatus.c
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

#include "syscollector.h"
#include "core/plugin.h"
#include "core/store.h"
#include "utils/string.h"
#include "utils/unixsock.h"

#include "liboconfig/utils.h"

#include <assert.h>

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SC_PLUGIN_MAGIC;

/*
 * private helper functions
 */

static int
sc_livestatus_parse_line(char *line,
		char **host, sc_time_t *timestamp)
{
	char *timestamp_str;
	double timestamp_dbl;

	char *hostname;

	char *endptr = NULL;

	hostname = line;
	timestamp_str = strchr(hostname, ';');

	if (! timestamp_str) {
		fprintf(stderr, "MK Livestatus backend: Failed to find timestamp "
				"in 'GET hosts' output, line: %s\n", line);
		return -1;
	}

	*timestamp_str = '\0';
	++timestamp_str;

	errno = 0;
	timestamp_dbl = strtod(timestamp_str, &endptr);
	if (errno || (timestamp_str == endptr)) {
		char errbuf[1024];
		fprintf(stderr, "MK Livestatus backend: Failed to "
				"parse timestamp (%s): %s\n", timestamp_str,
				sc_strerror(errno, errbuf, sizeof(errbuf)));
		return -1;
	}
	if (endptr && (*endptr != '\0'))
		fprintf(stderr, "MK Livestatus backend: Ignoring garbage "
				"after number when parsing timestamp: %s.\n",
				endptr);

	*timestamp = DOUBLE_TO_SC_TIME(timestamp_dbl);
	*host = hostname;
	return 0;
} /* sc_livestatus_parse_line */

/*
 * plugin API
 */

static int
sc_livestatus_init(sc_object_t *user_data)
{
	sc_unixsock_client_t *client;

	if (! user_data)
		return -1;

	client = SC_OBJ_WRAPPER(user_data)->data;
	if (sc_unixsock_client_connect(client)) {
		fprintf(stderr, "MK Livestatus backend: "
				"Failed to connect to livestatus @ %s.\n",
				sc_unixsock_client_path(client));
		return -1;
	}

	fprintf(stderr, "MK Livestatus backend: Successfully "
			"connected to livestatus @ %s.\n",
			sc_unixsock_client_path(client));
	return 0;
} /* sc_livestatus_init */

static int
sc_livestatus_collect(sc_object_t *user_data)
{
	sc_unixsock_client_t *client;

	int status;

	if (! user_data)
		return -1;

	client = SC_OBJ_WRAPPER(user_data)->data;

	status = sc_unixsock_client_send(client, "GET hosts\r\n"
			"Columns: name last_check");
	if (status <= 0) {
		fprintf(stderr, "MK Livestatus backend: Failed to send "
				"'GET hosts' command to livestatus @ %s.\n",
				sc_unixsock_client_path(client));
		return -1;
	}

	sc_unixsock_client_shutdown(client, SHUT_WR);

	while (42) {
		char  buffer[1024];
		char *line;

		char *hostname = NULL;
		sc_time_t timestamp = 0;

		sc_host_t host = SC_HOST_INIT;

		sc_unixsock_client_clearerr(client);
		line = sc_unixsock_client_recv(client, buffer, sizeof(buffer));

		if (! line)
			break;

		if (sc_livestatus_parse_line(line, &hostname, &timestamp)) {
			fprintf(stderr, "MK Livestatus backend: Failed to parse "
					"hosts line: %s\n", line);
			continue;
		}

		host.host_name = hostname;
		host.host_last_update = timestamp;

		status = sc_store_host(&host);

		if (status < 0) {
			fprintf(stderr, "MK Livestatus backend: Failed to store/update "
					"host '%s'.\n", hostname);
			continue;
		}
		else if (status > 0) /* value too old */
			continue;

		fprintf(stderr, "MK Livestatus backend: Added/updated host '%s' "
				"(last update timestamp = %"PRIscTIME").\n",
				hostname, timestamp);
	}

	if ((! sc_unixsock_client_eof(client))
			|| sc_unixsock_client_error(client)) {
		char errbuf[1024];
		fprintf(stderr, "MK Livestatus backend: Failed to read host "
				"from livestatus @ %s: %s\n",
				sc_unixsock_client_path(client),
				sc_strerror(errno, errbuf, sizeof(errbuf)));
		return -1;
	}
	return 0;
} /* sc_livestatus_collect */

static int
sc_livestatus_config_instance(oconfig_item_t *ci)
{
	char *name = NULL;
	char *socket = NULL;

	char cb_name[1024];

	sc_object_t *user_data;
	sc_unixsock_client_t *client;

	int i;

	if (oconfig_get_string(ci, &name)) {
		fprintf(stderr, "MK Livestatus backend: Instance requires a single "
				"string argument\n\tUsage: <Instance NAME>\n");
		return -1;
	}

	for (i = 0; i < ci->children_num; ++i) {
		oconfig_item_t *child = ci->children + i;

		if (! strcasecmp(child->key, "Socket"))
			oconfig_get_string(child, &socket);
		else
			fprintf(stderr, "MK Livestatus backend: Ignoring unknown config "
					"option '%s' inside <Instance %s>.\n",
					child->key, name);
	}

	if (! socket) {
		fprintf(stderr, "MK Livestatus backend: Instance '%s' missing "
				"the 'Socket' option.\n", name);
		return -1;
	}

	snprintf(cb_name, sizeof(cb_name), "mk-livestatus-%s", name);
	cb_name[sizeof(cb_name) - 1] = '\0';

	client = sc_unixsock_client_create(socket);
	if (! client) {
		char errbuf[1024];
		fprintf(stderr, "MK Livestatus backend: Failed to create unixsock "
				"client: %s\n", sc_strerror(errno, errbuf, sizeof(errbuf)));
		return -1;
	}

	user_data = sc_object_create_wrapper(client,
			(void (*)(void *))sc_unixsock_client_destroy);
	if (! user_data) {
		sc_unixsock_client_destroy(client);
		fprintf(stderr, "MK Livestatus backend: Failed to "
				"allocate sc_object_t\n");
		return -1;
	}

	sc_plugin_register_init(cb_name, sc_livestatus_init, user_data);
	sc_plugin_register_collector(cb_name, sc_livestatus_collect,
			/* interval */ NULL, user_data);

	/* pass control to the list */
	sc_object_deref(user_data);
	return 0;
} /* sc_livestatus_config_instance */

static int
sc_livestatus_config(oconfig_item_t *ci)
{
	int i;

	for (i = 0; i < ci->children_num; ++i) {
		oconfig_item_t *child = ci->children + i;

		if (! strcasecmp(child->key, "Instance"))
			sc_livestatus_config_instance(child);
		else
			fprintf(stderr, "MK Livestatus backend: Ignoring unknown config "
					"option '%s'.\n", child->key);
	}
	return 0;
} /* sc_livestatus_config */

int
sc_module_init(sc_plugin_info_t *info)
{
	sc_plugin_set_info(info, SC_PLUGIN_INFO_NAME, "MK-Livestatus");
	sc_plugin_set_info(info, SC_PLUGIN_INFO_DESC,
			"backend accessing Nagios/Icinga/Shinken using MK Livestatus");
	sc_plugin_set_info(info, SC_PLUGIN_INFO_COPYRIGHT,
			"Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>");
	sc_plugin_set_info(info, SC_PLUGIN_INFO_LICENSE, "BSD");
	sc_plugin_set_info(info, SC_PLUGIN_INFO_VERSION, SC_VERSION);
	sc_plugin_set_info(info, SC_PLUGIN_INFO_PLUGIN_VERSION, SC_VERSION);

	sc_plugin_register_config("mk-livestatus", sc_livestatus_config);
	return 0;
} /* sc_version_extra */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

