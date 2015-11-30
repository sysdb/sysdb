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

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "sysdb.h"

#include "core/plugin.h"
#include "frontend/connection-private.h"
#include "parser/ast.h"
#include "parser/parser.h"
#include "utils/error.h"
#include "utils/proto.h"
#include "utils/strbuf.h"

#include <errno.h>
#include <ctype.h>
#include <string.h>

/*
 * metric fetcher:
 * Implements the callbacks necessary to read a metric object.
 */

typedef struct {
	char *type;
	char *id;
} metric_store_t;

static int
metric_fetcher_host(sdb_store_host_t __attribute__((unused)) *host,
		sdb_object_t __attribute__((unused)) *user_data)
{
	return 0;
} /* metric_fetcher_host */

static int
metric_fetcher_metric(sdb_store_metric_t *metric, sdb_object_t *user_data)
{
	metric_store_t *st = SDB_OBJ_WRAPPER(user_data)->data;

	if ((! metric->store.type) || (! metric->store.id))
		return 0;

	st->type = strdup(metric->store.type);
	st->id = strdup(metric->store.id);
	if ((! st->type) || (! st->id))
		return -1;
	return 0;
} /* metric_fetcher_metric */

static sdb_store_writer_t metric_fetcher = {
	metric_fetcher_host, NULL, metric_fetcher_metric, NULL,
};

/*
 * private helper functions
 */

static char *
sstrdup(const char *s)
{
	return s ? strdup(s) : NULL;
} /* sstrdup */

static size_t
sstrlen(const char *s)
{
	return s ? strlen(s) : 0;
} /* sstrlen */

static int
exec_query(sdb_ast_node_t *ast, sdb_strbuf_t *buf, sdb_strbuf_t *errbuf)
{
	sdb_store_json_formatter_t *f;
	int type = 0, flags = 0;
	uint32_t res_type = 0;
	int status;

	switch (ast->type) {
	case SDB_AST_TYPE_FETCH:
		type = SDB_AST_FETCH(ast)->obj_type;
		res_type = htonl(SDB_CONNECTION_FETCH);
		break;
	case SDB_AST_TYPE_LIST:
		type = SDB_AST_LIST(ast)->obj_type;
		flags = SDB_WANT_ARRAY;
		res_type = htonl(SDB_CONNECTION_LIST);
		break;
	case SDB_AST_TYPE_LOOKUP:
		type = SDB_AST_LOOKUP(ast)->obj_type;
		flags = SDB_WANT_ARRAY;
		res_type = htonl(SDB_CONNECTION_LOOKUP);
		break;
	default:
		sdb_strbuf_sprintf(errbuf, "invalid command %s (%#x)",
				SDB_AST_TYPE_TO_STRING(ast), ast->type);
		return -1;
	}

	f = sdb_store_json_formatter(buf, type, flags);
	sdb_strbuf_memcpy(buf, &res_type, sizeof(res_type));
	status = sdb_plugin_query(ast, &sdb_store_json_writer, SDB_OBJ(f), errbuf);
	if (status < 0)
		sdb_strbuf_clear(buf);
	sdb_store_json_finish(f);
	sdb_object_deref(SDB_OBJ(f));
	return status;
} /* exec_query */

static int
exec_store(sdb_ast_store_t *st, sdb_strbuf_t *buf, sdb_strbuf_t *errbuf)
{
	char name[sstrlen(st->hostname) + sstrlen(st->parent) + sstrlen(st->name) + 3];
	sdb_metric_store_t metric_store;
	int type = st->obj_type, status = -1;

	switch (st->obj_type) {
	case SDB_HOST:
		strncpy(name, st->name, sizeof(name));
		status = sdb_plugin_store_host(st->name, st->last_update);
		break;

	case SDB_SERVICE:
		snprintf(name, sizeof(name), "%s.%s", st->hostname, st->name);
		status = sdb_plugin_store_service(st->hostname, st->name, st->last_update);
		break;

	case SDB_METRIC:
		snprintf(name, sizeof(name), "%s.%s", st->hostname, st->name);
		metric_store.type = st->store_type;
		metric_store.id = st->store_id;
		status = sdb_plugin_store_metric(st->hostname, st->name,
				&metric_store, st->last_update);
		break;

	case SDB_ATTRIBUTE:
		type |= st->parent_type;

		if (st->parent)
			snprintf(name, sizeof(name), "%s.%s.%s",
					st->hostname, st->parent, st->name);
		else
			snprintf(name, sizeof(name), "%s.%s", st->hostname, st->name);

		switch (st->parent_type) {
		case 0:
			type |= SDB_HOST;
			status = sdb_plugin_store_attribute(st->hostname,
					st->name, &st->value, st->last_update);
			break;

		case SDB_SERVICE:
			status = sdb_plugin_store_service_attribute(st->hostname, st->parent,
					st->name, &st->value, st->last_update);
			break;

		case SDB_METRIC:
			status = sdb_plugin_store_metric_attribute(st->hostname, st->parent,
					st->name, &st->value, st->last_update);
			break;

		default:
			sdb_log(SDB_LOG_ERR, "store: Invalid parent type in STORE: %s",
					SDB_STORE_TYPE_TO_NAME(st->parent_type));
			return -1;
		}
		break;

	default:
		sdb_log(SDB_LOG_ERR, "store: Invalid object type in STORE: %s",
				SDB_STORE_TYPE_TO_NAME(st->obj_type));
		return -1;
	}

	if (status < 0) {
		sdb_strbuf_sprintf(errbuf, "STORE: Failed to store %s object",
				SDB_STORE_TYPE_TO_NAME(type));
		return -1;
	}

	if (! status) {
		sdb_strbuf_sprintf(buf, "Successfully stored %s %s",
				SDB_STORE_TYPE_TO_NAME(type), name);
	}
	else {
		char type_str[32];
		strncpy(type_str, SDB_STORE_TYPE_TO_NAME(type), sizeof(type_str));
		type_str[0] = (char)toupper((int)type_str[0]);
		sdb_strbuf_sprintf(buf, "%s %s already up to date", type_str, name);
	}

	return SDB_CONNECTION_OK;
} /* exec_store */

static int
exec_timeseries(sdb_ast_timeseries_t *ts, sdb_strbuf_t *buf, sdb_strbuf_t *errbuf)
{
	metric_store_t st = { NULL, NULL };
	sdb_object_wrapper_t obj = SDB_OBJECT_WRAPPER_STATIC(&st);
	sdb_ast_fetch_t fetch = SDB_AST_FETCH_INIT;
	sdb_timeseries_opts_t opts = { 0, 0 };
	sdb_timeseries_t *series = NULL;
	int status;

	if ((! ts) || (! ts->hostname) || (! ts->metric))
		return -1;

	fetch.obj_type = SDB_METRIC;
	fetch.hostname = strdup(ts->hostname);
	fetch.name = strdup(ts->metric);
	opts.start = ts->start;
	opts.end = ts->end;

	status = sdb_plugin_query(SDB_AST_NODE(&fetch),
			&metric_fetcher, SDB_OBJ(&obj), errbuf);
	if ((status < 0) || (! st.type) || (! st.id)) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to fetch time-series '%s/%s' "
				"- no data-store configured for the stored metric",
				ts->hostname, ts->metric);
		status = -1;
	}
	if (status >= 0) {
		series = sdb_plugin_fetch_timeseries(st.type, st.id, &opts);
		if (! series) {
			sdb_log(SDB_LOG_ERR, "frontend: Failed to fetch time-series '%s/%s' "
					"- %s fetcher callback returned no data for '%s'",
					ts->hostname, ts->metric, st.type, st.id);
			status = -1;
		}
	}

	if (status >= 0) {
		sdb_timeseries_tojson(series, buf);
		sdb_timeseries_destroy(series);
	}

	free(fetch.hostname);
	free(fetch.name);
	if (st.type)
		free(st.type);
	if (st.id)
		free(st.id);
	return status;
} /* exec_timeseries */

static int
exec_cmd(sdb_conn_t *conn, sdb_ast_node_t *ast)
{
	sdb_strbuf_t *buf;
	int status;

	if (! ast) {
		sdb_strbuf_sprintf(conn->errbuf, "out of memory");
		return -1;
	}

	buf = sdb_strbuf_create(1024);
	if (! buf) {
		sdb_strbuf_sprintf(conn->errbuf, "Out of memory");
		return -1;
	}

	if (ast->type == SDB_AST_TYPE_STORE)
		status = exec_store(SDB_AST_STORE(ast), buf, conn->errbuf);
	else if (ast->type == SDB_AST_TYPE_TIMESERIES)
		status = exec_timeseries(SDB_AST_TIMESERIES(ast), buf, conn->errbuf);
	else
		status = exec_query(ast, buf, conn->errbuf);

	if (status < 0) {
		char query[conn->cmd_len + 1];
		strncpy(query, sdb_strbuf_string(conn->buf), conn->cmd_len);
		query[sizeof(query) - 1] = '\0';
		sdb_log(SDB_LOG_ERR, "frontend: failed to execute query '%s'", query);
	}
	else
		sdb_connection_send(conn, status,
				(uint32_t)sdb_strbuf_len(buf), sdb_strbuf_string(buf));

	sdb_strbuf_destroy(buf);
	return status < 0 ? status : 0;
} /* exec_cmd */

