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

static int
analyze_node(int context, sdb_ast_node_t *node, sdb_strbuf_t *errbuf);

/*
 * expression nodes
 */

static int
analyze_logical(int context, sdb_ast_op_t *op, sdb_strbuf_t *errbuf)
{
	switch (op->kind) {
	case SDB_AST_OR:
	case SDB_AST_AND:
		if (analyze_node(context, op->left, errbuf))
			return -1;
		/* fallthrough */
	case SDB_AST_NOT:
		if (analyze_node(context, op->right, errbuf))
			return -1;
		break;

	case SDB_AST_LT:
	case SDB_AST_LE:
	case SDB_AST_EQ:
	case SDB_AST_NE:
	case SDB_AST_GE:
	case SDB_AST_GT:
	{
		if (analyze_node(context, op->left, errbuf))
			return -1;
		if (analyze_node(context, op->right, errbuf))
			return -1;
		break;
	}

	case SDB_AST_REGEX:
	case SDB_AST_NREGEX:
		if (analyze_node(context, op->left, errbuf))
			return -1;
		if (analyze_node(context, op->right, errbuf))
			return -1;
		break;

	case SDB_AST_ISNULL:
		if (analyze_node(context, op->right, errbuf))
			return -1;
		break;

	case SDB_AST_IN:
		if (analyze_node(context, op->left, errbuf))
			return -1;
		if (analyze_node(context, op->right, errbuf))
			return -1;
		break;

	default:
		sdb_strbuf_sprintf(errbuf, "Unknown matcher type %d", op->kind);
		return -1;
	}
	return 0;
} /* analyze_logical */

static int
analyze_arith(int context, sdb_ast_op_t *op, sdb_strbuf_t *errbuf)
{
	if (analyze_node(context, op->left, errbuf))
		return -1;
	if (analyze_node(context, op->right, errbuf))
		return -1;
	SDB_AST_NODE(op)->data_type = sdb_data_expr_type(SDB_AST_OP_TO_DATA_OP(op->kind),
			op->left->data_type, op->right->data_type);

	/* TODO: replace constant arithmetic operations with a constant value */
	return 0;
} /* analyze_arith */

static int
analyze_iter(int context, sdb_ast_iter_t *iter, sdb_strbuf_t *errbuf)
{
	sdb_ast_const_t c = SDB_AST_CONST_INIT;
	int status;

	if (analyze_node(context, iter->iter, errbuf))
		return -1;
	c.super.data_type = iter->iter->data_type;

	/* TODO: support other setups as well */
	assert((iter->expr->type == SDB_AST_TYPE_OPERATOR)
			&& (! SDB_AST_OP(iter->expr)->left));
	SDB_AST_OP(iter->expr)->left = SDB_AST_NODE(&c);
	status = analyze_node(context, iter->expr, errbuf);
	SDB_AST_OP(iter->expr)->left = NULL;
	if (status)
		return -1;
	return 0;
} /* analyze_iter */

static int
analyze_const(int __attribute__((unused)) context, sdb_ast_const_t *c,
		sdb_strbuf_t __attribute__((unused)) *errbuf)
{
	SDB_AST_NODE(c)->data_type = c->value.type;
	return 0;
} /* analyze_const */

static int
analyze_value(int __attribute__((unused)) context, sdb_ast_value_t *v,
		sdb_strbuf_t __attribute__((unused)) *errbuf)
{
	if (v->type != SDB_ATTRIBUTE)
		SDB_AST_NODE(v)->data_type = SDB_FIELD_TYPE(v->type);
	return 0;
} /* analyze_value */

static int
analyze_typed(int __attribute__((unused)) context, sdb_ast_typed_t *t,
		sdb_strbuf_t *errbuf)
{
	if (analyze_node(t->type, t->expr, errbuf))
		return -1;
	SDB_AST_NODE(t)->data_type = t->expr->data_type;
	return 0;
} /* analyze_typed */

static int
analyze_node(int context, sdb_ast_node_t *node, sdb_strbuf_t *errbuf)
{
	if (! node) {
		sdb_strbuf_sprintf(errbuf, "Empty AST node");
		return -1;
	}

	/* unknown by default */
	node->data_type = -1;

	if ((node->type == SDB_AST_TYPE_OPERATOR)
			&& (SDB_AST_IS_LOGICAL(node)))
		return analyze_logical(context, SDB_AST_OP(node), errbuf);
	else if ((node->type == SDB_AST_TYPE_OPERATOR)
			&& (SDB_AST_IS_ARITHMETIC(node)))
		return analyze_arith(context, SDB_AST_OP(node), errbuf);
	else if (node->type == SDB_AST_TYPE_ITERATOR)
		return analyze_iter(context, SDB_AST_ITER(node), errbuf);
	else if (node->type == SDB_AST_TYPE_CONST)
		return analyze_const(context, SDB_AST_CONST(node), errbuf);
	else if (node->type == SDB_AST_TYPE_VALUE)
		return analyze_value(context, SDB_AST_VALUE(node), errbuf);
	else if (node->type == SDB_AST_TYPE_TYPED)
		return analyze_typed(context, SDB_AST_TYPED(node), errbuf);

	sdb_strbuf_sprintf(errbuf, "Invalid expression node "
			"of type %#x", node->type);
	return -1;
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
} /* analyze_fetch */

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
} /* analyze_list */

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
} /* analyze_lookup */

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
} /* analyze_store */

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
} /* analyze_timeseries */

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

	/* top-level nodes don't have a type */
	node->data_type = -1;

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
} /* sdb_parser_analyze */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

