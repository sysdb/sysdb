/*
 * SysDB - src/frontend/query.c
 * Copyright (C) 2013-2014 Sebastian 'tokkee' Harl <sh@tokkee.org>
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
#include "frontend/parser.h"
#include "utils/error.h"
#include "utils/proto.h"
#include "utils/strbuf.h"

#include <errno.h>
#include <string.h>

/*
 * private helper functions
 */

static int
list_tojson(sdb_store_obj_t *obj,
		sdb_store_matcher_t __attribute__((unused)) *filter,
		void *user_data)
{
	sdb_store_json_formatter_t *f = user_data;
	return sdb_store_json_emit(f, obj);
} /* list_tojson */

static int
lookup_tojson(sdb_store_obj_t *obj, sdb_store_matcher_t *filter,
		void *user_data)
{
	sdb_store_json_formatter_t *f = user_data;
	return sdb_store_json_emit_full(f, obj, filter);
} /* lookup_tojson */

/*
 * public API
 */

int
sdb_fe_query(sdb_conn_t *conn)
{
	sdb_llist_t *parsetree;
	sdb_conn_node_t *node = NULL;
	int status = 0;

	if ((! conn) || (conn->cmd != CONNECTION_QUERY))
		return -1;

	parsetree = sdb_fe_parse(sdb_strbuf_string(conn->buf),
			(int)conn->cmd_len, conn->errbuf);
	if (! parsetree) {
		char query[conn->cmd_len + 1];
		strncpy(query, sdb_strbuf_string(conn->buf), conn->cmd_len);
		query[sizeof(query) - 1] = '\0';
		sdb_log(SDB_LOG_ERR, "frontend: Failed to parse query '%s': %s",
				query, sdb_strbuf_string(conn->errbuf));
		return -1;
	}

	switch (sdb_llist_len(parsetree)) {
		case 0:
			/* skipping empty command; send back an empty reply */
			sdb_connection_send(conn, CONNECTION_DATA, 0, NULL);
			break;
		case 1:
			node = SDB_CONN_NODE(sdb_llist_get(parsetree, 0));
			break;

		default:
			{
				char query[conn->cmd_len + 1];
				strncpy(query, sdb_strbuf_string(conn->buf), conn->cmd_len);
				query[sizeof(query) - 1] = '\0';
				sdb_log(SDB_LOG_WARNING, "frontend: Ignoring %zu command%s "
						"in multi-statement query '%s'",
						sdb_llist_len(parsetree) - 1,
						sdb_llist_len(parsetree) == 2 ? "" : "s",
						query);
				node = SDB_CONN_NODE(sdb_llist_get(parsetree, 0));
			}
	}

	if (node) {
		status = sdb_fe_exec(conn, node);
		sdb_object_deref(SDB_OBJ(node));
	}

	sdb_llist_destroy(parsetree);
	return status;
} /* sdb_fe_query */

int
sdb_fe_fetch(sdb_conn_t *conn)
{
	char name[conn->cmd_len + 1];
	int type;

	if ((! conn) || (conn->cmd != CONNECTION_FETCH))
		return -1;

	if (conn->cmd_len < sizeof(uint32_t)) {
		sdb_log(SDB_LOG_ERR, "frontend: Invalid command length %d for "
				"FETCH command", conn->cmd_len);
		sdb_strbuf_sprintf(conn->errbuf, "FETCH: Invalid command length %d",
				conn->cmd_len);
		return -1;
	}

	type = sdb_proto_get_int(conn->buf, 0);
	strncpy(name, sdb_strbuf_string(conn->buf) + sizeof(uint32_t),
			conn->cmd_len - sizeof(uint32_t));
	name[sizeof(name) - 1] = '\0';
	/* TODO: support other types besides hosts */
	return sdb_fe_exec_fetch(conn, type, name, NULL, /* filter = */ NULL);
} /* sdb_fe_fetch */

