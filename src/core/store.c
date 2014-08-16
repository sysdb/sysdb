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
#include "utils/avltree.h"
#include "utils/error.h"

#include <assert.h>

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

/*
 * private variables
 */

static sdb_avltree_t *hosts = NULL;
static pthread_rwlock_t host_lock = PTHREAD_RWLOCK_INITIALIZER;

/*
 * private types
 */

static sdb_type_t sdb_host_type;
static sdb_type_t sdb_service_type;
static sdb_type_t sdb_metric_type;
static sdb_type_t sdb_attribute_type;

static int
store_obj_init(sdb_object_t *obj, va_list ap)
{
	sdb_store_obj_t *sobj = STORE_OBJ(obj);

	sobj->type = va_arg(ap, int);

	sobj->last_update = va_arg(ap, sdb_time_t);
	sobj->interval = 0;
	sobj->backends = NULL;
	sobj->backends_num = 0;
	sobj->parent = NULL;
	return 0;
} /* store_obj_init */

static void
store_obj_destroy(sdb_object_t *obj)
{
	sdb_store_obj_t *sobj = STORE_OBJ(obj);
	size_t i;

	for (i = 0; i < sobj->backends_num; ++i)
		free(sobj->backends[i]);
	free(sobj->backends);
	sobj->backends = NULL;
	sobj->backends_num = 0;

	if (sobj->parent)
		sdb_object_deref(SDB_OBJ(sobj->parent));
} /* store_obj_destroy */

static int
sdb_host_init(sdb_object_t *obj, va_list ap)
{
	sdb_host_t *sobj = HOST(obj);
	int ret;

	/* this will consume the first argument (type) of ap */
	ret = store_obj_init(obj, ap);
	if (ret)
		return ret;

	sobj->services = sdb_avltree_create();
	if (! sobj->services)
		return -1;
	sobj->metrics = sdb_avltree_create();
	if (! sobj->metrics)
		return -1;
	sobj->attributes = sdb_avltree_create();
	if (! sobj->attributes)
		return -1;
	return 0;
} /* sdb_host_init */

static void
sdb_host_destroy(sdb_object_t *obj)
{
	sdb_host_t *sobj = HOST(obj);
	assert(obj);

	store_obj_destroy(obj);

	if (sobj->services)
		sdb_avltree_destroy(sobj->services);
	if (sobj->metrics)
		sdb_avltree_destroy(sobj->metrics);
	if (sobj->attributes)
		sdb_avltree_destroy(sobj->attributes);
} /* sdb_host_destroy */

static int
sdb_service_init(sdb_object_t *obj, va_list ap)
{
	sdb_service_t *sobj = SVC(obj);
	int ret;

	/* this will consume the first argument (type) of ap */
	ret = store_obj_init(obj, ap);
	if (ret)
		return ret;

	sobj->attributes = sdb_avltree_create();
	if (! sobj->attributes)
		return -1;
	return 0;
} /* sdb_service_init */

static void
sdb_service_destroy(sdb_object_t *obj)
{
	sdb_service_t *sobj = SVC(obj);
	assert(obj);

	store_obj_destroy(obj);

	if (sobj->attributes)
		sdb_avltree_destroy(sobj->attributes);
} /* sdb_service_destroy */

static int
sdb_metric_init(sdb_object_t *obj, va_list ap)
{
	sdb_metric_t *sobj = METRIC(obj);
	int ret;

	/* this will consume the first argument (type) of ap */
	ret = store_obj_init(obj, ap);
	if (ret)
		return ret;

	sobj->attributes = sdb_avltree_create();
	if (! sobj->attributes)
		return -1;

	sobj->store.type = sobj->store.id = NULL;
	return 0;
} /* sdb_metric_init */

static void
sdb_metric_destroy(sdb_object_t *obj)
{
	sdb_metric_t *sobj = METRIC(obj);
	assert(obj);

	store_obj_destroy(obj);

	if (sobj->attributes)
		sdb_avltree_destroy(sobj->attributes);

	if (sobj->store.type)
		free(sobj->store.type);
	if (sobj->store.id)
		free(sobj->store.id);
} /* sdb_metric_destroy */

static int
sdb_attr_init(sdb_object_t *obj, va_list ap)
{
	const sdb_data_t *value;
	int ret;

	/* this will consume the first two arguments
	 * (type and last_update) of ap */
	ret = store_obj_init(obj, ap);
	if (ret)
		return ret;
	value = va_arg(ap, const sdb_data_t *);

	if (value)
		if (sdb_data_copy(&ATTR(obj)->value, value))
			return -1;
	return 0;
} /* sdb_attr_init */

static void
sdb_attr_destroy(sdb_object_t *obj)
{
	assert(obj);

	store_obj_destroy(obj);
	sdb_data_free_datum(&ATTR(obj)->value);
} /* sdb_attr_destroy */

static sdb_type_t sdb_host_type = {
	sizeof(sdb_host_t),
	sdb_host_init,
	sdb_host_destroy
};

static sdb_type_t sdb_service_type = {
	sizeof(sdb_service_t),
	sdb_service_init,
	sdb_service_destroy
};

static sdb_type_t sdb_metric_type = {
	sizeof(sdb_metric_t),
	sdb_metric_init,
	sdb_metric_destroy
};

static sdb_type_t sdb_attribute_type = {
	sizeof(sdb_attribute_t),
	sdb_attr_init,
	sdb_attr_destroy
};

/*
 * private helper functions
 */

static sdb_host_t *
lookup_host(const char *name)
{
	return HOST(sdb_avltree_lookup(hosts, name));
} /* lookup_host */

static int
record_backend(sdb_store_obj_t *obj)
{
	const sdb_plugin_info_t *info;
	char **tmp;
	size_t i;

	info = sdb_plugin_current();
	if (! info)
		return 0;

	for (i = 0; i < obj->backends_num; ++i)
		if (!strcasecmp(obj->backends[i], info->plugin_name))
			return 0;

	tmp = realloc(obj->backends,
			(obj->backends_num + 1) * sizeof(*obj->backends));
	if (! tmp)
		return -1;

	obj->backends = tmp;
	obj->backends[obj->backends_num] = strdup(info->plugin_name);
	if (! obj->backends[obj->backends_num])
		return -1;

	++obj->backends_num;
	return 0;
} /* record_backend */

static int
store_obj(sdb_avltree_t *parent_tree, int type, const char *name,
		sdb_time_t last_update, sdb_store_obj_t **updated_obj)
{
	sdb_store_obj_t *old, *new;
	int status = 0;

	assert(parent_tree);

	if (last_update <= 0)
		last_update = sdb_gettime();

	old = STORE_OBJ(sdb_avltree_lookup(parent_tree, name));
	if (old) {
		if (old->last_update > last_update) {
			sdb_log(SDB_LOG_DEBUG, "store: Cannot update %s '%s' - "
					"value too old (%"PRIsdbTIME" < %"PRIsdbTIME")",
					SDB_STORE_TYPE_TO_NAME(type), name,
					last_update, old->last_update);
			/* don't report an error; the object may be updated by multiple
			 * backends */
			status = 1;
		}
		else if (old->last_update == last_update) {
			/* don't report an error and also don't even log this to avoid
			 * excessive noise on high sampling frequencies */
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

		new = old;
		sdb_object_deref(SDB_OBJ(old));
	}
	else {
		if (type == SDB_ATTRIBUTE) {
			/* the value will be updated by the caller */
			new = STORE_OBJ(sdb_object_create(name, sdb_attribute_type,
						type, last_update, NULL));
		}
		else {
			sdb_type_t t;
			t = type == SDB_HOST
				? sdb_host_type
				: type == SDB_SERVICE
					? sdb_service_type
					: sdb_metric_type;
			new = STORE_OBJ(sdb_object_create(name, t, type, last_update));
		}

		if (new) {
			status = sdb_avltree_insert(parent_tree, SDB_OBJ(new));

			/* pass control to the tree or destroy in case of an error */
			sdb_object_deref(SDB_OBJ(new));
		}
		else {
			char errbuf[1024];
			sdb_log(SDB_LOG_ERR, "store: Failed to create %s '%s': %s",
					SDB_STORE_TYPE_TO_NAME(type), name,
					sdb_strerror(errno, errbuf, sizeof(errbuf)));
			status = -1;
		}
	}

	if (status < 0)
		return status;
	assert(new);

	if (updated_obj)
		*updated_obj = new;

	if (record_backend(new))
		return -1;
	return status;
} /* store_obj */

static int
store_attr(sdb_avltree_t *attributes, const char *key, const sdb_data_t *value,
		sdb_time_t last_update)
{
	sdb_store_obj_t *attr = NULL;
	int status;

	status = store_obj(attributes, SDB_ATTRIBUTE, key, last_update, &attr);
	if (status)
		return status;

	/* don't update unchanged values */
	if (! sdb_data_cmp(&ATTR(attr)->value, value))
		return status;

	assert(attr);
	if (sdb_data_copy(&ATTR(attr)->value, value))
		return -1;
	return status;
} /* store_attr */

/* The host_lock has to be acquired before calling this function. */
static sdb_avltree_t *
get_host_children(const char *hostname, int type)
{
	char *cname = NULL;
	sdb_host_t *host;

	assert(hostname);
	assert((type == SDB_SERVICE) || (type == SDB_METRIC)
			|| (type == SDB_ATTRIBUTE));

	if (! hosts)
		return NULL;

	cname = sdb_plugin_cname(strdup(hostname));
	if (! cname) {
		sdb_log(SDB_LOG_ERR, "store: strdup failed");
		return NULL;
	}

	host = lookup_host(cname);
	free(cname);
	if (! host)
		return NULL;

	sdb_object_deref(SDB_OBJ(host));
	if (type == SDB_ATTRIBUTE)
		return host->attributes;
	else if (type == SDB_METRIC)
		return host->metrics;
	else
		return host->services;
} /* get_host_children */

/*
 * store_common_tojson serializes common object attributes to JSON.
 *
 * The function never returns an error. Rather, an error message will be part
 * of the serialized data.
 */
static void
store_common_tojson(sdb_store_obj_t *obj, sdb_strbuf_t *buf)
{
	char time_str[64];
	char interval_str[64];
	size_t i;

	if (! sdb_strftime(time_str, sizeof(time_str),
				"%F %T %z", obj->last_update))
		snprintf(time_str, sizeof(time_str), "<error>");
	time_str[sizeof(time_str) - 1] = '\0';

	if (! sdb_strfinterval(interval_str, sizeof(interval_str),
				obj->interval))
		snprintf(interval_str, sizeof(interval_str), "<error>");
	interval_str[sizeof(interval_str) - 1] = '\0';

	sdb_strbuf_append(buf, "\"last_update\": \"%s\", "
			"\"update_interval\": \"%s\", \"backends\": [",
			time_str, interval_str);

	for (i = 0; i < obj->backends_num; ++i) {
		sdb_strbuf_append(buf, "\"%s\"", obj->backends[i]);
		if (i < obj->backends_num - 1)
			sdb_strbuf_append(buf, ",");
	}
	sdb_strbuf_append(buf, "]");
} /* store_common_tojson */

/*
 * store_obj_tojson serializes attribute / metric / service objects to JSON.
 *
 * The function never returns an error. Rather, an error message will be part
 * of the serialized data.
 */
static void
store_obj_tojson(sdb_avltree_t *tree, int type, sdb_strbuf_t *buf,
		sdb_store_matcher_t *filter, int flags)
{
	sdb_avltree_iter_t *iter;

	assert((type == SDB_ATTRIBUTE)
			|| (type == SDB_METRIC)
			|| (type == SDB_SERVICE));

	sdb_strbuf_append(buf, "[");
	iter = sdb_avltree_get_iter(tree);
	if (! iter) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "store: Failed to retrieve %ss: %s\n",
				SDB_STORE_TYPE_TO_NAME(type),
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		sdb_strbuf_append(buf, "{\"error\": \"failed to retrieve %ss: %s\"}",
				SDB_STORE_TYPE_TO_NAME(type), errbuf);
	}

	/* has_next returns false if the iterator is NULL */
	while (sdb_avltree_iter_has_next(iter)) {
		sdb_store_obj_t *sobj = STORE_OBJ(sdb_avltree_iter_get_next(iter));
		assert(sobj);
		assert(sobj->type == type);

		if (filter && (! sdb_store_matcher_matches(filter, sobj, NULL)))
			continue;

		sdb_strbuf_append(buf, "{\"name\": \"%s\", ", SDB_OBJ(sobj)->name);
		if (sobj->type == SDB_ATTRIBUTE) {
			char tmp[sdb_data_strlen(&ATTR(sobj)->value) + 1];
			sdb_data_format(&ATTR(sobj)->value, tmp, sizeof(tmp),
					SDB_DOUBLE_QUOTED);
			sdb_strbuf_append(buf, "\"value\": %s, ", tmp);
		}
		store_common_tojson(sobj, buf);

		if ((sobj->type == SDB_SERVICE)
				&& (! (flags & SDB_SKIP_ATTRIBUTES))) {
			sdb_strbuf_append(buf, ", \"attributes\": ");
			store_obj_tojson(SVC(sobj)->attributes, SDB_ATTRIBUTE,
					buf, filter, flags);
		}
		else if ((sobj->type == SDB_METRIC)
				&& (! (flags & SDB_SKIP_ATTRIBUTES))) {
			sdb_strbuf_append(buf, ", \"attributes\": ");
			store_obj_tojson(METRIC(sobj)->attributes, SDB_ATTRIBUTE,
					buf, filter, flags);
		}
		sdb_strbuf_append(buf, "}");

		if (sdb_avltree_iter_has_next(iter))
			sdb_strbuf_append(buf, ",");
	}

	sdb_avltree_iter_destroy(iter);
	sdb_strbuf_append(buf, "]");
} /* store_obj_tojson */

