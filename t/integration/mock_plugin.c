/*
 * SysDB - t/integration/mock_plugin.c
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
#include "core/plugin.h"
#include "core/store.h"
#include "utils/error.h"

#include "liboconfig/utils.h"

#include <stdlib.h>
#include <string.h>

#define MAGIC_DATA (void *)0x4711

SDB_PLUGIN_MAGIC;

static const char *hostnames[] = {
	"some.host.name",
	"other.host.name",
	"host1.example.com",
	"host2.example.com",
	"localhost",
};

static struct {
	const char *hostname;
	const char *metric;
	sdb_metric_store_t store;
} metrics[] = {
	{ "some.host.name", "foo/bar/qux",
		{ "dummy", "/var/lib/collectd/rrd/foo/bar/qux.rrd" } },
	{ "some.host.name", "foo/bar/baz",
		{ "dummy", "/var/lib/collectd/rrd/foo/bar/baz.rrd" } },
	{ "some.host.name", "foo2/bar/qux",
		{ "dummy", "/var/lib/collectd/rrd/foo2/bar/qux.rrd" } },
	{ "some.host.name", "foo2/bar/baz",
		{ "dummy", "/var/lib/collectd/rrd/foo2/bar/baz.rrd" } },
	{ "other.host.name", "foo/bar/qux",
		{ "dummy", "/var/lib/collectd/rrd/foo/bar/qux.rrd" } },
	{ "other.host.name", "foo/bar/baz",
		{ "dummy", "/var/lib/collectd/rrd/foo/bar/baz.rrd" } },
	{ "other.host.name", "foo2/bar/qux",
		{ "dummy", "/var/lib/collectd/rrd/foo2/bar/qux.rrd" } },
	{ "other.host.name", "foo2/bar/baz",
		{ "dummy", "/var/lib/collectd/rrd/foo2/bar/baz.rrd" } },
};

static struct {
	const char *hostname;
	const char *service;
} services[] = {
	{ "some.host.name", "mock service" },
	{ "some.host.name", "other service" },
	{ "some.host.name", "database" },
	{ "host1.example.com", "mock service" },
	{ "host1.example.com", "example service one" },
	{ "host1.example.com", "example service two" },
	{ "host1.example.com", "example service three" },
	{ "host2.example.com", "mock service" },
	{ "host2.example.com", "example service one" },
	{ "host2.example.com", "example service two" },
	{ "host2.example.com", "example service three" },
	{ "localhost", "sysdbd" },
};

static struct {
	const char *hostname;
	const char *name;
	const char *value;
} attributes[] = {
	{ "other.host.name", "attribute", "value" },
	{ "other.host.name", "architecture", "varch" },
	{ "other.host.name", "processor0", "Vendor TYPE4711 CPU MAGIC" },
	{ "other.host.name", "processor1", "Vendor TYPE4711 CPU MAGIC" },
	{ "other.host.name", "processor2", "Vendor TYPE4711 CPU MAGIC" },
	{ "other.host.name", "processor3", "Vendor TYPE4711 CPU MAGIC" },
	{ "host1.example.com", "other attribute", "special value" },
	{ "host1.example.com", "architecture", "x42" },
	{ "host1.example.com", "timezone", "UTC" },
	{ "host2.example.com", "other attribute", "special value" },
	{ "host2.example.com", "architecture", "x42" },
	{ "host2.example.com", "timezone", "UTC" },
	{ "localhost", "attr1", "value1" },
	{ "localhost", "attr2", "value2" },
	{ "localhost", "attr3", "value3" },
};

/*
 * plugin API
 */

static int
mock_init(sdb_object_t *user_data)
{
	if (SDB_OBJ_WRAPPER(user_data)->data != MAGIC_DATA) {
		sdb_log(SDB_LOG_ERR, "mock::plugin: Invalid user data %p "
				"passed to init", SDB_OBJ_WRAPPER(user_data)->data);
		exit(1);
	}
	return 0;
} /* mock_init */

static int
mock_shutdown(sdb_object_t *user_data)
{
	if (SDB_OBJ_WRAPPER(user_data)->data != MAGIC_DATA) {
		sdb_log(SDB_LOG_ERR, "mock::plugin: Invalid user data %p "
				"passed to shutdown", SDB_OBJ_WRAPPER(user_data)->data);
		exit(1);
	}
	return 0;
} /* mock_shutdown */

static int
mock_collect(sdb_object_t *user_data)
{
	size_t i;
	int check;

	if (SDB_OBJ_WRAPPER(user_data)->data != MAGIC_DATA) {
		sdb_log(SDB_LOG_ERR, "mock::plugin: Invalid user data %p "
				"passed to collect", SDB_OBJ_WRAPPER(user_data)->data);
		exit(1);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(hostnames); ++i) {
		if ((check = sdb_store_host(hostnames[i], sdb_gettime()))) {
			sdb_log(SDB_LOG_ERR, "mock::plugin: Failed to store host: "
					"status %d", check);
			exit(1);
		}
	}
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(metrics); ++i) {
		if ((check = sdb_store_metric(metrics[i].hostname,
						metrics[i].metric, &metrics[i].store,
						sdb_gettime()))) {
			sdb_log(SDB_LOG_ERR, "mock::plugin: Failed to store metric: "
					"status %d", check);
			exit(1);
		}
	}
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(services); ++i) {
		if ((check = sdb_store_service(services[i].hostname,
						services[i].service, sdb_gettime()))) {
			sdb_log(SDB_LOG_ERR, "mock::plugin: Failed to store service: "
					"status %d", check);
			exit(1);
		}
	}
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(attributes); ++i) {
		sdb_data_t datum = { SDB_TYPE_STRING, { .string = NULL } };
		datum.data.string = strdup(attributes[i].value);

		if ((check = sdb_store_attribute(attributes[i].hostname,
						attributes[i].name, &datum, sdb_gettime()))) {
			sdb_log(SDB_LOG_ERR, "mock::plugin: Failed to store attribute: "
					"status %d", check);
			exit(1);
		}

		free(datum.data.string);
	}
	return 0;
} /* mock_collect */

static int
mock_config(oconfig_item_t *ci)
{
	sdb_object_t *user_data;

	int i;

	if (! ci) /* deconfiguration */
		return 0;

	for (i = 0; i < ci->children_num; ++i) {
		oconfig_item_t *child = ci->children + i;

		sdb_log(SDB_LOG_WARNING, "mock::plugin: Ignoring unknown config "
				"option '%s'", child->key);
	}

	user_data = sdb_object_create_wrapper("mock_data", MAGIC_DATA, NULL);
	if (! user_data) {
		sdb_log(SDB_LOG_ERR, "mock::plugin: Failed to allocate user data");
		exit(1);
	}

	sdb_plugin_register_init("main", mock_init, user_data);
	sdb_plugin_register_shutdown("main", mock_shutdown, user_data);
	sdb_plugin_register_collector("main", mock_collect,
			/* interval = */ NULL, user_data);

	sdb_object_deref(user_data);
	return 0;
} /* mock_config */

int
sdb_module_init(sdb_plugin_info_t *info)
{
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_DESC, "a mock plugin");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_COPYRIGHT,
			"Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_LICENSE, "BSD");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_VERSION, SDB_VERSION);
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_PLUGIN_VERSION, SDB_VERSION);

	sdb_plugin_register_config(mock_config);
	return 0;
} /* sdb_module_init */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

