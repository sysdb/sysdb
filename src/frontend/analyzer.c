/*
 * SysDB - src/frontend/analyzer.c
 * Copyright (C) 2014 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include "core/store-private.h"
#include "frontend/connection-private.h"
#include "frontend/parser.h"
#include "utils/error.h"
#include "utils/strbuf.h"

#include <assert.h>

/*
 * private helper functions
 */

static void
iter_error(sdb_strbuf_t *errbuf, int op, int oper, int context)
{
	sdb_strbuf_sprintf(errbuf, "Cannot use %s %s in %s context",
			MATCHER_SYM(op), SDB_STORE_TYPE_TO_NAME(oper),
			SDB_STORE_TYPE_TO_NAME(context));
} /* iter_error */

static void
iter_array_error(sdb_strbuf_t *errbuf, int op,
		int array_type, int cmp, int value_type)
{
	sdb_strbuf_sprintf(errbuf, "Invalid array iterator %s %s %s %s",
			MATCHER_SYM(op), SDB_TYPE_TO_STRING(array_type),
			MATCHER_SYM(cmp), SDB_TYPE_TO_STRING(value_type));
	if ((array_type & 0xff) != value_type)
		sdb_strbuf_append(errbuf, " (type mismatch)");
	else
		sdb_strbuf_append(errbuf, " (invalid operator)");
} /* iter_array_error */

static void
cmp_error(sdb_strbuf_t *errbuf, int op, int left, int right)
{
	sdb_strbuf_sprintf(errbuf, "Invalid operator %s for types %s and %s",
			MATCHER_SYM(op), SDB_TYPE_TO_STRING(left),
			SDB_TYPE_TO_STRING(right));
} /* cmp_error */

static int
analyze_expr(int context, sdb_store_expr_t *e, sdb_strbuf_t *errbuf)
{
	if (! e)
		return 0;

	if ((e->type < TYPED_EXPR) || (SDB_DATA_CONCAT < e->type)) {
		sdb_strbuf_sprintf(errbuf, "Invalid expression of type %d", e->type);
		return -1;
	}

	switch (e->type) {
		case TYPED_EXPR:
			if (analyze_expr((int)e->data.data.integer, e->left, errbuf))
				return -1;
			if (context == (int)e->data.data.integer)
				return 0;
			if ((e->data.data.integer == SDB_HOST) &&
					((context == SDB_SERVICE) || (context == SDB_METRIC)))
				return 0;
			sdb_strbuf_sprintf(errbuf, "Invalid expression %s.%s "
					"in %s context",
					SDB_STORE_TYPE_TO_NAME(e->data.data.integer),
					EXPR_TO_STRING(e->left), SDB_STORE_TYPE_TO_NAME(context));
			return -1;

		case ATTR_VALUE:
		case FIELD_VALUE:
		case 0:
			break;

		default:
			if (analyze_expr(context, e->left, errbuf))
				return -1;
			if (analyze_expr(context, e->right, errbuf))
				return -1;
			break;
	}
	return 0;
} /* analyze_expr */

