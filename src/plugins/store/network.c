/*
 * SysDB - src/plugins/backend/store/network.c
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
#endif

#include "sysdb.h"
#include "core/plugin.h"
#include "client/sock.h"
#include "utils/error.h"
#include "utils/proto.h"
#include "utils/os.h"
#include "utils/ssl.h"

#include "liboconfig/utils.h"

#include <errno.h>

#include <stdlib.h>
#include <string.h>
#include <strings.h>

SDB_PLUGIN_MAGIC;

/*
 * private data types
 */

typedef struct {
	sdb_client_t *client;
	char *addr;
	char *username;
	sdb_ssl_options_t ssl_opts;
} user_data_t;
#define UD(obj) SDB_OBJ_WRAPPER(obj)->data

static void
user_data_destroy(void *obj)
{
	user_data_t *ud = obj;

	if (! ud)
		return;

	if (ud->client)
		sdb_client_destroy(ud->client);
	ud->client = NULL;
	if (ud->addr)
		free(ud->addr);
	ud->addr = NULL;
	if (ud->username)
		free(ud->username);
	ud->username = NULL;

	sdb_ssl_free_options(&ud->ssl_opts);

	free(ud);
} /* user_data_destroy */

/*
 * store writer implementation
 */

static int
store_rpc(user_data_t *ud, const char *msg, size_t msg_len)
{
	sdb_strbuf_t *buf = sdb_strbuf_create(128);
	uint32_t rstatus = 0;
	ssize_t status;

	if (sdb_client_eof(ud->client)) {
		sdb_client_close(ud->client);
		if (sdb_client_connect(ud->client, ud->username)) {
			sdb_log(SDB_LOG_ERR, "store::network: Failed to reconnect "
					"to SysDB at %s as user %s", ud->addr, ud->username);
			return -1;
		}
		sdb_log(SDB_LOG_INFO, "store::network: Successfully reconnected "
				"to SysDB at %s as user %s", ud->addr, ud->username);
	}

	status = sdb_client_rpc(ud->client, SDB_CONNECTION_STORE,
			(uint32_t)msg_len, msg, &rstatus, buf);
	if (status < 0)
		sdb_log(SDB_LOG_ERR, "store::network: %s", sdb_strbuf_string(buf));
	else if (rstatus != SDB_CONNECTION_OK) {
		sdb_log(SDB_LOG_ERR, "store::network: Failed to send object: %s",
				sdb_strbuf_string(buf));
		status = -1;
	}

	sdb_strbuf_destroy(buf);
	if (status < 0)
		return -1;
	return 0;
} /* store_rpc */

static int
store_host(sdb_store_host_t *host, sdb_object_t *user_data)
{
	sdb_proto_host_t h = { host->last_update, host->name };
	size_t len = sdb_proto_marshal_host(NULL, 0, &h);
	char buf[len];

	sdb_proto_marshal_host(buf, len, &h);
	return store_rpc(UD(user_data), buf, len);
} /* store_host */

static int
store_service(sdb_store_service_t *service, sdb_object_t *user_data)
{
	sdb_proto_service_t s = {
		service->last_update, service->hostname, service->name,
	};
	ssize_t len = sdb_proto_marshal_service(NULL, 0, &s);
	char buf[len];

	sdb_proto_marshal_service(buf, len, &s);
	return store_rpc(UD(user_data), buf, len);
} /* store_service */

static int
store_metric(sdb_store_metric_t *metric, sdb_object_t *user_data)
{
	sdb_proto_metric_t m = {
		metric->last_update, metric->hostname, metric->name,
		metric->store.type, metric->store.id, metric->store.last_update,
	};
	size_t len = sdb_proto_marshal_metric(NULL, 0, &m);
	char buf[len];

	sdb_proto_marshal_metric(buf, len, &m);
	return store_rpc(UD(user_data), buf, len);
} /* store_metric */

static int
store_attr(sdb_store_attribute_t *attr, sdb_object_t *user_data)
{
	sdb_proto_attribute_t a = {
		attr->last_update, attr->parent_type, attr->hostname, attr->parent,
		attr->key, attr->value,
	};
	size_t len = sdb_proto_marshal_attribute(NULL, 0, &a);
	char buf[len];

	sdb_proto_marshal_attribute(buf, len, &a);
	return store_rpc(UD(user_data), buf, len);
} /* store_attr */

static sdb_store_writer_t store_impl = {
	store_host, store_service, store_metric, store_attr,
};

/*
 * plugin API
 */

