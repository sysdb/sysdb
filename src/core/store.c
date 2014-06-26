/*
 * SysDB - src/core/store.c
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

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "sysdb.h"
#include "core/store-private.h"
#include "core/plugin.h"
#include "utils/error.h"
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

static sdb_llist_t *host_list = NULL;
static pthread_rwlock_t obj_lock = PTHREAD_RWLOCK_INITIALIZER;

/*
 * private types
 */

static sdb_type_t sdb_store_obj_type;
static sdb_type_t sdb_attribute_type;

static int
store_base_init(sdb_object_t *obj, va_list ap)
{
	sdb_store_base_t *sobj = STORE_BASE(obj);

	sobj->type = va_arg(ap, int);

	sobj->last_update = va_arg(ap, sdb_time_t);
	sobj->interval = 0;
	sobj->parent = NULL;
	return 0;
} /* store_base_init */

static void
store_base_destroy(sdb_object_t *obj)
{
	const sdb_store_base_t *sobj = STORE_CONST_BASE(obj);

	if (sobj->parent)
		sdb_object_deref(SDB_OBJ(sobj->parent));
} /* store_base_destroy */

static int
sdb_store_obj_init(sdb_object_t *obj, va_list ap)
{
	sdb_store_obj_t *sobj = SDB_STORE_OBJ(obj);
	int ret;

	/* this will consume the first argument (type) of ap */
	ret = store_base_init(obj, ap);
	if (ret)
		return ret;

	sobj->services = sdb_llist_create();
	if (! sobj->services)
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

	store_base_destroy(obj);

	if (sobj->services)
		sdb_llist_destroy(sobj->services);
	if (sobj->attributes)
		sdb_llist_destroy(sobj->attributes);
} /* sdb_store_obj_destroy */

static int
sdb_attr_init(sdb_object_t *obj, va_list ap)
{
	const sdb_data_t *value;
	int ret;

	/* this will consume the first two arguments
	 * (type and last_update) of ap */
	ret = store_base_init(obj, ap);
	if (ret)
		return ret;
	value = va_arg(ap, const sdb_data_t *);

	if (value)
		if (sdb_data_copy(&SDB_ATTR(obj)->value, value))
			return -1;
	return 0;
} /* sdb_attr_init */

static void
sdb_attr_destroy(sdb_object_t *obj)
{
	assert(obj);

	store_base_destroy(obj);
	sdb_data_free_datum(&SDB_ATTR(obj)->value);
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
store_lookup_in_list(sdb_llist_t *l, int type, const char *name)
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

		if ((STORE_BASE(sobj)->type == type)
				&& (! strcasecmp(SDB_OBJ(sobj)->name, name))) {
			sdb_llist_iter_destroy(iter);
			return sobj;
		}

		/* don't lookups non-host types from hierarchical hosts */
		if ((type != SDB_HOST) && (STORE_BASE(sobj)->type == SDB_HOST))
			continue;

		sobj = store_lookup_in_list(sobj->services, type, name);
		if (sobj) {
			sdb_llist_iter_destroy(iter);
			return sobj;
		}
	}
	sdb_llist_iter_destroy(iter);
	return NULL;
} /* store_lookup_in_list */

static sdb_store_obj_t *
store_lookup(int type, const char *name)
{
	return store_lookup_in_list(host_list, type, name);
} /* store_lookup */

