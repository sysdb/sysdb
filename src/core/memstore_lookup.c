/*
 * SysDB - src/core/memstore_lookup.c
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

/*
 * This module implements operators which may be used to select contents of
 * the store by matching various attributes of the stored objects. For now, a
 * simple full table scan is supported only.
 */

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "sysdb.h"
#include "core/memstore-private.h"
#include "core/object.h"
#include "utils/error.h"

#include <assert.h>

#include <sys/types.h>
#include <regex.h>

#include <stdlib.h>
#include <string.h>

#include <limits.h>

static int
expr_eval2(sdb_memstore_expr_t *e1, sdb_data_t *v1,
		sdb_memstore_expr_t *e2, sdb_data_t *v2,
		sdb_memstore_obj_t *obj, sdb_memstore_matcher_t *filter)
{
	if (e1->type) {
		if (sdb_memstore_expr_eval(e1, obj, v1, filter))
			return -1;
	}
	else
		*v1 = e1->data;
	if (e2->type) {
		if (sdb_memstore_expr_eval(e2, obj, v2, filter)) {
			if (e1->type)
				sdb_data_free_datum(v1);
			return -1;
		}
	}
	else
		*v2 = e2->data;
	return 0;
} /* expr_eval2 */

static void
expr_free_datum2(sdb_memstore_expr_t *e1, sdb_data_t *v1,
		sdb_memstore_expr_t *e2, sdb_data_t *v2)
{
	if (e1->type)
		sdb_data_free_datum(v1);
	if (e2->type)
		sdb_data_free_datum(v2);
} /* expr_free_datum2 */

/*
 * matcher implementations
 */

/*
 * cmp_expr:
 * Compare two values using the specified matcher operator. If strcmp_fallback
 * is enabled, compare the string values in case of a type mismatch.
 */
static int
match_cmp_value(int op, sdb_data_t *v1, sdb_data_t *v2, bool strcmp_fallback)
{
	int status;

	if (sdb_data_isnull(v1) || (sdb_data_isnull(v2)))
		status = INT_MAX;
	else if (v1->type == v2->type)
		status = sdb_data_cmp(v1, v2);
	else if (! strcmp_fallback)
		status = INT_MAX;
	else
		status = sdb_data_strcmp(v1, v2);

	if (status == INT_MAX)
		return 0;
	switch (op) {
		case MATCHER_LT: return status < 0;
		case MATCHER_LE: return status <= 0;
		case MATCHER_EQ: return status == 0;
		case MATCHER_NE: return status != 0;
		case MATCHER_GE: return status >= 0;
		case MATCHER_GT: return status > 0;
	}
	return 0;
} /* match_cmp_value */

static int
match_regex_value(int op, sdb_data_t *v, sdb_data_t *re)
{
	char value[sdb_data_strlen(v) + 1];
	int status = 0;

	assert((op == MATCHER_REGEX)
			|| (op == MATCHER_NREGEX));

	if (sdb_data_isnull(v) || sdb_data_isnull(re))
		return 0;

	if (re->type == SDB_TYPE_STRING) {
		sdb_data_t tmp = SDB_DATA_INIT;

		if (sdb_data_parse(re->data.string, SDB_TYPE_REGEX, &tmp))
			return 0;

		sdb_data_free_datum(re);
		*re = tmp;
	}
	else if (re->type != SDB_TYPE_REGEX)
		return 0;

	if (! sdb_data_format(v, value, sizeof(value), SDB_UNQUOTED))
		status = 0;
	else if (! regexec(&re->data.re.regex, value, 0, NULL, 0))
		status = 1;

	if (op == MATCHER_NREGEX)
		return !status;
	return status;
} /* match_regex_value */

static int
match_logical(sdb_memstore_matcher_t *m, sdb_memstore_obj_t *obj,
		sdb_memstore_matcher_t *filter)
{
	int status;

	assert((m->type == MATCHER_AND) || (m->type == MATCHER_OR));
	assert(OP_M(m)->left && OP_M(m)->right);

	status = sdb_memstore_matcher_matches(OP_M(m)->left, obj, filter);

	/* lazy evaluation */
	if ((! status) && (m->type == MATCHER_AND))
		return status;
	else if (status && (m->type == MATCHER_OR))
		return status;

	return sdb_memstore_matcher_matches(OP_M(m)->right, obj, filter);
} /* match_logical */

static int
match_uop(sdb_memstore_matcher_t *m, sdb_memstore_obj_t *obj,
		sdb_memstore_matcher_t *filter)
{
	assert(m->type == MATCHER_NOT);
	assert(UOP_M(m)->op);

	return !sdb_memstore_matcher_matches(UOP_M(m)->op, obj, filter);
} /* match_uop */

