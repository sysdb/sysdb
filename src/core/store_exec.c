/*
 * SysDB - src/core/store_exec.c
 * Copyright (C) 2014-2015 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include "core/object.h"
#include "core/plugin.h"
#include "core/store-private.h"
#include "frontend/connection.h"
#include "parser/ast.h"
#include "utils/error.h"

#include <errno.h>

#include <arpa/inet.h>
#include <ctype.h>
#include <stdlib.h>
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

static size_t
sstrlen(const char *s)
{
	return s ? strlen(s) : 0;
} /* sstrlen */

/*
 * query implementations
 */

static int
exec_fetch(sdb_store_t *store, sdb_strbuf_t *buf, sdb_strbuf_t *errbuf,
		int type, const char *hostname, const char *name,
		sdb_store_matcher_t *filter)
{
	uint32_t res_type = htonl(SDB_CONNECTION_FETCH);

	sdb_store_obj_t *host;
	sdb_store_obj_t *obj;

	sdb_store_json_formatter_t *f;

	if ((! name) || ((type == SDB_HOST) && hostname)
			|| ((type != SDB_HOST) && (! hostname))) {
		/* This is a programming error, not something the client did wrong */
		sdb_strbuf_sprintf(errbuf, "INTERNAL ERROR: invalid "
				"arguments to FETCH(%s, %s, %s)",
				SDB_STORE_TYPE_TO_NAME(type), hostname, name);
		return -1;
	}
	if (type == SDB_HOST)
		hostname = name;

	host = sdb_store_get_host(store, hostname);
	if ((! host)
			|| (filter && (! sdb_store_matcher_matches(filter, host, NULL)))) {
		sdb_strbuf_sprintf(errbuf, "Failed to fetch %s %s: "
				"host %s not found", SDB_STORE_TYPE_TO_NAME(type),
				name, hostname);
		sdb_object_deref(SDB_OBJ(host));
		return -1;
	}
	if (type == SDB_HOST) {
		obj = host;
	}
	else {
		obj = sdb_store_get_child(host, type, name);
		if ((! obj)
				|| (filter && (! sdb_store_matcher_matches(filter, obj, NULL)))) {
			sdb_strbuf_sprintf(errbuf, "Failed to fetch %s %s.%s: "
					"%s not found", SDB_STORE_TYPE_TO_NAME(type),
					hostname, name, name);
			if (obj)
				sdb_object_deref(SDB_OBJ(obj));
			sdb_object_deref(SDB_OBJ(host));
			return -1;
		}
		sdb_object_deref(SDB_OBJ(host));
	}
	host = NULL;

	f = sdb_store_json_formatter(buf, type, /* flags = */ 0);
	if (! f) {
		char err[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to create "
				"JSON formatter to handle FETCH command: %s",
				sdb_strerror(errno, err, sizeof(err)));

		sdb_strbuf_sprintf(errbuf, "Out of memory");
		sdb_object_deref(SDB_OBJ(obj));
		return -1;
	}

	sdb_strbuf_memcpy(buf, &res_type, sizeof(uint32_t));
	if (sdb_store_json_emit_full(f, obj, filter)) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to serialize "
				"%s %s.%s to JSON", SDB_STORE_TYPE_TO_NAME(type),
				hostname, name);
		sdb_strbuf_sprintf(errbuf, "Out of memory");
		sdb_object_deref(SDB_OBJ(f));
		sdb_object_deref(SDB_OBJ(obj));
		return -1;
	}

	sdb_object_deref(SDB_OBJ(obj));
	sdb_store_json_finish(f);
	sdb_object_deref(SDB_OBJ(f));

	return SDB_CONNECTION_DATA;
} /* exec_fetch */

static int
exec_list(sdb_store_t *store, sdb_strbuf_t *buf, sdb_strbuf_t *errbuf,
		int type, sdb_store_matcher_t *filter)
{
	uint32_t res_type = htonl(SDB_CONNECTION_LIST);
	sdb_store_json_formatter_t *f;

	f = sdb_store_json_formatter(buf, type, SDB_WANT_ARRAY);
	if (! f) {
		char err[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to create "
				"JSON formatter to handle LIST command: %s",
				sdb_strerror(errno, err, sizeof(err)));

		sdb_strbuf_sprintf(errbuf, "Out of memory");
		return -1;
	}

	sdb_strbuf_memcpy(buf, &res_type, sizeof(uint32_t));
	if (sdb_store_scan(store, type, /* m = */ NULL, filter, list_tojson, f)) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to serialize "
				"store to JSON");
		sdb_strbuf_sprintf(errbuf, "Out of memory");
		sdb_object_deref(SDB_OBJ(f));
		return -1;
	}

	sdb_store_json_finish(f);
	sdb_object_deref(SDB_OBJ(f));

	return SDB_CONNECTION_DATA;
} /* exec_list */

static int
exec_lookup(sdb_store_t *store, sdb_strbuf_t *buf, sdb_strbuf_t *errbuf,
		int type, sdb_store_matcher_t *m, sdb_store_matcher_t *filter)
{
	uint32_t res_type = htonl(SDB_CONNECTION_LOOKUP);
	sdb_store_json_formatter_t *f;

	f = sdb_store_json_formatter(buf, type, SDB_WANT_ARRAY);
	if (! f) {
		char err[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to create "
				"JSON formatter to handle LOOKUP command: %s",
				sdb_strerror(errno, err, sizeof(err)));

		sdb_strbuf_sprintf(errbuf, "Out of memory");
		return -1;
	}

	sdb_strbuf_memcpy(buf, &res_type, sizeof(uint32_t));

	if (sdb_store_scan(store, type, m, filter, lookup_tojson, f)) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to lookup %ss",
				SDB_STORE_TYPE_TO_NAME(type));
		sdb_strbuf_sprintf(errbuf, "Failed to lookup %ss",
				SDB_STORE_TYPE_TO_NAME(type));
		sdb_object_deref(SDB_OBJ(f));
		return -1;
	}

	sdb_store_json_finish(f);
	sdb_object_deref(SDB_OBJ(f));

	return SDB_CONNECTION_DATA;
} /* exec_lookup */

static int
exec_store(sdb_strbuf_t *buf, sdb_strbuf_t *errbuf, sdb_ast_store_t *st)
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
exec_timeseries(sdb_store_t *store, sdb_strbuf_t *buf, sdb_strbuf_t *errbuf,
		const char *hostname, const char *metric,
		sdb_timeseries_opts_t *opts)
{
	uint32_t res_type = htonl(SDB_CONNECTION_TIMESERIES);

	sdb_strbuf_memcpy(buf, &res_type, sizeof(uint32_t));
	if (sdb_store_fetch_timeseries(store, hostname, metric, opts, buf)) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to fetch time-series");
		sdb_strbuf_sprintf(errbuf, "Failed to fetch time-series");
		return -1;
	}

	return SDB_CONNECTION_DATA;
} /* exec_timeseries */

/*
 * public API
 */

int
sdb_store_query_execute(sdb_store_t *store, sdb_store_query_t *q,
		sdb_strbuf_t *buf, sdb_strbuf_t *errbuf)
{
	sdb_timeseries_opts_t ts_opts;
	sdb_ast_node_t *ast;

	if (! q)
		return -1;
	if (! q->ast) {
		sdb_log(SDB_LOG_ERR, "store: Invalid empty query");
		return -1;
	}

	ast = q->ast;
	switch (ast->type) {
	case SDB_AST_TYPE_FETCH:
		return exec_fetch(store, buf, errbuf, SDB_AST_FETCH(ast)->obj_type,
				SDB_AST_FETCH(ast)->hostname, SDB_AST_FETCH(ast)->name,
				q->filter);

	case SDB_AST_TYPE_LIST:
		return exec_list(store, buf, errbuf, SDB_AST_LIST(ast)->obj_type,
				q->filter);

	case SDB_AST_TYPE_LOOKUP:
		return exec_lookup(store, buf, errbuf, SDB_AST_LOOKUP(ast)->obj_type,
				q->matcher, q->filter);

	case SDB_AST_TYPE_STORE:
		if (ast->type != SDB_AST_TYPE_STORE) {
			sdb_log(SDB_LOG_ERR, "store: Invalid AST node for STORE command: %s",
					SDB_AST_TYPE_TO_STRING(ast));
			return -1;
		}
		return exec_store(buf, errbuf, SDB_AST_STORE(ast));

	case SDB_AST_TYPE_TIMESERIES:
		ts_opts.start = SDB_AST_TIMESERIES(ast)->start;
		ts_opts.end = SDB_AST_TIMESERIES(ast)->end;
		return exec_timeseries(store, buf, errbuf,
				SDB_AST_TIMESERIES(ast)->hostname,
				SDB_AST_TIMESERIES(ast)->metric, &ts_opts);

	default:
		sdb_log(SDB_LOG_ERR, "store: Invalid query of type %s",
				SDB_AST_TYPE_TO_STRING(ast));
		return -1;
	}

	return 0;
} /* sdb_store_query_execute */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */
