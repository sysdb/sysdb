/*
 * SysDB - src/backend/puppet-storeconfigs.c
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
#include "utils/dbi.h"
#include "utils/string.h"

#include "liboconfig/utils.h"

#include <dbi/dbi.h>

#include <assert.h>
#include <errno.h>

#include <string.h>
#include <strings.h>

SDB_PLUGIN_MAGIC;

/*
 * private helper functions
 */

static int
sdb_puppet_stcfg_get_hosts(sdb_dbi_client_t __attribute__((unused)) *client,
		size_t n, sdb_data_t *data,
		sdb_object_t __attribute__((unused)) *user_data)
{
	sdb_host_t host = SDB_HOST_INIT;

	int status;

	assert(n == 2);
	assert((data[0].type == SDB_TYPE_STRING)
			&& (data[1].type == SDB_TYPE_DATETIME));

	host.host_name = strdup(data[0].data.string);
	host.host_last_update = data[1].data.datetime;

	status = sdb_store_host(&host);

	if (status < 0) {
		fprintf(stderr, "puppet storeconfigs backend: Failed to store/update "
				"host '%s'.\n", host.host_name);
		free(host.host_name);
		return -1;
	}
	else if (! status)
		fprintf(stderr, "puppet storeconfigs backend: Added/updated host '%s' "
				"(last update timestamp = %"PRIscTIME").\n",
				host.host_name, host.host_last_update);
	free(host.host_name);
	return 0;
} /* sdb_puppet_stcfg_get_hosts */

static int
sdb_puppet_stcfg_get_attrs(sdb_dbi_client_t __attribute__((unused)) *client,
		size_t n, sdb_data_t *data,
		sdb_object_t __attribute__((unused)) *user_data)
{
	sdb_attribute_t attr = SDB_ATTR_INIT;

	int status;

	assert(n == 4);
	assert((data[0].type == SDB_TYPE_STRING)
			&& (data[1].type == SDB_TYPE_STRING)
			&& (data[2].type == SDB_TYPE_STRING)
			&& (data[3].type == SDB_TYPE_DATETIME));

	attr.hostname = strdup(data[0].data.string);
	attr.attr_name = strdup(data[1].data.string);
	attr.attr_value = strdup(data[2].data.string);
	attr.attr_last_update = data[3].data.datetime;

	status = sdb_store_attribute(&attr);

	if (status < 0) {
		fprintf(stderr, "puppet storeconfigs backend: Failed to store/update "
				"host attribute '%s' for host '%s'.\n", attr.attr_name,
				attr.hostname);
		free(attr.hostname);
		free(attr.attr_name);
		free(attr.attr_value);
		return -1;
	}

	free(attr.hostname);
	free(attr.attr_name);
	free(attr.attr_value);
	return 0;
} /* sdb_puppet_stcfg_get_attrs */

/*
 * plugin API
 */

static int
sdb_puppet_stcfg_init(sdb_object_t *user_data)
{
	sdb_dbi_client_t *client;

	if (! user_data)
		return -1;

	client = SDB_OBJ_WRAPPER(user_data)->data;
	if (sdb_dbi_client_connect(client)) {
		fprintf(stderr, "puppet storeconfigs backend: "
				"Failed to connect to the storeconfigs DB.\n");
		return -1;
	}

	fprintf(stderr, "puppet storeconfigs backend: Successfully "
			"connected to the storeconfigs DB.\n");
	return 0;
} /* sdb_puppet_stcfg_init */

static int
sdb_puppet_stcfg_collect(sdb_object_t *user_data)
{
	sdb_dbi_client_t *client;

	if (! user_data)
		return -1;

	client = SDB_OBJ_WRAPPER(user_data)->data;
	if (sdb_dbi_client_check_conn(client)) {
		fprintf(stderr, "puppet storeconfigs backend: "
				"Connection to storeconfigs DB failed.\n");
		return -1;
	}

	if (sdb_dbi_exec_query(client, "SELECT name, updated_at FROM hosts;",
				sdb_puppet_stcfg_get_hosts, NULL, /* #columns = */ 2,
				/* col types = */ SDB_TYPE_STRING, SDB_TYPE_DATETIME)) {
		fprintf(stderr, "puppet storeconfigs backend: Failed to retrieve "
				"hosts from the storeconfigs DB.\n");
		return -1;
	}

	if (sdb_dbi_exec_query(client, "SELECT "
					"hosts.name AS hostname, "
					"fact_names.name AS name, "
					"fact_values.value AS value, "
					"fact_values.updated_at AS updated_at "
				"FROM fact_values "
				"INNER JOIN hosts "
					"ON fact_values.host_id = hosts.id "
				"INNER JOIN fact_names "
					"ON fact_values.fact_name_id = fact_names.id;",
				sdb_puppet_stcfg_get_attrs, NULL, /* #columns = */ 4,
				/* col types = */ SDB_TYPE_STRING, SDB_TYPE_STRING,
				SDB_TYPE_STRING, SDB_TYPE_DATETIME)) {
		fprintf(stderr, "puppet storeconfigs backend: Failed to retrieve "
				"host attributes from the storeconfigs DB.\n");
		return -1;
	}
	return 0;
} /* sdb_collectd_collect */