/* iterate: ANY/ALL <iter> <cmp> <value> */
static int
match_iter(sdb_memstore_matcher_t *m, sdb_memstore_obj_t *obj,
		sdb_memstore_matcher_t *filter)
{
	sdb_memstore_expr_iter_t *iter = NULL;
	int status;
	int all = (int)(m->type == MATCHER_ALL);

	assert((m->type == MATCHER_ANY) || (m->type == MATCHER_ALL));
	assert((! CMP_M(ITER_M(m)->m)->left) && CMP_M(ITER_M(m)->m)->right);

	iter = sdb_memstore_expr_iter(ITER_M(m)->iter, obj, filter);
	if (! iter) {
		sdb_log(SDB_LOG_WARNING, "memstore: Invalid iterator");
		return 0;
	}

	status = all;
	while (sdb_memstore_expr_iter_has_next(iter)) {
		sdb_data_t v = sdb_memstore_expr_iter_get_next(iter);
		sdb_memstore_expr_t expr = CONST_EXPR(v);
		bool matches;

		CMP_M(ITER_M(m)->m)->left = &expr;
		matches = sdb_memstore_matcher_matches(ITER_M(m)->m, obj, filter);
		CMP_M(ITER_M(m)->m)->left = NULL;
		sdb_data_free_datum(&v);

		if (matches) {
			if (! all) {
				status = 1;
				break;
			}
		} else if (all) {
			status = 0;
			break;
		}
	}
	sdb_memstore_expr_iter_destroy(iter);
	return status;
} /* match_iter */

static int
match_cmp(sdb_memstore_matcher_t *m, sdb_memstore_obj_t *obj,
		sdb_memstore_matcher_t *filter)
{
	sdb_memstore_expr_t *e1 = CMP_M(m)->left;
	sdb_memstore_expr_t *e2 = CMP_M(m)->right;
	sdb_data_t v1 = SDB_DATA_INIT, v2 = SDB_DATA_INIT;
	int status;

	assert((m->type == MATCHER_LT)
			|| (m->type == MATCHER_LE)
			|| (m->type == MATCHER_EQ)
			|| (m->type == MATCHER_NE)
			|| (m->type == MATCHER_GE)
			|| (m->type == MATCHER_GT));
	assert(e1 && e2);

	if (expr_eval2(e1, &v1, e2, &v2, obj, filter))
		return 0;

	status = match_cmp_value(m->type, &v1, &v2,
			(e1->data_type) < 0 || (e2->data_type < 0));

	expr_free_datum2(e1, &v1, e2, &v2);
	return status;
} /* match_cmp */

static int
match_in(sdb_memstore_matcher_t *m, sdb_memstore_obj_t *obj,
		sdb_memstore_matcher_t *filter)
{
	sdb_data_t value = SDB_DATA_INIT, array = SDB_DATA_INIT;
	int status = 1;

	assert(m->type == MATCHER_IN);
	assert(CMP_M(m)->left && CMP_M(m)->right);

	if (expr_eval2(CMP_M(m)->left, &value,
				CMP_M(m)->right, &array, obj, filter))
		status = 0;

	if (status)
		status = sdb_data_inarray(&value, &array);

	expr_free_datum2(CMP_M(m)->left, &value, CMP_M(m)->right, &array);
	return status;
} /* match_in */

static int
match_regex(sdb_memstore_matcher_t *m, sdb_memstore_obj_t *obj,
		sdb_memstore_matcher_t *filter)
{
	sdb_data_t regex = SDB_DATA_INIT, v = SDB_DATA_INIT;
	int status = 0;

	assert((m->type == MATCHER_REGEX)
			|| (m->type == MATCHER_NREGEX));
	assert(CMP_M(m)->left && CMP_M(m)->right);

	if (expr_eval2(CMP_M(m)->left, &v, CMP_M(m)->right, &regex, obj, filter))
		return 0;

	status = match_regex_value(m->type, &v, &regex);

	expr_free_datum2(CMP_M(m)->left, &v, CMP_M(m)->right, &regex);
	return status;
} /* match_regex */

