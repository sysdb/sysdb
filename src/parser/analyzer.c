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
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#define VALID_OBJ_TYPE(t) ((SDB_HOST <= (t)) && ((t) <= SDB_METRIC))

typedef struct {
	int type;
	bool iter;
} context_t;

#define FILTER_CONTEXT -1
static const context_t FILTER_CTX = { FILTER_CONTEXT, 0 };

static int
analyze_node(context_t ctx, sdb_ast_node_t *node, sdb_strbuf_t *errbuf);

/*
 * error reporting
 */

static void
op_error(sdb_strbuf_t *errbuf, sdb_ast_op_t *op, const char *reason)
{
	sdb_strbuf_sprintf(errbuf, "Invalid operation %s %s %s (%s)",
			SDB_TYPE_TO_STRING(op->left->data_type),
			SDB_AST_OP_TO_STRING(op->kind),
			SDB_TYPE_TO_STRING(op->right->data_type),
			reason);
} /* op_error */

static void
__attribute__((format(printf, 3, 4)))
iter_error(sdb_strbuf_t *errbuf, sdb_ast_iter_t *iter, const char *reason, ...)
{
	char r[1024];
	va_list ap;

	va_start(ap, reason);
	vsnprintf(r, sizeof(r), reason, ap);
	va_end(ap);

	assert((iter->expr->type == SDB_AST_TYPE_OPERATOR)
			&& (! SDB_AST_OP(iter->expr)->left));
	sdb_strbuf_sprintf(errbuf, "Invalid iterator %s %s %s %s (%s)",
			SDB_AST_OP_TO_STRING(iter->kind),
			SDB_TYPE_TO_STRING(iter->iter->data_type),
			SDB_AST_OP_TO_STRING(SDB_AST_OP(iter->expr)->kind),
			SDB_TYPE_TO_STRING(SDB_AST_OP(iter->expr)->right->data_type),
			r);
} /* iter_error */

/*
 * expression nodes
 */

static int
analyze_logical(context_t ctx, sdb_ast_op_t *op, sdb_strbuf_t *errbuf)
{
	if (ctx.iter) {
		op_error(errbuf, op, "cannot evaluate in iterator context");
		return -1;
	}

	switch (op->kind) {
	case SDB_AST_OR:
	case SDB_AST_AND:
		if (! SDB_AST_IS_LOGICAL(op->left)) {
			sdb_strbuf_sprintf(errbuf, "Invalid left operand (%s) "
					"in %s expression", SDB_AST_TYPE_TO_STRING(op->left),
					SDB_AST_OP_TO_STRING(op->kind));
			return -1;
		}
		if (analyze_node(ctx, op->left, errbuf))
			return -1;
		/* fallthrough */
	case SDB_AST_NOT:
		if (! SDB_AST_IS_LOGICAL(op->right)) {
			sdb_strbuf_sprintf(errbuf, "Invalid right operand (%s) "
					"in %s expression", SDB_AST_TYPE_TO_STRING(op->right),
					SDB_AST_OP_TO_STRING(op->kind));
			return -1;
		}
		if (analyze_node(ctx, op->right, errbuf))
			return -1;
		break;

	case SDB_AST_LT:
	case SDB_AST_LE:
	case SDB_AST_EQ:
	case SDB_AST_NE:
	case SDB_AST_GE:
	case SDB_AST_GT:
	{
		if (analyze_node(ctx, op->left, errbuf))
			return -1;
		if (analyze_node(ctx, op->right, errbuf))
			return -1;

		if ((op->left->data_type > 0) && (op->right->data_type > 0)) {
			if (op->left->data_type == op->right->data_type)
				return 0;
			op_error(errbuf, op, "type mismatch");
			return -1;
		}
		if ((op->left->data_type > 0) && (op->left->data_type & SDB_TYPE_ARRAY)) {
			op_error(errbuf, op, "array not allowed");
			return -1;
		}
		if ((op->right->data_type > 0) && (op->right->data_type & SDB_TYPE_ARRAY)) {
			op_error(errbuf, op, "array not allowed");
			return -1;
		}
		break;
	}

	case SDB_AST_REGEX:
	case SDB_AST_NREGEX:
		if (analyze_node(ctx, op->left, errbuf))
			return -1;
		if (analyze_node(ctx, op->right, errbuf))
			return -1;

		/* all types are supported for the left operand
		 * TODO: introduce a cast operator if it's not a string */
		if ((op->right->data_type > 0)
				&& (op->right->data_type != SDB_TYPE_REGEX)
				&& (op->right->data_type != SDB_TYPE_STRING)) {
			op_error(errbuf, op, "invalid regex");
			return -1;
		}
		break;

	case SDB_AST_ISNULL:
	case SDB_AST_ISTRUE:
	case SDB_AST_ISFALSE:
		if (analyze_node(ctx, op->right, errbuf))
			return -1;
		break;

	case SDB_AST_IN:
		if (analyze_node(ctx, op->left, errbuf))
			return -1;
		if (analyze_node(ctx, op->right, errbuf))
			return -1;

		if ((op->right->data_type > 0) && (! (op->right->data_type & SDB_TYPE_ARRAY))) {
			op_error(errbuf, op, "array expected");
			return -1;
		}
		/* the left operand may be a scalar or an array but the element
		 * type has to match */
		if ((op->left->data_type > 0) && (op->right->data_type > 0)
				&& ((op->left->data_type & 0xff) != (op->right->data_type & 0xff))) {
			op_error(errbuf, op, "type mismatch");
			return -1;
		}
		break;

	default:
		sdb_strbuf_sprintf(errbuf, "Unknown operand type %d", op->kind);
		return -1;
	}
	return 0;
} /* analyze_logical */

