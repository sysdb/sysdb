/*
 * SysDB - src/frontend/query.c
 * Copyright (C) 2013 Sebastian 'tokkee' Harl <sh@tokkee.org>
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
#include "utils/strbuf.h"

#include <errno.h>

/*
 * private helper functions
 */

static int
lookup_tojson(sdb_store_obj_t *obj, void *user_data)
{
	sdb_strbuf_t *buf = user_data;
	if (sdb_strbuf_len(buf) > 1)
		sdb_strbuf_append(buf, ",");
	return sdb_store_host_tojson(obj, buf, /* flags = */ 0);
} /* lookup_tojson */

/*
 * public API
 */

int
sdb_fe_exec(sdb_conn_t *conn, sdb_conn_node_t *node)
{
	if (! node)
		return -1;

	switch (node->cmd) {
		case CONNECTION_FETCH:
			return sdb_fe_fetch(conn, CONN_FETCH(node)->name);
		case CONNECTION_LIST:
			return sdb_fe_list(conn);
		case CONNECTION_LOOKUP:
			return sdb_fe_lookup(conn, CONN_LOOKUP(node)->matcher->matcher);

		default:
			sdb_log(SDB_LOG_ERR, "frontend: Unknown command %i", node->cmd);
			return -1;
	}
	return -1;
} /* sdb_fe_exec */

int
sdb_fe_fetch(sdb_conn_t *conn, const char *name)
{
	sdb_strbuf_t *buf;
	sdb_store_obj_t *host;

	host = sdb_store_get_host(name);
	if (! host) {
		sdb_log(SDB_LOG_DEBUG, "frontend: Failed to fetch host '%s': "
				"not found", name);

		sdb_strbuf_sprintf(conn->errbuf, "Host %s not found", name);
		return -1;
	}

	buf = sdb_strbuf_create(1024);
	if (! buf) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to create "
				"buffer to handle FETCH command: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));

		sdb_strbuf_sprintf(conn->errbuf, "Out of memory");
		sdb_strbuf_destroy(buf);
		sdb_object_deref(SDB_OBJ(host));
		return -1;
	}

	if (sdb_store_host_tojson(host, buf, /* flags = */ 0)) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to serialize "
				"host '%s' to JSON", name);
		sdb_strbuf_sprintf(conn->errbuf, "Out of memory");
		sdb_strbuf_destroy(buf);
		sdb_object_deref(SDB_OBJ(host));
		return -1;
	}

	sdb_connection_send(conn, CONNECTION_OK,
			(uint32_t)sdb_strbuf_len(buf), sdb_strbuf_string(buf));
	sdb_strbuf_destroy(buf);
	sdb_object_deref(SDB_OBJ(host));
	return 0;
} /* sdb_fe_fetch */

int
sdb_fe_list(sdb_conn_t *conn)
{
	sdb_strbuf_t *buf;

	buf = sdb_strbuf_create(1024);
	if (! buf) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to create "
				"buffer to handle LIST command: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));

		sdb_strbuf_sprintf(conn->errbuf, "Out of memory");
		sdb_strbuf_destroy(buf);
		return -1;
	}

	if (sdb_store_tojson(buf, /* flags = */ SDB_SKIP_ALL)) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to serialize "
				"store to JSON");
		sdb_strbuf_sprintf(conn->errbuf, "Out of memory");
		sdb_strbuf_destroy(buf);
		return -1;
	}

	sdb_connection_send(conn, CONNECTION_OK,
			(uint32_t)sdb_strbuf_len(buf), sdb_strbuf_string(buf));
	sdb_strbuf_destroy(buf);
	return 0;
} /* sdb_fe_list */

int
sdb_fe_lookup(sdb_conn_t *conn, sdb_store_matcher_t *m)
{
	sdb_strbuf_t *buf;

	buf = sdb_strbuf_create(1024);
	if (! buf) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to create "
				"buffer to handle LOOKUP command: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));

		sdb_strbuf_sprintf(conn->errbuf, "Out of memory");
		sdb_strbuf_destroy(buf);
		return -1;
	}

	sdb_strbuf_append(buf, "[");

	if (sdb_store_scan(m, lookup_tojson, buf)) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to lookup hosts");
		sdb_strbuf_sprintf(conn->errbuf, "Failed to lookup hosts");
		sdb_strbuf_destroy(buf);
		return -1;
	}

	sdb_strbuf_append(buf, "]");

	sdb_connection_send(conn, CONNECTION_OK,
			(uint32_t)sdb_strbuf_len(buf), sdb_strbuf_string(buf));
	sdb_strbuf_destroy(buf);
	return 0;
} /* sdb_fe_lookup */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