static int
match_unary(sdb_memstore_matcher_t *m, sdb_memstore_obj_t *obj,
		sdb_memstore_matcher_t *filter)
{
	sdb_data_t v = SDB_DATA_INIT;
	int status;

	assert((m->type == MATCHER_ISNULL)
			|| (m->type == MATCHER_ISTRUE)
			|| (m->type == MATCHER_ISFALSE));

	if (UNARY_M(m)->expr->type) {
		/* TODO: this might hide real errors;
		 * improve error reporting and propagation */
		if (sdb_memstore_expr_eval(UNARY_M(m)->expr, obj, &v, filter))
			return 1;
	}
	else
		v = UNARY_M(m)->expr->data;

	if (m->type == MATCHER_ISNULL)
		status = sdb_data_isnull(&v) ? 1 : 0;
	else { /* ISTRUE or ISFALSE */
		if ((v.type == SDB_TYPE_BOOLEAN)
				&& (v.data.boolean == (m->type == MATCHER_ISTRUE)))
			status = 1;
		else
			status = 0;
	}

	if (UNARY_M(m)->expr->type)
		sdb_data_free_datum(&v);
	return status;
} /* match_unary */

typedef int (*matcher_cb)(sdb_memstore_matcher_t *, sdb_memstore_obj_t *,
		sdb_memstore_matcher_t *);

/* this array needs to be indexable by the matcher types;
 * -> update the enum in memstore-private.h when updating this */
static matcher_cb
matchers[] = {
	match_logical,
	match_logical,
	match_uop,
	match_iter,
	match_iter,
	match_in,

	/* unary operators */
	match_unary,
	match_unary,
	match_unary,

	/* ary operators */
	match_cmp,
	match_cmp,
	match_cmp,
	match_cmp,
	match_cmp,
	match_cmp,
	match_regex,
	match_regex,

	NULL, /* QUERY */
};

/*
 * private matcher types
 */

static int
op_matcher_init(sdb_object_t *obj, va_list ap)
{
	M(obj)->type = va_arg(ap, int);
	if ((M(obj)->type != MATCHER_OR) && (M(obj)->type != MATCHER_AND))
		return -1;

	OP_M(obj)->left = va_arg(ap, sdb_memstore_matcher_t *);
	sdb_object_ref(SDB_OBJ(OP_M(obj)->left));
	OP_M(obj)->right = va_arg(ap, sdb_memstore_matcher_t *);
	sdb_object_ref(SDB_OBJ(OP_M(obj)->right));

	if ((! OP_M(obj)->left) || (! OP_M(obj)->right))
		return -1;
	return 0;
} /* op_matcher_init */

static void
op_matcher_destroy(sdb_object_t *obj)
{
	if (OP_M(obj)->left)
		sdb_object_deref(SDB_OBJ(OP_M(obj)->left));
	if (OP_M(obj)->right)
		sdb_object_deref(SDB_OBJ(OP_M(obj)->right));
} /* op_matcher_destroy */

static int
iter_matcher_init(sdb_object_t *obj, va_list ap)
{
	M(obj)->type = va_arg(ap, int);
	ITER_M(obj)->iter = va_arg(ap, sdb_memstore_expr_t *);
	ITER_M(obj)->m = va_arg(ap, sdb_memstore_matcher_t *);

	sdb_object_ref(SDB_OBJ(ITER_M(obj)->iter));
	sdb_object_ref(SDB_OBJ(ITER_M(obj)->m));

	if ((! ITER_M(obj)->iter) || (! ITER_M(obj)->m))
		return -1;
	return 0;
} /* iter_matcher_init */

static void
iter_matcher_destroy(sdb_object_t *obj)
{
	sdb_object_deref(SDB_OBJ(ITER_M(obj)->iter));
	sdb_object_deref(SDB_OBJ(ITER_M(obj)->m));
} /* iter_matcher_destroy */

static int
cmp_matcher_init(sdb_object_t *obj, va_list ap)
{
	M(obj)->type = va_arg(ap, int);

	CMP_M(obj)->left = va_arg(ap, sdb_memstore_expr_t *);
	sdb_object_ref(SDB_OBJ(CMP_M(obj)->left));
	CMP_M(obj)->right = va_arg(ap, sdb_memstore_expr_t *);
	sdb_object_ref(SDB_OBJ(CMP_M(obj)->right));

	if (! CMP_M(obj)->right)
		return -1;
	return 0;
} /* cmp_matcher_init */

static void
cmp_matcher_destroy(sdb_object_t *obj)
{
	sdb_object_deref(SDB_OBJ(CMP_M(obj)->left));
	sdb_object_deref(SDB_OBJ(CMP_M(obj)->right));
} /* cmp_matcher_destroy */

static int
uop_matcher_init(sdb_object_t *obj, va_list ap)
{
	M(obj)->type = va_arg(ap, int);
	if (M(obj)->type != MATCHER_NOT)
		return -1;

	UOP_M(obj)->op = va_arg(ap, sdb_memstore_matcher_t *);
	sdb_object_ref(SDB_OBJ(UOP_M(obj)->op));

	if (! UOP_M(obj)->op)
		return -1;
	return 0;
} /* uop_matcher_init */

