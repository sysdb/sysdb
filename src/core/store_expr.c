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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/*
 * private data types
 */

/* iterate through either a list of child nodes or arrays */
struct sdb_store_expr_iter {
	sdb_store_obj_t *obj;
	sdb_store_expr_t *expr;

	sdb_avltree_iter_t *tree;

	sdb_data_t array;
	size_t array_idx;
	bool free_array;

	sdb_store_matcher_t *filter;
};

/*
 * private types
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
		if ((type == TYPED_EXPR) && (! left))
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

	/* unknown for now */
	expr->data_type = -1;
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
	sdb_store_expr_t *e;

	if ((op < 0) || (SDB_DATA_CONCAT < op) || (! left) || (! right))
		return NULL;

	if (left->type || right->type) {
		e = SDB_STORE_EXPR(sdb_object_create("store-expr", expr_type,
					op, left, right, NULL));
		e->data_type = sdb_data_expr_type(op, left->type, right->type);
		return e;
	}
	/* else: both expressions are constant values; evaluate now */

	if (sdb_data_expr_eval(op, &left->data, &right->data, &value))
		return NULL;
	e = SDB_STORE_EXPR(sdb_object_create("store-constvalue", expr_type,
				0, NULL, NULL, &value));
	e->data_type = value.type;
	return e;
} /* sdb_store_expr_create */

sdb_store_expr_t *
sdb_store_expr_typed(int typ, sdb_store_expr_t *expr)
{
	sdb_data_t value = { SDB_TYPE_INTEGER, { .integer = typ } };
	sdb_store_expr_t *e;

	if ((typ < SDB_HOST) || (SDB_ATTRIBUTE < typ))
		return NULL;

	e = SDB_STORE_EXPR(sdb_object_create("store-typedexpr", expr_type,
				TYPED_EXPR, expr, NULL, &value));
	e->data_type = expr->data_type;
	return e;
} /* sdb_store_expr_typed */

sdb_store_expr_t *
sdb_store_expr_fieldvalue(int field)
{
	sdb_data_t value = { SDB_TYPE_INTEGER, { .integer = field } };
	sdb_store_expr_t *e;

	if ((field < SDB_FIELD_NAME) || (SDB_FIELD_TIMESERIES < field))
		return NULL;
	e = SDB_STORE_EXPR(sdb_object_create("store-fieldvalue", expr_type,
				FIELD_VALUE, NULL, NULL, &value));
	e->data_type = SDB_FIELD_TYPE(field);
	return e;
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
	expr->data_type = -1;
	return expr;
} /* sdb_store_expr_attrvalue */

sdb_store_expr_t *
sdb_store_expr_constvalue(const sdb_data_t *value)
{
	sdb_data_t data = SDB_DATA_INIT;
	sdb_store_expr_t *e;

	if (sdb_data_copy(&data, value))
		return NULL;
	e = SDB_STORE_EXPR(sdb_object_create("store-constvalue", expr_type,
				0, NULL, NULL, &data));
	e->data_type = data.type;
	return e;
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
	else if (expr->type == ATTR_VALUE) {
		status = sdb_store_get_attr(obj, expr->data.data.string, res, filter);
		if ((status < 0) && obj) {
			/* attribute does not exist => NULL */
			status = 0;
			res->type = SDB_TYPE_STRING;
			res->data.string = NULL;
		}
		return status;
	}
	else if (expr->type == TYPED_EXPR) {
		int typ = (int)expr->data.data.integer;
		if (typ != obj->type) {
			/* we support self-references and { service, metric } -> host */
			if ((typ != SDB_HOST)
					|| ((obj->type != SDB_SERVICE)
						&& (obj->type != SDB_METRIC)))
				return -1;
			obj = obj->parent;
		}
		return sdb_store_expr_eval(expr->left, obj, res, filter);
	}

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

sdb_store_expr_iter_t *
sdb_store_expr_iter(sdb_store_expr_t *expr, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	sdb_store_expr_iter_t *iter;
	sdb_avltree_iter_t *tree = NULL;
	sdb_data_t array = SDB_DATA_INIT;
	bool free_array = 0;

	if (! expr)
		return NULL;

	while (expr->type == TYPED_EXPR) {
		int type = (int)expr->data.data.integer;

		if (obj->type == type) {
			/* self reference */
		}
		else if ((type == SDB_HOST)
				&& ((obj->type == SDB_SERVICE)
					|| (obj->type == SDB_METRIC))) {
			/* reference to parent host */
			obj = obj->parent;
		}
		else
			break;
		expr = expr->left;
	}

	if (expr->type == TYPED_EXPR) {
		if (! obj)
			return NULL;
		if (obj->type == SDB_HOST) {
			if (expr->data.data.integer == SDB_SERVICE)
				tree = sdb_avltree_get_iter(HOST(obj)->services);
			else if (expr->data.data.integer == SDB_METRIC)
				tree = sdb_avltree_get_iter(HOST(obj)->metrics);
			else if (expr->data.data.integer == SDB_ATTRIBUTE)
				tree = sdb_avltree_get_iter(HOST(obj)->attributes);
		}
		else if (obj->type == SDB_SERVICE) {
			if (expr->data.data.integer == SDB_ATTRIBUTE)
				tree = sdb_avltree_get_iter(SVC(obj)->attributes);
		}
		else if (obj->type == SDB_METRIC) {
			if (expr->data.data.integer == SDB_ATTRIBUTE)
				tree = sdb_avltree_get_iter(METRIC(obj)->attributes);
		}
	}
	else if (expr->type == FIELD_VALUE) {
		if (! obj)
			return NULL;
		if (expr->data.data.integer == SDB_FIELD_BACKEND) {
			/* while scanning the store, we hold a read lock, so it's safe to
			 * access the data without copying */
			array.type = SDB_TYPE_ARRAY | SDB_TYPE_STRING;
			array.data.array.length = obj->backends_num;
			array.data.array.values = obj->backends;
		}
	}
	else if (! expr->type) {
		if (expr->data.type & SDB_TYPE_ARRAY)
			array = expr->data;
	}
	else {
		sdb_data_t value = SDB_DATA_INIT;
		if (sdb_store_expr_eval(expr, obj, &value, filter))
			return NULL;
		if (! (value.type & SDB_TYPE_ARRAY)) {
			sdb_data_free_datum(&value);
			return NULL;
		}
		array = value;
		free_array = 1;
	}

	if ((! tree) && (array.type == SDB_TYPE_NULL))
		return NULL;

	iter = calloc(1, sizeof(*iter));
	if (! iter) {
		if (free_array)
			sdb_data_free_datum(&array);
		return NULL;
	}

	sdb_object_ref(SDB_OBJ(obj));
	sdb_object_ref(SDB_OBJ(expr));
	sdb_object_ref(SDB_OBJ(filter));

	iter->obj = obj;
	iter->expr = expr;
	iter->tree = tree;
	iter->array = array;
	iter->free_array = free_array;
	iter->filter = filter;
	return iter;
} /* sdb_store_expr_iter */

void
sdb_store_expr_iter_destroy(sdb_store_expr_iter_t *iter)
{
	sdb_data_t null = SDB_DATA_INIT;

	if (! iter)
		return;

	if (iter->tree)
		sdb_avltree_iter_destroy(iter->tree);
	iter->tree = NULL;

	if (iter->free_array)
		sdb_data_free_datum(&iter->array);
	iter->array = null;
	iter->array_idx = 0;

	sdb_object_deref(SDB_OBJ(iter->obj));
	sdb_object_deref(SDB_OBJ(iter->expr));
	sdb_object_deref(SDB_OBJ(iter->filter));
	free(iter);
} /* sdb_store_expr_iter_destroy */

bool
sdb_store_expr_iter_has_next(sdb_store_expr_iter_t *iter)
{
	if (! iter)
		return 0;

	if (iter->tree) {
		/* this function may be called before get_next,
		 * so we'll have to apply filters here as well */
		if (iter->filter) {
			sdb_store_obj_t *child;
			while ((child = STORE_OBJ(sdb_avltree_iter_peek_next(iter->tree)))) {
				if (sdb_store_matcher_matches(iter->filter, child, NULL))
					break;
				(void)sdb_avltree_iter_get_next(iter->tree);
			}
		}

		return sdb_avltree_iter_has_next(iter->tree);
	}

	return iter->array_idx < iter->array.data.array.length;
} /* sdb_store_expr_iter_has_next */

sdb_data_t
sdb_store_expr_iter_get_next(sdb_store_expr_iter_t *iter)
{
	sdb_data_t null = SDB_DATA_INIT;
	sdb_data_t ret = SDB_DATA_INIT;
	sdb_data_t tmp = SDB_DATA_INIT;

	if (! iter)
		return null;

	if (iter->tree) {
		sdb_store_obj_t *child;

		while (42) {
			child = STORE_OBJ(sdb_avltree_iter_get_next(iter->tree));
			if (! child)
				break;
			if (iter->filter
					&& (! sdb_store_matcher_matches(iter->filter, child, NULL)))
				continue;

			if (sdb_store_expr_eval(iter->expr, child, &ret, iter->filter))
				return null;
			break;
		}

		/* Skip over any filtered objects */
		if (iter->filter) {
			while ((child = STORE_OBJ(sdb_avltree_iter_peek_next(iter->tree)))) {
				if (sdb_store_matcher_matches(iter->filter, child, NULL))
					break;
				(void)sdb_avltree_iter_get_next(iter->tree);
			}
		}

		return ret;
	}

	if (iter->array_idx >= iter->array.data.array.length)
		return null;

	++iter->array_idx;
	if (sdb_data_array_get(&iter->array, iter->array_idx - 1, &ret))
		return null;
	if (sdb_data_copy(&tmp, &ret))
		return null;
	ret = tmp;
	return ret;
} /* sdb_store_expr_iter_get_next */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

