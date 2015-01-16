/*
 * SysDB - src/frontend/store.c
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

#include "sysdb.h"

#include "core/store.h"
#include "frontend/connection-private.h"
#include "utils/error.h"
#include "utils/proto.h"
#include "utils/strbuf.h"

#include <ctype.h>
#include <string.h>

/*
 * private helper functions
 */

static int
store_reply(sdb_conn_t *conn, int type, const char *name, int status)
{
	if (status < 0) {
		sdb_strbuf_sprintf(conn->errbuf, "STORE: Failed to store %s object",
				SDB_STORE_TYPE_TO_NAME(type));
		return -1;
	}

	if (! status) {
		char msg[strlen(name) + 64];
		snprintf(msg, sizeof(msg), "Successfully stored %s %s",
				SDB_STORE_TYPE_TO_NAME(type), name);
		msg[sizeof(msg) - 1] = '\0';
		sdb_connection_send(conn, SDB_CONNECTION_OK,
				(uint32_t)strlen(msg), msg);
	}
	else {
		char msg[strlen(name) + 64];
		snprintf(msg, sizeof(msg), "%s %s already up to date",
				SDB_STORE_TYPE_TO_NAME(type), name);
		msg[0] = (char)toupper((int)msg[0]);
		msg[sizeof(msg) - 1] = '\0';
		sdb_connection_send(conn, SDB_CONNECTION_OK,
				(uint32_t)strlen(msg), msg);
	}
	return 0;
} /* store_reply */

/*
 * public API
 */

int
sdb_fe_store(sdb_conn_t *conn)
{
	uint32_t type;

	const char *buf = sdb_strbuf_string(conn->buf);
	size_t len = conn->cmd_len;
	ssize_t n;

	if ((! conn) || (conn->cmd != SDB_CONNECTION_STORE))
		return -1;

	if ((n = sdb_proto_unmarshal_int32(buf, len, &type)) < 0) {
		sdb_log(SDB_LOG_ERR, "frontend: Invalid command length %zu for "
				"STORE command", len);
		sdb_strbuf_sprintf(conn->errbuf,
				"STORE: Invalid command length %zu", len);
		return -1;
	}

	switch (type) {
		case SDB_HOST:
		{
			sdb_proto_host_t host;
			if (sdb_proto_unmarshal_host(buf, len, &host) < 0) {
				sdb_strbuf_sprintf(conn->errbuf,
						"STORE: Failed to unmarshal host object");
				return -1;
			}
			return sdb_fe_store_host(conn, &host);
		}
		case SDB_SERVICE:
		{
			sdb_proto_service_t svc;
			if (sdb_proto_unmarshal_service(buf, len, &svc) < 0) {
				sdb_strbuf_sprintf(conn->errbuf,
						"STORE: Failed to unmarshal service object");
				return -1;
			}
			return sdb_fe_store_service(conn, &svc);
		}
		case SDB_METRIC:
		{
			sdb_proto_metric_t metric;
			if (sdb_proto_unmarshal_metric(buf, len, &metric) < 0) {
				sdb_strbuf_sprintf(conn->errbuf,
						"STORE: Failed to unmarshal metric object");
				return -1;
			}
			return sdb_fe_store_metric(conn, &metric);
		}
		case SDB_ATTRIBUTE:
		{
			sdb_proto_attribute_t attr;
			if (sdb_proto_unmarshal_attribute(buf, len, &attr) < 0) {
				sdb_strbuf_sprintf(conn->errbuf,
						"STORE: Failed to unmarshal attribute object");
				return -1;
			}
			return sdb_fe_store_attribute(conn, &attr);
		}
	}

	sdb_log(SDB_LOG_ERR, "frontend: Invalid object type %d for "
			"STORE COMMAND", type);
	sdb_strbuf_sprintf(conn->errbuf, "STORE: Invalid object type %d", type);
	return -1;
} /* sdb_fe_store */

int
sdb_fe_store_host(sdb_conn_t *conn, const sdb_proto_host_t *host)
{
	if ((! conn) || (! host) || (! host->name))
		return -1;

	return store_reply(conn, SDB_HOST, host->name,
			sdb_store_host(host->name, host->last_update));
} /* sdb_fe_store_host */

int
sdb_fe_store_service(sdb_conn_t *conn, const sdb_proto_service_t *svc)
{
	char name[svc ? strlen(svc->hostname) + strlen(svc->name) + 2 : 2];

	if ((! conn) || (! svc) || (! svc->hostname) || (! svc->name))
		return -1;

	snprintf(name, sizeof(name), svc->hostname, svc->name);
	return store_reply(conn, SDB_SERVICE, name,
			sdb_store_service(svc->hostname, svc->name, svc->last_update));
} /* sdb_fe_store_service */

int
sdb_fe_store_metric(sdb_conn_t *conn, const sdb_proto_metric_t *metric)
{
	sdb_metric_store_t store;
	char name[metric ? strlen(metric->hostname) + strlen(metric->name) + 2 : 2];

	if ((! conn) || (! metric) || (! metric->hostname) || (! metric->name))
		return -1;

	store.type = metric->store_type;
	store.id = metric->store_id;
	snprintf(name, sizeof(name), metric->hostname, metric->name);
	return store_reply(conn, SDB_METRIC, name,
			sdb_store_metric(metric->hostname, metric->name,
				&store, metric->last_update));
} /* sdb_fe_store_metric */

int
sdb_fe_store_attribute(sdb_conn_t *conn, const sdb_proto_attribute_t *attr)
{
	char name[attr ? strlen(attr->parent) + strlen(attr->key) + 2 : 2];
	int status;

	if ((! conn) || (! attr) || (! attr->parent) || (! attr->key))
		return -1;

	if (attr->parent_type == SDB_HOST)
		status = sdb_store_attribute(attr->parent,
				attr->key, &attr->value, attr->last_update);
	else if (attr->parent_type == SDB_SERVICE)
		status = sdb_store_service_attr(attr->hostname, attr->parent,
				attr->key, &attr->value, attr->last_update);
	else if (attr->parent_type == SDB_METRIC)
		status = sdb_store_metric_attr(attr->hostname, attr->parent,
				attr->key, &attr->value, attr->last_update);
	else {
		sdb_strbuf_sprintf(conn->errbuf,
				"STORE: Invalid parent object type %d",
				attr->parent_type);
		return -1;
	}

	snprintf(name, sizeof(name), "%s.%s", attr->parent, attr->key);
	return store_reply(conn, attr->parent_type | SDB_ATTRIBUTE, name, status);
} /* sdb_fe_store_attribute */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

