/*
 * SysDB - src/core/store-private.h
 * Copyright (C) 2012-2013 Sebastian 'tokkee' Harl <sh@tokkee.org>
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
 * private data structures used by the store module
 */

#ifndef SDB_CORE_STORE_PRIVATE_H
#define SDB_CORE_STORE_PRIVATE_H 1

#include "core/store.h"
#include "utils/avltree.h"

#include <sys/types.h>
#include <regex.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * core types
 */

struct sdb_store_obj {
	sdb_object_t super;

	/* object type */
	int type;

	/* common meta information */
	sdb_time_t last_update;
	sdb_time_t interval; /* moving average */
	char **backends;
	size_t backends_num;
	sdb_store_obj_t *parent;
};
#define STORE_OBJ(obj) ((sdb_store_obj_t *)(obj))
#define STORE_CONST_OBJ(obj) ((const sdb_store_obj_t *)(obj))

typedef struct {
	sdb_store_obj_t super;

	sdb_data_t value;
} sdb_attribute_t;
#define ATTR(obj) ((sdb_attribute_t *)(obj))
#define CONST_ATTR(obj) ((const sdb_attribute_t *)(obj))

typedef struct {
	sdb_store_obj_t super;

	sdb_avltree_t *attributes;
} sdb_service_t;
#define SVC(obj) ((sdb_service_t *)(obj))
#define CONST_SVC(obj) ((const sdb_service_t *)(obj))

typedef struct {
	sdb_store_obj_t super;

	sdb_avltree_t *attributes;
	struct {
		char *type;
		char *id;
	} store;
} sdb_metric_t;
#define METRIC(obj) ((sdb_metric_t *)(obj))

typedef struct {
	sdb_store_obj_t super;

	sdb_avltree_t *services;
	sdb_avltree_t *metrics;
	sdb_avltree_t *attributes;
} sdb_host_t;
#define HOST(obj) ((sdb_host_t *)(obj))
#define CONST_HOST(obj) ((const sdb_host_t *)(obj))

/* shortcuts for accessing service/host attributes */
#define _last_update super.last_update
#define _interval super.interval

/*
 * conditionals
 */

/* compares an object using the specified conditional and taking the specified
 * filter into account */
typedef int (*cmp_cb)(sdb_store_obj_t *, sdb_store_cond_t *,
		sdb_store_matcher_t *);

struct sdb_store_cond {
	sdb_object_t super;
	cmp_cb cmp;
};

typedef struct {
	sdb_store_cond_t super;
	char *name;
	sdb_store_expr_t *expr;
} attr_cond_t;
#define ATTR_C(obj) ((attr_cond_t *)(obj))

typedef struct {
	sdb_store_cond_t super;
	int field;
	sdb_store_expr_t *expr;
} obj_cond_t;
#define OBJ_C(obj) ((obj_cond_t *)(obj))

/*
 * matchers
 */

/* when adding to this, also update 'matchers' and 'matchers_tostring'
 * in store_lookup.c */
enum {
	MATCHER_OR,
	MATCHER_AND,
	MATCHER_NOT,
	MATCHER_NAME,
	MATCHER_ATTR,
	MATCHER_SERVICE,
	MATCHER_METRIC,
	MATCHER_ATTRIBUTE,
	MATCHER_LT,
	MATCHER_LE,
	MATCHER_EQ,
	MATCHER_GE,
	MATCHER_GT,
	MATCHER_CMP_LT,
	MATCHER_CMP_LE,
	MATCHER_CMP_EQ,
	MATCHER_CMP_GE,
	MATCHER_CMP_GT,
	MATCHER_ISNULL,
};

#define MATCHER_SYM(t) \
	(((t) == MATCHER_OR) ? "OR" \
		: ((t) == MATCHER_AND) ? "AND" \
		: ((t) == MATCHER_NOT) ? "NOT" \
		: ((t) == MATCHER_NAME) ? "NAME" \
		: ((t) == MATCHER_ATTR) ? "ATTR" \
		: ((t) == MATCHER_SERVICE) ? "SERVICE" \
		: ((t) == MATCHER_METRIC) ? "METRIC" \
		: ((t) == MATCHER_ATTRIBUTE) ? "ATTRIBUTE" \
		: ((t) == MATCHER_LT) ? "<" \
		: ((t) == MATCHER_LE) ? "<=" \
		: ((t) == MATCHER_EQ) ? "=" \
		: ((t) == MATCHER_GE) ? ">=" \
		: ((t) == MATCHER_GT) ? ">" \
		: ((t) == MATCHER_ISNULL) ? "IS NULL" \
		: "UNKNOWN")

/* match the name of something */
typedef struct {
	char    *name;
	regex_t *name_re;
} string_matcher_t;

/* matcher base type */
struct sdb_store_matcher {
	sdb_object_t super;
	/* type of the matcher */
	int type;
};
#define M(m) ((sdb_store_matcher_t *)(m))

/* infix operator matcher */
typedef struct {
	sdb_store_matcher_t super;

	/* left and right hand operands */
	sdb_store_matcher_t *left;
	sdb_store_matcher_t *right;
} op_matcher_t;
#define OP_M(m) ((op_matcher_t *)(m))

/* unary operator matcher */
typedef struct {
	sdb_store_matcher_t super;

	/* operand */
	sdb_store_matcher_t *op;
} uop_matcher_t;
#define UOP_M(m) ((uop_matcher_t *)(m))

/* child matcher */
typedef struct {
	sdb_store_matcher_t super;
	sdb_store_matcher_t *m;
} child_matcher_t;
#define CHILD_M(m) ((child_matcher_t *)(m))

/* compare operator matcher */
typedef struct {
	sdb_store_matcher_t super;

	/* left and right hand expressions */
	sdb_store_expr_t *left;
	sdb_store_expr_t *right;
} cmp_matcher_t;
#define CMP_M(m) ((cmp_matcher_t *)(m))

/* match any type of object by it's name */
typedef struct {
	sdb_store_matcher_t super;

	int obj_type;
	string_matcher_t name;
} name_matcher_t;
#define NAME_M(m) ((name_matcher_t *)(m))

/* match attributes */
typedef struct {
	sdb_store_matcher_t super;
	char *name;
	string_matcher_t value;
} attr_matcher_t;
#define ATTR_M(m) ((attr_matcher_t *)(m))

typedef struct {
	sdb_store_matcher_t super;
	char *attr_name; /* we only support matching attributes */
} isnull_matcher_t;
#define ISNULL_M(m) ((isnull_matcher_t *)(m))

/* match using conditionals */
typedef struct {
	sdb_store_matcher_t super;
	sdb_store_cond_t *cond;
} cond_matcher_t;
#define COND_M(m) ((cond_matcher_t *)(m))

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_CORE_STORE_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