static int
analyze_matcher(int context, sdb_store_matcher_t *m, sdb_strbuf_t *errbuf)
{
	if (! m)
		return 0;

	switch (m->type) {
		case MATCHER_OR:
		case MATCHER_AND:
			assert(OP_M(m)->left && OP_M(m)->right);
			if (analyze_matcher(context, OP_M(m)->left, errbuf))
				return -1;
			if (analyze_matcher(context, OP_M(m)->right, errbuf))
				return -1;
			break;

		case MATCHER_NOT:
			assert(UOP_M(m)->op);
			if (analyze_matcher(context, UOP_M(m)->op, errbuf))
				return -1;
			break;

		case MATCHER_ANY:
		case MATCHER_ALL:
			assert(ITER_M(m)->m);
			if ((context != SDB_HOST)
					&& (context != SDB_SERVICE)
					&& (context != SDB_METRIC)) {
				iter_error(errbuf, m->type, ITER_M(m)->type, context);
				return -1;
			}
			if (ITER_M(m)->type == context) {
				iter_error(errbuf, m->type, ITER_M(m)->type, context);
				return -1;
			}
			if ((ITER_M(m)->type != SDB_SERVICE)
					&& (ITER_M(m)->type != SDB_METRIC)
					&& (ITER_M(m)->type != SDB_ATTRIBUTE)
					&& (ITER_M(m)->type != SDB_FIELD_BACKEND)) {
				iter_error(errbuf, m->type, ITER_M(m)->type, context);
				return -1;
			}
			if ((context == SDB_SERVICE)
					&& (ITER_M(m)->type == SDB_METRIC)) {
				iter_error(errbuf, m->type, ITER_M(m)->type, context);
				return -1;
			}
			else if ((context == SDB_METRIC)
					&& (ITER_M(m)->type == SDB_SERVICE)) {
				iter_error(errbuf, m->type, ITER_M(m)->type, context);
				return -1;
			}
			if (ITER_M(m)->type == SDB_FIELD_BACKEND) {
				/* array iterators only support simple comparison atm */
				if ((ITER_M(m)->m->type != MATCHER_LT)
						&& (ITER_M(m)->m->type != MATCHER_LE)
						&& (ITER_M(m)->m->type != MATCHER_EQ)
						&& (ITER_M(m)->m->type != MATCHER_NE)
						&& (ITER_M(m)->m->type != MATCHER_GE)
						&& (ITER_M(m)->m->type != MATCHER_GT)
						&& (ITER_M(m)->m->type != MATCHER_REGEX)
						&& (ITER_M(m)->m->type != MATCHER_NREGEX)) {
					iter_array_error(errbuf, m->type,
							CMP_M(ITER_M(m)->m)->left->data_type,
							ITER_M(m)->m->type,
							CMP_M(ITER_M(m)->m)->right->data_type);
					return -1;
				}
				if (CMP_M(ITER_M(m)->m)->right->data_type < 0)
					return 0; /* skip further type checks */
				if (CMP_M(ITER_M(m)->m)->right->data_type & SDB_TYPE_ARRAY) {
					iter_array_error(errbuf, m->type,
							CMP_M(ITER_M(m)->m)->left->data_type,
							ITER_M(m)->m->type,
							CMP_M(ITER_M(m)->m)->right->data_type);
					return -1;
				}
				if ((CMP_M(ITER_M(m)->m)->left->data_type & 0xff)
						!= CMP_M(ITER_M(m)->m)->right->data_type) {
					iter_array_error(errbuf, m->type,
							CMP_M(ITER_M(m)->m)->left->data_type,
							ITER_M(m)->m->type,
							CMP_M(ITER_M(m)->m)->right->data_type);
					return -1;
				}
			}
			else if (analyze_matcher(ITER_M(m)->type, ITER_M(m)->m, errbuf))
				return -1;
			break;

		case MATCHER_LT:
		case MATCHER_LE:
		case MATCHER_EQ:
		case MATCHER_NE:
		case MATCHER_GE:
		case MATCHER_GT:
			assert(CMP_M(m)->left && CMP_M(m)->right);
			if (analyze_expr(context, CMP_M(m)->left, errbuf))
				return -1;
			if (analyze_expr(context, CMP_M(m)->right, errbuf))
				return -1;

			if ((CMP_M(m)->left->data_type > 0)
					&& (CMP_M(m)->right->data_type > 0)) {
				if (CMP_M(m)->left->data_type == CMP_M(m)->right->data_type)
					return 0;
				cmp_error(errbuf, m->type, CMP_M(m)->left->data_type,
						CMP_M(m)->right->data_type);
				return -1;
			}
			if ((CMP_M(m)->left->data_type > 0)
					&& (CMP_M(m)->left->data_type & SDB_TYPE_ARRAY)) {
				cmp_error(errbuf, m->type, CMP_M(m)->left->data_type,
						CMP_M(m)->right->data_type);
				return -1;
			}
			if ((CMP_M(m)->right->data_type > 0)
					&& (CMP_M(m)->right->data_type & SDB_TYPE_ARRAY)) {
				cmp_error(errbuf, m->type, CMP_M(m)->left->data_type,
						CMP_M(m)->right->data_type);
				return -1;
			}
			break;

		case MATCHER_IN:
			if (analyze_expr(context, CMP_M(m)->left, errbuf))
				return -1;
			if (analyze_expr(context, CMP_M(m)->right, errbuf))
				return -1;

			if ((CMP_M(m)->left->data_type > 0)
					&& (CMP_M(m)->left->data_type & SDB_TYPE_ARRAY)) {
				cmp_error(errbuf, m->type, CMP_M(m)->left->data_type,
						CMP_M(m)->right->data_type);
				return -1;
			}
			if ((CMP_M(m)->right->data_type > 0)
					&& (! (CMP_M(m)->right->data_type & SDB_TYPE_ARRAY))) {
				cmp_error(errbuf, m->type, CMP_M(m)->left->data_type,
						CMP_M(m)->right->data_type);
				return -1;
			}
			break;

		case MATCHER_REGEX:
		case MATCHER_NREGEX:
			if (analyze_expr(context, CMP_M(m)->left, errbuf))
				return -1;
			if (analyze_expr(context, CMP_M(m)->right, errbuf))
				return -1;

			/* all types are supported for the left operand */
			if ((CMP_M(m)->right->data_type > 0)
					&& (CMP_M(m)->right->data_type != SDB_TYPE_REGEX)
					&& (CMP_M(m)->right->data_type != SDB_TYPE_STRING)) {
				cmp_error(errbuf, m->type, CMP_M(m)->left->data_type,
						CMP_M(m)->right->data_type);
				return -1;
			}
			break;

		case MATCHER_ISNULL:
		case MATCHER_ISNNULL:
			if (analyze_expr(context, ISNULL_M(m)->expr, errbuf))
				return -1;
			break;

		default:
			sdb_strbuf_sprintf(errbuf, "Unknown matcher type %d", m->type);
			return -1;
	}
	return 0;
} /* analyze_matcher */