/*
 * public API
 */

int
sdb_conn_query(sdb_conn_t *conn)
{
	sdb_llist_t *parsetree;
	sdb_ast_node_t *ast = NULL;
	int status = 0;

	if ((! conn) || (conn->cmd != SDB_CONNECTION_QUERY))
		return -1;

	parsetree = sdb_parser_parse(sdb_strbuf_string(conn->buf),
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
			sdb_connection_send(conn, SDB_CONNECTION_DATA, 0, NULL);
			break;
		case 1:
			ast = SDB_AST_NODE(sdb_llist_get(parsetree, 0));
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
				ast = SDB_AST_NODE(sdb_llist_get(parsetree, 0));
			}
	}

	if (ast) {
		status = exec_cmd(conn, ast);
		sdb_object_deref(SDB_OBJ(ast));
	}
	sdb_llist_destroy(parsetree);
	return status;
} /* sdb_conn_query */

int
sdb_conn_fetch(sdb_conn_t *conn)
{
	sdb_ast_node_t *ast;
	char hostname[conn->cmd_len + 1];
	char name[conn->cmd_len + 1];
	uint32_t type;
	int status;

	if ((! conn) || (conn->cmd != SDB_CONNECTION_FETCH))
		return -1;

	if (conn->cmd_len < sizeof(uint32_t)) {
		sdb_log(SDB_LOG_ERR, "frontend: Invalid command length %d for "
				"FETCH command", conn->cmd_len);
		sdb_strbuf_sprintf(conn->errbuf, "FETCH: Invalid command length %d",
				conn->cmd_len);
		return -1;
	}

	/* TODO: support other types besides hosts */
	hostname[0] = '\0';

	sdb_proto_unmarshal_int32(SDB_STRBUF_STR(conn->buf), &type);
	strncpy(name, sdb_strbuf_string(conn->buf) + sizeof(uint32_t),
			conn->cmd_len - sizeof(uint32_t));
	name[sizeof(name) - 1] = '\0';

	ast = sdb_ast_fetch_create((int)type,
			hostname[0] ? strdup(hostname) : NULL,
			-1, NULL,
			name[0] ? strdup(name) : NULL,
			/* full */ 1, /* filter = */ NULL);
	status = exec_cmd(conn, ast);
	sdb_object_deref(SDB_OBJ(ast));
	return status;
} /* sdb_conn_fetch */

int
sdb_conn_list(sdb_conn_t *conn)
{
	sdb_ast_node_t *ast;
	uint32_t type = SDB_HOST;
	int status;

	if ((! conn) || (conn->cmd != SDB_CONNECTION_LIST))
		return -1;

	if (conn->cmd_len == sizeof(uint32_t))
		sdb_proto_unmarshal_int32(SDB_STRBUF_STR(conn->buf), &type);
	else if (conn->cmd_len) {
		sdb_log(SDB_LOG_ERR, "frontend: Invalid command length %d for "
				"LIST command", conn->cmd_len);
		sdb_strbuf_sprintf(conn->errbuf, "LIST: Invalid command length %d",
				conn->cmd_len);
		return -1;
	}

	ast = sdb_ast_list_create((int)type, /* filter = */ NULL);
	status = exec_cmd(conn, ast);
	sdb_object_deref(SDB_OBJ(ast));
	return status;
} /* sdb_conn_list */

