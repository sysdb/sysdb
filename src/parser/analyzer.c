/*
 * SysDB - src/parser/analyzer.c
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

#include "sysdb.h"

#include "parser/ast.h"
#include "parser/parser.h"
#include "utils/error.h"
#include "utils/strbuf.h"

#include <assert.h>

#define VALID_OBJ_TYPE(t) ((SDB_HOST <= (t)) && ((t) <= SDB_METRIC))

/*
 * private helper functions
 */

static int
analyze_node(int context, sdb_ast_node_t *node, sdb_strbuf_t *errbuf)
{
	(void)context;
	if (! node) {
		sdb_strbuf_sprintf(errbuf, "Empty AST node");
		return -1;
	}
	return 0;
} /* analyze_node */

/*
 * top level / command nodes
 */

static int
analyze_fetch(sdb_ast_fetch_t *fetch, sdb_strbuf_t *errbuf)
{
	if (! VALID_OBJ_TYPE(fetch->obj_type)) {
		sdb_strbuf_sprintf(errbuf, "Invalid object type %#x "
				"in FETCH command", fetch->obj_type);
		return -1;
	}
	if (! fetch->name) {
		sdb_strbuf_sprintf(errbuf, "Missing object name in "
				"FETCH %s command", SDB_STORE_TYPE_TO_NAME(fetch->obj_type));
		return -1;
	}

	if ((fetch->obj_type == SDB_HOST) && fetch->hostname) {
		sdb_strbuf_sprintf(errbuf, "Unexpected parent hostname '%s' "
				"in FETCH HOST command", fetch->hostname);
		return -1;
	}
	else if ((fetch->obj_type != SDB_HOST) && (! fetch->hostname)) {
		sdb_strbuf_sprintf(errbuf, "Missing parent hostname for '%s' "
				"in FETCH %s command", fetch->name,
				SDB_STORE_TYPE_TO_NAME(fetch->obj_type));
		return -1;
	}

	if (fetch->filter)
		return analyze_node(-1, fetch->filter, errbuf);
	return 0;
}

static int
analyze_list(sdb_ast_list_t *list, sdb_strbuf_t *errbuf)
{
	if (! VALID_OBJ_TYPE(list->obj_type)) {
		sdb_strbuf_sprintf(errbuf, "Invalid object type %#x "
				"in LIST command", list->obj_type);
		return -1;
	}
	if (list->filter)
		return analyze_node(-1, list->filter, errbuf);
	return 0;
}

static int
analyze_lookup(sdb_ast_lookup_t *lookup, sdb_strbuf_t *errbuf)
{
	if (! VALID_OBJ_TYPE(lookup->obj_type)) {
		sdb_strbuf_sprintf(errbuf, "Invalid object type %#x "
				"in LOOKUP command", lookup->obj_type);
		return -1;
	}
	if (lookup->matcher)
		if (analyze_node(lookup->obj_type, lookup->matcher, errbuf))
			return -1;
	if (lookup->filter)
		return analyze_node(-1, lookup->filter, errbuf);
	return 0;
}

