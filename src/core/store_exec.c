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
#include <stdlib.h>
#include <string.h>

/*
 * private helper functions
 */

typedef struct {
	sdb_store_obj_t *current_host;

	sdb_store_writer_t *w;
	sdb_object_t *wd;
} iter_t;

static int
maybe_emit_host(iter_t *iter, sdb_store_obj_t *obj)
{
	if ((obj->type == SDB_HOST) || (obj->type == SDB_ATTRIBUTE))
		return 0;
	if (iter->current_host == obj->parent)
		return 0;
	iter->current_host = obj->parent;
	return sdb_store_emit(obj->parent, iter->w, iter->wd);
} /* maybe_emit_host */

static int
list_tojson(sdb_store_obj_t *obj,
		sdb_store_matcher_t __attribute__((unused)) *filter,
		void *user_data)
{
	iter_t *iter = user_data;
	maybe_emit_host(iter, obj);
	return sdb_store_emit(obj, iter->w, iter->wd);
} /* list_tojson */

static int
lookup_tojson(sdb_store_obj_t *obj, sdb_store_matcher_t *filter,
		void *user_data)
{
	iter_t *iter = user_data;
	maybe_emit_host(iter, obj);
	return sdb_store_emit_full(obj, filter, iter->w, iter->wd);
} /* lookup_tojson */

/*
 * query implementations
 */

static int
exec_fetch(sdb_store_t *store,
		sdb_store_writer_t *w, sdb_object_t *wd, sdb_strbuf_t *errbuf,
		int type, const char *hostname, const char *name,
		sdb_store_matcher_t *filter)
{
	sdb_store_obj_t *host;
	sdb_store_obj_t *obj;

	int status = 0;

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

	if (type != SDB_HOST)
		status = sdb_store_emit(obj->parent, w, wd);
	if (status || sdb_store_emit_full(obj, filter, w, wd)) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to serialize "
				"%s %s.%s to JSON", SDB_STORE_TYPE_TO_NAME(type),
				hostname, name);
		sdb_strbuf_sprintf(errbuf, "Out of memory");
		sdb_object_deref(SDB_OBJ(obj));
		return -1;
	}

	sdb_object_deref(SDB_OBJ(obj));
	return SDB_CONNECTION_DATA;
} /* exec_fetch */

static int
exec_list(sdb_store_t *store,
		sdb_store_writer_t *w, sdb_object_t *wd, sdb_strbuf_t *errbuf,
		int type, sdb_store_matcher_t *filter)
{
	iter_t iter = { NULL, w, wd };

	if (sdb_store_scan(store, type, /* m = */ NULL, filter, list_tojson, &iter)) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to serialize "
				"store to JSON");
		sdb_strbuf_sprintf(errbuf, "Out of memory");
		return -1;
	}

	return SDB_CONNECTION_DATA;
} /* exec_list */

static int
exec_lookup(sdb_store_t *store,
		sdb_store_writer_t *w, sdb_object_t *wd, sdb_strbuf_t *errbuf,
		int type, sdb_store_matcher_t *m, sdb_store_matcher_t *filter)
{
	iter_t iter = { NULL, w, wd };

	if (sdb_store_scan(store, type, m, filter, lookup_tojson, &iter)) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to lookup %ss",
				SDB_STORE_TYPE_TO_NAME(type));
		sdb_strbuf_sprintf(errbuf, "Failed to lookup %ss",
				SDB_STORE_TYPE_TO_NAME(type));
		return -1;
	}

	return SDB_CONNECTION_DATA;
} /* exec_lookup */

/*
 * public API
 */

int
sdb_store_query_execute(sdb_store_t *store, sdb_store_query_t *q,
		sdb_store_writer_t *w, sdb_object_t *wd, sdb_strbuf_t *errbuf)
{
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
		return exec_fetch(store, w, wd, errbuf, SDB_AST_FETCH(ast)->obj_type,
				SDB_AST_FETCH(ast)->hostname, SDB_AST_FETCH(ast)->name,
				q->filter);

	case SDB_AST_TYPE_LIST:
		return exec_list(store, w, wd, errbuf, SDB_AST_LIST(ast)->obj_type,
				q->filter);

	case SDB_AST_TYPE_LOOKUP:
		return exec_lookup(store, w, wd, errbuf, SDB_AST_LOOKUP(ast)->obj_type,
				q->matcher, q->filter);

	default:
		sdb_log(SDB_LOG_ERR, "store: Invalid query of type %s",
				SDB_AST_TYPE_TO_STRING(ast));
		return -1;
	}

	return 0;
} /* sdb_store_query_execute */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */
