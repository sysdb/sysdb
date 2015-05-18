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

#include "core/store.h"
#include "frontend/connection-private.h"
#include "parser/ast.h"
#include "parser/parser.h"
#include "utils/error.h"
#include "utils/proto.h"
#include "utils/strbuf.h"

#include <errno.h>
#include <string.h>

/*
 * private helper functions
 */

static char *
sstrdup(const char *s)
{
	return s ? strdup(s) : NULL;
} /* sstrdup */

static int
query_exec(sdb_conn_t *conn, sdb_ast_node_t *ast)
{
	sdb_store_query_t *q;
	sdb_strbuf_t *buf;
	int status;

	if (! ast) {
		sdb_strbuf_sprintf(conn->errbuf, "out of memory");
		return -1;
	}

	q = sdb_store_query_prepare(ast);
	if (! q) {
		/* this shouldn't happen */
		sdb_strbuf_sprintf(conn->errbuf, "failed to compile AST");
		sdb_log(SDB_LOG_ERR, "frontend: failed to compile AST");
		return -1;
	}

	buf = sdb_strbuf_create(1024);
	if (! buf) {
		sdb_strbuf_sprintf(conn->errbuf, "Out of memory");
		sdb_object_deref(SDB_OBJ(q));
		return -1;
	}
	status = sdb_store_query_execute(q, buf, conn->errbuf);
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
	sdb_object_deref(SDB_OBJ(q));
	return status < 0 ? status : 0;
} /* query_exec */

/*
 * public API
 */

int
sdb_fe_query(sdb_conn_t *conn)
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
		status = query_exec(conn, ast);
		sdb_object_deref(SDB_OBJ(ast));
	}
	sdb_llist_destroy(parsetree);
	return status;
} /* sdb_fe_query */

int
sdb_fe_fetch(sdb_conn_t *conn)
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
			name[0] ? strdup(name) : NULL,
			/* filter = */ NULL);
	status = query_exec(conn, ast);
	sdb_object_deref(SDB_OBJ(ast));
	return status;
} /* sdb_fe_fetch */

int
sdb_fe_list(sdb_conn_t *conn)
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
	status = query_exec(conn, ast);
	sdb_object_deref(SDB_OBJ(ast));
	return status;
} /* sdb_fe_list */

int
sdb_fe_lookup(sdb_conn_t *conn)
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
	m = sdb_parser_parse_conditional(matcher, (int)matcher_len, conn->errbuf);
	if (! m) {
		char expr[matcher_len + 1];
		strncpy(expr, matcher, sizeof(expr));
		expr[sizeof(expr) - 1] = '\0';
		sdb_log(SDB_LOG_ERR, "frontend: Failed to parse lookup condition '%s': %s",
				expr, sdb_strbuf_string(conn->errbuf));
		return -1;
	}

	ast = sdb_ast_lookup_create((int)type, m, /* filter = */ NULL);
	/* run analyzer using the full context */
	if (ast && sdb_parser_analyze(ast, conn->errbuf)) {
		char expr[matcher_len + 1];
		char err[sdb_strbuf_len(conn->errbuf) + sizeof(expr) + 64];
		strncpy(expr, matcher, sizeof(expr));
		expr[sizeof(expr) - 1] = '\0';
		snprintf(err, sizeof(err), "Failed to parse lookup condition '%s': %s",
				expr, sdb_strbuf_string(conn->errbuf));
		sdb_strbuf_sprintf(conn->errbuf, "%s", err);
		status = -1;
	}
	else
		status = query_exec(conn, ast);
	if (! ast)
		sdb_object_deref(SDB_OBJ(m));
	sdb_object_deref(SDB_OBJ(ast));
	return status;
} /* sdb_fe_lookup */

int
sdb_fe_store(sdb_conn_t *conn)
{
	sdb_ast_node_t *ast;
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
		status = query_exec(conn, ast);
	sdb_object_deref(SDB_OBJ(ast));
	return status;
} /* sdb_fe_store */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

