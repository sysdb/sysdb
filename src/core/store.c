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

	// We don't currently keep an extra reference for parent objects to
	// avoid circular self-references which are not handled correctly by
	// the ref-count base management layer.
	//sdb_object_deref(SDB_OBJ(sobj->parent));
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
lookup_host(const char *name, _Bool canonicalize)
{
	sdb_host_t *host;
	char *cname;

	assert(name);
	if (! canonicalize)
		return HOST(sdb_avltree_lookup(hosts, name));

	cname = strdup(name);
	cname = sdb_plugin_cname(cname);
	if (! cname) {
		sdb_log(SDB_LOG_ERR, "store: strdup failed");
		return NULL;
	}

	host = HOST(sdb_avltree_lookup(hosts, cname));
	free(cname);
	return host;
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
store_obj(sdb_store_obj_t *parent, sdb_avltree_t *parent_tree,
		int type, const char *name, sdb_time_t last_update,
		sdb_store_obj_t **updated_obj)
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

	if (new->parent != parent) {
		// Avoid circular self-references which are not handled
		// correctly by the ref-count based management layer.
		//sdb_object_deref(SDB_OBJ(new->parent));
		//sdb_object_ref(SDB_OBJ(parent));
		new->parent = parent;
	}

	if (updated_obj)
		*updated_obj = new;

	if (record_backend(new))
		return -1;
	return status;
} /* store_obj */

static int
store_attr(sdb_store_obj_t *parent, sdb_avltree_t *attributes,
		const char *key, const sdb_data_t *value, sdb_time_t last_update)
{
	sdb_store_obj_t *attr = NULL;
	int status;

	status = store_obj(parent, attributes, SDB_ATTRIBUTE,
			key, last_update, &attr);
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
get_host_children(sdb_host_t *host, int type)
{
	assert((type == SDB_SERVICE) || (type == SDB_METRIC)
			|| (type == SDB_ATTRIBUTE));

	if (! host)
		return NULL;

	if (type == SDB_ATTRIBUTE)
		return host->attributes;
	else if (type == SDB_METRIC)
		return host->metrics;
	else
		return host->services;
} /* get_host_children */

/*
 * ts_tojson serializes a time-series to JSON.
 *
 * The function never returns an error. Rather, an error message will be part
 * of the serialized data.
 */
static void
ts_tojson(sdb_timeseries_t *ts, sdb_strbuf_t *buf)
{
	char start_str[64];
	char end_str[64];

	size_t i;

	/* TODO: make time format configurable */
	if (! sdb_strftime(start_str, sizeof(start_str),
				"%F %T %z", ts->start))
		snprintf(start_str, sizeof(start_str), "<error>");
	start_str[sizeof(start_str) - 1] = '\0';
	if (! sdb_strftime(end_str, sizeof(end_str),
				"%F %T %z", ts->end))
		snprintf(end_str, sizeof(end_str), "<error>");
	end_str[sizeof(end_str) - 1] = '\0';

	sdb_strbuf_append(buf, "{\"start\": \"%s\", \"end\": \"%s\", \"data\": {",
			start_str, end_str);

	for (i = 0; i < ts->data_names_len; ++i) {
		size_t j;
		sdb_strbuf_append(buf, "\"%s\": [", ts->data_names[i]);

		for (j = 0; j < ts->data_len; ++j) {
			char time_str[64];

			if (! sdb_strftime(time_str, sizeof(time_str),
						"%F %T %z", ts->data[i][j].timestamp))
				snprintf(time_str, sizeof(time_str), "<error>");
			time_str[sizeof(time_str) - 1] = '\0';

			sdb_strbuf_append(buf, "{\"timestamp\": \"%s\", "
					"\"value\": \"%f\"}", time_str, ts->data[i][j].value);

			if (j < ts->data_len - 1)
				sdb_strbuf_append(buf, ",");
		}

		if (i < ts->data_names_len - 1)
			sdb_strbuf_append(buf, "],");
		else
			sdb_strbuf_append(buf, "]");
	}
	sdb_strbuf_append(buf, "}}");
} /* ts_tojson */

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
		status = store_obj(NULL, hosts, SDB_HOST, cname, last_update, NULL);
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

	host = lookup_host(name, /* canonicalize = */ 0);
	sdb_object_deref(SDB_OBJ(host));
	return host != NULL;
} /* sdb_store_has_host */

sdb_store_obj_t *
sdb_store_get_host(const char *name)
{
	sdb_host_t *host;

	if (! name)
		return NULL;

	host = lookup_host(name, /* canonicalize = */ 0);
	if (! host)
		return NULL;

	return STORE_OBJ(host);
} /* sdb_store_get_host */

int
sdb_store_attribute(const char *hostname,
		const char *key, const sdb_data_t *value,
		sdb_time_t last_update)
{
	sdb_host_t *host;
	sdb_avltree_t *attrs;
	int status = 0;

	if ((! hostname) || (! key))
		return -1;

	pthread_rwlock_wrlock(&host_lock);
	host = lookup_host(hostname, /* canonicalize = */ 1);
	attrs = get_host_children(host, SDB_ATTRIBUTE);
	if (! attrs) {
		sdb_log(SDB_LOG_ERR, "store: Failed to store attribute '%s' - "
				"host '%s' not found", key, hostname);
		status = -1;
	}

	if (! status)
		status = store_attr(STORE_OBJ(host), attrs, key, value, last_update);

	sdb_object_deref(SDB_OBJ(host));
	pthread_rwlock_unlock(&host_lock);
	return status;
} /* sdb_store_attribute */

int
sdb_store_service(const char *hostname, const char *name,
		sdb_time_t last_update)
{
	sdb_host_t *host;
	sdb_avltree_t *services;

	int status = 0;

	if ((! hostname) || (! name))
		return -1;

	pthread_rwlock_wrlock(&host_lock);
	host = lookup_host(hostname, /* canonicalize = */ 1);
	services = get_host_children(host, SDB_SERVICE);
	if (! services) {
		sdb_log(SDB_LOG_ERR, "store: Failed to store service '%s' - "
				"host '%s' not found", name, hostname);
		status = -1;
	}

	if (! status)
		status = store_obj(STORE_OBJ(host), services, SDB_SERVICE,
				name, last_update, NULL);

	sdb_object_deref(SDB_OBJ(host));
	pthread_rwlock_unlock(&host_lock);
	return status;
} /* sdb_store_service */

int
sdb_store_service_attr(const char *hostname, const char *service,
		const char *key, const sdb_data_t *value, sdb_time_t last_update)
{
	sdb_host_t *host;
	sdb_service_t *svc;
	sdb_avltree_t *services;
	int status = 0;

	if ((! hostname) || (! service) || (! key))
		return -1;

	pthread_rwlock_wrlock(&host_lock);
	host = lookup_host(hostname, /* canonicalize = */ 1);
	services = get_host_children(host, SDB_SERVICE);
	sdb_object_deref(SDB_OBJ(host));
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
		status = store_attr(STORE_OBJ(svc), svc->attributes,
				key, value, last_update);

	sdb_object_deref(SDB_OBJ(svc));
	pthread_rwlock_unlock(&host_lock);
	return status;
} /* sdb_store_service_attr */