int
sdb_conn_lookup(sdb_conn_t *conn)
{
	sdb_ast_node_t *ast, *m;
	const char *matcher;
	size_t matcher_len;

	uint32_t type;
	int status;

	if ((! conn) || (conn->cmd != SDB_CONNECTION_LOOKUP))
		return -1;

	if (conn->cmd_len < sizeof(uint32_t)) {
		sdb_log(SDB_LOG_ERR, "frontend: Invalid command length %d for "
				"LOOKUP command", conn->cmd_len);
		sdb_strbuf_sprintf(conn->errbuf, "LOOKUP: Invalid command length %d",
				conn->cmd_len);
		return -1;
	}
	sdb_proto_unmarshal_int32(SDB_STRBUF_STR(conn->buf), &type);

	matcher = sdb_strbuf_string(conn->buf) + sizeof(uint32_t);
	matcher_len = conn->cmd_len - sizeof(uint32_t);
	m = sdb_parser_parse_conditional((int)type,
			matcher, (int)matcher_len, conn->errbuf);
	if (! m) {
		char expr[matcher_len + 1];
		char err[sdb_strbuf_len(conn->errbuf) + sizeof(expr) + 64];
		strncpy(expr, matcher, sizeof(expr));
		expr[sizeof(expr) - 1] = '\0';
		snprintf(err, sizeof(err), "Failed to parse lookup condition '%s': %s",
				expr, sdb_strbuf_string(conn->errbuf));
		sdb_log(SDB_LOG_ERR, "frontend: %s", err);
		sdb_strbuf_sprintf(conn->errbuf, "%s", err);
		return -1;
	}

	ast = sdb_ast_lookup_create((int)type, m, /* filter = */ NULL);
	status = exec_cmd(conn, ast);
	if (! ast)
		sdb_object_deref(SDB_OBJ(m));
	sdb_object_deref(SDB_OBJ(ast));
	return status;
} /* sdb_conn_lookup */

int
sdb_conn_store(sdb_conn_t *conn)
{
	sdb_ast_node_t *ast = NULL;
	const char *buf = sdb_strbuf_string(conn->buf);
	size_t len = conn->cmd_len;
	uint32_t type;
	ssize_t n;
	int status;

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
			ast = sdb_ast_store_create(SDB_HOST, /* host */ NULL,
					/* parent */ 0, NULL, sstrdup(host.name), host.last_update,
					/* metric store */ NULL, NULL, SDB_DATA_NULL);
		}
		break;

		case SDB_SERVICE:
		{
			sdb_proto_service_t svc;
			if (sdb_proto_unmarshal_service(buf, len, &svc) < 0) {
				sdb_strbuf_sprintf(conn->errbuf,
						"STORE: Failed to unmarshal service object");
				return -1;
			}
			ast = sdb_ast_store_create(SDB_SERVICE, sstrdup(svc.hostname),
					/* parent */ 0, NULL, sstrdup(svc.name), svc.last_update,
					/* metric store */ NULL, NULL, SDB_DATA_NULL);
		}
		break;

		case SDB_METRIC:
		{
			sdb_proto_metric_t metric;
			if (sdb_proto_unmarshal_metric(buf, len, &metric) < 0) {
				sdb_strbuf_sprintf(conn->errbuf,
						"STORE: Failed to unmarshal metric object");
				return -1;
			}
			ast = sdb_ast_store_create(SDB_METRIC, sstrdup(metric.hostname),
					/* parent */ 0, NULL, sstrdup(metric.name), metric.last_update,
					sstrdup(metric.store_type), sstrdup(metric.store_id),
					SDB_DATA_NULL);
		}
		break;
	}

	if (type & SDB_ATTRIBUTE) {
		sdb_proto_attribute_t attr;
		const char *hostname, *parent;
		int parent_type;
		if (sdb_proto_unmarshal_attribute(buf, len, &attr) < 0) {
			sdb_strbuf_sprintf(conn->errbuf,
					"STORE: Failed to unmarshal attribute object");
			return -1;
		}
		if (attr.parent_type == SDB_HOST) {
			hostname = attr.parent;
			parent_type = 0;
			parent = NULL;
		}
		else {
			hostname = attr.hostname;
			parent_type = attr.parent_type;
			parent = attr.parent;
		}
		ast = sdb_ast_store_create(SDB_ATTRIBUTE, sstrdup(hostname),
				parent_type, sstrdup(parent), sstrdup(attr.key),
				attr.last_update, /* metric store */ NULL, NULL,
				attr.value);
	}

	if (! ast) {
		sdb_log(SDB_LOG_ERR, "frontend: Invalid object type %d for "
				"STORE COMMAND", type);
		sdb_strbuf_sprintf(conn->errbuf, "STORE: Invalid object type %d", type);
		return -1;
	}

	status = sdb_parser_analyze(ast, conn->errbuf);
	if (! status)
		status = exec_cmd(conn, ast);
	sdb_object_deref(SDB_OBJ(ast));
	return status;
} /* sdb_conn_store */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

