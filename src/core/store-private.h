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

#include <sys/types.h>
#include <regex.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sdb_store_base {
	sdb_object_t super;

	/* object type */
	int type;

	/* common meta information */
	sdb_time_t last_update;
	sdb_time_t interval; /* moving average */
	sdb_store_base_t *parent;
};
#define STORE_BASE(obj) ((sdb_store_base_t *)(obj))
#define STORE_CONST_BASE(obj) ((const sdb_store_base_t *)(obj))

typedef struct {
	sdb_store_base_t super;

	sdb_data_t value;
} sdb_attribute_t;
#define SDB_ATTR(obj) ((sdb_attribute_t *)(obj))
#define SDB_CONST_ATTR(obj) ((const sdb_attribute_t *)(obj))

typedef struct {
	sdb_store_base_t super;

	sdb_llist_t *children;
	sdb_llist_t *attributes;
} sdb_store_obj_t;
#define SDB_STORE_OBJ(obj) ((sdb_store_obj_t *)(obj))
#define SDB_CONST_STORE_OBJ(obj) ((const sdb_store_obj_t *)(obj))

enum {
	SDB_HOST = 1,
	SDB_SERVICE,
	SDB_ATTRIBUTE,
};
#define TYPE_TO_NAME(t) \
	(((t) == SDB_HOST) ? "host" \
		: ((t) == SDB_SERVICE) ? "service" \
		: ((t) == SDB_ATTRIBUTE) ? "attribute" : "unknown")

/* shortcuts for accessing the sdb_store_obj_t attributes
 * of inheriting objects */
#define _last_update super.last_update
#define _interval super.interval

/*
 * matchers
 */

/* when adding to this, also update 'matchers' in store_lookup.c */
enum {
	MATCHER_OR,
	MATCHER_AND,
	MATCHER_ATTR,
	MATCHER_SERVICE,
	MATCHER_HOST,
};

/* match the name of something */
typedef struct {
	char    *name;
	regex_t *name_re;
} name_matcher_t;

/* matcher base type */
struct sdb_store_matcher {
	sdb_object_t super;
	/* type of the matcher */
	int type;
};
#define M(m) ((sdb_store_matcher_t *)(m))

/* logical operator matcher */
typedef struct {
	sdb_store_matcher_t super;

	/* left and right hand operands */
	sdb_store_matcher_t *left;
	sdb_store_matcher_t *right;
} op_matcher_t;
#define OP_M(m) ((op_matcher_t *)(m))

/* match any type of object by it's base information */
typedef struct {
	sdb_store_matcher_t super;

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

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_CORE_STORE_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