static int
analyze_arith(context_t ctx, sdb_ast_op_t *op, sdb_strbuf_t *errbuf)
{
	if (analyze_node(ctx, op->left, errbuf))
		return -1;
	if (analyze_node(ctx, op->right, errbuf))
		return -1;
	SDB_AST_NODE(op)->data_type = sdb_data_expr_type(SDB_AST_OP_TO_DATA_OP(op->kind),
			op->left->data_type, op->right->data_type);

	if ((op->left->data_type > 0) && (op->right->data_type > 0)
			&& (SDB_AST_NODE(op)->data_type <= 0)) {
		op_error(errbuf, op, "type mismatch");
		return -1;
	}

	/* TODO: replace constant arithmetic operations with a constant value */
	return 0;
} /* analyze_arith */

static int
analyze_iter(context_t ctx, sdb_ast_iter_t *iter, sdb_strbuf_t *errbuf)
{
	sdb_ast_const_t c = SDB_AST_CONST_INIT;
	context_t iter_ctx = ctx;
	int status;

	if (ctx.iter) {
		iter_error(errbuf, iter, "nested iterators are not supported");
		return -1;
	}

	iter_ctx.iter = 1;
	if (analyze_node(iter_ctx, iter->iter, errbuf))
		return -1;

	if (iter->iter->data_type > 0) {
		if (! (iter->iter->data_type & SDB_TYPE_ARRAY)) {
			iter_error(errbuf, iter, "cannot iterate values of type %s",
					SDB_TYPE_TO_STRING(iter->iter->data_type));
			return -1;
		}
		c.value.type = iter->iter->data_type & 0xff;
	}

	/* TODO: support other setups as well */
	assert((iter->expr->type == SDB_AST_TYPE_OPERATOR)
			&& (! SDB_AST_OP(iter->expr)->left));

	SDB_AST_OP(iter->expr)->left = SDB_AST_NODE(&c);
	status = analyze_node(ctx, iter->expr, errbuf);
	SDB_AST_OP(iter->expr)->left = NULL;
	if (status)
		return -1;
	return 0;
} /* analyze_iter */

static int
analyze_const(context_t __attribute__((unused)) ctx, sdb_ast_const_t *c,
		sdb_strbuf_t __attribute__((unused)) *errbuf)
{
	SDB_AST_NODE(c)->data_type = c->value.type;
	return 0;
} /* analyze_const */