static int
store_init(sdb_object_t *user_data)
{
	user_data_t *ud;

	if (! user_data)
		return -1;

	ud = SDB_OBJ_WRAPPER(user_data)->data;
	if (sdb_client_connect(ud->client, ud->username)) {
		sdb_log(SDB_LOG_ERR, "store::network: Failed to connect "
				"to SysDB at %s as user %s", ud->addr, ud->username);
		return -1;
	}

	sdb_log(SDB_LOG_INFO, "store::network: Successfully connected "
			"to SysDB at %s as user %s", ud->addr, ud->username);
	return 0;
} /* store_init */

static int
store_config_server(oconfig_item_t *ci)
{
	sdb_object_t *user_data;
	user_data_t *ud;
	int ret = 0;
	int i;

	ud = calloc(1, sizeof(*ud));
	if (! ud) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "store::network: Failed to allocate "
				"a user-data object: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return -1;
	}

	if (oconfig_get_string(ci, &ud->addr)) {
		sdb_log(SDB_LOG_ERR, "store::network: Server requires "
				"a single string argument\n\tUsage: <Server ADDRESS>");
		user_data_destroy(ud);
		return -1;
	}
	ud->addr = strdup(ud->addr);
	if (! ud->addr) {
		sdb_log(SDB_LOG_ERR, "store::network: Failed to duplicate "
				"a string");
		user_data_destroy(ud);
		return -1;
	}

	ud->client = sdb_client_create(ud->addr);
	if (! ud->client) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "store::network: Failed to create client "
				"connecting to '%s': %s", ud->addr,
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		user_data_destroy(ud);
		return -1;
	}

	for (i = 0; i < ci->children_num; ++i) {
		oconfig_item_t *child = ci->children + i;
		char *tmp = NULL;

		if (! strcasecmp(child->key, "Username")) {
			if (oconfig_get_string(child, &tmp)) {
				ret = -1;
				break;
			}
			ud->username = strdup(tmp);
		}
		else if (! strcasecmp(child->key, "SSLCertificate")) {
			if (oconfig_get_string(child, &tmp)) {
				ret = -1;
				break;
			}
			ud->ssl_opts.cert_file = strdup(tmp);
		}
		else if (! strcasecmp(child->key, "SSLCertificateKey")) {
			if (oconfig_get_string(child, &tmp)) {
				ret = -1;
				break;
			}
			ud->ssl_opts.key_file = strdup(tmp);
		}
		else if (! strcasecmp(child->key, "SSLCACertificates")) {
			if (oconfig_get_string(child, &tmp)) {
				ret = -1;
				break;
			}
			ud->ssl_opts.ca_file = strdup(tmp);
		}
		else
			sdb_log(SDB_LOG_WARNING, "store::network: Ignoring "
					"unknown config option '%s' inside <Server %s>.",
					child->key, ud->addr);
	}

	if (ret) {
		user_data_destroy(ud);
		return ret;
	}
	if (! ud->username)
		ud->username = sdb_get_current_user();

	if (sdb_client_set_ssl_options(ud->client, &ud->ssl_opts)) {
		sdb_log(SDB_LOG_ERR, "store::network: Failed to apply SSL options");
		user_data_destroy(ud);
		return -1;
	}

	user_data = sdb_object_create_wrapper("store-network-userdata", ud,
			user_data_destroy);
	if (! user_data) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "store::network: Failed to allocate "
				"a user-data wrapper object: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		user_data_destroy(ud);
		return -1;
	}

	sdb_plugin_register_init(ud->addr, store_init, user_data);
	sdb_plugin_register_writer(ud->addr, &store_impl, user_data);
	sdb_object_deref(user_data);
	return 0;
} /* store_config_server */

static int
store_config(oconfig_item_t *ci)
{
	int i;

	if (! ci) /* nothing to do to deconfigure this plugin */
		return 0;

	for (i = 0; i < ci->children_num; ++i) {
		oconfig_item_t *child = ci->children + i;

		if (! strcasecmp(child->key, "Server"))
			store_config_server(child);
		else
			sdb_log(SDB_LOG_WARNING, "store::network: Ignoring "
					"unknown config option '%s'.", child->key);
	}
	return 0;
} /* store_config */

int
sdb_module_init(sdb_plugin_info_t *info)
{
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_DESC,
			"send stored objects to a remote SysDB instance");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_COPYRIGHT,
			"Copyright (C) 2015 Sebastian 'tokkee' Harl <sh@tokkee.org>");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_LICENSE, "BSD");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_VERSION, SDB_VERSION);
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_PLUGIN_VERSION, SDB_VERSION);

	sdb_plugin_register_config(store_config);
	return 0;
} /* sdb_module_init */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

