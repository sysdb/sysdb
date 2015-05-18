/*
 * SysDB - src/core/store_query.c
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
#include "core/store-private.h"
#include "parser/ast.h"
#include "utils/error.h"

#include <assert.h>

static sdb_store_matcher_t *
node_to_matcher(sdb_ast_node_t *n);

static sdb_store_expr_t *
node_to_expr(sdb_ast_node_t *n)
{
	sdb_store_expr_t *left = NULL, *right = NULL;
	sdb_store_expr_t *e;
	int op;

	if (! n) {
		sdb_log(SDB_LOG_ERR, "store: Encountered empty AST expression node");
		return NULL;
	}

	switch (n->type) {
	case SDB_AST_TYPE_OPERATOR:
		if (! SDB_AST_IS_ARITHMETIC(n)) {
			sdb_log(SDB_LOG_ERR, "store: Invalid arithmetic operator of "
					"type %s (%#x)", SDB_AST_TYPE_TO_STRING(n), n->type);
			return NULL;
		}

		left = node_to_expr(SDB_AST_OP(n)->left);
		if (! left)
			return NULL;
		right = node_to_expr(SDB_AST_OP(n)->right);
		if (! right) {
			sdb_object_deref(SDB_OBJ(left));
			return NULL;
		}
		op = SDB_AST_OP_TO_DATA_OP(SDB_AST_OP(n)->kind);
		e = sdb_store_expr_create(op, left, right);
		break;

	case SDB_AST_TYPE_CONST:
		return sdb_store_expr_constvalue(&SDB_AST_CONST(n)->value);

	case SDB_AST_TYPE_VALUE:
		if (SDB_AST_VALUE(n)->type == SDB_ATTRIBUTE)
			return sdb_store_expr_attrvalue(SDB_AST_VALUE(n)->name);
		return sdb_store_expr_fieldvalue(SDB_AST_VALUE(n)->type);

	case SDB_AST_TYPE_TYPED:
		right = node_to_expr(SDB_AST_TYPED(n)->expr);
		if (! right)
			return NULL;
		e = sdb_store_expr_typed(SDB_AST_TYPED(n)->type, right);
		break;

	default:
		sdb_log(SDB_LOG_ERR, "store: Invalid matcher node of type %s (%#x)",
				SDB_AST_TYPE_TO_STRING(n), n->type);
		e = NULL;
	}

	/* expressions take a reference */
	sdb_object_deref(SDB_OBJ(left));
	sdb_object_deref(SDB_OBJ(right));
	return e;
} /* node_to_expr */

static sdb_store_matcher_t *
logical_to_matcher(sdb_ast_node_t *n)
{
	sdb_store_matcher_t *left = NULL, *right;
	sdb_store_matcher_t *m;

	if (SDB_AST_OP(n)->left) {
		left = node_to_matcher(SDB_AST_OP(n)->left);
		if (! left)
			return NULL;
	}
	right = node_to_matcher(SDB_AST_OP(n)->right);
	if (! right) {
		sdb_object_deref(SDB_OBJ(left));
		return NULL;
	}

	switch (SDB_AST_OP(n)->kind) {
	case SDB_AST_AND:
		m = sdb_store_con_matcher(left, right);
		break;
	case SDB_AST_OR:
		m = sdb_store_dis_matcher(left, right);
		break;
	case SDB_AST_NOT:
		m = sdb_store_inv_matcher(right);
		break;

	default:
		m = NULL;
	}

	/* matchers take a reference */
	sdb_object_deref(SDB_OBJ(left));
	sdb_object_deref(SDB_OBJ(right));
	return m;
} /* logical_to_matcher */

static sdb_store_matcher_t *
cmp_to_matcher(sdb_ast_node_t *n)
{
	sdb_store_expr_t *left = NULL, *right;
	sdb_store_matcher_t *m;

	if (SDB_AST_OP(n)->left) {
		left = node_to_expr(SDB_AST_OP(n)->left);
		if (! left)
			return NULL;
	}
	right = node_to_expr(SDB_AST_OP(n)->right);
	if (! right) {
		sdb_object_deref(SDB_OBJ(left));
		return NULL;
	}

	switch (SDB_AST_OP(n)->kind) {
	case SDB_AST_LT:
		m = sdb_store_lt_matcher(left, right);
		break;
	case SDB_AST_LE:
		m = sdb_store_le_matcher(left, right);
		break;
	case SDB_AST_EQ:
		m = sdb_store_eq_matcher(left, right);
		break;
	case SDB_AST_NE:
		m = sdb_store_ne_matcher(left, right);
		break;
	case SDB_AST_GE:
		m = sdb_store_ge_matcher(left, right);
		break;
	case SDB_AST_GT:
		m = sdb_store_gt_matcher(left, right);
		break;
	case SDB_AST_REGEX:
		m = sdb_store_regex_matcher(left, right);
		break;
	case SDB_AST_NREGEX:
		m = sdb_store_nregex_matcher(left, right);
		break;
	case SDB_AST_ISNULL:
		m = sdb_store_isnull_matcher(right);
		break;
	case SDB_AST_IN:
		m = sdb_store_in_matcher(left, right);
		break;

	default:
		sdb_log(SDB_LOG_ERR, "store: Invalid matcher node of type %s (%#x)",
				SDB_AST_TYPE_TO_STRING(n), n->type);
		m = NULL;
	}

	/* matchers take a reference */
	sdb_object_deref(SDB_OBJ(left));
	sdb_object_deref(SDB_OBJ(right));
	return m;
} /* cmp_to_matcher */