static int
sdb_puppet_stcfg_config_conn(oconfig_item_t *ci)
{
	char *name = NULL;
	char cb_name[1024];

	sdb_object_t *user_data;
	sdb_dbi_client_t *client;
	sdb_dbi_options_t *options = NULL;

	char *driver = NULL;
	char *database = NULL;

	int i;

	if (oconfig_get_string(ci, &name)) {
		fprintf(stderr, "puppet storeconfigs backend: Connection requires a "
				"single string argument\n\tUsage: <Connection NAME>\n");
		return -1;
	}

	for (i = 0; i < ci->children_num; ++i) {
		oconfig_item_t *child = ci->children + i;
		char *key = NULL, *value = NULL;

		int status = 0;

		if (! strcasecmp(child->key, "DBAdapter")) {
			if (oconfig_get_string(child, &driver)) {
				fprintf(stderr, "puppet storeconfigs backend: DBAdapter "
						"requires a single string argument inside "
						"<Connection %s>\n\tUsage: DBAdapter NAME\n",
						name);
			}
			continue;
		}
		else if (! strcasecmp(child->key, "DBName")) {
			if (oconfig_get_string(child, &database)) {
				fprintf(stderr, "puppet storeconfigs backend: DBName"
						"requires a single string argument inside "
						"<Connection %s>\n\tUsage: DBName NAME\n",
						name);
			}
			continue;
		}
		else if (! strcasecmp(child->key, "DBServer")) {
			status = oconfig_get_string(child, &value);
			key = "host";
		}
		else if (! strcasecmp(child->key, "DBPort")) {
			status = oconfig_get_string(child, &value);
			key = "port";
		}
		else if (! strcasecmp(child->key, "DBUser")) {
			status = oconfig_get_string(child, &value);
			key = "username";
		}
		else if (! strcasecmp(child->key, "DBPassword")) {
			status = oconfig_get_string(child, &value);
			key = "password";
		}
		else if (! strcasecmp(child->key, "DBIOption")) {
			if ((child->values_num != 2)
					|| (child->values[0].type != OCONFIG_TYPE_STRING)
					|| (child->values[1].type != OCONFIG_TYPE_STRING)) {
				fprintf(stderr, "puppet storeconfigs backend: DBIOption "
						"requires exactly two string arguments inside "
						"<Connection %s>\n\tUsage: DBIOption KEY VALUE\n",
						name);
				continue;
			}

			status = 0;
			key = child->values[0].value.string;
			value = child->values[1].value.string;
		}
		else {
			fprintf(stderr, "puppet storeconfigs backend: Ignoring unknown "
					"config option '%s' inside <Connection %s>.\n",
					child->key, name);
			continue;
		}

		if (status) {
			fprintf(stderr, "puppet storeconfigs backend: Option '%s' "
					"requires a single string argument inside "
					"<Connection %s>\n\tUsage: DBAdapter NAME\n",
					child->key, name);
			continue;
		}

		assert(key && value);

		if (! options) {
			if (! (options = sdb_dbi_options_create())) {
				char errmsg[1024];
				fprintf(stderr, "puppet storeconfigs backend: Failed to "
						"create DBI options object: %s\n",
						sdb_strerror(errno, errmsg, sizeof(errmsg)));
				continue;
			}
		}

		if (sdb_dbi_options_add(options, key, value)) {
			char errmsg[1024];
			fprintf(stderr, "puppet storeconfigs backend: Failed to add "
					"option '%s': %s\n", key,
					sdb_strerror(errno, errmsg, sizeof(errmsg)));
			continue;
		}
	}

	if (! driver) {
		fprintf(stderr, "puppet storeconfigs backend: Connection '%s' "
				"missing the 'DBAdapter' option.\n", name);
		return -1;
	}
	if (! database) {
		fprintf(stderr, "puppet storeconfigs backend: Connection '%s' "
				"missing the 'DBName' option.\n", name);
		return -1;
	}

	snprintf(cb_name, sizeof(cb_name), "puppet-storeconfigs-%s", name);
	cb_name[sizeof(cb_name) - 1] = '\0';

	client = sdb_dbi_client_create(driver, database);
	if (! client) {
		char errbuf[1024];
		fprintf(stderr, "puppet storeconfigs backend: "
				"Failed to create DBI client: %s\n",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return -1;
	}

	sdb_dbi_client_set_options(client, options);

	user_data = sdb_object_create_wrapper(client,
			(void (*)(void *))sdb_dbi_client_destroy);
	if (! user_data) {
		sdb_dbi_client_destroy(client);
		fprintf(stderr, "puppet storeconfigs backend: "
				"Failed to allocate sdb_object_t\n");
		return -1;
	}

	sdb_plugin_register_init(cb_name, sdb_puppet_stcfg_init, user_data);
	sdb_plugin_register_collector(cb_name, sdb_puppet_stcfg_collect,
			/* interval */ NULL, user_data);

	/* pass control to the list */
	sdb_object_deref(user_data);
	return 0;
} /* sdb_puppet_stcfg_config_conn */

static int
sdb_puppet_stcfg_config(oconfig_item_t *ci)
{
	int i;

	for (i = 0; i < ci->children_num; ++i) {
		oconfig_item_t *child = ci->children + i;

		if (! strcasecmp(child->key, "Connection"))
			sdb_puppet_stcfg_config_conn(child);
		else
			fprintf(stderr, "puppet storeconfigs backend: Ignoring unknown "
					"config option '%s'.\n", child->key);
	}
	return 0;
} /* sdb_puppet_stcfg_config */

int
sdb_module_init(sdb_plugin_info_t *info)
{
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_NAME, "puppet-storeconfigs");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_DESC,
			"backend accessing the Puppet stored configuration database");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_COPYRIGHT,
			"Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_LICENSE, "BSD");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_VERSION, SDB_VERSION);
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_PLUGIN_VERSION, SDB_VERSION);

	if (dbi_initialize(/* driver dir = */ NULL) < 0) {
		fprintf(stderr, "puppet storeconfigs backend: failed to initialize "
				"DBI; possibly you don't have any drivers installed.\n");
		return -1;
	}

	sdb_plugin_register_config("puppet-storeconfigs",
			sdb_puppet_stcfg_config);
	return 0;
} /* sdb_version_extra */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