static int
analyze_store(sdb_ast_store_t *st, sdb_strbuf_t *errbuf)
{
	if ((st->obj_type != SDB_ATTRIBUTE)
			&& (! VALID_OBJ_TYPE(st->obj_type))) {
		sdb_strbuf_sprintf(errbuf, "Invalid object type %#x "
				"in STORE command", st->obj_type);
		return -1;
	}
	if (! st->name) {
		sdb_strbuf_sprintf(errbuf, "Missing object name in "
				"STORE %s command", SDB_STORE_TYPE_TO_NAME(st->obj_type));
		return -1;
	}

	if ((st->obj_type == SDB_HOST) && st->hostname) {
		sdb_strbuf_sprintf(errbuf, "Unexpected parent hostname '%s' "
				"in STORE HOST command", st->hostname);
		return -1;
	}
	else if ((st->obj_type != SDB_HOST) && (! st->hostname)) {
		sdb_strbuf_sprintf(errbuf, "Missing parent hostname for '%s' "
				"in STORE %s command", st->name,
				SDB_STORE_TYPE_TO_NAME(st->obj_type));
		return -1;
	}

	if (st->obj_type == SDB_ATTRIBUTE) {
		if ((st->parent_type <= 0) && st->parent) {
			sdb_strbuf_sprintf(errbuf, "Unexpected parent hostname '%s' "
					"in STORE %s command", st->parent,
					SDB_STORE_TYPE_TO_NAME(st->obj_type));
			return -1;
		}
		else if (st->parent_type > 0) {
			if (! VALID_OBJ_TYPE(st->parent_type)) {
				sdb_strbuf_sprintf(errbuf, "Invalid parent type %#x "
						"in STORE %s command", st->parent_type,
						SDB_STORE_TYPE_TO_NAME(st->obj_type));
				return -1;
			}
			if (! st->parent) {
				sdb_strbuf_sprintf(errbuf, "Missing %s parent name "
						"in STORE %s command",
						SDB_STORE_TYPE_TO_NAME(st->parent_type),
						SDB_STORE_TYPE_TO_NAME(st->obj_type));
				return -1;
			}
		}
	}
	else if ((st->parent_type > 0) || st->parent) {
		sdb_strbuf_sprintf(errbuf, "Unexpected %s parent name '%s' "
				"in STORE %s command",
				SDB_STORE_TYPE_TO_NAME(st->parent_type),
				st->parent ? st->parent : "<unknown>",
				SDB_STORE_TYPE_TO_NAME(st->obj_type));
		return -1;
	}

	if (st->obj_type == SDB_METRIC) {
		if ((! st->store_type) != (! st->store_id)) {
			sdb_strbuf_sprintf(errbuf, "Incomplete metric store %s %s "
					"in STORE METRIC command",
					st->store_type ? st->store_type : "<unknown>",
					st->store_id ? st->store_id : "<unknown>");
			return -1;
		}
	}
	else if (st->store_type || st->store_id) {
		sdb_strbuf_sprintf(errbuf, "Unexpected metric store %s %s "
				"in STORE %s command",
				st->store_type ? st->store_type : "<unknown>",
				st->store_id ? st->store_id : "<unknown>",
				SDB_STORE_TYPE_TO_NAME(st->obj_type));
		return -1;
	}

	if ((! (st->obj_type == SDB_ATTRIBUTE))
			&& (st->value.type != SDB_TYPE_NULL)) {
		char v_str[sdb_data_format(&st->value, NULL, 0, SDB_DOUBLE_QUOTED) + 1];
		sdb_data_format(&st->value, v_str, sizeof(v_str), SDB_DOUBLE_QUOTED);
		sdb_strbuf_sprintf(errbuf, "Unexpected value %s in STORE %s command",
				v_str, SDB_STORE_TYPE_TO_NAME(st->obj_type));
		return -1;
	}
	return 0;
}

static int
analyze_timeseries(sdb_ast_timeseries_t *ts, sdb_strbuf_t *errbuf)
{
	if (! ts->hostname) {
		sdb_strbuf_sprintf(errbuf, "Missing hostname in STORE command");
		return -1;
	}
	if (! ts->metric) {
		sdb_strbuf_sprintf(errbuf, "Missing metric name in STORE command");
		return -1;
	}
	if (ts->end <= ts->start) {
		char start_str[64], end_str[64];
		sdb_strftime(start_str, sizeof(start_str), "%F %T Tz", ts->start);
		sdb_strftime(end_str, sizeof(end_str), "%F %T Tz", ts->end);
		sdb_strbuf_sprintf(errbuf, "Start time (%s) greater than "
				"end time (%s) in STORE command", start_str, end_str);
		return -1;
	}
	return 0;
}

/*
 * public API
 */

int
sdb_parser_analyze(sdb_ast_node_t *node, sdb_strbuf_t *errbuf)
{
	if (! node) {
		sdb_strbuf_sprintf(errbuf, "Empty AST node");
		return -1;
	}

	if (node->type == SDB_AST_TYPE_FETCH)
		return analyze_fetch(SDB_AST_FETCH(node), errbuf);
	else if (node->type == SDB_AST_TYPE_LIST)
		return analyze_list(SDB_AST_LIST(node), errbuf);
	else if (node->type == SDB_AST_TYPE_LOOKUP)
		return analyze_lookup(SDB_AST_LOOKUP(node), errbuf);
	else if (node->type == SDB_AST_TYPE_STORE)
		return analyze_store(SDB_AST_STORE(node), errbuf);
	else if (node->type == SDB_AST_TYPE_TIMESERIES)
		return analyze_timeseries(SDB_AST_TIMESERIES(node), errbuf);

	sdb_strbuf_sprintf(errbuf, "Invalid top-level AST node "
			"of type %#x", node->type);
	return -1;
} /* sdb_fe_analyze */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

