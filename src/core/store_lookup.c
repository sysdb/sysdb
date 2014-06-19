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

/*
 * private data types
 */

typedef struct {
	sdb_store_matcher_t *m;
	sdb_store_lookup_cb  cb;
	void *user_data;
} lookup_iter_data_t;

/*
 * private helper functions
 */

static int
lookup_iter(sdb_store_base_t *obj, void *user_data)
{
	lookup_iter_data_t *d = user_data;

	if (sdb_store_matcher_matches(d->m, obj))
		return d->cb(obj, d->user_data);
	return 0;
} /* lookup_iter */

/*
 * matcher implementations
 */

static int
match_string(string_matcher_t *m, const char *name)
{
	assert(m);

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
match_logical(sdb_store_matcher_t *m, sdb_store_base_t *obj)
{
	int status;

	assert(m && obj);
	assert(OP_M(m)->left && OP_M(m)->right);

	status = sdb_store_matcher_matches(OP_M(m)->left, obj);
	/* lazy evaluation */
	if ((! status) && (m->type == MATCHER_AND))
		return status;
	else if (status && (m->type == MATCHER_OR))
		return status;

	return sdb_store_matcher_matches(OP_M(m)->right, obj);
} /* match_logical */

static int
match_unary(sdb_store_matcher_t *m, sdb_store_base_t *obj)
{
	assert(m && obj);
	assert(UOP_M(m)->op);

	assert(m->type == MATCHER_NOT);

	return !sdb_store_matcher_matches(UOP_M(m)->op, obj);
} /* match_unary */

static int
match_name(sdb_store_matcher_t *m, sdb_store_base_t *obj)
{
	sdb_llist_iter_t *iter = NULL;
	int status = 0;

	assert(m && obj);
	assert(m->type == MATCHER_NAME);

	if (obj->type != SDB_HOST)
		return 0;

	switch (NAME_M(m)->obj_type) {
		case SDB_HOST:
			return match_string(&NAME_M(m)->name, obj->super.name);
			break;
		case SDB_SERVICE:
			iter = sdb_llist_get_iter(SDB_STORE_OBJ(obj)->children);
			break;
		case SDB_ATTRIBUTE:
			iter = sdb_llist_get_iter(SDB_STORE_OBJ(obj)->attributes);
			break;
	}

	while (sdb_llist_iter_has_next(iter)) {
		sdb_store_base_t *child = STORE_BASE(sdb_llist_iter_get_next(iter));
		if (match_string(&NAME_M(m)->name, child->super.name)) {
			status = 1;
			break;
		}
	}
	sdb_llist_iter_destroy(iter);
	return status;
} /* match_name */

static int
match_attr(sdb_store_matcher_t *m, sdb_store_base_t *obj)
{
	sdb_llist_iter_t *iter = NULL;
	int status = 0;

	assert(m && obj);
	assert(m->type == MATCHER_ATTR);

	if (obj->type != SDB_HOST)
		return 0;

	iter = sdb_llist_get_iter(SDB_STORE_OBJ(obj)->attributes);
	while (sdb_llist_iter_has_next(iter)) {
		sdb_attribute_t *attr = SDB_ATTR(sdb_llist_iter_get_next(iter));
		char buf[sdb_data_strlen(&attr->value) + 1];

		if (ATTR_M(m)->name && strcasecmp(ATTR_M(m)->name, SDB_OBJ(attr)->name))
			continue;

		if (sdb_data_format(&attr->value, buf, sizeof(buf), SDB_UNQUOTED) <= 0)
			return 0;
		if (match_string(&ATTR_M(m)->value, buf)) {
			status = 1;
			break;
		}
	}
	sdb_llist_iter_destroy(iter);
	return status;
} /* match_attr */

typedef int (*matcher_cb)(sdb_store_matcher_t *, sdb_store_base_t *);

/* this array needs to be indexable by the matcher types;
 * -> update the enum in store-private.h when updating this */
static matcher_cb matchers[] = {
	match_logical,
	match_logical,
	match_unary,
	match_name,
	match_attr,
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

typedef char *(*matcher_tostring_cb)(sdb_store_matcher_t *, char *, size_t);

/* this array needs to be indexable by the matcher types;
 * -> update the enum in store-private.h when updating this */
static matcher_tostring_cb matchers_tostring[] = {
	op_tostring,
	op_tostring,
	uop_tostring,
	name_tostring,
	attr_tostring,
};

/*
 * public API
 */

sdb_store_matcher_t *
sdb_store_name_matcher(int type, const char *name, _Bool re)
{
	sdb_store_matcher_t *m;

	if (re)
		m = M(sdb_object_create("name-matcher", name_type,
					NULL, name));
	else
		m = M(sdb_object_create("name-matcher", name_type,
					name, NULL));

	if (! m)
		return NULL;

	NAME_M(m)->obj_type = type;
	return m;
} /* sdb_store_name_matcher */

sdb_store_matcher_t *
sdb_store_attr_matcher(const char *name, const char *value, _Bool re)
{
	if (re)
		return M(sdb_object_create("attr-matcher", attr_type,
					name, NULL, value));
	return M(sdb_object_create("attr-matcher", attr_type,
				name, value, NULL));
} /* sdb_store_attr_matcher */

sdb_store_matcher_t *
sdb_store_matcher_parse_cmp(const char *obj_type, const char *attr,
		const char *op, const char *value)
{
	int type = -1;
	_Bool inv = 0;
	_Bool re = 0;

	sdb_store_matcher_t *m = NULL;

	if (! strcasecmp(obj_type, "host"))
		type = SDB_HOST;
	else if (! strcasecmp(obj_type, "service"))
		type = SDB_SERVICE;
	else if (! strcasecmp(obj_type, "attribute"))
		type = SDB_ATTRIBUTE;

	/* TODO: support other operators */
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

	if (! strcasecmp(attr, "name"))
		m = sdb_store_name_matcher(type, value, re);
	else if (type == SDB_ATTRIBUTE)
		m = sdb_store_attr_matcher(attr, value, re);

	if (! m)
		return NULL;

	if (inv) {
		sdb_store_matcher_t *tmp;
		tmp = sdb_store_inv_matcher(m);
		/* pass ownership to the inverse matcher */
		sdb_object_deref(SDB_OBJ(m));
		m = tmp;
	}
	return m;
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
sdb_store_matcher_matches(sdb_store_matcher_t *m, sdb_store_base_t *obj)
{
	/* "NULL" always matches */
	if ((! m) || (! obj))
		return 1;

	if ((m->type < 0) || ((size_t)m->type >= SDB_STATIC_ARRAY_LEN(matchers)))
		return 0;

	return matchers[m->type](m, obj);
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
sdb_store_lookup(sdb_store_matcher_t *m, sdb_store_lookup_cb cb,
		void *user_data)
{
	lookup_iter_data_t data = { m, cb, user_data };

	if (! cb)
		return -1;
	return sdb_store_iterate(lookup_iter, &data);
} /* sdb_store_lookup */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