static sdb_store_matcher_t *
iter_to_matcher(sdb_ast_node_t *n)
{
	sdb_store_expr_t *iter;
	sdb_store_matcher_t *expr, *m;

	assert((SDB_AST_ITER(n)->expr->type == SDB_AST_TYPE_OPERATOR)
			&& (! SDB_AST_OP(SDB_AST_ITER(n)->expr)->left));

	iter = node_to_expr(SDB_AST_ITER(n)->iter);
	if (! iter)
		return NULL;
	expr = cmp_to_matcher(SDB_AST_ITER(n)->expr);
	if (! expr) {
		sdb_object_deref(SDB_OBJ(iter));
		return NULL;
	}

	switch (SDB_AST_ITER(n)->kind) {
	case SDB_AST_ALL:
		m = sdb_store_all_matcher(iter, expr);
		break;
	case SDB_AST_ANY:
		m = sdb_store_any_matcher(iter, expr);
		break;

	default:
		sdb_log(SDB_LOG_ERR, "store: Invalid iterator node of type %s (%#x)",
				SDB_AST_OP_TO_STRING(SDB_AST_ITER(n)->kind), SDB_AST_ITER(n)->kind);
		m = NULL;
	}

	/* matchers take a reference */
	sdb_object_deref(SDB_OBJ(iter));
	sdb_object_deref(SDB_OBJ(expr));
	return m;
} /* iter_to_matcher */

static sdb_store_matcher_t *
node_to_matcher(sdb_ast_node_t *n)
{
	int kind;

	if (! n) {
		sdb_log(SDB_LOG_ERR, "store: Encountered empty AST matcher node");
		return NULL;
	}

	switch (n->type) {
	case SDB_AST_TYPE_OPERATOR:
		if (! SDB_AST_IS_LOGICAL(n)) {
			sdb_log(SDB_LOG_ERR, "store: Invalid logical operator of "
					"type %s (%#x)", SDB_AST_TYPE_TO_STRING(n), n->type);
			return NULL;
		}

		kind = SDB_AST_OP(n)->kind;
		if ((kind == SDB_AST_AND) || (kind == SDB_AST_OR) || (kind == SDB_AST_NOT))
			return logical_to_matcher(n);
		else
			return cmp_to_matcher(n);

	case SDB_AST_TYPE_ITERATOR:
		return iter_to_matcher(n);
	}

	sdb_log(SDB_LOG_ERR, "store: Invalid matcher node of type %s (%#x)",
			SDB_AST_TYPE_TO_STRING(n), n->type);
	return NULL;
} /* node_to_matcher */

/*
 * query type
 */

static int
query_init(sdb_object_t *obj, va_list ap)
{
	sdb_ast_node_t *ast = va_arg(ap, sdb_ast_node_t *);
	sdb_ast_node_t *matcher = NULL, *filter = NULL;

	QUERY(obj)->ast = ast;
	sdb_object_ref(SDB_OBJ(ast));

	switch (ast->type) {
	case SDB_AST_TYPE_FETCH:
		filter = SDB_AST_FETCH(ast)->filter;
		break;
	case SDB_AST_TYPE_LIST:
		filter = SDB_AST_LIST(ast)->filter;
		break;
	case SDB_AST_TYPE_LOOKUP:
		matcher = SDB_AST_LOOKUP(ast)->matcher;
		filter = SDB_AST_LOOKUP(ast)->filter;
		break;
	case SDB_AST_TYPE_STORE:
	case SDB_AST_TYPE_TIMESERIES:
		/* nothing to do */
		break;

	default:
		sdb_log(SDB_LOG_ERR, "store: Invalid top-level AST node "
				"of type %#x", ast->type);
		return -1;
	}

	if (matcher) {
		QUERY(obj)->matcher = node_to_matcher(matcher);
		if (! QUERY(obj)->matcher)
			return -1;
	}
	if (filter) {
		QUERY(obj)->filter = node_to_matcher(filter);
		if (! QUERY(obj)->filter)
			return -1;
	}

	return 0;
} /* query_init */

static void
query_destroy(sdb_object_t *obj)
{
	sdb_object_deref(SDB_OBJ(QUERY(obj)->ast));
	sdb_object_deref(SDB_OBJ(QUERY(obj)->matcher));
	sdb_object_deref(SDB_OBJ(QUERY(obj)->filter));
} /* query_destroy */

static sdb_type_t query_type = {
	/* size = */ sizeof(sdb_store_query_t),
	/* init = */ query_init,
	/* destroy = */ query_destroy,
};

/*
 * public API
 */

sdb_store_query_t *
sdb_store_query_prepare(sdb_ast_node_t *ast)
{
	if (! ast)
		return NULL;
	return QUERY(sdb_object_create(SDB_AST_TYPE_TO_STRING(ast), query_type, ast));
} /* sdb_store_query_prepare */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */
