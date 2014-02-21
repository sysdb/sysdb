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

#include "sysdb.h"
#include "core/store-private.h"

#include <assert.h>

#include <sys/types.h>
#include <regex.h>

#include <string.h>

/*
 * private data types
 */

/* match the name of something */
typedef struct {
	const char *name;
	regex_t    *name_re;
} name_matcher_t;

/* matcher base type */
typedef struct {
	/* type of the matcher */
	int type;
} matcher_t;
#define M(m) ((matcher_t *)(m))

/* logical operator matcher */
typedef struct {
	matcher_t super;

	/* left and right hand operands */
	matcher_t *left;
	matcher_t *right;
} op_matcher_t;
#define OP_M(m) ((op_matcher_t *)(m))

/* match any type of object by it's base information */
typedef struct {
	matcher_t super;

	/* match by the name of the object */
	name_matcher_t name;
} obj_matcher_t;
#define OBJ_M(m) ((obj_matcher_t *)(m))

/* match attributes */
typedef struct {
	obj_matcher_t super;
	/* XXX: this needs to be more flexible;
	 *      add support for type-specific operators */
	name_matcher_t value;
} attr_matcher_t;
#define ATTR_M(m) ((attr_matcher_t *)(m))

/* match services */
typedef struct {
	obj_matcher_t super;
	/* match by attributes assigned to the service */
	attr_matcher_t *attr;
} service_matcher_t;
#define SERVICE_M(m) ((service_matcher_t *)(m))

/* match hosts */
typedef struct {
	obj_matcher_t super;
	/* match by services assigned to the host */
	service_matcher_t *service;
	/* match by attributes assigned to the host */
	attr_matcher_t *attr;
} host_matcher_t;
#define HOST_M(m) ((host_matcher_t *)(m))

/*
 * matcher implementations
 */

static int
match_logical(matcher_t *m, sdb_store_base_t *obj);
static int
match_obj(matcher_t *m, sdb_store_base_t *obj);

/* specific matchers */

static int
match_name(name_matcher_t *m, const char *name)
{
	assert(m);

	if ((! m->name) && (! m->name_re))
		return 0;

	if (! name)
		name = "";

	if (m->name && strcasecmp(m->name, name))
		return -1;
	if (m->name_re && regexec(m->name_re, name,
					/* matches */ 0, NULL, /* flags = */ 0))
		return -1;
	return 0;
} /* match_name */

/* match attribute specific values;
 * always call this function through match_obj() */
static int
match_attr(attr_matcher_t *m, sdb_store_base_t *obj)
{
	assert(m && obj);

	if (obj->type != SDB_ATTRIBUTE)
		return -1;

	{
		sdb_attribute_t *attr = SDB_ATTR(obj);
		char buf[sdb_data_strlen(&attr->value) + 1];

		if (sdb_data_format(&attr->value, buf, sizeof(buf), SDB_UNQUOTED) <= 0)
			return -1;
		return match_name(&m->value, buf);
	}
} /* match_attr */

/* match service specific values;
 * always call this function through match_obj() */
static int
match_service(service_matcher_t *m, sdb_store_base_t *obj)
{
	sdb_llist_iter_t *iter;

	assert(m && obj);

	if (obj->type != SDB_SERVICE)
		return -1;

	iter = sdb_llist_get_iter(SDB_STORE_OBJ(obj)->attributes);
	while (sdb_llist_iter_has_next(iter)) {
		sdb_store_base_t *attr = STORE_BASE(sdb_llist_iter_get_next(iter));

		/* if any of the attributes matches we found a matching service */
		if (! match_obj(M(m->attr), attr)) {
			sdb_llist_iter_destroy(iter);
			return 0;
		}
	}
	sdb_llist_iter_destroy(iter);
	return -1;
} /* match_service */

/* match host specific values;
 * always call this function through match_obj() */
static int
match_host(host_matcher_t *m, sdb_store_base_t *obj)
{
	sdb_llist_iter_t *iter;
	int status;

	assert(m && obj);

	if (obj->type != SDB_HOST)
		return -1;

	if (m->service) {
		iter = sdb_llist_get_iter(SDB_STORE_OBJ(obj)->children);
		status = -1;
	}
	else {
		iter = NULL;
		status = 0;
	}
	while (sdb_llist_iter_has_next(iter)) {
		sdb_store_base_t *service = STORE_BASE(sdb_llist_iter_get_next(iter));

		/* found a matching service */
		if (! match_obj(M(m->service), service)) {
			status = 0;
			break;
		}
	}
	sdb_llist_iter_destroy(iter);

	if (status)
		return status;
	else if (! m->attr)
		return 0;

	iter = sdb_llist_get_iter(SDB_STORE_OBJ(obj)->attributes);
	while (sdb_llist_iter_has_next(iter)) {
		sdb_store_base_t *attr = STORE_BASE(sdb_llist_iter_get_next(iter));

		/* if any attribute matches, we found a matching host */
		if (! match_obj(M(m->attr), attr)) {
			sdb_llist_iter_destroy(iter);
			return 0;
		}
	}
	sdb_llist_iter_destroy(iter);
	return -1;
} /* match_host */

/* generic matchers */

enum {
	MATCHER_OR,
	MATCHER_AND,
	MATCHER_ATTR,
	MATCHER_SERVICE,
	MATCHER_HOST,
};

typedef int (*matcher_cb)(matcher_t *, sdb_store_base_t *);

/* this array needs to be indexable by the matcher types */
static matcher_cb matchers[] = {
	match_logical,
	match_logical,
	match_obj,
	match_obj,
	match_obj,
};

static int
match(matcher_t *m, sdb_store_base_t *obj)
{
	assert(m && obj);
	assert((0 <= m->type)
			&& ((size_t)m->type < SDB_STATIC_ARRAY_LEN(matchers)));

	return matchers[m->type](m, obj);
} /* match */

static int
match_logical(matcher_t *m, sdb_store_base_t *obj)
{
	int status;

	assert(m && obj);
	assert(OP_M(m)->left && OP_M(m)->right);

	status = match(OP_M(m)->left, obj);
	/* lazy evaluation */
	if (status && (m->type == MATCHER_AND))
		return status;
	else if ((! status) && (m->type == MATCHER_OR))
		return status;

	return match(OP_M(m)->right, obj);
} /* match_logical */

static int
match_obj(matcher_t *m, sdb_store_base_t *obj)
{
	int status;

	assert(m && obj);

	status = match_name(&OBJ_M(m)->name, obj->super.name);
	if (status)
		return status;

	switch (m->type) {
		case MATCHER_ATTR:
			return match_attr(ATTR_M(m), obj);
			break;
		case MATCHER_SERVICE:
			return match_service(SERVICE_M(m), obj);
			break;
		case MATCHER_HOST:
			return match_host(HOST_M(m), obj);
			break;
	}
	return -1;
} /* match_obj */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

