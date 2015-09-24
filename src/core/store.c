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

#include <math.h>
#include <pthread.h>

/*
 * private types
 */

struct sdb_store {
	sdb_object_t super;

	/* hosts are the top-level entries and
	 * reference everything else */
	sdb_avltree_t *hosts;
	pthread_rwlock_t host_lock;
};

/* internal representation of a to-be-stored object */
typedef struct {
	sdb_store_obj_t *parent;
	sdb_avltree_t *parent_tree;
	int type;
	const char *name;
	sdb_time_t last_update;
	const char **backends;
	size_t backends_num;
} store_obj_t;
#define STORE_OBJ_INIT { NULL, NULL, 0, NULL, 0, NULL, 0 }

static sdb_type_t host_type;
static sdb_type_t service_type;
static sdb_type_t metric_type;
static sdb_type_t attribute_type;

static int
store_init(sdb_object_t *obj, va_list __attribute__((unused)) ap)
{
	int err;
	if (! (SDB_STORE(obj)->hosts = sdb_avltree_create()))
		return -1;
	if ((err = pthread_rwlock_init(&SDB_STORE(obj)->host_lock,
					/* attr = */ NULL))) {
		char errbuf[128];
		sdb_log(SDB_LOG_ERR, "store: Failed to initialize lock: %s",
				sdb_strerror(err, errbuf, sizeof(errbuf)));
		return -1;
	}
	return 0;
} /* store_init */

static void
store_destroy(sdb_object_t *obj)
{
	int err;
	if ((err = pthread_rwlock_destroy(&SDB_STORE(obj)->host_lock))) {
		char errbuf[128];
		sdb_log(SDB_LOG_ERR, "store: Failed to destroy lock: %s",
				sdb_strerror(err, errbuf, sizeof(errbuf)));
		return;
	}
	sdb_avltree_destroy(SDB_STORE(obj)->hosts);
	SDB_STORE(obj)->hosts = NULL;
} /* store_destroy */

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
host_init(sdb_object_t *obj, va_list ap)
{
	host_t *sobj = HOST(obj);
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
} /* host_init */

static void
host_destroy(sdb_object_t *obj)
{
	host_t *sobj = HOST(obj);
	assert(obj);

	store_obj_destroy(obj);

	if (sobj->services)
		sdb_avltree_destroy(sobj->services);
	if (sobj->metrics)
		sdb_avltree_destroy(sobj->metrics);
	if (sobj->attributes)
		sdb_avltree_destroy(sobj->attributes);
} /* host_destroy */

static int
service_init(sdb_object_t *obj, va_list ap)
{
	service_t *sobj = SVC(obj);
	int ret;

	/* this will consume the first argument (type) of ap */
	ret = store_obj_init(obj, ap);
	if (ret)
		return ret;

	sobj->attributes = sdb_avltree_create();
	if (! sobj->attributes)
		return -1;
	return 0;
} /* service_init */

static void
service_destroy(sdb_object_t *obj)
{
	service_t *sobj = SVC(obj);
	assert(obj);

	store_obj_destroy(obj);

	if (sobj->attributes)
		sdb_avltree_destroy(sobj->attributes);
} /* service_destroy */

static int
metric_init(sdb_object_t *obj, va_list ap)
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
} /* metric_init */

static void
metric_destroy(sdb_object_t *obj)
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
} /* metric_destroy */

static int
attr_init(sdb_object_t *obj, va_list ap)
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
} /* attr_init */

static void
attr_destroy(sdb_object_t *obj)
{
	assert(obj);

	store_obj_destroy(obj);
	sdb_data_free_datum(&ATTR(obj)->value);
} /* attr_destroy */

static sdb_type_t store_type = {
	/* size = */ sizeof(sdb_store_t),
	/* init = */ store_init,
	/* destroy = */ store_destroy,
};

static sdb_type_t host_type = {
	/* size = */ sizeof(host_t),
	/* init = */ host_init,
	/* destroy = */ host_destroy
};

static sdb_type_t service_type = {
	/* size = */ sizeof(service_t),
	/* init = */ service_init,
	/* destroy = */ service_destroy
};

static sdb_type_t metric_type = {
	/* size = */ sizeof(sdb_metric_t),
	/* init = */ metric_init,
	/* destroy = */ metric_destroy
};

static sdb_type_t attribute_type = {
	/* size = */ sizeof(attr_t),
	/* init = */ attr_init,
	/* destroy = */ attr_destroy
};

/*
 * private helper functions
 */

static int
record_backends(sdb_store_obj_t *obj,
		const char **backends, size_t backends_num)
{
	char **tmp;
	size_t i;

	for (i = 0; i < backends_num; i++) {
		bool found = 0;
		size_t j;

		for (j = 0; j < obj->backends_num; ++j) {
			if (!strcasecmp(obj->backends[j], backends[i])) {
				found = 1;
				break;
			}
		}
		if (found)
			continue;

		tmp = realloc(obj->backends,
				(obj->backends_num + 1) * sizeof(*obj->backends));
		if (! tmp)
			return -1;

		obj->backends = tmp;
		obj->backends[obj->backends_num] = strdup(backends[i]);
		if (! obj->backends[obj->backends_num])
			return -1;

		++obj->backends_num;
	}
	return 0;
} /* record_backends */

static int
store_obj(store_obj_t *obj, sdb_store_obj_t **updated_obj)
{
	sdb_store_obj_t *old, *new;
	int status = 0;

	assert(obj->parent_tree);

	if (obj->last_update <= 0)
		obj->last_update = sdb_gettime();

	old = STORE_OBJ(sdb_avltree_lookup(obj->parent_tree, obj->name));
	if (old) {
		if (old->last_update > obj->last_update) {
			sdb_log(SDB_LOG_DEBUG, "store: Cannot update %s '%s' - "
					"value too old (%"PRIsdbTIME" < %"PRIsdbTIME")",
					SDB_STORE_TYPE_TO_NAME(obj->type), obj->name,
					obj->last_update, old->last_update);
			/* don't report an error; the object may be updated by multiple
			 * backends */
			status = 1;
		}
		else if (old->last_update == obj->last_update) {
			/* don't report an error and also don't even log this to avoid
			 * excessive noise on high sampling frequencies */
			status = 1;
		}
		else {
			sdb_time_t interval = obj->last_update - old->last_update;
			old->last_update = obj->last_update;
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
		if (obj->type == SDB_ATTRIBUTE) {
			/* the value will be updated by the caller */
			new = STORE_OBJ(sdb_object_create(obj->name, attribute_type,
						obj->type, obj->last_update, NULL));
		}
		else {
			sdb_type_t t;
			t = obj->type == SDB_HOST
				? host_type
				: obj->type == SDB_SERVICE
					? service_type
					: metric_type;
			new = STORE_OBJ(sdb_object_create(obj->name, t,
						obj->type, obj->last_update));
		}

		if (new) {
			status = sdb_avltree_insert(obj->parent_tree, SDB_OBJ(new));

			/* pass control to the tree or destroy in case of an error */
			sdb_object_deref(SDB_OBJ(new));
		}
		else {
			char errbuf[1024];
			sdb_log(SDB_LOG_ERR, "store: Failed to create %s '%s': %s",
					SDB_STORE_TYPE_TO_NAME(obj->type), obj->name,
					sdb_strerror(errno, errbuf, sizeof(errbuf)));
			status = -1;
		}
	}

	if (status < 0)
		return status;
	assert(new);

	if (new->parent != obj->parent) {
		// Avoid circular self-references which are not handled
		// correctly by the ref-count based management layer.
		//sdb_object_deref(SDB_OBJ(new->parent));
		//sdb_object_ref(SDB_OBJ(obj->parent));
		new->parent = obj->parent;
	}

	if (updated_obj)
		*updated_obj = new;

	if (record_backends(new, obj->backends, obj->backends_num))
		return -1;
	return status;
} /* store_obj */

static int
store_metric_store(sdb_metric_t *metric, sdb_store_metric_t *m)
{
	char *type = metric->store.type;
	char *id = metric->store.id;

	if ((! metric->store.type) || strcasecmp(metric->store.type, m->store.type)) {
		if (! (type = strdup(m->store.type)))
			return -1;
	}
	if ((! metric->store.id) || strcasecmp(metric->store.id, m->store.id)) {
		if (! (id = strdup(m->store.id))) {
			if (type != metric->store.type)
				free(type);
			return -1;
		}
	}

	if (type != metric->store.type) {
		if (metric->store.type)
			free(metric->store.type);
		metric->store.type = type;
	}
	if (id != metric->store.id) {
		if (metric->store.id)
			free(metric->store.id);
		metric->store.id = id;
	}
	return 0;
} /* store_metric_store */

/* The store's host_lock has to be acquired before calling this function. */
static sdb_avltree_t *
get_host_children(host_t *host, int type)
{
	if ((type != SDB_SERVICE) && (type != SDB_METRIC)
			&& (type != SDB_ATTRIBUTE))
		return NULL;

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
	if (! sdb_strftime(start_str, sizeof(start_str), ts->start))
		snprintf(start_str, sizeof(start_str), "<error>");
	start_str[sizeof(start_str) - 1] = '\0';
	if (! sdb_strftime(end_str, sizeof(end_str), ts->end))
		snprintf(end_str, sizeof(end_str), "<error>");
	end_str[sizeof(end_str) - 1] = '\0';

	sdb_strbuf_append(buf, "{\"start\": \"%s\", \"end\": \"%s\", \"data\": {",
			start_str, end_str);

	for (i = 0; i < ts->data_names_len; ++i) {
		size_t j;
		sdb_strbuf_append(buf, "\"%s\": [", ts->data_names[i]);

		for (j = 0; j < ts->data_len; ++j) {
			char time_str[64];

			if (! sdb_strftime(time_str, sizeof(time_str), ts->data[i][j].timestamp))
				snprintf(time_str, sizeof(time_str), "<error>");
			time_str[sizeof(time_str) - 1] = '\0';

			/* Some GNU libc versions may print '-nan' which we dont' want */
			if (isnan(ts->data[i][j].value))
				sdb_strbuf_append(buf, "{\"timestamp\": \"%s\", "
						"\"value\": \"nan\"}", time_str);
			else
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
 * store writer API
 */

static int
store_attribute(sdb_store_attribute_t *attr, sdb_object_t *user_data)
{
	sdb_store_t *st = SDB_STORE(user_data);
	store_obj_t obj = STORE_OBJ_INIT;
	sdb_store_obj_t *new = NULL;
	const char *hostname;
	host_t *host;

	sdb_avltree_t *children = NULL;
	int status = 0;

	if ((! attr) || (! attr->parent) || (! attr->key))
		return -1;

	hostname = attr->hostname;
	if (attr->parent_type == SDB_HOST)
		hostname = attr->parent;
	if (! hostname)
		return -1;

	pthread_rwlock_wrlock(&st->host_lock);
	host = HOST(sdb_avltree_lookup(st->hosts, hostname));
	if (! host) {
		sdb_log(SDB_LOG_ERR, "store: Failed to store attribute '%s' - "
				"host '%s' not found", attr->key, hostname);
		status = -1;
	}

	switch (attr->parent_type) {
	case SDB_HOST:
		obj.parent = STORE_OBJ(host);
		obj.parent_tree = get_host_children(host, SDB_ATTRIBUTE);
		break;
	case SDB_SERVICE:
		children = get_host_children(host, SDB_SERVICE);
		break;
	case SDB_METRIC:
		children = get_host_children(host, SDB_METRIC);
		break;
	default:
		status = -1;
		break;
	}

	if (children) {
		obj.parent = STORE_OBJ(sdb_avltree_lookup(children, attr->parent));
		if (! obj.parent) {
			sdb_log(SDB_LOG_ERR, "store: Failed to store attribute '%s' - "
					"%s '%s/%s' not found", attr->key,
					SDB_STORE_TYPE_TO_NAME(attr->parent_type),
					attr->hostname, attr->parent);
			status = -1;
		}
		else
			obj.parent_tree = attr->parent_type == SDB_SERVICE
				? SVC(obj.parent)->attributes
				: METRIC(obj.parent)->attributes;
	}

	obj.type = SDB_ATTRIBUTE;
	obj.name = attr->key;
	obj.last_update = attr->last_update;
	obj.backends = attr->backends;
	obj.backends_num = attr->backends_num;
	if (! status)
		status = store_obj(&obj, &new);

	if (! status) {
		assert(new);
		/* update the value if it changed */
		if (sdb_data_cmp(&ATTR(new)->value, &attr->value))
			if (sdb_data_copy(&ATTR(new)->value, &attr->value))
				status = -1;
	}

	if (obj.parent != STORE_OBJ(host))
		sdb_object_deref(SDB_OBJ(obj.parent));
	sdb_object_deref(SDB_OBJ(host));
	pthread_rwlock_unlock(&st->host_lock);

	return status;
} /* store_attribute */

static int
store_host(sdb_store_host_t *host, sdb_object_t *user_data)
{
	sdb_store_t *st = SDB_STORE(user_data);
	store_obj_t obj = { NULL, st->hosts, SDB_HOST, NULL, 0, NULL, 0 };
	int status = 0;

	if ((! host) || (! host->name))
		return -1;

	obj.name = host->name;
	obj.last_update = host->last_update;
	obj.backends = host->backends;
	obj.backends_num = host->backends_num;
	pthread_rwlock_wrlock(&st->host_lock);
	status = store_obj(&obj, NULL);
	pthread_rwlock_unlock(&st->host_lock);

	return status;
} /* store_host */

static int
store_service(sdb_store_service_t *service, sdb_object_t *user_data)
{
	sdb_store_t *st = SDB_STORE(user_data);
	store_obj_t obj = STORE_OBJ_INIT;
	host_t *host;

	int status = 0;

	if ((! service) || (! service->hostname) || (! service->name))
		return -1;

	pthread_rwlock_wrlock(&st->host_lock);
	host = HOST(sdb_avltree_lookup(st->hosts, service->hostname));
	obj.parent = STORE_OBJ(host);
	obj.parent_tree = get_host_children(host, SDB_SERVICE);
	obj.type = SDB_SERVICE;
	if (! obj.parent_tree) {
		sdb_log(SDB_LOG_ERR, "store: Failed to store service '%s' - "
				"host '%s' not found", service->name, service->hostname);
		status = -1;
	}

	obj.name = service->name;
	obj.last_update = service->last_update;
	obj.backends = service->backends;
	obj.backends_num = service->backends_num;
	if (! status)
		status = store_obj(&obj, NULL);

	sdb_object_deref(SDB_OBJ(host));
	pthread_rwlock_unlock(&st->host_lock);
	return status;
} /* store_service */

static int
store_metric(sdb_store_metric_t *metric, sdb_object_t *user_data)
{
	sdb_store_t *st = SDB_STORE(user_data);
	store_obj_t obj = STORE_OBJ_INIT;
	sdb_store_obj_t *new = NULL;
	host_t *host;

	int status = 0;

	if ((! metric) || (! metric->hostname) || (! metric->name))
		return -1;

	if ((metric->store.type != NULL) != (metric->store.id != NULL))
		return -1;

	pthread_rwlock_wrlock(&st->host_lock);
	host = HOST(sdb_avltree_lookup(st->hosts, metric->hostname));
	obj.parent = STORE_OBJ(host);
	obj.parent_tree = get_host_children(host, SDB_METRIC);
	obj.type = SDB_METRIC;
	if (! obj.parent_tree) {
		sdb_log(SDB_LOG_ERR, "store: Failed to store metric '%s' - "
				"host '%s' not found", metric->name, metric->hostname);
		status = -1;
	}

	obj.name = metric->name;
	obj.last_update = metric->last_update;
	obj.backends = metric->backends;
	obj.backends_num = metric->backends_num;
	if (! status)
		status = store_obj(&obj, &new);
	sdb_object_deref(SDB_OBJ(host));

	if (status) {
		pthread_rwlock_unlock(&st->host_lock);
		return status;
	}

	assert(new);
	if (metric->store.type && metric->store.id)
		if (store_metric_store(METRIC(new), metric))
			status = -1;
	pthread_rwlock_unlock(&st->host_lock);
	return status;
} /* store_metric */

sdb_store_writer_t sdb_store_writer = {
	store_host, store_service, store_metric, store_attribute,
};

static sdb_object_t *
prepare_query(sdb_ast_node_t *ast,
		sdb_strbuf_t __attribute__((unused)) *errbuf,
		sdb_object_t __attribute__((unused)) *user_data)
{
	return SDB_OBJ(sdb_store_query_prepare(ast));
} /* prepare_query */

static int
execute_query(sdb_object_t *q,
		sdb_strbuf_t *buf, sdb_strbuf_t *errbuf,
		sdb_object_t *user_data)
{
	return sdb_store_query_execute(SDB_STORE(user_data),
			QUERY(q), buf, errbuf);
} /* execute_query */

sdb_store_reader_t sdb_store_reader = {
	prepare_query, execute_query,
};

/*
 * public API
 */

sdb_store_t *
sdb_store_create(void)
{
	return SDB_STORE(sdb_object_create("store", store_type));
} /* sdb_store_create */

int
sdb_store_host(sdb_store_t *store, const char *name, sdb_time_t last_update)
{
	sdb_store_host_t host = {
		name, last_update, 0, NULL, 0,
	};
	return store_host(&host, SDB_OBJ(store));
} /* sdb_store_host */

int
sdb_store_service(sdb_store_t *store, const char *hostname, const char *name,
		sdb_time_t last_update)
{
	sdb_store_service_t service = {
		hostname, name, last_update, 0, NULL, 0,
	};
	return store_service(&service, SDB_OBJ(store));
} /* sdb_store_service */

int
sdb_store_metric(sdb_store_t *store, const char *hostname, const char *name,
		sdb_metric_store_t *metric_store, sdb_time_t last_update)
{
	sdb_store_metric_t metric = {
		hostname, name, { NULL, NULL }, last_update, 0, NULL, 0,
	};
	if (metric_store) {
		metric.store.type = metric_store->type;
		metric.store.id = metric_store->id;
	}
	return store_metric(&metric, SDB_OBJ(store));
} /* sdb_store_metric */

int
sdb_store_attribute(sdb_store_t *store, const char *hostname,
		const char *key, const sdb_data_t *value, sdb_time_t last_update)
{
	sdb_store_attribute_t attr = {
		NULL, SDB_HOST, hostname, key, SDB_DATA_INIT, last_update, 0, NULL, 0,
	};
	if (value) {
		attr.value = *value;
	}
	return store_attribute(&attr, SDB_OBJ(store));
} /* sdb_store_attribute */

int
sdb_store_service_attr(sdb_store_t *store, const char *hostname,
		const char *service, const char *key, const sdb_data_t *value,
		sdb_time_t last_update)
{
	sdb_store_attribute_t attr = {
		hostname, SDB_SERVICE, service, key, SDB_DATA_INIT, last_update, 0, NULL, 0,
	};
	if (value) {
		attr.value = *value;
	}
	return store_attribute(&attr, SDB_OBJ(store));
} /* sdb_store_service_attr */

int
sdb_store_metric_attr(sdb_store_t *store, const char *hostname,
		const char *metric, const char *key, const sdb_data_t *value,
		sdb_time_t last_update)
{
	sdb_store_attribute_t attr = {
		hostname, SDB_METRIC, metric, key, SDB_DATA_INIT, last_update, 0, NULL, 0,
	};
	if (value) {
		attr.value = *value;
	}
	return store_attribute(&attr, SDB_OBJ(store));
} /* sdb_store_metric_attr */

sdb_store_obj_t *
sdb_store_get_host(sdb_store_t *store, const char *name)
{
	host_t *host;

	if ((! store) || (! name))
		return NULL;

	host = HOST(sdb_avltree_lookup(store->hosts, name));
	if (! host)
		return NULL;

	return STORE_OBJ(host);
} /* sdb_store_get_host */

sdb_store_obj_t *
sdb_store_get_child(sdb_store_obj_t *host, int type, const char *name)
{
	sdb_avltree_t *children;

	if ((! host) || (host->type != SDB_HOST) || (! name))
		return NULL;

	children = get_host_children(HOST(host), type);
	if (! children)
		return NULL;
	return STORE_OBJ(sdb_avltree_lookup(children, name));
} /* sdb_store_get_child */

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
			if (! res)
				return 0;
			tmp.type = SDB_TYPE_ARRAY | SDB_TYPE_STRING;
			tmp.data.array.length = obj->backends_num;
			tmp.data.array.values = obj->backends;
			return sdb_data_copy(res, &tmp);
		case SDB_FIELD_VALUE:
			if (obj->type != SDB_ATTRIBUTE)
				return -1;
			if (! res)
				return 0;
			return sdb_data_copy(res, &ATTR(obj)->value);
		case SDB_FIELD_TIMESERIES:
			if (obj->type != SDB_METRIC)
				return -1;
			tmp.type = SDB_TYPE_BOOLEAN;
			tmp.data.boolean = METRIC(obj)->store.type != NULL;
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

/* TODO: sdb_store_fetch_timeseries should move into the plugin module */

int
sdb_store_fetch_timeseries(sdb_store_t *store,
		const char *hostname, const char *metric,
		sdb_timeseries_opts_t *opts, sdb_strbuf_t *buf)
{
	sdb_avltree_t *metrics;
	host_t *host;
	sdb_metric_t *m;

	sdb_timeseries_t *ts;

	int status = 0;

	if ((! store) || (! hostname) || (! metric) || (! opts) || (! buf))
		return -1;

	pthread_rwlock_rdlock(&store->host_lock);
	host = HOST(sdb_avltree_lookup(store->hosts, hostname));
	metrics = get_host_children(host, SDB_METRIC);
	sdb_object_deref(SDB_OBJ(host));
	if (! metrics) {
		sdb_log(SDB_LOG_ERR, "store: Failed to fetch time-series '%s/%s' "
				"- host '%s' not found", hostname, metric, hostname);
		pthread_rwlock_unlock(&store->host_lock);
		return -1;
	}

	m = METRIC(sdb_avltree_lookup(metrics, metric));
	if (! m) {
		sdb_log(SDB_LOG_ERR, "store: Failed to fetch time-series '%s/%s' "
				"- metric '%s' not found", hostname, metric, metric);
		pthread_rwlock_unlock(&store->host_lock);
		return -1;
	}

	if ((! m->store.type) || (! m->store.id)) {
		sdb_log(SDB_LOG_ERR, "store: Failed to fetch time-series '%s/%s' "
				"- no data-store configured for the stored metric",
				hostname, metric);
		sdb_object_deref(SDB_OBJ(m));
		pthread_rwlock_unlock(&store->host_lock);
		return -1;
	}

	{
		char type[strlen(m->store.type) + 1];
		char id[strlen(m->store.id) + 1];

		strncpy(type, m->store.type, sizeof(type));
		strncpy(id, m->store.id, sizeof(id));
		pthread_rwlock_unlock(&store->host_lock);

		ts = sdb_plugin_fetch_timeseries(type, id, opts);
		if (! ts) {
			sdb_log(SDB_LOG_ERR, "store: Failed to fetch time-series '%s/%s' "
					"- %s fetcher callback returned no data for '%s'",
					hostname, metric, type, id);
			status = -1;
		}
	}

	ts_tojson(ts, buf);
	sdb_object_deref(SDB_OBJ(m));
	sdb_timeseries_destroy(ts);
	return status;
} /* sdb_store_fetch_timeseries */

int
sdb_store_scan(sdb_store_t *store, int type,
		sdb_store_matcher_t *m, sdb_store_matcher_t *filter,
		sdb_store_lookup_cb cb, void *user_data)
{
	sdb_avltree_iter_t *host_iter = NULL;
	int status = 0;

	if ((! store) || (! cb))
		return -1;

	if ((type != SDB_HOST) && (type != SDB_SERVICE) && (type != SDB_METRIC)) {
		sdb_log(SDB_LOG_ERR, "store: Cannot scan objects of type %d", type);
		return -1;
	}

	pthread_rwlock_rdlock(&store->host_lock);
	host_iter = sdb_avltree_get_iter(store->hosts);
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
	pthread_rwlock_unlock(&store->host_lock);
	return status;
} /* sdb_store_scan */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

