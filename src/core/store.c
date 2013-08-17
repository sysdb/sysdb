/*
 * SysDB - src/core/store.c
 * Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>
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
#include "core/store.h"
#include "core/error.h"
#include "core/plugin.h"
#include "utils/llist.h"

#include <assert.h>

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

/*
 * private variables
 */

static sdb_llist_t *obj_list = NULL;
static pthread_rwlock_t obj_lock = PTHREAD_RWLOCK_INITIALIZER;

/*
 * private types
 */

static sdb_type_t sdb_store_obj_type;
static sdb_type_t sdb_attribute_type;

struct store_obj;
typedef struct store_obj store_obj_t;

struct store_obj {
	sdb_object_t super;
	sdb_time_t last_update;
	store_obj_t *parent;
};
#define STORE_OBJ(obj) ((store_obj_t *)(obj))
#define STORE_CONST_OBJ(obj) ((const store_obj_t *)(obj))

typedef struct {
	store_obj_t super;

	char *value;
} sdb_attribute_t;
#define SDB_ATTR(obj) ((sdb_attribute_t *)(obj))
#define SDB_CONST_ATTR(obj) ((const sdb_attribute_t *)(obj))

typedef struct {
	store_obj_t super;

	int type;
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

static int
store_obj_init(sdb_object_t *obj, va_list ap)
{
	store_obj_t *sobj = STORE_OBJ(obj);
	sobj->last_update = va_arg(ap, sdb_time_t);

	sobj->parent = NULL;
	return 0;
} /* store_obj_init */

static void
store_obj_destroy(sdb_object_t *obj)
{
	const store_obj_t *sobj = STORE_OBJ(obj);

	if (sobj->parent)
		sdb_object_deref(SDB_OBJ(sobj->parent));
} /* store_obj_destroy */

static int
sdb_store_obj_init(sdb_object_t *obj, va_list ap)
{
	sdb_store_obj_t *sobj = SDB_STORE_OBJ(obj);
	int ret;

	ret = store_obj_init(obj, ap);
	if (ret)
		return ret;

	sobj->type = va_arg(ap, int);

	sobj->children = sdb_llist_create();
	if (! sobj->children)
		return -1;
	sobj->attributes = sdb_llist_create();
	if (! sobj->attributes)
		return -1;
	return 0;
} /* sdb_store_obj_init */

static void
sdb_store_obj_destroy(sdb_object_t *obj)
{
	sdb_store_obj_t *sobj = SDB_STORE_OBJ(obj);

	assert(obj);

	store_obj_destroy(obj);

	if (sobj->children)
		sdb_llist_destroy(sobj->children);
	if (sobj->attributes)
		sdb_llist_destroy(sobj->attributes);
} /* sdb_store_obj_destroy */

static int
sdb_attr_init(sdb_object_t *obj, va_list ap)
{
	const char *value;
	int ret;

	ret = store_obj_init(obj, ap);
	if (ret)
		return ret;
	value = va_arg(ap, const char *);

	if (value) {
		SDB_ATTR(obj)->value = strdup(value);
		if (! SDB_ATTR(obj)->value)
			return -1;
	}
	return 0;
} /* sdb_attr_init */

static void
sdb_attr_destroy(sdb_object_t *obj)
{
	assert(obj);

	store_obj_destroy(obj);

	if (SDB_ATTR(obj)->value)
		free(SDB_ATTR(obj)->value);
} /* sdb_attr_destroy */

static sdb_type_t sdb_store_obj_type = {
	sizeof(sdb_store_obj_t),

	sdb_store_obj_init,
	sdb_store_obj_destroy
};

static sdb_type_t sdb_attribute_type = {
	sizeof(sdb_attribute_t),

	sdb_attr_init,
	sdb_attr_destroy
};

/*
 * private helper functions
 */

static sdb_store_obj_t *
sdb_store_lookup_in_list(sdb_llist_t *l, int type, const char *name)
{
	sdb_llist_iter_t *iter;

	if (! l)
		return NULL;

	iter = sdb_llist_get_iter(l);
	if (! iter)
		return NULL;

	while (sdb_llist_iter_has_next(iter)) {
		sdb_store_obj_t *sobj = SDB_STORE_OBJ(sdb_llist_iter_get_next(iter));
		assert(sobj);

		if ((sobj->type == type)
				&& (! strcasecmp(SDB_OBJ(sobj)->name, name))) {
			sdb_llist_iter_destroy(iter);
			return sobj;
		}

		sobj = sdb_store_lookup_in_list(sobj->children, type, name);
		if (sobj) {
			sdb_llist_iter_destroy(iter);
			return sobj;
		}
	}
	sdb_llist_iter_destroy(iter);
	return NULL;
} /* sdb_store_lookup_in_list */

static sdb_store_obj_t *
sdb_store_lookup(int type, const char *name)
{
	return sdb_store_lookup_in_list(obj_list, type, name);
} /* sdb_store_lookup */

/* The obj_lock has to be acquired before calling this function. */
static int
store_obj(int parent_type, const char *parent_name,
		int type, const char *name, sdb_time_t last_update,
		store_obj_t **updated_obj)
{
	char *parent_cname = NULL, *cname = NULL;

	sdb_llist_t *parent_list;
	store_obj_t *old;
	int status = 0;

	if (last_update <= 0)
		last_update = sdb_gettime();

	assert((parent_type == 0)
			|| (parent_type == SDB_HOST)
			|| (parent_type == SDB_SERVICE));
	assert((type == 0)
			|| (type == SDB_HOST)
			|| (type == SDB_SERVICE)
			|| (type == SDB_ATTRIBUTE));

	if (parent_type == SDB_HOST) {
		parent_cname = sdb_plugin_cname(strdup(parent_name));
		if (! parent_cname) {
			sdb_log(SDB_LOG_ERR, "store: strdup failed");
			return -1;
		}
		parent_name = parent_cname;
	}
	if (type == SDB_HOST) {
		cname = sdb_plugin_cname(strdup(name));
		if (! cname) {
			sdb_log(SDB_LOG_ERR, "store: strdup failed");
			return -1;
		}
		name = cname;
	}

	if (! obj_list) {
		if (! (obj_list = sdb_llist_create())) {
			free(parent_cname);
			free(cname);
			return -1;
		}
	}
	parent_list = obj_list;

	if (parent_type && parent_name) {
		sdb_store_obj_t *parent;

		parent = sdb_store_lookup(parent_type, parent_name);
		if (! parent) {
			sdb_log(SDB_LOG_ERR, "store: Failed to store %s '%s' - "
					"parent %s '%s' not found", TYPE_TO_NAME(type), name,
					TYPE_TO_NAME(parent_type), parent_name);
			free(parent_cname);
			free(cname);
			return -1;
		}

		if (type == SDB_ATTRIBUTE)
			parent_list = parent->attributes;
		else
			parent_list = parent->children;
	}

	/* TODO: only look into direct children? */
	if (type == SDB_ATTRIBUTE)
		old = STORE_OBJ(sdb_llist_search_by_name(parent_list, name));
	else
		old = STORE_OBJ(sdb_store_lookup_in_list(parent_list, type, name));

	if (old) {
		if (old->last_update > last_update) {
			sdb_log(SDB_LOG_DEBUG, "store: Cannot update %s '%s' - "
					"value too old (%"PRIscTIME" < %"PRIscTIME")",
					TYPE_TO_NAME(type), name, last_update, old->last_update);
			/* don't report an error; the object may be updated by multiple
			 * backends */
			status = 1;
		}
		else {
			old->last_update = last_update;
		}

		if (updated_obj)
			*updated_obj = old;
	}
	else {
		store_obj_t *new;

		if (type == SDB_ATTRIBUTE)
			/* the value will be updated by the caller */
			new = STORE_OBJ(sdb_object_create(name, sdb_attribute_type,
						last_update, NULL));
		else
			new = STORE_OBJ(sdb_object_create(name, sdb_store_obj_type,
						last_update, type));

		if (! new) {
			char errbuf[1024];
			sdb_log(SDB_LOG_ERR, "store: Failed to create %s '%s': %s",
					TYPE_TO_NAME(type), name,
					sdb_strerror(errno, errbuf, sizeof(errbuf)));
			free(parent_cname);
			free(cname);
			return -1;
		}

		/* TODO: insert type-aware; the current version works as long as we
		 * don't support to store hierarchical data */
		status = sdb_llist_insert_sorted(parent_list, SDB_OBJ(new),
				sdb_object_cmp_by_name);

		/* pass control to the list or destroy in case of an error */
		sdb_object_deref(SDB_OBJ(new));

		if (updated_obj)
			*updated_obj = new;
	}
	free(parent_cname);
	free(cname);
	return status;
} /* sdb_store_obj */

/*
 * public API
 */

int
sdb_store_host(const char *name, sdb_time_t last_update)
{
	int status;

	if (! name)
		return -1;

	pthread_rwlock_wrlock(&obj_lock);
	status = store_obj(/* parent = */ 0, NULL,
			/* stored object = */ SDB_HOST, name, last_update,
			/* updated_obj = */ NULL);
	pthread_rwlock_unlock(&obj_lock);
	return status;
} /* sdb_store_host */

_Bool
sdb_store_has_host(const char *name)
{
	sdb_store_obj_t *host;

	if (! name)
		return NULL;

	host = sdb_store_lookup(SDB_HOST, name);
	return host != NULL;
} /* sdb_store_has_host */

int
sdb_store_attribute(const char *hostname, const char *key, const char *value,
		sdb_time_t last_update)
{
	int status;

	store_obj_t *updated_attr = NULL;

	if ((! hostname) || (! key))
		return -1;

	pthread_rwlock_wrlock(&obj_lock);
	status = store_obj(/* parent = */ SDB_HOST, hostname,
			/* stored object = */ SDB_ATTRIBUTE, key, last_update,
			&updated_attr);

	SDB_ATTR(updated_attr)->value = strdup(value);
	if (! SDB_ATTR(updated_attr)->value) {
		sdb_object_deref(SDB_OBJ(updated_attr));
		status = -1;
	}
	pthread_rwlock_unlock(&obj_lock);
	return status;
} /* sdb_store_attribute */

int
sdb_store_service(const char *hostname, const char *name,
		sdb_time_t last_update)
{
	int status;

	if ((! hostname) || (! name))
		return -1;

	pthread_rwlock_wrlock(&obj_lock);
	status = store_obj(/* parent = */ SDB_HOST, hostname,
			/* stored object = */ SDB_SERVICE, name, last_update,
			/* updated obj = */ NULL);
	pthread_rwlock_unlock(&obj_lock);
	return status;
} /* sdb_store_service */

/* TODO: actually support hierarchical data */
int
sdb_store_dump(FILE *fh)
{
	sdb_llist_iter_t *host_iter;

	if (! fh)
		return -1;

	pthread_rwlock_rdlock(&obj_lock);

	host_iter = sdb_llist_get_iter(obj_list);
	if (! host_iter) {
		pthread_rwlock_unlock(&obj_lock);
		return -1;
	}

	while (sdb_llist_iter_has_next(host_iter)) {
		sdb_store_obj_t *host = SDB_STORE_OBJ(sdb_llist_iter_get_next(host_iter));
		sdb_llist_iter_t *svc_iter;
		sdb_llist_iter_t *attr_iter;

		char time_str[64];

		assert(host);

		if (! sdb_strftime(time_str, sizeof(time_str),
					"%F %T %z", host->_last_update))
			snprintf(time_str, sizeof(time_str), "<error>");
		time_str[sizeof(time_str) - 1] = '\0';

		fprintf(fh, "Host '%s' (last updated: %s):\n",
				SDB_OBJ(host)->name, time_str);

		attr_iter = sdb_llist_get_iter(host->attributes);
		if (! attr_iter) {
			char errbuf[1024];
			fprintf(fh, "Failed to retrieve attributes: %s\n",
					sdb_strerror(errno, errbuf, sizeof(errbuf)));
			continue;
		}

		while (sdb_llist_iter_has_next(attr_iter)) {
			sdb_attribute_t *attr = SDB_ATTR(sdb_llist_iter_get_next(attr_iter));
			assert(attr);

			if (! sdb_strftime(time_str, sizeof(time_str),
						"%F %T %z", attr->_last_update))
				snprintf(time_str, sizeof(time_str), "<error>");
			time_str[sizeof(time_str) - 1] = '\0';

			fprintf(fh, "\tAttribute '%s' -> '%s' (last updated: %s)\n",
					SDB_OBJ(attr)->name, attr->value, time_str);
		}

		sdb_llist_iter_destroy(attr_iter);

		svc_iter = sdb_llist_get_iter(host->children);
		if (! svc_iter) {
			char errbuf[1024];
			fprintf(fh, "Failed to retrieve services: %s\n",
					sdb_strerror(errno, errbuf, sizeof(errbuf)));
			continue;
		}

		while (sdb_llist_iter_has_next(svc_iter)) {
			sdb_store_obj_t *svc = SDB_STORE_OBJ(sdb_llist_iter_get_next(svc_iter));
			assert(svc);

			if (! sdb_strftime(time_str, sizeof(time_str),
						"%F %T %z", svc->_last_update))
				snprintf(time_str, sizeof(time_str), "<error>");
			time_str[sizeof(time_str) - 1] = '\0';

			fprintf(fh, "\tService '%s' (last updated: %s)\n",
					SDB_OBJ(svc)->name, time_str);
		}

		sdb_llist_iter_destroy(svc_iter);
	}

	sdb_llist_iter_destroy(host_iter);
	pthread_rwlock_unlock(&obj_lock);
	return 0;
} /* sdb_store_dump */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