/*
 * public API
 */

void
sdb_store_clear(void)
{
	sdb_avltree_destroy(hosts);
	hosts = NULL;
} /* sdb_store_clear */

int
sdb_store_host(const char *name, sdb_time_t last_update)
{
	char *cname = NULL;
	int status = 0;

	if (! name)
		return -1;

	cname = sdb_plugin_cname(strdup(name));
	if (! cname) {
		sdb_log(SDB_LOG_ERR, "store: strdup failed");
		return -1;
	}

	pthread_rwlock_wrlock(&host_lock);
	if (! hosts)
		if (! (hosts = sdb_avltree_create()))
			status = -1;

	if (! status)
		status = store_obj(hosts, SDB_HOST, cname, last_update, NULL);
	pthread_rwlock_unlock(&host_lock);

	free(cname);
	return status;
} /* sdb_store_host */

_Bool
sdb_store_has_host(const char *name)
{
	sdb_host_t *host;

	if (! name)
		return NULL;

	host = lookup_host(name);
	return host != NULL;
} /* sdb_store_has_host */

sdb_store_obj_t *
sdb_store_get_host(const char *name)
{
	sdb_host_t *host;

	if (! name)
		return NULL;

	host = lookup_host(name);
	if (! host)
		return NULL;

	return STORE_OBJ(host);
} /* sdb_store_get_host */

int
sdb_store_attribute(const char *hostname,
		const char *key, const sdb_data_t *value,
		sdb_time_t last_update)
{
	sdb_avltree_t *attrs;
	int status = 0;

	if ((! hostname) || (! key))
		return -1;

	pthread_rwlock_wrlock(&host_lock);
	attrs = get_host_children(hostname, SDB_ATTRIBUTE);
	if (! attrs) {
		sdb_log(SDB_LOG_ERR, "store: Failed to store attribute '%s' - "
				"host '%s' not found", key, hostname);
		status = -1;
	}

	if (! status)
		status = store_attr(attrs, key, value, last_update);

	pthread_rwlock_unlock(&host_lock);
	return status;
} /* sdb_store_attribute */

int
sdb_store_service(const char *hostname, const char *name,
		sdb_time_t last_update)
{
	sdb_avltree_t *services;

	int status = 0;

	if ((! hostname) || (! name))
		return -1;

	pthread_rwlock_wrlock(&host_lock);
	services = get_host_children(hostname, SDB_SERVICE);
	if (! services) {
		sdb_log(SDB_LOG_ERR, "store: Failed to store service '%s' - "
				"host '%s' not found", name, hostname);
		status = -1;
	}

	if (! status)
		status = store_obj(services, SDB_SERVICE, name, last_update, NULL);
	pthread_rwlock_unlock(&host_lock);
	return status;
} /* sdb_store_service */

int
sdb_store_service_attr(const char *hostname, const char *service,
		const char *key, const sdb_data_t *value, sdb_time_t last_update)
{
	sdb_avltree_t *services;
	sdb_service_t *svc;
	int status = 0;

	if ((! hostname) || (! service) || (! key))
		return -1;

	pthread_rwlock_wrlock(&host_lock);
	services = get_host_children(hostname, SDB_SERVICE);
	if (! services) {
		sdb_log(SDB_LOG_ERR, "store: Failed to store attribute '%s' "
				"for service '%s' - host '%ss' not found",
				key, service, hostname);
		pthread_rwlock_unlock(&host_lock);
		return -1;
	}

	svc = SVC(sdb_avltree_lookup(services, service));
	if (! svc) {
		sdb_log(SDB_LOG_ERR, "store: Failed to store attribute '%s' - "
				"service '%s/%s' not found", key, hostname, service);
		status = -1;
	}

	if (! status)
		status = store_attr(svc->attributes, key, value, last_update);

	sdb_object_deref(SDB_OBJ(svc));
	pthread_rwlock_unlock(&host_lock);
	return status;
} /* sdb_store_service_attr */

int
sdb_store_metric(const char *hostname, const char *name,
		sdb_metric_store_t *store, sdb_time_t last_update)
{
	sdb_store_obj_t *obj = NULL;
	sdb_metric_t *metric;

	sdb_avltree_t *metrics;

	int status = 0;

	if ((! hostname) || (! name))
		return -1;
	if (store && ((! store->type) || (! store->id)))
		return -1;

	pthread_rwlock_wrlock(&host_lock);
	metrics = get_host_children(hostname, SDB_METRIC);
	if (! metrics) {
		sdb_log(SDB_LOG_ERR, "store: Failed to store metric '%s' - "
				"host '%s' not found", name, hostname);
		status = -1;
	}

	if (! status)
		status = store_obj(metrics, SDB_METRIC, name, last_update, &obj);

	if (status || (! store)) {
		pthread_rwlock_unlock(&host_lock);
		return status;
	}

	assert(obj);
	metric = METRIC(obj);

	if ((! metric->store.type) || strcasecmp(metric->store.type, store->type)) {
		if (metric->store.type)
			free(metric->store.type);
		metric->store.type = strdup(store->type);
	}
	if ((! metric->store.id) || strcasecmp(metric->store.id, store->id)) {
		if (metric->store.id)
			free(metric->store.id);
		metric->store.id = strdup(store->id);
	}

	if ((! metric->store.type) || (! metric->store.id)) {
		if (metric->store.type)
			free(metric->store.type);
		if (metric->store.id)
			free(metric->store.id);
		metric->store.type = metric->store.id = NULL;
		status = -1;
	}
	pthread_rwlock_unlock(&host_lock);
	return status;
} /* sdb_store_metric */

int
sdb_store_metric_attr(const char *hostname, const char *metric,
		const char *key, const sdb_data_t *value, sdb_time_t last_update)
{
	sdb_avltree_t *metrics;
	sdb_metric_t *m;
	int status = 0;

	if ((! hostname) || (! metric) || (! key))
		return -1;

	pthread_rwlock_wrlock(&host_lock);
	metrics = get_host_children(hostname, SDB_METRIC);
	if (! metrics) {
		sdb_log(SDB_LOG_ERR, "store: Failed to store attribute '%s' "
				"for metric '%s' - host '%ss' not found",
				key, metric, hostname);
		pthread_rwlock_unlock(&host_lock);
		return -1;
	}

	m = METRIC(sdb_avltree_lookup(metrics, metric));
	if (! m) {
		sdb_log(SDB_LOG_ERR, "store: Failed to store attribute '%s' - "
				"metric '%s/%s' not found", key, hostname, metric);
		status = -1;
	}

	if (! status)
		status = store_attr(m->attributes, key, value, last_update);

	sdb_object_deref(SDB_OBJ(m));
	pthread_rwlock_unlock(&host_lock);
	return status;
} /* sdb_store_metric_attr */

int
sdb_store_get_field(sdb_store_obj_t *obj, int field, sdb_data_t *res)
{
	if ((! obj) || (! res))
		return -1;

	switch (field) {
		case SDB_FIELD_LAST_UPDATE:
			res->type = SDB_TYPE_DATETIME;
			res->data.datetime = obj->last_update;
			break;
		case SDB_FIELD_AGE:
			res->type = SDB_TYPE_DATETIME;
			res->data.datetime = sdb_gettime() - obj->last_update;
			break;
		case SDB_FIELD_INTERVAL:
			res->type = SDB_TYPE_DATETIME;
			res->data.datetime = obj->interval;
			break;
		case SDB_FIELD_BACKEND:
			/* TODO: add support for storing array values in a data object
			 * for now, fall thru to the error case */
		default:
			return -1;
	}
	return 0;
} /* sdb_store_get_field */

int
sdb_store_host_tojson(sdb_store_obj_t *h, sdb_strbuf_t *buf,
		sdb_store_matcher_t *filter, int flags)
{
	sdb_host_t *host = HOST(h);

	if ((! h) || (h->type != SDB_HOST) || (! buf))
		return -1;

	sdb_strbuf_append(buf, "{\"name\": \"%s\", ", SDB_OBJ(host)->name);
	store_common_tojson(h, buf);

	if (! (flags & SDB_SKIP_ATTRIBUTES)) {
		sdb_strbuf_append(buf, ", \"attributes\": ");
		store_obj_tojson(host->attributes, SDB_ATTRIBUTE, buf, filter, flags);
	}

	if (! (flags & SDB_SKIP_METRICS)) {
		sdb_strbuf_append(buf, ", \"metrics\": ");
		store_obj_tojson(host->metrics, SDB_METRIC, buf, filter, flags);
	}

	if (! (flags & SDB_SKIP_SERVICES)) {
		sdb_strbuf_append(buf, ", \"services\": ");
		store_obj_tojson(host->services, SDB_SERVICE, buf, filter, flags);
	}

	sdb_strbuf_append(buf, "}");
	return 0;
} /* sdb_store_host_tojson */

int
sdb_store_tojson(sdb_strbuf_t *buf, sdb_store_matcher_t *filter, int flags)
{
	sdb_avltree_iter_t *host_iter;
	size_t len;

	if (! buf)
		return -1;

	pthread_rwlock_rdlock(&host_lock);

	host_iter = sdb_avltree_get_iter(hosts);
	if (! host_iter) {
		pthread_rwlock_unlock(&host_lock);
		return -1;
	}

	sdb_strbuf_append(buf, "{\"hosts\":[");

	len = sdb_strbuf_len(buf);
	while (sdb_avltree_iter_has_next(host_iter)) {
		sdb_store_obj_t *host;

		host = STORE_OBJ(sdb_avltree_iter_get_next(host_iter));
		assert(host);

		if (filter && (! sdb_store_matcher_matches(filter, host, NULL)))
			continue;

		if (sdb_strbuf_len(buf) > len)
			sdb_strbuf_append(buf, ",");
		len = sdb_strbuf_len(buf);

		if (sdb_store_host_tojson(host, buf, filter, flags))
			return -1;
	}

	sdb_strbuf_append(buf, "]}");

	sdb_avltree_iter_destroy(host_iter);
	pthread_rwlock_unlock(&host_lock);
	return 0;
} /* sdb_store_tojson */

/* TODO: actually support hierarchical data */
int
sdb_store_iterate(sdb_store_iter_cb cb, void *user_data)
{
	sdb_avltree_iter_t *host_iter;
	int status = 0;

	pthread_rwlock_rdlock(&host_lock);

	host_iter = sdb_avltree_get_iter(hosts);
	if (! host_iter)
		status = -1;

	/* has_next returns false if the iterator is NULL */
	while (sdb_avltree_iter_has_next(host_iter)) {
		sdb_store_obj_t *host;

		host = STORE_OBJ(sdb_avltree_iter_get_next(host_iter));
		assert(host);

		if (cb(host, user_data)) {
			status = -1;
			break;
		}
	}

	sdb_avltree_iter_destroy(host_iter);
	pthread_rwlock_unlock(&host_lock);
	return status;
} /* sdb_store_iterate */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