static int
analyze_value(context_t ctx, sdb_ast_value_t *v, sdb_strbuf_t *errbuf)
{
	if (v->type != SDB_ATTRIBUTE)
		SDB_AST_NODE(v)->data_type = SDB_FIELD_TYPE(v->type);

	if ((v->type != SDB_ATTRIBUTE) && v->name) {
		sdb_strbuf_sprintf(errbuf, "Invalid expression %s[%s]",
				SDB_FIELD_TO_NAME(v->type), v->name);
		return -1;
	}
	else if ((v->type == SDB_ATTRIBUTE) && (! v->name)) {
		sdb_strbuf_sprintf(errbuf, "Invalid expression attribute[] "
				"(missing name)");
		return -1;
	}

	/* this would be caught by the type check in analyze_iter but we're able
	 * to provide a more specific error message here */
	if (ctx.iter && (v->type != SDB_FIELD_BACKEND)) {
		/* only backend values are iterable */
		char value_str[64 + (v->name ? strlen(v->name) : 0)];
		if (v->type == SDB_ATTRIBUTE)
			snprintf(value_str, sizeof(value_str), "attribute[%s]", v->name);
		else
			snprintf(value_str, sizeof(value_str), "'%s'", SDB_FIELD_TO_NAME(v->type));
		sdb_strbuf_sprintf(errbuf, "Cannot iterate %s (scalar value)", value_str);
		return -1;
	}

	if ((ctx.type != SDB_ATTRIBUTE) && (v->type == SDB_FIELD_VALUE)) {
		sdb_strbuf_sprintf(errbuf, "Invalid expression %s.value",
				SDB_FIELD_TO_NAME(ctx.type));
		return -1;
	}
	if ((ctx.type != SDB_METRIC) && (v->type == SDB_FIELD_TIMESERIES)) {
		sdb_strbuf_sprintf(errbuf, "Invalid expression %s.timeseries",
				SDB_FIELD_TO_NAME(ctx.type));
		return -1;
	}
	return 0;
} /* analyze_value */

static int
analyze_typed(context_t ctx, sdb_ast_typed_t *t, sdb_strbuf_t *errbuf)
{
	context_t child_ctx = ctx;
	bool needs_iter = 0;
	bool valid = 1;

	if ((t->expr->type != SDB_AST_TYPE_VALUE)
			&& (t->expr->type != SDB_AST_TYPE_TYPED)) {
		sdb_strbuf_sprintf(errbuf, "Invalid expression %s.%s",
				SDB_STORE_TYPE_TO_NAME(t->type),
				SDB_AST_TYPE_TO_STRING(t->expr));
		return -1;
	}
	if ((t->type != SDB_ATTRIBUTE) && (! VALID_OBJ_TYPE(t->type))) {
		sdb_strbuf_sprintf(errbuf, "Invalid expression %#x.%s",
				t->type, SDB_AST_TYPE_TO_STRING(t->expr));
		return -1;
	}

	if (ctx.type > 0) {
		if ((ctx.type == t->type)
				|| ((t->type == SDB_HOST) && (ctx.type != SDB_ATTRIBUTE))) {
			/* self-references and references to the parent host are always fine */
		}
		else if (t->type == SDB_ATTRIBUTE) {
			/* references to attributes are always fine */
			needs_iter = 1;
		}
		else if ((ctx.type == SDB_HOST)
				&& ((t->type == SDB_SERVICE) || (t->type == SDB_METRIC))) {
			/* only hosts may reference services and metrics */
			needs_iter = 1;
		}
		else {
			valid = 0;
		}
	}
	else if (ctx.type == FILTER_CONTEXT) {
		if (t->type == SDB_ATTRIBUTE) {
			/* all objects have attributes */
			needs_iter = 1;
		}
		else if ((t->type == SDB_SERVICE) || (t->type == SDB_METRIC)) {
			/* these will be iterators for *some* operations;
			 * better forbid this altogether */
			valid = 0;
		}
	}

	if (needs_iter) {
		if (! ctx.iter)
			valid = 0;
		else
			child_ctx.iter = 0;
	} /* else: push ctx.iter down to the child node */

	if (! valid) {
		sdb_strbuf_sprintf(errbuf, "Invalid expression %s.%s in %s context",
				SDB_STORE_TYPE_TO_NAME(t->type),
				SDB_AST_TYPE_TO_STRING(t->expr),
				SDB_STORE_TYPE_TO_NAME(ctx.type));
		return -1;
	}

	child_ctx.type = t->type;
	if (analyze_node(child_ctx, t->expr, errbuf))
		return -1;
	SDB_AST_NODE(t)->data_type = t->expr->data_type;

	if (needs_iter && (SDB_AST_NODE(t)->data_type > 0)) {
		if (SDB_AST_NODE(t)->data_type & SDB_TYPE_ARRAY) {
			sdb_strbuf_sprintf(errbuf, "Cannot access array inside iterator");
			return -1;
		}
		/* Tell the caller that we're accessing an iterator. */
		SDB_AST_NODE(t)->data_type |= SDB_TYPE_ARRAY;
	}
	return 0;
} /* analyze_typed */