int
sdb_fe_list(sdb_conn_t *conn)
{
	int type = SDB_HOST;

	if ((! conn) || (conn->cmd != CONNECTION_LIST))
		return -1;

	if (conn->cmd_len == sizeof(uint32_t))
		type = sdb_proto_get_int(conn->buf, 0);
	else if (conn->cmd_len) {
		sdb_log(SDB_LOG_ERR, "frontend: Invalid command length %d for "
				"LIST command", conn->cmd_len);
		sdb_strbuf_sprintf(conn->errbuf, "LIST: Invalid command length %d",
				conn->cmd_len);
		return -1;
	}
	return sdb_fe_exec_list(conn, type, /* filter = */ NULL);
} /* sdb_fe_list */

int
sdb_fe_lookup(sdb_conn_t *conn)
{
	sdb_store_matcher_t *m;
	const char *matcher;
	size_t matcher_len;

	int type;
	int status;

	conn_matcher_t m_node = {
		{ SDB_OBJECT_INIT, CONNECTION_MATCHER }, NULL
	};
	conn_lookup_t node = {
		{ SDB_OBJECT_INIT, CONNECTION_LOOKUP },
		-1, &m_node, NULL
	};

	if ((! conn) || (conn->cmd != CONNECTION_LOOKUP))
		return -1;

	if (conn->cmd_len < sizeof(uint32_t)) {
		sdb_log(SDB_LOG_ERR, "frontend: Invalid command length %d for "
				"LOOKUP command", conn->cmd_len);
		sdb_strbuf_sprintf(conn->errbuf, "LOOKUP: Invalid command length %d",
				conn->cmd_len);
		return -1;
	}
	type = sdb_proto_get_int(conn->buf, 0);

	matcher = sdb_strbuf_string(conn->buf) + sizeof(uint32_t);
	matcher_len = conn->cmd_len - sizeof(uint32_t);
	m = sdb_fe_parse_matcher(matcher, (int)matcher_len, conn->errbuf);
	if (! m) {
		char expr[matcher_len + 1];
		strncpy(expr, matcher, sizeof(expr));
		expr[sizeof(expr) - 1] = '\0';
		sdb_log(SDB_LOG_ERR, "frontend: Failed to parse "
				"lookup condition '%s': %s", expr,
				sdb_strbuf_string(conn->errbuf));
		return -1;
	}

	node.type = type;
	m_node.matcher = m;

	/* run analyzer separately; parse_matcher is missing
	 * the right context to do so */
	if (sdb_fe_analyze(SDB_CONN_NODE(&node))) {
		char expr[matcher_len + 1];
		strncpy(expr, matcher, sizeof(expr));
		expr[sizeof(expr) - 1] = '\0';
		sdb_strbuf_sprintf(conn->errbuf, "Failed to verify "
				"lookup condition '%s'", expr);
		status = -1;
	}
	else
		status = sdb_fe_exec_lookup(conn, type, m, /* filter = */ NULL);
	sdb_object_deref(SDB_OBJ(m));
	return status;
} /* sdb_fe_lookup */

int
sdb_fe_exec(sdb_conn_t *conn, sdb_conn_node_t *node)
{
	sdb_store_matcher_t *m = NULL, *filter = NULL;

	if (! node)
		return -1;

	switch (node->cmd) {
		case CONNECTION_FETCH:
			if (CONN_FETCH(node)->filter)
				filter = CONN_FETCH(node)->filter->matcher;
			return sdb_fe_exec_fetch(conn, CONN_FETCH(node)->type,
					CONN_FETCH(node)->host, CONN_FETCH(node)->name, filter);
		case CONNECTION_LIST:
			if (CONN_LIST(node)->filter)
				filter = CONN_LIST(node)->filter->matcher;
			return sdb_fe_exec_list(conn, CONN_LIST(node)->type, filter);
		case CONNECTION_LOOKUP:
			if (CONN_LOOKUP(node)->matcher)
				m = CONN_LOOKUP(node)->matcher->matcher;
			if (CONN_LOOKUP(node)->filter)
				filter = CONN_LOOKUP(node)->filter->matcher;
			return sdb_fe_exec_lookup(conn,
					CONN_LOOKUP(node)->type, m, filter);
		case CONNECTION_TIMESERIES:
			return sdb_fe_exec_timeseries(conn,
					CONN_TS(node)->hostname, CONN_TS(node)->metric,
					&CONN_TS(node)->opts);

		default:
			sdb_log(SDB_LOG_ERR, "frontend: Unknown command %i", node->cmd);
			return -1;
	}
	return -1;
} /* sdb_fe_exec */