int
sdb_store_metric(const char *hostname, const char *name,
		sdb_metric_store_t *store, sdb_time_t last_update)
{
	sdb_store_obj_t *obj = NULL;
	sdb_host_t *host;
	sdb_metric_t *metric;

	sdb_avltree_t *metrics;

	int status = 0;

	if ((! hostname) || (! name))
		return -1;
	if (store && ((! store->type) || (! store->id)))
		return -1;

	pthread_rwlock_wrlock(&host_lock);
	host = lookup_host(hostname, /* canonicalize = */ 1);
	metrics = get_host_children(host, SDB_METRIC);
	if (! metrics) {
		sdb_log(SDB_LOG_ERR, "store: Failed to store metric '%s' - "
				"host '%s' not found", name, hostname);
		status = -1;
	}

	if (! status)
		status = store_obj(STORE_OBJ(host), metrics, SDB_METRIC,
				name, last_update, &obj);
	sdb_object_deref(SDB_OBJ(host));

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
	sdb_host_t *host;
	sdb_metric_t *m;
	int status = 0;

	if ((! hostname) || (! metric) || (! key))
		return -1;

	pthread_rwlock_wrlock(&host_lock);
	host = lookup_host(hostname, /* canonicalize = */ 1);
	metrics = get_host_children(host, SDB_METRIC);
	sdb_object_deref(SDB_OBJ(host));
	if (! metrics) {
		sdb_log(SDB_LOG_ERR, "store: Failed to store attribute '%s' "
				"for metric '%s' - host '%s' not found",
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
		status = store_attr(STORE_OBJ(m), m->attributes,
				key, value, last_update);

	sdb_object_deref(SDB_OBJ(m));
	pthread_rwlock_unlock(&host_lock);
	return status;
} /* sdb_store_metric_attr */

int
sdb_store_fetch_timeseries(const char *hostname, const char *metric,
		sdb_timeseries_opts_t *opts, sdb_strbuf_t *buf)
{
	sdb_avltree_t *metrics;
	sdb_host_t *host;
	sdb_metric_t *m;

	sdb_timeseries_t *ts;

	if ((! hostname) || (! metric) || (! opts) || (! buf))
		return -1;

	pthread_rwlock_rdlock(&host_lock);
	host = lookup_host(hostname, /* canonicalize = */ 1);
	metrics = get_host_children(host, SDB_METRIC);
	sdb_object_deref(SDB_OBJ(host));
	if (! metrics) {
		sdb_log(SDB_LOG_ERR, "store: Failed to fetch time-series '%s/%s' "
				"- host '%s' not found", hostname, metric, hostname);
		pthread_rwlock_unlock(&host_lock);
		return -1;
	}

	m = METRIC(sdb_avltree_lookup(metrics, metric));
	if (! m) {
		sdb_log(SDB_LOG_ERR, "store: Failed to fetch time-series '%s/%s' "
				"- metric '%s' not found", hostname, metric, metric);
		pthread_rwlock_unlock(&host_lock);
		return -1;
	}

	if ((! m->store.type) || (! m->store.id)) {
		sdb_log(SDB_LOG_ERR, "store: Failed to fetch time-series '%s/%s' "
				"- no data-store configured for the stored metric",
				hostname, metric);
		pthread_rwlock_unlock(&host_lock);
		return -1;
	}

	{
		char type[strlen(m->store.type) + 1];
		char id[strlen(m->store.id) + 1];

		strncpy(type, m->store.type, sizeof(type));
		strncpy(id, m->store.id, sizeof(id));
		pthread_rwlock_unlock(&host_lock);

		ts = sdb_plugin_fetch_timeseries(type, id, opts);
		if (! ts) {
			sdb_log(SDB_LOG_ERR, "store: Failed to fetch time-series '%s/%s' "
					"- %s fetcher callback returned no data for '%s'",
					hostname, metric, type, id);
			return -1;
		}
	}

	ts_tojson(ts, buf);
	sdb_timeseries_destroy(ts);
	return 0;
} /* sdb_store_fetch_timeseries */

int
sdb_store_get_field(sdb_store_obj_t *obj, int field, sdb_data_t *res)
{
	sdb_data_t tmp;

	if (! obj)
		return -1;

	switch (field) {
		case SDB_FIELD_NAME:
			tmp.type = SDB_TYPE_STRING;
			tmp.data.string = strdup(SDB_OBJ(obj)->name);
			if (! tmp.data.string)
				return -1;
			break;
		case SDB_FIELD_LAST_UPDATE:
			tmp.type = SDB_TYPE_DATETIME;
			tmp.data.datetime = obj->last_update;
			break;
		case SDB_FIELD_AGE:
			tmp.type = SDB_TYPE_DATETIME;
			tmp.data.datetime = sdb_gettime() - obj->last_update;
			break;
		case SDB_FIELD_INTERVAL:
			tmp.type = SDB_TYPE_DATETIME;
			tmp.data.datetime = obj->interval;
			break;
		case SDB_FIELD_BACKEND:
		{
			tmp.type = SDB_TYPE_ARRAY | SDB_TYPE_STRING;
			tmp.data.array.length = obj->backends_num;
			tmp.data.array.values = obj->backends;
			return sdb_data_copy(res, &tmp);
			break;
		}
		default:
			return -1;
	}
	if (res)
		*res = tmp;
	else
		sdb_data_free_datum(&tmp);
	return 0;
} /* sdb_store_get_field */

int
sdb_store_get_attr(sdb_store_obj_t *obj, const char *name, sdb_data_t *res,
		sdb_store_matcher_t *filter)
{
	sdb_avltree_t *tree = NULL;
	sdb_store_obj_t *attr;

	if ((! obj) || (! name))
		return -1;

	if (obj->type == SDB_HOST)
		tree = HOST(obj)->attributes;
	else if (obj->type == SDB_SERVICE)
		tree = SVC(obj)->attributes;
	else if (obj->type == SDB_METRIC)
		tree = METRIC(obj)->attributes;

	if (! tree)
		return -1;

	attr = STORE_OBJ(sdb_avltree_lookup(tree, name));
	if (! attr)
		return -1;
	if (filter && (! sdb_store_matcher_matches(filter, attr, NULL))) {
		sdb_object_deref(SDB_OBJ(attr));
		return -1;
	}

	assert(STORE_OBJ(attr)->type == SDB_ATTRIBUTE);
	if (res)
		sdb_data_copy(res, &ATTR(attr)->value);
	sdb_object_deref(SDB_OBJ(attr));
	return 0;
} /* sdb_store_get_attr */

int
sdb_store_scan(int type, sdb_store_matcher_t *m, sdb_store_matcher_t *filter,
		sdb_store_lookup_cb cb, void *user_data)
{
	sdb_avltree_iter_t *host_iter;
	int status = 0;

	if (! cb)
		return -1;

	if ((type != SDB_HOST) && (type != SDB_SERVICE) && (type != SDB_METRIC)) {
		sdb_log(SDB_LOG_ERR, "store: Cannot scan objects of type %d", type);
		return -1;
	}

	pthread_rwlock_rdlock(&host_lock);

	host_iter = sdb_avltree_get_iter(hosts);
	if (! host_iter)
		status = -1;

	/* has_next returns false if the iterator is NULL */
	while (sdb_avltree_iter_has_next(host_iter)) {
		sdb_store_obj_t *host;
		sdb_avltree_iter_t *iter = NULL;

		host = STORE_OBJ(sdb_avltree_iter_get_next(host_iter));
		assert(host);

		if (! sdb_store_matcher_matches(filter, host, NULL))
			continue;

		if (type == SDB_SERVICE)
			iter = sdb_avltree_get_iter(HOST(host)->services);
		else if (type == SDB_METRIC)
			iter = sdb_avltree_get_iter(HOST(host)->metrics);

		if (iter) {
			while (sdb_avltree_iter_has_next(iter)) {
				sdb_store_obj_t *obj;
				obj = STORE_OBJ(sdb_avltree_iter_get_next(iter));
				assert(obj);

				if (sdb_store_matcher_matches(m, obj, filter)) {
					if (cb(obj, filter, user_data)) {
						sdb_log(SDB_LOG_ERR, "store: Callback returned "
								"an error while scanning");
						status = -1;
						break;
					}
				}
			}
		}
		else if (sdb_store_matcher_matches(m, host, filter)) {
			if (cb(host, filter, user_data)) {
				sdb_log(SDB_LOG_ERR, "store: Callback returned "
						"an error while scanning");
				status = -1;
			}
		}

		sdb_avltree_iter_destroy(iter);
		if (status)
			break;
	}

	sdb_avltree_iter_destroy(host_iter);
	pthread_rwlock_unlock(&host_lock);
	return status;
} /* sdb_store_scan */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