static void
uop_matcher_destroy(sdb_object_t *obj)
{
	if (UOP_M(obj)->op)
		sdb_object_deref(SDB_OBJ(UOP_M(obj)->op));
} /* uop_matcher_destroy */

static int
unary_matcher_init(sdb_object_t *obj, va_list ap)
{
	M(obj)->type = va_arg(ap, int);
	if ((M(obj)->type != MATCHER_ISNULL)
			&& (M(obj)->type != MATCHER_ISTRUE)
			&& (M(obj)->type != MATCHER_ISFALSE))
		return -1;

	UNARY_M(obj)->expr = va_arg(ap, sdb_memstore_expr_t *);
	sdb_object_ref(SDB_OBJ(UNARY_M(obj)->expr));
	return 0;
} /* unary_matcher_init */

static void
unary_matcher_destroy(sdb_object_t *obj)
{
	sdb_object_deref(SDB_OBJ(UNARY_M(obj)->expr));
	UNARY_M(obj)->expr = NULL;
} /* unary_matcher_destroy */

static sdb_type_t op_type = {
	/* size = */ sizeof(op_matcher_t),
	/* init = */ op_matcher_init,
	/* destroy = */ op_matcher_destroy,
};

static sdb_type_t uop_type = {
	/* size = */ sizeof(uop_matcher_t),
	/* init = */ uop_matcher_init,
	/* destroy = */ uop_matcher_destroy,
};

static sdb_type_t iter_type = {
	/* size = */ sizeof(iter_matcher_t),
	/* init = */ iter_matcher_init,
	/* destroy = */ iter_matcher_destroy,
};

static sdb_type_t cmp_type = {
	/* size = */ sizeof(cmp_matcher_t),
	/* init = */ cmp_matcher_init,
	/* destroy = */ cmp_matcher_destroy,
};

static sdb_type_t unary_type = {
	/* size = */ sizeof(unary_matcher_t),
	/* init = */ unary_matcher_init,
	/* destroy = */ unary_matcher_destroy,
};

/*
 * public API
 */

sdb_memstore_matcher_t *
sdb_memstore_any_matcher(sdb_memstore_expr_t *iter, sdb_memstore_matcher_t *m)
{
	if ((m->type < MATCHER_LT) || (MATCHER_NREGEX < m->type)) {
		sdb_log(SDB_LOG_ERR, "memstore: Invalid ANY -> %s matcher "
				"(invalid operator)", MATCHER_SYM(m->type));
		return NULL;
	}
	if (CMP_M(m)->left) {
		sdb_log(SDB_LOG_ERR, "memstore: Invalid ANY %s %s %s matcher "
				"(invalid left operand)",
				SDB_TYPE_TO_STRING(CMP_M(m)->left->data_type),
				MATCHER_SYM(m->type),
				SDB_TYPE_TO_STRING(CMP_M(m)->right->data_type));
		return NULL;
	}
	return M(sdb_object_create("any-matcher", iter_type,
				MATCHER_ANY, iter, m));
} /* sdb_memstore_any_matcher */

sdb_memstore_matcher_t *
sdb_memstore_all_matcher(sdb_memstore_expr_t *iter, sdb_memstore_matcher_t *m)
{
	if ((m->type < MATCHER_LT) || (MATCHER_NREGEX < m->type)) {
		sdb_log(SDB_LOG_ERR, "memstore: Invalid ALL -> %s matcher "
				"(invalid operator)", MATCHER_SYM(m->type));
		return NULL;
	}
	if (CMP_M(m)->left) {
		sdb_log(SDB_LOG_ERR, "memstore: Invalid ALL %s %s %s matcher "
				"(invalid left operand)",
				SDB_TYPE_TO_STRING(CMP_M(m)->left->data_type),
				MATCHER_SYM(m->type),
				SDB_TYPE_TO_STRING(CMP_M(m)->right->data_type));
		return NULL;
	}
	return M(sdb_object_create("all-matcher", iter_type,
				MATCHER_ALL, iter, m));
} /* sdb_memstore_all_matcher */

sdb_memstore_matcher_t *
sdb_memstore_lt_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right)
{
	return M(sdb_object_create("lt-matcher", cmp_type,
				MATCHER_LT, left, right));
} /* sdb_memstore_lt_matcher */

sdb_memstore_matcher_t *
sdb_memstore_le_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right)
{
	return M(sdb_object_create("le-matcher", cmp_type,
				MATCHER_LE, left, right));
} /* sdb_memstore_le_matcher */

sdb_memstore_matcher_t *
sdb_memstore_eq_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right)
{
	return M(sdb_object_create("eq-matcher", cmp_type,
				MATCHER_EQ, left, right));
} /* sdb_memstore_eq_matcher */

sdb_memstore_matcher_t *
sdb_memstore_ne_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right)
{
	return M(sdb_object_create("ne-matcher", cmp_type,
				MATCHER_NE, left, right));
} /* sdb_memstore_ne_matcher */

sdb_memstore_matcher_t *
sdb_memstore_ge_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right)
{
	return M(sdb_object_create("ge-matcher", cmp_type,
				MATCHER_GE, left, right));
} /* sdb_memstore_ge_matcher */

sdb_memstore_matcher_t *
sdb_memstore_gt_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right)
{
	return M(sdb_object_create("gt-matcher", cmp_type,
				MATCHER_GT, left, right));
} /* sdb_memstore_gt_matcher */

sdb_memstore_matcher_t *
sdb_memstore_in_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right)
{
	return M(sdb_object_create("in-matcher", cmp_type,
				MATCHER_IN, left, right));
} /* sdb_memstore_in_matcher */

sdb_memstore_matcher_t *
sdb_memstore_regex_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right)
{
	if (! right->type) {
		if ((right->data.type != SDB_TYPE_STRING)
				&& (right->data.type != SDB_TYPE_REGEX))
			return NULL;

		if (right->data.type == SDB_TYPE_STRING) {
			char *raw = right->data.data.string;
			if (sdb_data_parse(raw, SDB_TYPE_REGEX, &right->data))
				return NULL;
			free(raw);
		}
	}
	return M(sdb_object_create("regex-matcher", cmp_type,
				MATCHER_REGEX, left, right));
} /* sdb_memstore_regex_matcher */

sdb_memstore_matcher_t *
sdb_memstore_nregex_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right)
{
	sdb_memstore_matcher_t *m = sdb_memstore_regex_matcher(left, right);
	if (! m)
		return NULL;
	m->type = MATCHER_NREGEX;
	return m;
} /* sdb_memstore_nregex_matcher */

sdb_memstore_matcher_t *
sdb_memstore_isnull_matcher(sdb_memstore_expr_t *expr)
{
	return M(sdb_object_create("isnull-matcher", unary_type,
				MATCHER_ISNULL, expr));
} /* sdb_memstore_isnull_matcher */

sdb_memstore_matcher_t *
sdb_memstore_istrue_matcher(sdb_memstore_expr_t *expr)
{
	return M(sdb_object_create("istrue-matcher", unary_type,
				MATCHER_ISTRUE, expr));
} /* sdb_memstore_istrue_matcher */

sdb_memstore_matcher_t *
sdb_memstore_isfalse_matcher(sdb_memstore_expr_t *expr)
{
	return M(sdb_object_create("isfalse-matcher", unary_type,
				MATCHER_ISFALSE, expr));
} /* sdb_memstore_isfalse_matcher */

sdb_memstore_matcher_t *
sdb_memstore_dis_matcher(sdb_memstore_matcher_t *left, sdb_memstore_matcher_t *right)
{
	return M(sdb_object_create("dis-matcher", op_type, MATCHER_OR,
				left, right));
} /* sdb_memstore_dis_matcher */

sdb_memstore_matcher_t *
sdb_memstore_con_matcher(sdb_memstore_matcher_t *left, sdb_memstore_matcher_t *right)
{
	return M(sdb_object_create("con-matcher", op_type, MATCHER_AND,
				left, right));
} /* sdb_memstore_con_matcher */

sdb_memstore_matcher_t *
sdb_memstore_inv_matcher(sdb_memstore_matcher_t *m)
{
	return M(sdb_object_create("inv-matcher", uop_type, MATCHER_NOT, m));
} /* sdb_memstore_inv_matcher */

int
sdb_memstore_matcher_matches(sdb_memstore_matcher_t *m, sdb_memstore_obj_t *obj,
		sdb_memstore_matcher_t *filter)
{
	if (filter && (! sdb_memstore_matcher_matches(filter, obj, NULL)))
		return 0;

	/* "NULL" always matches */
	if ((! m) || (! obj))
		return 1;

	if ((m->type < 0) || ((size_t)m->type >= SDB_STATIC_ARRAY_LEN(matchers)))
		return 0;

	if (! matchers[m->type])
		return 0;
	return matchers[m->type](m, obj, filter);
} /* sdb_memstore_matcher_matches */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