int
sdb_fe_exec_fetch(sdb_conn_t *conn, int type,
		const char *hostname, const char *name, sdb_store_matcher_t *filter)
{
	uint32_t res_type = htonl(CONNECTION_FETCH);

	sdb_store_obj_t *host;
	sdb_store_obj_t *obj;

	sdb_store_json_formatter_t *f;
	sdb_strbuf_t *buf;

	if ((! hostname) || ((type == SDB_HOST) && name)
			|| ((type != SDB_HOST) && (! name))) {
		/* This is a programming error, not something the client did wrong */
		sdb_strbuf_sprintf(conn->errbuf, "INTERNAL ERROR: invalid "
				"arguments to sdb_fe_exec_fetch(%s, %s, %s)",
				SDB_STORE_TYPE_TO_NAME(type), hostname, name);
		return -1;
	}

	host = sdb_store_get_host(hostname);
	if ((! host) || (filter
				&& (! sdb_store_matcher_matches(filter, host, NULL)))) {
		sdb_strbuf_sprintf(conn->errbuf, "Failed to fetch %s %s: "
				"host %s not found", SDB_STORE_TYPE_TO_NAME(type),
				name, hostname);
		return -1;
	}
	if (type == SDB_HOST) {
		obj = host;
	}
	else {
		obj = sdb_store_get_child(host, type, name);
		if ((! obj) || (filter
					&& (! sdb_store_matcher_matches(filter, obj, NULL)))) {
			sdb_strbuf_sprintf(conn->errbuf, "Failed to fetch %s %s.%s: "
					"%s not found", SDB_STORE_TYPE_TO_NAME(type),
					hostname, name, name);
			return -1;
		}
		sdb_object_deref(SDB_OBJ(host));
	}

	buf = sdb_strbuf_create(1024);
	if (! buf) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to create "
				"buffer to handle FETCH command: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));

		sdb_strbuf_sprintf(conn->errbuf, "Out of memory");
		sdb_strbuf_destroy(buf);
		sdb_object_deref(SDB_OBJ(obj));
		return -1;
	}
	f = sdb_store_json_formatter(buf, type, /* flags = */ 0);
	if (! f) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to create "
				"JSON formatter to handle FETCH command: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));

		sdb_strbuf_sprintf(conn->errbuf, "Out of memory");
		sdb_strbuf_destroy(buf);
		sdb_object_deref(SDB_OBJ(obj));
		return -1;
	}

	sdb_strbuf_memcpy(buf, &res_type, sizeof(uint32_t));
	if (sdb_store_json_emit_full(f, obj, filter)) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to serialize "
				"%s %s.%s to JSON", SDB_STORE_TYPE_TO_NAME(type),
				hostname, name);
		sdb_strbuf_sprintf(conn->errbuf, "Out of memory");
		sdb_strbuf_destroy(buf);
		free(f);
		sdb_object_deref(SDB_OBJ(obj));
		return -1;
	}
	sdb_store_json_finish(f);

	sdb_connection_send(conn, CONNECTION_DATA,
			(uint32_t)sdb_strbuf_len(buf), sdb_strbuf_string(buf));
	sdb_strbuf_destroy(buf);
	free(f);
	sdb_object_deref(SDB_OBJ(obj));
	return 0;
} /* sdb_fe_exec_fetch */