static int
analyze_node(context_t ctx, sdb_ast_node_t *node, sdb_strbuf_t *errbuf)
{
	if (! node) {
		sdb_strbuf_sprintf(errbuf, "Empty AST node");
		return -1;
	}

	/* unknown by default */
	node->data_type = -1;

	if ((node->type == SDB_AST_TYPE_OPERATOR)
			&& (SDB_AST_IS_LOGICAL(node)))
		return analyze_logical(ctx, SDB_AST_OP(node), errbuf);
	else if ((node->type == SDB_AST_TYPE_OPERATOR)
			&& (SDB_AST_IS_ARITHMETIC(node)))
		return analyze_arith(ctx, SDB_AST_OP(node), errbuf);
	else if (node->type == SDB_AST_TYPE_ITERATOR)
		return analyze_iter(ctx, SDB_AST_ITER(node), errbuf);
	else if (node->type == SDB_AST_TYPE_CONST)
		return analyze_const(ctx, SDB_AST_CONST(node), errbuf);
	else if (node->type == SDB_AST_TYPE_VALUE)
		return analyze_value(ctx, SDB_AST_VALUE(node), errbuf);
	else if (node->type == SDB_AST_TYPE_TYPED)
		return analyze_typed(ctx, SDB_AST_TYPED(node), errbuf);

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
		return analyze_node(FILTER_CTX, fetch->filter, errbuf);
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
		return analyze_node(FILTER_CTX, list->filter, errbuf);
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
	if (lookup->matcher) {
		context_t ctx = { lookup->obj_type, 0 };
		if (analyze_node(ctx, lookup->matcher, errbuf))
			return -1;
	}
	if (lookup->filter)
		return analyze_node(FILTER_CTX, lookup->filter, errbuf);
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
		sdb_strbuf_sprintf(errbuf, "Missing hostname in TIMESERIES command");
		return -1;
	}
	if (! ts->metric) {
		sdb_strbuf_sprintf(errbuf, "Missing metric name in TIMESERIES command");
		return -1;
	}
	if (ts->end <= ts->start) {
		char start_str[64], end_str[64];
		sdb_strftime(start_str, sizeof(start_str), ts->start);
		sdb_strftime(end_str, sizeof(end_str), ts->end);
		sdb_strbuf_sprintf(errbuf, "Start time (%s) greater than "
				"end time (%s) in TIMESERIES command", start_str, end_str);
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

int
sdb_parser_analyze_conditional(int context,
		sdb_ast_node_t *node, sdb_strbuf_t *errbuf)
{
	context_t ctx = { context, 0 };
	if (! VALID_OBJ_TYPE(context)) {
		sdb_strbuf_sprintf(errbuf, "Invalid object type %#x", context);
		return -1;
	}
	if (! node) {
		sdb_strbuf_sprintf(errbuf, "Empty conditional node");
		return -1;
	}
	if (! SDB_AST_IS_LOGICAL(node)) {
		sdb_strbuf_sprintf(errbuf, "Not a conditional node (got %s)",
				SDB_AST_TYPE_TO_STRING(node));
		return -1;
	}
	return analyze_node(ctx, node, errbuf);
} /* sdb_parser_analyze_conditional */

int
sdb_parser_analyze_arith(int context,
		sdb_ast_node_t *node, sdb_strbuf_t *errbuf)
{
	context_t ctx = { context, 0 };
	if (! VALID_OBJ_TYPE(context)) {
		sdb_strbuf_sprintf(errbuf, "Invalid object type %#x", context);
		return -1;
	}
	if (! node) {
		sdb_strbuf_sprintf(errbuf, "Empty arithmetic node");
		return -1;
	}
	if (! SDB_AST_IS_ARITHMETIC(node)) {
		sdb_strbuf_sprintf(errbuf, "Not an arithmetic node (got %s)",
				SDB_AST_TYPE_TO_STRING(node));
		return -1;
	}
	return analyze_node(ctx, node, errbuf);
} /* sdb_parser_analyze_arith */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

