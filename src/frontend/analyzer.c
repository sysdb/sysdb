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

#include <core/store-private.h>
#include <frontend/connection-private.h>
#include <frontend/parser.h>

#include <assert.h>

/*
 * private helper functions
 */

static int
analyze_matcher(int context, sdb_store_matcher_t *m)
{
	int status = 0;

	if (! m)
		return 0;

	switch (m->type) {
		case MATCHER_OR:
		case MATCHER_AND:
			assert(OP_M(m)->left && OP_M(m)->right);
			if (analyze_matcher(context, OP_M(m)->left))
				status = -1;
			if (analyze_matcher(context, OP_M(m)->right))
				status = -1;
			break;

		case MATCHER_NOT:
			assert(UOP_M(m)->op);
			if (analyze_matcher(context, UOP_M(m)->op))
				status = -1;
			break;

		case MATCHER_ANY:
		case MATCHER_ALL:
			assert(ITER_M(m)->m);
			if (ITER_M(m)->type == context)
				status = -1;
			if ((context != SDB_HOST)
					&& (context != SDB_SERVICE)
					&& (context != SDB_METRIC))
				status = -1;
			if ((ITER_M(m)->type != SDB_SERVICE)
					&& (ITER_M(m)->type != SDB_METRIC)
					&& (ITER_M(m)->type != SDB_ATTRIBUTE))
				status = -1;
			if ((context == SDB_SERVICE)
					&& (ITER_M(m)->type == SDB_METRIC))
				status = -1;
			else if ((context == SDB_METRIC)
					&& (ITER_M(m)->type == SDB_SERVICE))
				status = -1;
			if (analyze_matcher(ITER_M(m)->type, ITER_M(m)->m))
				status = -1;
			break;

		case MATCHER_LT:
		case MATCHER_LE:
		case MATCHER_EQ:
		case MATCHER_NE:
		case MATCHER_GE:
		case MATCHER_GT:
			assert(CMP_M(m)->left && CMP_M(m)->right);
			if ((CMP_M(m)->left->data_type > 0)
					&& (CMP_M(m)->left->data_type & SDB_TYPE_ARRAY))
				status = -1;
			if ((CMP_M(m)->right->data_type > 0)
					&& (CMP_M(m)->right->data_type & SDB_TYPE_ARRAY))
				status = -1;
			break;

		case MATCHER_IN:
			if ((CMP_M(m)->left->data_type > 0)
					&& (CMP_M(m)->left->data_type & SDB_TYPE_ARRAY))
				status = -1;
			if ((CMP_M(m)->right->data_type > 0)
					&& (! (CMP_M(m)->right->data_type & SDB_TYPE_ARRAY)))
				status = -1;
			break;

		case MATCHER_REGEX:
		case MATCHER_NREGEX:
			/* all types are supported for the left operand */
			if ((CMP_M(m)->right->data_type > 0)
					&& (CMP_M(m)->right->data_type != SDB_TYPE_REGEX)
					&& (CMP_M(m)->right->data_type != SDB_TYPE_STRING))
				status = -1;
			break;

		case MATCHER_ISNULL:
		case MATCHER_ISNNULL:
			break;

		default:
			return -1;
	}
	return status;
} /* analyze_matcher */

/*
 * public API
 */

int
sdb_fe_analyze(sdb_conn_node_t *node)
{
	sdb_store_matcher_t *m = NULL, *filter = NULL;
	int context = -1;
	int status = 0;

	if (! node)
		return -1;

	/* For now, this function checks basic matcher attributes only;
	 * later, this may be turned into one of multiple AST visitors. */
	if (node->cmd == CONNECTION_FETCH) {
		if (CONN_FETCH(node)->filter)
			filter = CONN_FETCH(node)->filter->matcher;
		context = CONN_FETCH(node)->type;
	}
	else if (node->cmd == CONNECTION_LIST) {
		if (CONN_LIST(node)->filter)
			filter = CONN_LIST(node)->filter->matcher;
		context = CONN_LIST(node)->type;
	}
	else if (node->cmd == CONNECTION_LOOKUP) {
		if (CONN_LOOKUP(node)->matcher)
			m = CONN_LOOKUP(node)->matcher->matcher;
		if (CONN_LOOKUP(node)->filter)
			filter = CONN_LOOKUP(node)->filter->matcher;
		context = CONN_LOOKUP(node)->type;
	}
	else if (node->cmd == CONNECTION_TIMESERIES)
		return 0;
	else
		return -1;

	if (analyze_matcher(context, m))
		status = -1;
	if (analyze_matcher(-1, filter))
		status = -1;
	return status;
} /* sdb_fe_analyze */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