int
sdb_fe_exec_list(sdb_conn_t *conn, int type, sdb_store_matcher_t *filter)
{
	uint32_t res_type = htonl(CONNECTION_LIST);

	sdb_store_json_formatter_t *f;
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
	f = sdb_store_json_formatter(buf, type, SDB_WANT_ARRAY);
	if (! f) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to create "
				"JSON formatter to handle LIST command: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));

		sdb_strbuf_sprintf(conn->errbuf, "Out of memory");
		sdb_strbuf_destroy(buf);
		return -1;
	}

	sdb_strbuf_memcpy(buf, &res_type, sizeof(uint32_t));
	if (sdb_store_scan(type, /* m = */ NULL, filter, list_tojson, f)) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to serialize "
				"store to JSON");
		sdb_strbuf_sprintf(conn->errbuf, "Out of memory");
		sdb_strbuf_destroy(buf);
		free(f);
		return -1;
	}
	sdb_store_json_finish(f);

	sdb_connection_send(conn, CONNECTION_DATA,
			(uint32_t)sdb_strbuf_len(buf), sdb_strbuf_string(buf));
	sdb_strbuf_destroy(buf);
	free(f);
	return 0;
} /* sdb_fe_exec_list */

int
sdb_fe_exec_lookup(sdb_conn_t *conn, int type,
		sdb_store_matcher_t *m, sdb_store_matcher_t *filter)
{
	uint32_t res_type = htonl(CONNECTION_LOOKUP);

	sdb_store_json_formatter_t *f;
	sdb_strbuf_t *buf;

	/* XXX: support other types */
	if (type != SDB_HOST) {
		sdb_log(SDB_LOG_ERR, "frontend: Invalid object type %d "
				"in LOOKUP command", type);
		sdb_strbuf_sprintf(conn->errbuf,
				"LOOKUP: Invalid object type %d", type);
		return -1;
	}

	buf = sdb_strbuf_create(1024);
	if (! buf) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to create "
				"buffer to handle LOOKUP command: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));

		sdb_strbuf_sprintf(conn->errbuf, "Out of memory");
		return -1;
	}
	f = sdb_store_json_formatter(buf, type, SDB_WANT_ARRAY);
	if (! f) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to create "
				"JSON formatter to handle LOOKUP command: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));

		sdb_strbuf_sprintf(conn->errbuf, "Out of memory");
		sdb_strbuf_destroy(buf);
		return -1;
	}

	sdb_strbuf_memcpy(buf, &res_type, sizeof(uint32_t));

	if (sdb_store_scan(SDB_HOST, m, filter, lookup_tojson, f)) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to lookup hosts");
		sdb_strbuf_sprintf(conn->errbuf, "Failed to lookup hosts");
		sdb_strbuf_destroy(buf);
		free(f);
		return -1;
	}
	sdb_store_json_finish(f);

	sdb_connection_send(conn, CONNECTION_DATA,
			(uint32_t)sdb_strbuf_len(buf), sdb_strbuf_string(buf));
	sdb_strbuf_destroy(buf);
	free(f);
	return 0;
} /* sdb_fe_exec_lookup */

int
sdb_fe_exec_timeseries(sdb_conn_t *conn,
		const char *hostname, const char *metric,
		sdb_timeseries_opts_t *opts)
{
	sdb_strbuf_t *buf;
	uint32_t res_type = htonl(CONNECTION_TIMESERIES);

	buf = sdb_strbuf_create(1024);
	if (! buf) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to create "
				"buffer to handle TIMESERIES command: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));

		sdb_strbuf_sprintf(conn->errbuf, "Out of memory");
		return -1;
	}

	sdb_strbuf_memcpy(buf, &res_type, sizeof(uint32_t));
	if (sdb_store_fetch_timeseries(hostname, metric, opts, buf)) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to fetch time-series");
		sdb_strbuf_sprintf(conn->errbuf, "Failed to fetch time-series");
		sdb_strbuf_destroy(buf);
		return -1;
	}

	sdb_connection_send(conn, CONNECTION_DATA,
			(uint32_t)sdb_strbuf_len(buf), sdb_strbuf_string(buf));
	sdb_strbuf_destroy(buf);
	return 0;
} /* sdb_fe_exec_timeseries */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