/* The obj_lock has to be acquired before calling this function. */
static int
store_obj(const char *hostname, int type, const char *name,
		sdb_time_t last_update, sdb_store_base_t **updated_obj)
{
	char *host_cname = NULL, *cname = NULL;

	sdb_llist_t *parent_list;
	sdb_store_base_t *old;
	int status = 0;

	if (last_update <= 0)
		last_update = sdb_gettime();

	assert((type == 0)
			|| (type == SDB_HOST)
			|| (type == SDB_SERVICE)
			|| (type == SDB_ATTRIBUTE));

	if (! host_list)
		if (! (host_list = sdb_llist_create()))
			return -1;
	parent_list = host_list;

	if (type == SDB_HOST) {
		cname = sdb_plugin_cname(strdup(name));
		if (! cname) {
			sdb_log(SDB_LOG_ERR, "store: strdup failed");
			return -1;
		}
		name = cname;
	}

	if (hostname) {
		sdb_store_obj_t *parent;

		host_cname = sdb_plugin_cname(strdup(hostname));
		if (! host_cname) {
			sdb_log(SDB_LOG_ERR, "store: strdup failed");
			free(cname);
			return -1;
		}
		hostname = host_cname;

		parent = store_lookup(SDB_HOST, hostname);
		if (! parent) {
			sdb_log(SDB_LOG_ERR, "store: Failed to store %s '%s' - "
					"host '%s' not found", SDB_STORE_TYPE_TO_NAME(type),
					name, hostname);
			free(host_cname);
			free(cname);
			return -1;
		}

		if (type == SDB_ATTRIBUTE)
			parent_list = parent->attributes;
		else
			parent_list = parent->services;
	}

	if (type == SDB_HOST)
		/* make sure that each host is unique */
		old = STORE_BASE(store_lookup_in_list(host_list, type, name));
	else if (type == SDB_ATTRIBUTE)
		/* look into attributes of this host */
		old = STORE_BASE(sdb_llist_search_by_name(parent_list, name));
	else
		/* look into services assigned to this host (store_lookup_in_list
		 * does not look up services from hierarchical hosts) */
		old = STORE_BASE(store_lookup_in_list(parent_list, type, name));

	if (old) {
		if (old->last_update > last_update) {
			sdb_log(SDB_LOG_DEBUG, "store: Cannot update %s '%s' - "
					"value too old (%"PRIscTIME" < %"PRIscTIME")",
					SDB_STORE_TYPE_TO_NAME(type), name,
					last_update, old->last_update);
			/* don't report an error; the object may be updated by multiple
			 * backends */
			status = 1;
		}
		else {
			sdb_time_t interval = last_update - old->last_update;
			old->last_update = last_update;
			if (interval) {
				if (old->interval)
					old->interval = (sdb_time_t)((0.9 * (double)old->interval)
							+ (0.1 * (double)interval));
				else
					old->interval = interval;
			}
		}

		if (updated_obj)
			*updated_obj = old;
	}
	else {
		sdb_store_base_t *new;

		if (type == SDB_ATTRIBUTE)
			/* the value will be updated by the caller */
			new = STORE_BASE(sdb_object_create(name, sdb_attribute_type,
						type, last_update, NULL));
		else
			new = STORE_BASE(sdb_object_create(name, sdb_store_obj_type,
						type, last_update));

		if (! new) {
			char errbuf[1024];
			sdb_log(SDB_LOG_ERR, "store: Failed to create %s '%s': %s",
					SDB_STORE_TYPE_TO_NAME(type), name,
					sdb_strerror(errno, errbuf, sizeof(errbuf)));
			free(host_cname);
			free(cname);
			return -1;
		}

		status = sdb_llist_insert_sorted(parent_list, SDB_OBJ(new),
				sdb_object_cmp_by_name);

		/* pass control to the list or destroy in case of an error */
		sdb_object_deref(SDB_OBJ(new));

		if (updated_obj)
			*updated_obj = new;
	}
	free(host_cname);
	free(cname);
	return status;
} /* store_obj */

/*
 * store_obj_tojson serializes attribute / service objects to JSON.
 *
 * The function never returns an error. Rather, an error message will be part
 * of the serialized data.
 */
static void
store_obj_tojson(sdb_llist_t *list, int type, sdb_strbuf_t *buf)
{
	sdb_llist_iter_t *iter;
	char time_str[64];
	char interval_str[64];

	assert((type == SDB_ATTRIBUTE) || (type == SDB_SERVICE));

	sdb_strbuf_append(buf, "[");

	iter = sdb_llist_get_iter(list);
	if (! iter) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "store: Failed to retrieve %ss: %s\n",
				SDB_STORE_TYPE_TO_NAME(type),
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		sdb_strbuf_append(buf, "{\"error\": \"failed to retrieve %ss: %s\"}",
				SDB_STORE_TYPE_TO_NAME(type), errbuf);
	}

	/* has_next returns false if the iterator is NULL */
	while (sdb_llist_iter_has_next(iter)) {
		sdb_store_base_t *sobj = STORE_BASE(sdb_llist_iter_get_next(iter));
		assert(sobj);

		if (! sdb_strftime(time_str, sizeof(time_str),
					"%F %T %z", sobj->last_update))
			snprintf(time_str, sizeof(time_str), "<error>");
		time_str[sizeof(time_str) - 1] = '\0';

		if (! sdb_strfinterval(interval_str, sizeof(interval_str),
					sobj->interval))
			snprintf(interval_str, sizeof(interval_str), "<error>");
		interval_str[sizeof(interval_str) - 1] = '\0';

		sdb_strbuf_append(buf, "{\"name\": \"%s\", ", SDB_OBJ(sobj)->name);
		if (type == SDB_ATTRIBUTE) {
			char tmp[sdb_data_strlen(&SDB_ATTR(sobj)->value) + 1];
			sdb_data_format(&SDB_ATTR(sobj)->value, tmp, sizeof(tmp),
					SDB_DOUBLE_QUOTED);
			sdb_strbuf_append(buf, "\"value\": %s, \"last_update\": \"%s\", "
					"\"update_interval\": \"%s\"}", tmp, time_str,
					interval_str);
		}
		else
			sdb_strbuf_append(buf, "\"last_update\": \"%s\", "
					"\"update_interval\": \"%s\"}", time_str, interval_str);

		if (sdb_llist_iter_has_next(iter))
			sdb_strbuf_append(buf, ",");
	}

	sdb_llist_iter_destroy(iter);
	sdb_strbuf_append(buf, "]");
} /* store_obj_tojson */

/*
 * public API
 */

void
sdb_store_clear(void)
{
	sdb_llist_destroy(host_list);
	host_list = NULL;
} /* sdb_store_clear */

