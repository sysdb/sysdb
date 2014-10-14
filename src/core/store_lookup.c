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
 * conditional implementations
 */

static int
attr_cmp(sdb_store_obj_t *obj, sdb_store_cond_t *cond,
		sdb_store_matcher_t *filter)
{
	sdb_attribute_t *attr;
	sdb_data_t value = SDB_DATA_INIT;
	int status;

	if (obj->type != SDB_HOST)
		return INT_MAX;

	if (sdb_store_expr_eval(ATTR_C(cond)->expr, obj, &value, filter))
		return INT_MAX;

	attr = attr_get(HOST(obj), ATTR_C(cond)->name, filter);
	if (! attr)
		status = INT_MAX;
	else if (attr->value.type != value.type)
		status = sdb_data_strcmp(&attr->value, &value);
	else
		status = sdb_data_cmp(&attr->value, &value);
	sdb_data_free_datum(&value);
	return status;
} /* attr_cmp */

static int
obj_cmp(sdb_store_obj_t *obj, sdb_store_cond_t *cond,
		sdb_store_matcher_t *filter)
{
	sdb_data_t obj_value = SDB_DATA_INIT;
	sdb_data_t value = SDB_DATA_INIT;
	int status;

	if (sdb_store_expr_eval(OBJ_C(cond)->expr, obj, &value, filter))
		return INT_MAX;

	if (OBJ_C(cond)->field == SDB_FIELD_BACKEND) {
		/* this implementation is not actually a conditional but rather checks
		 * for equality (or rather, existence) only */
		size_t i;

		if (value.type != SDB_TYPE_STRING)
			return INT_MAX;

		status = INT_MAX;
		for (i = 0; i < obj->backends_num; ++i) {
			if (! strcasecmp(obj->backends[i], value.data.string)) {
				status = 0;
				break;
			}
		}
		sdb_data_free_datum(&value);
		return status;
	}

	if (sdb_store_get_field(obj, OBJ_C(cond)->field, &obj_value))
		return INT_MAX;
	if (obj_value.type != value.type) {
		sdb_data_free_datum(&obj_value);
		sdb_data_free_datum(&value);
		return INT_MAX;
	}

	status = sdb_data_cmp(&obj_value, &value);
	sdb_data_free_datum(&obj_value);
	sdb_data_free_datum(&value);
	return status;
} /* obj_cmp */

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

static int
match_lt(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	int status;
	assert(m->type == MATCHER_LT);
	status = COND_M(m)->cond->cmp(obj, COND_M(m)->cond, filter);
	return (status != INT_MAX) && (status < 0);
} /* match_lt */

static int
match_le(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	int status;
	assert(m->type == MATCHER_LE);
	status = COND_M(m)->cond->cmp(obj, COND_M(m)->cond, filter);
	return (status != INT_MAX) && (status <= 0);
} /* match_le */

static int
match_eq(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	int status;
	assert(m->type == MATCHER_EQ);
	status = COND_M(m)->cond->cmp(obj, COND_M(m)->cond, filter);
	return (status != INT_MAX) && (! status);
} /* match_eq */

static int
match_ge(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	int status;
	assert(m->type == MATCHER_GE);
	status = COND_M(m)->cond->cmp(obj, COND_M(m)->cond, filter);
	return (status != INT_MAX) && (status >= 0);
} /* match_ge */

static int
match_gt(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	int status;
	assert(m->type == MATCHER_GT);
	status = COND_M(m)->cond->cmp(obj, COND_M(m)->cond, filter);
	return (status != INT_MAX) && (status > 0);
} /* match_gt */

/*
 * cmp_expr:
 * Compare the values of two expressions when evaluating them using the
 * specified stored object and filter. Returns a value less than, equal to, or
 * greater than zero if the value of the first expression compares less than,
 * equal to, or greater than the value of the second expression. Returns
 * INT_MAX if any of the expressions could not be evaluated.
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

	if (v1.type == v2.type)
		status = sdb_data_cmp(&v1, &v2);
	else
		status = sdb_data_strcmp(&v1, &v2);

	sdb_data_free_datum(&v1);
	sdb_data_free_datum(&v2);
	return status;
} /* cmp_expr */

static int
match_cmp_lt(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	int status;
	assert(m->type == MATCHER_CMP_LT);
	status = cmp_expr(CMP_M(m)->left, CMP_M(m)->right, obj, filter);
	return (status != INT_MAX) && (status < 0);
} /* match_cmp_lt */

static int
match_cmp_le(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	int status;
	assert(m->type == MATCHER_CMP_LE);
	status = cmp_expr(CMP_M(m)->left, CMP_M(m)->right, obj, filter);
	return (status != INT_MAX) && (status <= 0);
} /* match_cmp_le */

static int
match_cmp_eq(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	int status;
	assert(m->type == MATCHER_CMP_EQ);
	status = cmp_expr(CMP_M(m)->left, CMP_M(m)->right, obj, filter);
	return (status != INT_MAX) && (! status);
} /* match_cmp_eq */

static int
match_cmp_ge(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	int status;
	assert(m->type == MATCHER_CMP_GE);
	status = cmp_expr(CMP_M(m)->left, CMP_M(m)->right, obj, filter);
	return (status != INT_MAX) && (status >= 0);
} /* match_cmp_ge */

static int
match_cmp_gt(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	int status;
	assert(m->type == MATCHER_CMP_GT);
	status = cmp_expr(CMP_M(m)->left, CMP_M(m)->right, obj, filter);
	return (status != INT_MAX) && (status > 0);
} /* match_cmp_gt */

static int
match_isnull(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter)
{
	assert(m->type == MATCHER_ISNULL);
	if (obj->type != SDB_HOST)
		return 0;
	return attr_get(HOST(obj), ISNULL_M(m)->attr_name, filter) == NULL;
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
	match_ge,
	match_gt,
	match_cmp_lt,
	match_cmp_le,
	match_cmp_eq,
	match_cmp_ge,
	match_cmp_gt,
	match_isnull,
};

/*
 * private conditional types
 */

static int
attr_cond_init(sdb_object_t *obj, va_list ap)
{
	const char *name = va_arg(ap, const char *);
	sdb_store_expr_t *expr = va_arg(ap, sdb_store_expr_t *);

	if (! name)
		return -1;

	SDB_STORE_COND(obj)->cmp = attr_cmp;

	ATTR_C(obj)->name = strdup(name);
	if (! ATTR_C(obj)->name)
		return -1;
	ATTR_C(obj)->expr = expr;
	sdb_object_ref(SDB_OBJ(expr));
	return 0;
} /* attr_cond_init */

static void
attr_cond_destroy(sdb_object_t *obj)
{
	if (ATTR_C(obj)->name)
		free(ATTR_C(obj)->name);
	sdb_object_deref(SDB_OBJ(ATTR_C(obj)->expr));
} /* attr_cond_destroy */

static sdb_type_t attr_cond_type = {
	/* size = */ sizeof(attr_cond_t),
	/* init = */ attr_cond_init,
	/* destroy = */ attr_cond_destroy,
};

static int
obj_cond_init(sdb_object_t *obj, va_list ap)
{
	int field = va_arg(ap, int);
	sdb_store_expr_t *expr = va_arg(ap, sdb_store_expr_t *);

	SDB_STORE_COND(obj)->cmp = obj_cmp;

	OBJ_C(obj)->field = field;
	OBJ_C(obj)->expr = expr;
	sdb_object_ref(SDB_OBJ(expr));
	return 0;
} /* obj_cond_init */

static void
obj_cond_destroy(sdb_object_t *obj)
{
	sdb_object_deref(SDB_OBJ(OBJ_C(obj)->expr));
} /* obj_cond_destroy */

static sdb_type_t obj_cond_type = {
	/* size = */ sizeof(obj_cond_t),
	/* init = */ obj_cond_init,
	/* destroy = */ obj_cond_destroy,
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

static char *
string_tostring(string_matcher_t *m, char *buf, size_t buflen)
{
	snprintf(buf, buflen, "{ %s%s%s, %p }",
			m->name ? "'" : "", m->name ? m->name : "NULL", m->name ? "'" : "",
			m->name_re);
	return buf;
} /* string_tostring */

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

static char *
name_tostring(sdb_store_matcher_t *m, char *buf, size_t buflen)
{
	char name[buflen + 1];
	assert(m->type == MATCHER_NAME);
	snprintf(buf, buflen, "OBJ[%s]{ NAME%s }",
			SDB_STORE_TYPE_TO_NAME(NAME_M(m)->obj_type),
			string_tostring(&NAME_M(m)->name, name, sizeof(name)));
	return buf;
} /* name_tostring */

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

static char *
attr_tostring(sdb_store_matcher_t *m, char *buf, size_t buflen)
{
	char value[buflen + 1];

	if (! m) {
		snprintf(buf, buflen, "ATTR{}");
		return buf;
	}

	assert(m->type == MATCHER_ATTR);
	snprintf(buf, buflen, "ATTR[%s]{ VALUE%s }", ATTR_M(m)->name,
			string_tostring(&ATTR_M(m)->value, value, sizeof(value)));
	return buf;
} /* attr_tostring */

static int
cond_matcher_init(sdb_object_t *obj, va_list ap)
{
	int type = va_arg(ap, int);
	sdb_store_cond_t *cond = va_arg(ap, sdb_store_cond_t *);

	if (! cond)
		return -1;

	sdb_object_ref(SDB_OBJ(cond));

	M(obj)->type = type;
	COND_M(obj)->cond = cond;
	return 0;
} /* cond_matcher_init */

static void
cond_matcher_destroy(sdb_object_t *obj)
{
	sdb_object_deref(SDB_OBJ(COND_M(obj)->cond));
} /* cond_matcher_destroy */

static char *
cond_tostring(sdb_store_matcher_t *m, char *buf, size_t buflen)
{
	const char *type, *id;
	sdb_data_t value = SDB_DATA_INIT;
	char value_str[buflen];
	sdb_store_expr_t *expr;

	if (COND_M(m)->cond->cmp == attr_cmp) {
		type = "ATTR";
		id = ATTR_C(COND_M(m)->cond)->name;
		expr = ATTR_C(COND_M(m)->cond)->expr;
	}
	else if (COND_M(m)->cond->cmp == obj_cmp) {
		type = "OBJ";
		id = SDB_FIELD_TO_NAME(OBJ_C(COND_M(m)->cond)->field);
		expr = OBJ_C(COND_M(m)->cond)->expr;
	}
	else {
		snprintf(buf, buflen, "<unknown>");
		return buf;
	}

	if (sdb_store_expr_eval(expr, /* obj */ NULL, &value, /* filter */ NULL))
		snprintf(value_str, sizeof(value_str), "ERR");
	else if (sdb_data_format(&value, value_str, sizeof(value_str),
				SDB_SINGLE_QUOTED) < 0)
		snprintf(value_str, sizeof(value_str), "ERR");
	snprintf(buf, buflen, "%s[%s]{ %s %s }", type, id,
			MATCHER_SYM(m->type), value_str);
	sdb_data_free_datum(&value);
	return buf;
} /* cond_tostring */

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

static char *
op_tostring(sdb_store_matcher_t *m, char *buf, size_t buflen)
{
	char left[buflen + 1], right[buflen + 1];

	if (! m) {
		/* this should not happen */
		snprintf(buf, buflen, "()");
		return buf;
	}

	assert((m->type == MATCHER_OR) || (m->type == MATCHER_AND));
	snprintf(buf, buflen, "(%s, %s, %s)",
			m->type == MATCHER_OR ? "OR" : "AND",
			sdb_store_matcher_tostring(OP_M(m)->left, left, sizeof(left)),
			sdb_store_matcher_tostring(OP_M(m)->right, right, sizeof(right)));
	return buf;
} /* op_tostring */

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

static char *
child_tostring(sdb_store_matcher_t *m, char *buf, size_t buflen)
{
	snprintf(buf, buflen, "%s:", MATCHER_SYM(m->type));
	buf[buflen - 1] = '\0';
	sdb_store_matcher_tostring(CHILD_M(m)->m,
			buf + strlen(buf), buflen - strlen(buf));
	return buf;
} /* child_tostring */

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

static char *
cmp_tostring(sdb_store_matcher_t *m, char *buf, size_t buflen)
{
	if (! m) {
		/* this should not happen */
		snprintf(buf, buflen, "()");
		return buf;
	}

	/* TODO */
	snprintf(buf, buflen, "CMP_MATCHER(%d)", m->type);
	return buf;
} /* cmp_tostring */

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

static char *
uop_tostring(sdb_store_matcher_t *m, char *buf, size_t buflen)
{
	char op[buflen + 1];

	if (! m) {
		/* this should not happen */
		snprintf(buf, buflen, "()");
		return buf;
	}

	assert(m->type == MATCHER_NOT);
	snprintf(buf, buflen, "(NOT, %s)",
			sdb_store_matcher_tostring(UOP_M(m)->op, op, sizeof(op)));
	return buf;
} /* uop_tostring */

static int
isnull_matcher_init(sdb_object_t *obj, va_list ap)
{
	const char *name;

	M(obj)->type = va_arg(ap, int);
	if (M(obj)->type != MATCHER_ISNULL)
		return -1;

	name = va_arg(ap, const char *);
	if (! name)
		return -1;
	ISNULL_M(obj)->attr_name = strdup(name);
	if (! ISNULL_M(obj)->attr_name)
		return -1;
	return 0;
} /* isnull_matcher_init */

static void
isnull_matcher_destroy(sdb_object_t *obj)
{
	if (ISNULL_M(obj)->attr_name)
		free(ISNULL_M(obj)->attr_name);
	ISNULL_M(obj)->attr_name = NULL;
} /* isnull_matcher_destroy */

static char *
isnull_tostring(sdb_store_matcher_t *m, char *buf, size_t buflen)
{
	snprintf(buf, buflen, "(IS NULL, ATTR[%s])", ISNULL_M(m)->attr_name);
	return buf;
} /* isnull_tostring */

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

static sdb_type_t cond_type = {
	/* size = */ sizeof(cond_matcher_t),
	/* init = */ cond_matcher_init,
	/* destroy = */ cond_matcher_destroy,
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

typedef char *(*matcher_tostring_cb)(sdb_store_matcher_t *, char *, size_t);

/* this array needs to be indexable by the matcher types;
 * -> update the enum in store-private.h when updating this */
static matcher_tostring_cb
matchers_tostring[] = {
	op_tostring,
	op_tostring,
	uop_tostring,
	name_tostring,
	attr_tostring,
	child_tostring,
	child_tostring,
	child_tostring,
	cond_tostring,
	cond_tostring,
	cond_tostring,
	cond_tostring,
	cond_tostring,
	cmp_tostring,
	cmp_tostring,
	cmp_tostring,
	cmp_tostring,
	cmp_tostring,
	isnull_tostring,
};

/*
 * public API
 */

sdb_store_cond_t *
sdb_store_attr_cond(const char *name, sdb_store_expr_t *expr)
{
	return SDB_STORE_COND(sdb_object_create("attr-cond", attr_cond_type,
				name, expr));
} /* sdb_store_attr_cond */

sdb_store_cond_t *
sdb_store_obj_cond(int field, sdb_store_expr_t *expr)
{
	return SDB_STORE_COND(sdb_object_create("obj-cond", obj_cond_type,
				field, expr));
} /* sdb_store_obj_cond */

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
sdb_store_lt_matcher(sdb_store_cond_t *cond)
{
	return M(sdb_object_create("lt-matcher", cond_type,
				MATCHER_LT, cond));
} /* sdb_store_lt_matcher */

sdb_store_matcher_t *
sdb_store_le_matcher(sdb_store_cond_t *cond)
{
	return M(sdb_object_create("le-matcher", cond_type,
				MATCHER_LE, cond));
} /* sdb_store_le_matcher */

sdb_store_matcher_t *
sdb_store_eq_matcher(sdb_store_cond_t *cond)
{
	return M(sdb_object_create("eq-matcher", cond_type,
				MATCHER_EQ, cond));
} /* sdb_store_eq_matcher */

sdb_store_matcher_t *
sdb_store_ge_matcher(sdb_store_cond_t *cond)
{
	return M(sdb_object_create("ge-matcher", cond_type,
				MATCHER_GE, cond));
} /* sdb_store_ge_matcher */

sdb_store_matcher_t *
sdb_store_gt_matcher(sdb_store_cond_t *cond)
{
	return M(sdb_object_create("gt-matcher", cond_type,
				MATCHER_GT, cond));
} /* sdb_store_gt_matcher */

/*
 * TODO: Rename sdb_store_cmp_* to sdb_store_* once the old code is unused and
 * has been removed.
 */

sdb_store_matcher_t *
sdb_store_cmp_lt(sdb_store_expr_t *left, sdb_store_expr_t *right)
{
	return M(sdb_object_create("lt-matcher", cmp_type,
				MATCHER_CMP_LT, left, right));
} /* sdb_store_cmp_lt */

sdb_store_matcher_t *
sdb_store_cmp_le(sdb_store_expr_t *left, sdb_store_expr_t *right)
{
	return M(sdb_object_create("le-matcher", cmp_type,
				MATCHER_CMP_LE, left, right));
} /* sdb_store_cmp_le */

sdb_store_matcher_t *
sdb_store_cmp_eq(sdb_store_expr_t *left, sdb_store_expr_t *right)
{
	return M(sdb_object_create("eq-matcher", cmp_type,
				MATCHER_CMP_EQ, left, right));
} /* sdb_store_cmp_eq */

sdb_store_matcher_t *
sdb_store_cmp_ge(sdb_store_expr_t *left, sdb_store_expr_t *right)
{
	return M(sdb_object_create("ge-matcher", cmp_type,
				MATCHER_CMP_GE, left, right));
} /* sdb_store_cmp_ge */

sdb_store_matcher_t *
sdb_store_cmp_gt(sdb_store_expr_t *left, sdb_store_expr_t *right)
{
	return M(sdb_object_create("gt-matcher", cmp_type,
				MATCHER_CMP_GT, left, right));
} /* sdb_store_cmp_gt */

sdb_store_matcher_t *
sdb_store_isnull_matcher(const char *attr_name)
{
	return M(sdb_object_create("isnull-matcher", isnull_type,
				MATCHER_ISNULL, attr_name));
} /* sdb_store_isnull_matcher */

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

static int
parse_cond_op(const char *op,
		sdb_store_matcher_t *(**matcher)(sdb_store_cond_t *), _Bool *inv)
{
	*inv = 0;
	if (! strcasecmp(op, "<"))
		*matcher = sdb_store_lt_matcher;
	else if (! strcasecmp(op, "<="))
		*matcher = sdb_store_le_matcher;
	else if (! strcasecmp(op, "="))
		*matcher = sdb_store_eq_matcher;
	else if (! strcasecmp(op, ">="))
		*matcher = sdb_store_ge_matcher;
	else if (! strcasecmp(op, ">"))
		*matcher = sdb_store_gt_matcher;
	else if (! strcasecmp(op, "!=")) {
		*matcher = sdb_store_eq_matcher;
		*inv = 1;
	}
	else
		return -1;
	return 0;
} /* parse_cond_op */

static sdb_store_matcher_t *
parse_attr_cmp(const char *attr, const char *op, sdb_store_expr_t *expr)
{
	sdb_store_matcher_t *(*matcher)(sdb_store_cond_t *) = NULL;
	sdb_store_matcher_t *m;
	sdb_store_cond_t *cond;
	_Bool inv = 0;

	if (! attr)
		return NULL;

	if (! strcasecmp(op, "IS")) {
		if (! expr)
			return sdb_store_isnull_matcher(attr);
		else
			return NULL;
	}
	else if (! expr)
		return NULL;
	else if (parse_cond_op(op, &matcher, &inv))
		return NULL;

	cond = sdb_store_attr_cond(attr, expr);
	if (! cond)
		return NULL;

	m = matcher(cond);
	/* pass ownership to 'm' or destroy in case of an error */
	sdb_object_deref(SDB_OBJ(cond));
	return maybe_inv_matcher(m, inv);
} /* parse_attr_cmp */

sdb_store_matcher_t *
sdb_store_matcher_parse_cmp(const char *obj_type, const char *attr,
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
	else if (type == SDB_ATTRIBUTE)
		return parse_attr_cmp(attr, op, expr);
	else
		return NULL;

	if (! expr)
		return NULL;

	if (sdb_store_expr_eval(expr, /* obj */ NULL, &value, /* filter */ NULL)
			|| (value.type != SDB_TYPE_STRING)) {
		sdb_data_free_datum(&value);
		if (type != SDB_ATTRIBUTE)
			return NULL;
		return parse_attr_cmp(attr, op, expr);
	}

	if (! attr)
		m = sdb_store_name_matcher(type, value.data.string, re);
	else if (type == SDB_ATTRIBUTE)
		m = sdb_store_attr_matcher(attr, value.data.string, re);

	sdb_data_free_datum(&value);
	return maybe_inv_matcher(m, inv);
} /* sdb_store_matcher_parse_cmp */

sdb_store_matcher_t *
sdb_store_matcher_parse_field_cmp(const char *name, const char *op,
		sdb_store_expr_t *expr)
{
	sdb_store_matcher_t *(*matcher)(sdb_store_cond_t *) = NULL;
	sdb_store_matcher_t *m;
	sdb_store_cond_t *cond;
	_Bool inv = 0;

	int field;

	if (! expr)
		return NULL;

	field = sdb_store_parse_field_name(name);
	if (field < 0)
		return NULL;

	if (parse_cond_op(op, &matcher, &inv))
		return NULL;
	cond = sdb_store_obj_cond(field, expr);
	if (! cond)
		return NULL;

	assert(matcher);
	m = matcher(cond);
	/* pass ownership to 'm' or destroy in case of an error */
	sdb_object_deref(SDB_OBJ(cond));
	return maybe_inv_matcher(m, inv);
} /* sdb_store_matcher_parse_field_cmp */

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

char *
sdb_store_matcher_tostring(sdb_store_matcher_t *m, char *buf, size_t buflen)
{
	if (! m)
		return NULL;

	if ((m->type < 0)
			|| (((size_t)m->type >= SDB_STATIC_ARRAY_LEN(matchers_tostring))))
		return NULL;
	return matchers_tostring[m->type](m, buf, buflen);
} /* sdb_store_matcher_tostring */

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

