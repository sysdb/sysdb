/*
 * SysDB - src/core/store_lookup.c
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
#include "core/store-private.h"
#include "core/object.h"

#include <assert.h>

#include <sys/types.h>
#include <regex.h>

#include <stdlib.h>
#include <string.h>

#include <limits.h>

/*
 * private data types
 */

typedef struct {
	sdb_store_matcher_t *m;
	sdb_store_matcher_t *filter;
	sdb_store_lookup_cb  cb;
	void *user_data;
} scan_iter_data_t;

/*
 * private helper functions
 */

static int
scan_iter(sdb_store_obj_t *obj, void *user_data)
{
	scan_iter_data_t *d = user_data;

	if (sdb_store_matcher_matches(d->m, obj, d->filter))
		return d->cb(obj, d->user_data);
	return 0;
} /* scan_iter */

static sdb_attribute_t *
attr_get(sdb_host_t *host, const char *name, sdb_store_matcher_t *filter)
{
	sdb_avltree_iter_t *iter = NULL;
	sdb_attribute_t *attr = NULL;

	iter = sdb_avltree_get_iter(host->attributes);
	while (sdb_avltree_iter_has_next(iter)) {
		sdb_attribute_t *a = ATTR(sdb_avltree_iter_get_next(iter));

		if (strcasecmp(name, SDB_OBJ(a)->name))
			continue;

		assert(STORE_OBJ(a)->type == SDB_ATTRIBUTE);
		attr = a;
		break;
	}
	sdb_avltree_iter_destroy(iter);

	if (filter && (! sdb_store_matcher_matches(filter, STORE_OBJ(attr),
					NULL)))
		return NULL;
	return attr;
} /* attr_get */

/*
 * matcher implementations
 */

static int
match_string(string_matcher_t *m, const char *name)
{
	if ((! m->name) && (! m->name_re))
		return 1;

	if (! name)
		name = "";

	if (m->name && strcasecmp(m->name, name))
		return 0;
	if (m->name_re && regexec(m->name_re, name,
					/* matches */ 0, NULL, /* flags = */ 0))
		return 0;
	return 1;
} /* match_string */

static int
match_logical(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	int status;

	assert((m->type == MATCHER_AND) || (m->type == MATCHER_OR));
	assert(OP_M(m)->left && OP_M(m)->right);

	status = sdb_store_matcher_matches(OP_M(m)->left, obj, filter);

	/* lazy evaluation */
	if ((! status) && (m->type == MATCHER_AND))
		return status;
	else if (status && (m->type == MATCHER_OR))
		return status;

	return sdb_store_matcher_matches(OP_M(m)->right, obj, filter);
} /* match_logical */

static int
match_unary(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	assert(m->type == MATCHER_NOT);
	assert(UOP_M(m)->op);

	return !sdb_store_matcher_matches(UOP_M(m)->op, obj, filter);
} /* match_unary */

static int
match_name(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	sdb_avltree_iter_t *iter = NULL;
	int status = 0;

	assert(m->type == MATCHER_NAME);

	if (obj->type == NAME_M(m)->obj_type)
		return match_string(&NAME_M(m)->name, SDB_OBJ(obj)->name);
	else if (obj->type != SDB_HOST)
		return 0;

	switch (NAME_M(m)->obj_type) {
		case SDB_SERVICE:
			iter = sdb_avltree_get_iter(HOST(obj)->services);
			break;
		case SDB_METRIC:
			iter = sdb_avltree_get_iter(HOST(obj)->metrics);
			break;
		case SDB_ATTRIBUTE:
			iter = sdb_avltree_get_iter(HOST(obj)->attributes);
			break;
	}

	while (sdb_avltree_iter_has_next(iter)) {
		sdb_object_t *child = sdb_avltree_iter_get_next(iter);
		if (filter && (! sdb_store_matcher_matches(filter, STORE_OBJ(child),
						NULL)))
			continue;
		if (match_string(&NAME_M(m)->name, child->name)) {
			status = 1;
			break;
		}
	}
	sdb_avltree_iter_destroy(iter);
	return status;
} /* match_name */

static int
match_attr(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	sdb_attribute_t *attr;

	assert(m->type == MATCHER_ATTR);
	assert(ATTR_M(m)->name);

	if (obj->type != SDB_HOST)
		return 0;

	attr = attr_get(HOST(obj), ATTR_M(m)->name, filter);
	if (attr) {
		char buf[sdb_data_strlen(&attr->value) + 1];
		if (sdb_data_format(&attr->value, buf, sizeof(buf), SDB_UNQUOTED) <= 0)
			return 0;
		if (match_string(&ATTR_M(m)->value, buf))
			return 1;
	}
	return 0;
} /* match_attr */

static int
match_child(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	sdb_avltree_iter_t *iter = NULL;
	int status = 0;

	assert((m->type == MATCHER_SERVICE)
			|| (m->type == MATCHER_METRIC)
			|| (m->type == MATCHER_ATTRIBUTE));

	/* TODO: support all object types */
	if (obj->type != SDB_HOST)
		return 0;

	if (m->type == MATCHER_SERVICE)
		iter = sdb_avltree_get_iter(HOST(obj)->services);
	else if (m->type == MATCHER_METRIC)
		iter = sdb_avltree_get_iter(HOST(obj)->metrics);
	else if (m->type == SDB_ATTRIBUTE)
		iter = sdb_avltree_get_iter(HOST(obj)->attributes);

	while (sdb_avltree_iter_has_next(iter)) {
		sdb_object_t *child = sdb_avltree_iter_get_next(iter);
		if (filter && (! sdb_store_matcher_matches(filter,
						STORE_OBJ(child), NULL)))
			continue;

		if (sdb_store_matcher_matches(CHILD_M(m)->m, obj, filter)) {
			status = 1;
			break;
		}
	}
	sdb_avltree_iter_destroy(iter);
	return status;
} /* match_child */

/*
 * cmp_expr:
 * Compare the values of two expressions when evaluating them using the
 * specified stored object and filter. Returns a value less than, equal to, or
 * greater than zero if the value of the first expression compares less than,
 * equal to, or greater than the value of the second expression. Returns
 * INT_MAX if any of the expressions could not be evaluated or if any of them
 * evaluated to NULL.
 */
static int
cmp_expr(sdb_store_expr_t *e1, sdb_store_expr_t *e2,
		sdb_store_obj_t *obj, sdb_store_matcher_t *filter)
{
	sdb_data_t v1 = SDB_DATA_INIT, v2 = SDB_DATA_INIT;
	int status;

	if (sdb_store_expr_eval(e1, obj, &v1, filter))
		return INT_MAX;
	if (sdb_store_expr_eval(e2, obj, &v2, filter)) {
		sdb_data_free_datum(&v1);
		return INT_MAX;
	}

	if (sdb_data_isnull(&v1) || (sdb_data_isnull(&v2)))
		status = INT_MAX;
	else if (v1.type == v2.type)
		status = sdb_data_cmp(&v1, &v2);
	else
		status = sdb_data_strcmp(&v1, &v2);

	sdb_data_free_datum(&v1);
	sdb_data_free_datum(&v2);
	return status;
} /* cmp_expr */

static int
match_lt(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	int status;
	assert(m->type == MATCHER_LT);
	status = cmp_expr(CMP_M(m)->left, CMP_M(m)->right, obj, filter);
	return (status != INT_MAX) && (status < 0);
} /* match_lt */

static int
match_le(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	int status;
	assert(m->type == MATCHER_LE);
	status = cmp_expr(CMP_M(m)->left, CMP_M(m)->right, obj, filter);
	return (status != INT_MAX) && (status <= 0);
} /* match_le */

static int
match_eq(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	int status;
	assert(m->type == MATCHER_EQ);
	status = cmp_expr(CMP_M(m)->left, CMP_M(m)->right, obj, filter);
	return (status != INT_MAX) && (! status);
} /* match_eq */

static int
match_ne(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	int status;
	assert(m->type == MATCHER_NE);
	status = cmp_expr(CMP_M(m)->left, CMP_M(m)->right, obj, filter);
	return (status != INT_MAX) && status;
} /* match_ne */

static int
match_ge(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	int status;
	assert(m->type == MATCHER_GE);
	status = cmp_expr(CMP_M(m)->left, CMP_M(m)->right, obj, filter);
	return (status != INT_MAX) && (status >= 0);
} /* match_ge */

static int
match_gt(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	int status;
	assert(m->type == MATCHER_GT);
	status = cmp_expr(CMP_M(m)->left, CMP_M(m)->right, obj, filter);
	return (status != INT_MAX) && (status > 0);
} /* match_gt */

static int
match_in(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	sdb_data_t value = SDB_DATA_INIT, array = SDB_DATA_INIT;
	int status = 1;

	assert(m->type == MATCHER_IN);

	if ((sdb_store_expr_eval(CMP_M(m)->left, obj, &value, filter))
			|| (sdb_store_expr_eval(CMP_M(m)->right, obj, &array, filter)))
		status = 0;

	if (status)
		status = sdb_data_inarray(&value, &array);

	sdb_data_free_datum(&value);
	sdb_data_free_datum(&array);
	return status;
} /* match_in */

static int
match_regex(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	sdb_data_t v = SDB_DATA_INIT;
	int status = 0;

	regex_t regex;
	_Bool free_regex = 0;

	assert((m->type == MATCHER_REGEX)
			|| (m->type == MATCHER_NREGEX));

	if (! CMP_M(m)->right->type) {
		assert(CMP_M(m)->right->data.type == SDB_TYPE_REGEX);
		regex = CMP_M(m)->right->data.data.re.regex;
	}
	else {
		sdb_data_t tmp = SDB_DATA_INIT;
		char *raw;

		if (sdb_store_expr_eval(CMP_M(m)->right, obj, &tmp, filter))
			return 0;

		if (tmp.type != SDB_TYPE_STRING) {
			sdb_data_free_datum(&tmp);
			return 0;
		}

		raw = tmp.data.string;
		if (sdb_data_parse(raw, SDB_TYPE_REGEX, &tmp)) {
			free(raw);
			return 0;
		}

		regex = tmp.data.re.regex;
		free_regex = 1;
		free(tmp.data.re.raw);
		free(raw);
	}

	if ((sdb_store_expr_eval(CMP_M(m)->left, obj, &v, filter))
			|| (sdb_data_isnull(&v)))
		status = 0;
	else {
		char value[sdb_data_strlen(&v) + 1];
		if (sdb_data_format(&v, value, sizeof(value), SDB_UNQUOTED) < 0)
			status = 0;
		else if (! regexec(&regex, value, 0, NULL, 0))
			status = 1;
	}

	if (free_regex)
		regfree(&regex);
	sdb_data_free_datum(&v);
	if (m->type == MATCHER_NREGEX)
		return !status;
	return status;
} /* match_regex */

static int
match_isnull(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	sdb_data_t v = SDB_DATA_INIT;
	int status;

	assert((m->type == MATCHER_ISNULL) || (m->type == MATCHER_ISNNULL));

	/* TODO: this might hide real errors;
	 * improve error reporting and propagation */
	if (sdb_store_expr_eval(ISNULL_M(m)->expr, obj, &v, filter)
			|| sdb_data_isnull(&v))
		status = 1;
	else
		status = 0;

	sdb_data_free_datum(&v);
	if (m->type == MATCHER_ISNNULL)
		return !status;
	return status;
} /* match_isnull */

typedef int (*matcher_cb)(sdb_store_matcher_t *, sdb_store_obj_t *,
		sdb_store_matcher_t *);

/* this array needs to be indexable by the matcher types;
 * -> update the enum in store-private.h when updating this */
static matcher_cb
matchers[] = {
	match_logical,
	match_logical,
	match_unary,
	match_name,
	match_attr,
	match_child,
	match_child,
	match_child,
	match_lt,
	match_le,
	match_eq,
	match_ne,
	match_ge,
	match_gt,
	match_in,
	match_regex,
	match_regex,
	match_isnull,
	match_isnull,
};

/*
 * private matcher types
 */

/* initializes a string matcher consuming two elements from ap */
static int
string_matcher_init(string_matcher_t *m, va_list ap)
{
	const char *name = va_arg(ap, const char *);
	const char *name_re = va_arg(ap, const char *);

	if (name) {
		m->name = strdup(name);
		if (! m->name)
			return -1;
	}
	if (name_re) {
		m->name_re = malloc(sizeof(*m->name_re));
		if (! m->name_re)
			return -1;
		if (regcomp(m->name_re, name_re, REG_EXTENDED | REG_ICASE | REG_NOSUB))
			return -1;
	}
	return 0;
} /* string_matcher_init */

static void
string_matcher_destroy(string_matcher_t *m)
{
	if (m->name)
		free(m->name);
	if (m->name_re) {
		regfree(m->name_re);
		free(m->name_re);
	}
} /* string_matcher_destroy */

/* initializes a name matcher */
static int
name_matcher_init(sdb_object_t *obj, va_list ap)
{
	name_matcher_t *m = NAME_M(obj);
	M(obj)->type = MATCHER_NAME;
	return string_matcher_init(&m->name, ap);
} /* name_matcher_init */

static void
name_matcher_destroy(sdb_object_t *obj)
{
	name_matcher_t *m = NAME_M(obj);
	string_matcher_destroy(&m->name);
} /* name_matcher_destroy */

static int
attr_matcher_init(sdb_object_t *obj, va_list ap)
{
	attr_matcher_t *attr = ATTR_M(obj);
	const char *name = va_arg(ap, const char *);

	M(obj)->type = MATCHER_ATTR;
	if (name) {
		attr->name = strdup(name);
		if (! attr->name)
			return -1;
	}
	return string_matcher_init(&attr->value, ap);
} /* attr_matcher_init */

static void
attr_matcher_destroy(sdb_object_t *obj)
{
	attr_matcher_t *attr = ATTR_M(obj);
	if (attr->name)
		free(attr->name);
	attr->name = NULL;
	string_matcher_destroy(&attr->value);
} /* attr_matcher_destroy */

static int
op_matcher_init(sdb_object_t *obj, va_list ap)
{
	M(obj)->type = va_arg(ap, int);
	if ((M(obj)->type != MATCHER_OR) && (M(obj)->type != MATCHER_AND))
		return -1;

	OP_M(obj)->left = va_arg(ap, sdb_store_matcher_t *);
	sdb_object_ref(SDB_OBJ(OP_M(obj)->left));
	OP_M(obj)->right = va_arg(ap, sdb_store_matcher_t *);
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
child_matcher_init(sdb_object_t *obj, va_list ap)
{
	M(obj)->type = va_arg(ap, int);
	CHILD_M(obj)->m = va_arg(ap, sdb_store_matcher_t *);

	if (! CHILD_M(obj)->m)
		return -1;

	sdb_object_ref(SDB_OBJ(CHILD_M(obj)->m));
	return 0;
} /* child_matcher_init */

static void
child_matcher_destroy(sdb_object_t *obj)
{
	sdb_object_deref(SDB_OBJ(CHILD_M(obj)->m));
} /* child_matcher_destroy */

static int
cmp_matcher_init(sdb_object_t *obj, va_list ap)
{
	M(obj)->type = va_arg(ap, int);

	CMP_M(obj)->left = va_arg(ap, sdb_store_expr_t *);
	sdb_object_ref(SDB_OBJ(CMP_M(obj)->left));
	CMP_M(obj)->right = va_arg(ap, sdb_store_expr_t *);
	sdb_object_ref(SDB_OBJ(CMP_M(obj)->right));

	if ((! CMP_M(obj)->left) || (! CMP_M(obj)->right))
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

	UOP_M(obj)->op = va_arg(ap, sdb_store_matcher_t *);
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
isnull_matcher_init(sdb_object_t *obj, va_list ap)
{
	M(obj)->type = va_arg(ap, int);
	if ((M(obj)->type != MATCHER_ISNULL) && (M(obj)->type != MATCHER_ISNNULL))
		return -1;

	ISNULL_M(obj)->expr = va_arg(ap, sdb_store_expr_t *);
	sdb_object_ref(SDB_OBJ(ISNULL_M(obj)->expr));
	return 0;
} /* isnull_matcher_init */

static void
isnull_matcher_destroy(sdb_object_t *obj)
{
	sdb_object_deref(SDB_OBJ(ISNULL_M(obj)->expr));
	ISNULL_M(obj)->expr = NULL;
} /* isnull_matcher_destroy */

static sdb_type_t name_type = {
	/* size = */ sizeof(name_matcher_t),
	/* init = */ name_matcher_init,
	/* destroy = */ name_matcher_destroy,
};

static sdb_type_t attr_type = {
	/* size = */ sizeof(attr_matcher_t),
	/* init = */ attr_matcher_init,
	/* destroy = */ attr_matcher_destroy,
};

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

static sdb_type_t child_type = {
	/* size = */ sizeof(child_matcher_t),
	/* init = */ child_matcher_init,
	/* destroy = */ child_matcher_destroy,
};

static sdb_type_t cmp_type = {
	/* size = */ sizeof(cmp_matcher_t),
	/* init = */ cmp_matcher_init,
	/* destroy = */ cmp_matcher_destroy,
};

static sdb_type_t isnull_type = {
	/* size = */ sizeof(isnull_matcher_t),
	/* init = */ isnull_matcher_init,
	/* destroy = */ isnull_matcher_destroy,
};

/*
 * public API
 */

sdb_store_matcher_t *
sdb_store_name_matcher(int type, const char *name, _Bool re)
{
	sdb_store_matcher_t *m;

	if (re)
		m = M(sdb_object_create("name-matcher", name_type, NULL, name));
	else
		m = M(sdb_object_create("name-matcher", name_type, name, NULL));

	if (! m)
		return NULL;

	NAME_M(m)->obj_type = type;
	return m;
} /* sdb_store_name_matcher */

sdb_store_matcher_t *
sdb_store_attr_matcher(const char *name, const char *value, _Bool re)
{
	sdb_store_matcher_t *m;

	if (! name)
		return NULL;

	if (re)
		m = M(sdb_object_create("attr-matcher", attr_type,
					name, NULL, value));
	else
		m = M(sdb_object_create("attr-matcher", attr_type,
					name, value, NULL));
	return m;
} /* sdb_store_attr_matcher */

sdb_store_matcher_t *
sdb_store_child_matcher(int type, sdb_store_matcher_t *m)
{
	if (type == SDB_SERVICE)
		type = MATCHER_SERVICE;
	else if (type == SDB_METRIC)
		type = MATCHER_METRIC;
	else if (type == SDB_ATTRIBUTE)
		type = MATCHER_ATTRIBUTE;
	else
		return NULL;
	return M(sdb_object_create("any-matcher", child_type, type, m));
} /* sdb_store_child_matcher */

sdb_store_matcher_t *
sdb_store_lt_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right)
{
	return M(sdb_object_create("lt-matcher", cmp_type,
				MATCHER_LT, left, right));
} /* sdb_store_lt_matcher */

sdb_store_matcher_t *
sdb_store_le_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right)
{
	return M(sdb_object_create("le-matcher", cmp_type,
				MATCHER_LE, left, right));
} /* sdb_store_le_matcher */

sdb_store_matcher_t *
sdb_store_eq_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right)
{
	return M(sdb_object_create("eq-matcher", cmp_type,
				MATCHER_EQ, left, right));
} /* sdb_store_eq_matcher */

sdb_store_matcher_t *
sdb_store_ne_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right)
{
	return M(sdb_object_create("ne-matcher", cmp_type,
				MATCHER_NE, left, right));
} /* sdb_store_ne_matcher */

sdb_store_matcher_t *
sdb_store_ge_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right)
{
	return M(sdb_object_create("ge-matcher", cmp_type,
				MATCHER_GE, left, right));
} /* sdb_store_ge_matcher */

sdb_store_matcher_t *
sdb_store_gt_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right)
{
	return M(sdb_object_create("gt-matcher", cmp_type,
				MATCHER_GT, left, right));
} /* sdb_store_gt_matcher */

sdb_store_matcher_t *
sdb_store_in_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right)
{
	return M(sdb_object_create("in-matcher", cmp_type,
				MATCHER_IN, left, right));
} /* sdb_store_in_matcher */

sdb_store_matcher_t *
sdb_store_regex_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right)
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
} /* sdb_store_regex_matcher */

sdb_store_matcher_t *
sdb_store_nregex_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right)
{
	sdb_store_matcher_t *m = sdb_store_regex_matcher(left, right);
	if (! m)
		return NULL;
	m->type = MATCHER_NREGEX;
	return m;
} /* sdb_store_nregex_matcher */

sdb_store_matcher_t *
sdb_store_isnull_matcher(sdb_store_expr_t *expr)
{
	return M(sdb_object_create("isnull-matcher", isnull_type,
				MATCHER_ISNULL, expr));
} /* sdb_store_isnull_matcher */

sdb_store_matcher_t *
sdb_store_isnnull_matcher(sdb_store_expr_t *expr)
{
	return M(sdb_object_create("isnull-matcher", isnull_type,
				MATCHER_ISNNULL, expr));
} /* sdb_store_isnnull_matcher */

sdb_store_matcher_op_cb
sdb_store_parse_matcher_op(const char *op)
{
	if (! strcasecmp(op, "<"))
		return sdb_store_lt_matcher;
	else if (! strcasecmp(op, "<="))
		return sdb_store_le_matcher;
	else if (! strcasecmp(op, "="))
		return sdb_store_eq_matcher;
	else if (! strcasecmp(op, "!="))
		return sdb_store_ne_matcher;
	else if (! strcasecmp(op, ">="))
		return sdb_store_ge_matcher;
	else if (! strcasecmp(op, ">"))
		return sdb_store_gt_matcher;
	else if (! strcasecmp(op, "=~"))
		return sdb_store_regex_matcher;
	else if (! strcasecmp(op, "!~"))
		return sdb_store_nregex_matcher;
	return NULL;
} /* sdb_store_parse_matcher_op */

int
sdb_store_parse_object_type_plural(const char *name)
{
	if (! strcasecmp(name, "hosts"))
		return SDB_HOST;
	else if (! strcasecmp(name, "services"))
		return SDB_SERVICE;
	else if (! strcasecmp(name, "metrics"))
		return SDB_METRIC;
	return -1;
} /* sdb_store_parse_object_type_plural */

int
sdb_store_parse_field_name(const char *name)
{
	if (! strcasecmp(name, "name"))
		return SDB_FIELD_NAME;
	else if (! strcasecmp(name, "last_update"))
		return SDB_FIELD_LAST_UPDATE;
	else if (! strcasecmp(name, "age"))
		return SDB_FIELD_AGE;
	else if (! strcasecmp(name, "interval"))
		return SDB_FIELD_INTERVAL;
	else if (! strcasecmp(name, "backend"))
		return SDB_FIELD_BACKEND;
	return -1;
} /* sdb_store_parse_field_name */

static sdb_store_matcher_t *
maybe_inv_matcher(sdb_store_matcher_t *m, _Bool inv)
{
	sdb_store_matcher_t *tmp;

	if ((! m) || (! inv))
		return m;

	tmp = sdb_store_inv_matcher(m);
	/* pass ownership to the inverse matcher */
	sdb_object_deref(SDB_OBJ(m));
	return tmp;
} /* maybe_inv_matcher */

sdb_store_matcher_t *
sdb_store_matcher_parse_cmp(const char *obj_type,
		const char *op, sdb_store_expr_t *expr)
{
	int type = -1;
	_Bool inv = 0;
	_Bool re = 0;

	sdb_data_t value = SDB_DATA_INIT;
	sdb_store_matcher_t *m = NULL;

	if (! strcasecmp(obj_type, "host"))
		type = SDB_HOST;
	else if (! strcasecmp(obj_type, "service"))
		type = SDB_SERVICE;
	else if (! strcasecmp(obj_type, "metric"))
		type = SDB_METRIC;
	else if (! strcasecmp(obj_type, "attribute"))
		type = SDB_ATTRIBUTE;
	else
		return NULL;

	/* XXX: this code sucks! */
	if (! strcasecmp(op, "=")) {
		/* nothing to do */
	}
	else if (! strcasecmp(op, "!=")) {
		inv = 1;
	}
	else if (! strcasecmp(op, "=~")) {
		re = 1;
	}
	else if (! strcasecmp(op, "!~")) {
		inv = 1;
		re = 1;
	}
	else
		return NULL;

	if (! expr)
		return NULL;

	if (sdb_store_expr_eval(expr, /* obj */ NULL, &value, /* filter */ NULL)
			|| (value.type != SDB_TYPE_STRING)) {
		sdb_data_free_datum(&value);
		return NULL;
	}

	m = sdb_store_name_matcher(type, value.data.string, re);
	sdb_data_free_datum(&value);
	return maybe_inv_matcher(m, inv);
} /* sdb_store_matcher_parse_cmp */

sdb_store_matcher_t *
sdb_store_dis_matcher(sdb_store_matcher_t *left, sdb_store_matcher_t *right)
{
	return M(sdb_object_create("dis-matcher", op_type, MATCHER_OR,
				left, right));
} /* sdb_store_dis_matcher */

sdb_store_matcher_t *
sdb_store_con_matcher(sdb_store_matcher_t *left, sdb_store_matcher_t *right)
{
	return M(sdb_object_create("con-matcher", op_type, MATCHER_AND,
				left, right));
} /* sdb_store_con_matcher */

sdb_store_matcher_t *
sdb_store_inv_matcher(sdb_store_matcher_t *m)
{
	return M(sdb_object_create("inv-matcher", uop_type, MATCHER_NOT, m));
} /* sdb_store_inv_matcher */

int
sdb_store_matcher_matches(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	if (filter && (! sdb_store_matcher_matches(filter, obj, NULL)))
		return 0;

	/* "NULL" always matches */
	if ((! m) || (! obj))
		return 1;

	if ((m->type < 0) || ((size_t)m->type >= SDB_STATIC_ARRAY_LEN(matchers)))
		return 0;

	return matchers[m->type](m, obj, filter);
} /* sdb_store_matcher_matches */

int
sdb_store_scan(sdb_store_matcher_t *m, sdb_store_matcher_t *filter,
		sdb_store_lookup_cb cb, void *user_data)
{
	scan_iter_data_t data = { m, filter, cb, user_data };

	if (! cb)
		return -1;
	return sdb_store_iterate(scan_iter, &data);
} /* sdb_store_scan */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