int
sdb_store_host(const char *name, sdb_time_t last_update)
{
	int status;

	if (! name)
		return -1;

	pthread_rwlock_wrlock(&obj_lock);
	status = store_obj(/* hostname = */ NULL,
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

	host = store_lookup(SDB_HOST, name);
	return host != NULL;
} /* sdb_store_has_host */

sdb_store_base_t *
sdb_store_get_host(const char *name)
{
	sdb_store_obj_t *host;

	if (! name)
		return NULL;

	host = store_lookup(SDB_HOST, name);
	if (! host)
		return NULL;

	sdb_object_ref(SDB_OBJ(host));
	return STORE_BASE(host);
} /* sdb_store_get_host */

int
sdb_store_attribute(const char *hostname,
		const char *key, const sdb_data_t *value,
		sdb_time_t last_update)
{
	int status;

	sdb_store_base_t *updated_attr = NULL;

	if ((! hostname) || (! key))
		return -1;

	pthread_rwlock_wrlock(&obj_lock);
	status = store_obj(hostname,
			/* stored object = */ SDB_ATTRIBUTE, key, last_update,
			&updated_attr);

	if (status >= 0) {
		assert(updated_attr);
		sdb_data_free_datum(&SDB_ATTR(updated_attr)->value);
		if (sdb_data_copy(&SDB_ATTR(updated_attr)->value, value)) {
			sdb_object_deref(SDB_OBJ(updated_attr));
			status = -1;
		}
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
	status = store_obj(hostname,
			/* stored object = */ SDB_SERVICE, name, last_update,
			/* updated obj = */ NULL);
	pthread_rwlock_unlock(&obj_lock);
	return status;
} /* sdb_store_service */

int
sdb_store_host_tojson(sdb_store_base_t *h, sdb_strbuf_t *buf, int flags)
{
	sdb_store_obj_t *host;
	char time_str[64];
	char interval_str[64];

	if ((! h) || (h->type != SDB_HOST) || (! buf))
		return -1;

	host = SDB_STORE_OBJ(h);

	if (! sdb_strftime(time_str, sizeof(time_str),
				"%F %T %z", host->_last_update))
		snprintf(time_str, sizeof(time_str), "<error>");
	time_str[sizeof(time_str) - 1] = '\0';

	if (! sdb_strfinterval(interval_str, sizeof(interval_str),
				host->_interval))
		snprintf(interval_str, sizeof(interval_str), "<error>");
	interval_str[sizeof(interval_str) - 1] = '\0';

	sdb_strbuf_append(buf, "{\"name\": \"%s\", "
			"\"last_update\": \"%s\", \"update_interval\": \"%s\"",
			SDB_OBJ(host)->name, time_str, interval_str);

	if (! (flags & SDB_SKIP_ATTRIBUTES)) {
		sdb_strbuf_append(buf, ", \"attributes\": ");
		store_obj_tojson(host->attributes, SDB_ATTRIBUTE, buf);
	}

	if (! (flags & SDB_SKIP_SERVICES)) {
		sdb_strbuf_append(buf, ", \"services\": ");
		store_obj_tojson(host->services, SDB_SERVICE, buf);
	}

	sdb_strbuf_append(buf, "}");
	return 0;
} /* sdb_store_host_tojson */

int
sdb_store_tojson(sdb_strbuf_t *buf, int flags)
{
	sdb_llist_iter_t *host_iter;

	if (! buf)
		return -1;

	pthread_rwlock_rdlock(&obj_lock);

	host_iter = sdb_llist_get_iter(host_list);
	if (! host_iter) {
		pthread_rwlock_unlock(&obj_lock);
		return -1;
	}

	sdb_strbuf_append(buf, "{\"hosts\":[");

	while (sdb_llist_iter_has_next(host_iter)) {
		sdb_store_base_t *host = STORE_BASE(sdb_llist_iter_get_next(host_iter));
		assert(host);

		if (sdb_store_host_tojson(host, buf, flags))
			return -1;

		if (sdb_llist_iter_has_next(host_iter))
			sdb_strbuf_append(buf, ",");
	}

	sdb_strbuf_append(buf, "]}");

	sdb_llist_iter_destroy(host_iter);
	pthread_rwlock_unlock(&obj_lock);
	return 0;
} /* sdb_store_tojson */

/* TODO: actually support hierarchical data */
int
sdb_store_iterate(sdb_store_iter_cb cb, void *user_data)
{
	sdb_llist_iter_t *host_iter;
	int status = 0;

	pthread_rwlock_rdlock(&obj_lock);

	host_iter = sdb_llist_get_iter(host_list);
	if (! host_iter)
		status = -1;

	/* has_next returns false if the iterator is NULL */
	while (sdb_llist_iter_has_next(host_iter)) {
		sdb_store_base_t *host = STORE_BASE(sdb_llist_iter_get_next(host_iter));
		assert(host);

		if (cb(host, user_data)) {
			status = -1;
			break;
		}
	}

	sdb_llist_iter_destroy(host_iter);
	pthread_rwlock_unlock(&obj_lock);
	return status;
} /* sdb_store_iterate */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

