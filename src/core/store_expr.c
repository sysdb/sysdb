/*
 * SysDB - src/core/store_expr.c
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
 * This module implements expressions which may be executed in the store.
 */

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "sysdb.h"
#include "core/store-private.h"
#include "core/data.h"
#include "core/object.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/*
 * private data types
 */

static int
expr_init(sdb_object_t *obj, va_list ap)
{
	int type = va_arg(ap, int);
	sdb_store_expr_t *left  = va_arg(ap, sdb_store_expr_t *);
	sdb_store_expr_t *right = va_arg(ap, sdb_store_expr_t *);
	const sdb_data_t *value = va_arg(ap, const sdb_data_t *);

	sdb_store_expr_t *expr = SDB_STORE_EXPR(obj);

	if (type <= 0) {
		if (! value)
			return -1;
	} else {
		if (value)
			return -1;
		if ((! left) || (! right))
			return -1;
	}

	if (value)
		expr->data = *value;

	sdb_object_ref(SDB_OBJ(left));
	sdb_object_ref(SDB_OBJ(right));

	expr->type  = type;
	expr->left  = left;
	expr->right = right;
	return 0;
} /* expr_init */

static void
expr_destroy(sdb_object_t *obj)
{
	sdb_store_expr_t *expr = SDB_STORE_EXPR(obj);
	sdb_object_deref(SDB_OBJ(expr->left));
	sdb_object_deref(SDB_OBJ(expr->right));

	if (expr->data.type)
		sdb_data_free_datum(&expr->data);
} /* expr_destroy */

static sdb_type_t expr_type = {
	/* size = */ sizeof(sdb_store_expr_t),
	/* init = */ expr_init,
	/* destroy = */ expr_destroy,
};

/*
 * public API
 */

sdb_store_expr_t *
sdb_store_expr_create(int op, sdb_store_expr_t *left, sdb_store_expr_t *right)
{
	sdb_data_t value = SDB_DATA_INIT;

	if ((op < 0) || (SDB_DATA_CONCAT < op) || (! left) || (! right))
		return NULL;

	if (left->type || right->type)
		return SDB_STORE_EXPR(sdb_object_create("store-expr", expr_type,
					op, left, right, NULL));
	/* else: both expressions are constant values; evaluate now */

	if (sdb_data_expr_eval(op, &left->data, &right->data, &value))
		return NULL;
	return SDB_STORE_EXPR(sdb_object_create("store-constvalue", expr_type,
				0, NULL, NULL, &value));
} /* sdb_store_expr_create */

sdb_store_expr_t *
sdb_store_expr_fieldvalue(int field)
{
	sdb_data_t value = { SDB_TYPE_INTEGER, { .integer = field } };
	if ((field < 0) || (SDB_FIELD_BACKEND < field))
		return NULL;
	return SDB_STORE_EXPR(sdb_object_create("store-fieldvalue", expr_type,
				FIELD_VALUE, NULL, NULL, &value));
} /* sdb_store_expr_fieldvalue */

sdb_store_expr_t *
sdb_store_expr_attrvalue(const char *name)
{
	sdb_data_t value = { SDB_TYPE_STRING, { .string = NULL} };
	sdb_store_expr_t *expr;

	value.data.string = strdup(name);
	if (! value.data.string)
		return NULL;

	expr = SDB_STORE_EXPR(sdb_object_create("store-attrvalue", expr_type,
				ATTR_VALUE, NULL, NULL, &value));
	if (! expr)
		free(value.data.string);
	return expr;
} /* sdb_store_expr_attrvalue */

sdb_store_expr_t *
sdb_store_expr_constvalue(const sdb_data_t *value)
{
	sdb_data_t data = SDB_DATA_INIT;
	if (sdb_data_copy(&data, value))
		return NULL;
	return SDB_STORE_EXPR(sdb_object_create("store-constvalue", expr_type,
				0, NULL, NULL, &data));
} /* sdb_store_expr_constvalue */

int
sdb_store_expr_eval(sdb_store_expr_t *expr, sdb_store_obj_t *obj,
		sdb_data_t *res, sdb_store_matcher_t *filter)
{
	sdb_data_t v1 = SDB_DATA_INIT, v2 = SDB_DATA_INIT;
	int status = 0;

	if ((! expr) || (! res))
		return -1;

	if (filter && obj && (! sdb_store_matcher_matches(filter, obj, NULL)))
		obj = NULL; /* this object does not exist */

	if (! expr->type)
		return sdb_data_copy(res, &expr->data);
	else if (expr->type == FIELD_VALUE)
		return sdb_store_get_field(obj, (int)expr->data.data.integer, res);
	else if (expr->type == ATTR_VALUE)
		return sdb_store_get_attr(obj, expr->data.data.string, res, filter);

	if (sdb_store_expr_eval(expr->left, obj, &v1, filter))
		return -1;
	if (sdb_store_expr_eval(expr->right, obj, &v2, filter)) {
		sdb_data_free_datum(&v1);
		return -1;
	}

	if (sdb_data_expr_eval(expr->type, &v1, &v2, res))
		status = -1;
	sdb_data_free_datum(&v1);
	sdb_data_free_datum(&v2);
	return status;
} /* sdb_store_expr_eval */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