/*
 * public API
 */

int
sdb_fe_analyze(sdb_conn_node_t *node, sdb_strbuf_t *errbuf)
{
	sdb_store_matcher_t *m = NULL, *filter = NULL;
	int context = -1;
	int status = 0;

	if (! node)
		return -1;

	/* For now, this function checks basic matcher attributes only;
	 * later, this may be turned into one of multiple AST visitors. */
	if (node->cmd == SDB_CONNECTION_FETCH) {
		conn_fetch_t *fetch = CONN_FETCH(node);
		if ((fetch->type == SDB_HOST) && fetch->name) {
			sdb_strbuf_sprintf(errbuf, "Unexpected STRING '%s'", fetch->name);
			return -1;
		}
		if ((fetch->type != SDB_HOST) && (! fetch->name)) {
			sdb_strbuf_sprintf(errbuf, "Missing %s name",
					SDB_STORE_TYPE_TO_NAME(fetch->type));
			return -1;
		}
		if (fetch->filter)
			filter = fetch->filter->matcher;
		context = fetch->type;
	}
	else if (node->cmd == SDB_CONNECTION_LIST) {
		if (CONN_LIST(node)->filter)
			filter = CONN_LIST(node)->filter->matcher;
		context = CONN_LIST(node)->type;
	}
	else if (node->cmd == SDB_CONNECTION_LOOKUP) {
		if (CONN_LOOKUP(node)->matcher)
			m = CONN_LOOKUP(node)->matcher->matcher;
		if (CONN_LOOKUP(node)->filter)
			filter = CONN_LOOKUP(node)->filter->matcher;
		context = CONN_LOOKUP(node)->type;
	}
	else if ((node->cmd == SDB_CONNECTION_STORE_HOST)
			|| (node->cmd == SDB_CONNECTION_STORE_SERVICE)
			|| (node->cmd == SDB_CONNECTION_STORE_METRIC)
			|| (node->cmd == SDB_CONNECTION_STORE_ATTRIBUTE)) {
		return 0;
	}
	else if (node->cmd == SDB_CONNECTION_TIMESERIES) {
		return 0;
	}
	else {
		sdb_strbuf_sprintf(errbuf,
				"Don't know how to analyze command %#x", node->cmd);
		return -1;
	}

	if (analyze_matcher(context, m, errbuf))
		status = -1;
	if (analyze_matcher(-1, filter, errbuf))
		status = -1;
	return status;
} /* sdb_fe_analyze */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

