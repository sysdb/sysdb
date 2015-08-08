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
 * private variables
 */

struct sdb_store {
	sdb_object_t super;

	/* hosts are the top-level entries and
	 * reference everything else */
	sdb_avltree_t *hosts;
	pthread_rwlock_t host_lock;
};

/*
 * private types
 */

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
} /* host_init */

static void
host_destroy(sdb_object_t *obj)
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
} /* host_destroy */

static int
service_init(sdb_object_t *obj, va_list ap)
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
} /* service_init */

static void
service_destroy(sdb_object_t *obj)
{
	sdb_service_t *sobj = SVC(obj);
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
	/* size = */ sizeof(sdb_host_t),
	/* init = */ host_init,
	/* destroy = */ host_destroy
};

static sdb_type_t service_type = {
	/* size = */ sizeof(sdb_service_t),
	/* init = */ service_init,
	/* destroy = */ service_destroy
};

static sdb_type_t metric_type = {
	/* size = */ sizeof(sdb_metric_t),
	/* init = */ metric_init,
	/* destroy = */ metric_destroy
};

static sdb_type_t attribute_type = {
	/* size = */ sizeof(sdb_attribute_t),
	/* init = */ attr_init,
	/* destroy = */ attr_destroy
};

/*
 * private helper functions
 */

static sdb_host_t *
lookup_host(sdb_store_t *st, const char *name, bool canonicalize)
{
	sdb_host_t *host;
	char *cname;

	assert(name);
	if (! canonicalize)
		return HOST(sdb_avltree_lookup(st->hosts, name));

	cname = strdup(name);
	cname = sdb_plugin_cname(cname);
	if (! cname) {
		sdb_log(SDB_LOG_ERR, "store: strdup failed");
		return NULL;
	}

	host = HOST(sdb_avltree_lookup(st->hosts, cname));
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
			new = STORE_OBJ(sdb_object_create(name, attribute_type,
						type, last_update, NULL));
		}
		else {
			sdb_type_t t;
			t = type == SDB_HOST
				? host_type
				: type == SDB_SERVICE
					? service_type
					: metric_type;
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

static int
store_metric_store(sdb_metric_t *metric, sdb_metric_store_t *store)
{
	char *type = metric->store.type;
	char *id = metric->store.id;

	if ((! metric->store.type) || strcasecmp(metric->store.type, store->type)) {
		if (! (type = strdup(store->type)))
			return -1;
	}
	if ((! metric->store.id) || strcasecmp(metric->store.id, store->id)) {
		if (! (id = strdup(store->id))) {
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
get_host_children(sdb_host_t *host, int type)
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
store_attribute(const char *hostname,
		const char *key, const sdb_data_t *value,
		sdb_time_t last_update, sdb_object_t *user_data)
{
	sdb_store_t *st = SDB_STORE(user_data);

	sdb_host_t *host;
	sdb_avltree_t *attrs;
	int status = 0;

	if ((! hostname) || (! key))
		return -1;

	pthread_rwlock_wrlock(&st->host_lock);
	host = lookup_host(st, hostname, /* canonicalize = */ 1);
	attrs = get_host_children(host, SDB_ATTRIBUTE);
	if (! attrs) {
		sdb_log(SDB_LOG_ERR, "store: Failed to store attribute '%s' - "
				"host '%s' not found", key, hostname);
		status = -1;
	}

	if (! status)
		status = store_attr(STORE_OBJ(host), attrs, key, value, last_update);

	sdb_object_deref(SDB_OBJ(host));
	pthread_rwlock_unlock(&st->host_lock);

	return status;
} /* store_attribute */

static int
store_host(const char *name, sdb_time_t last_update, sdb_object_t *user_data)
{
	sdb_store_t *st = SDB_STORE(user_data);

	char *cname = NULL;
	int status = 0;

	if (! name)
		return -1;

	cname = sdb_plugin_cname(strdup(name));
	if (! cname) {
		sdb_log(SDB_LOG_ERR, "store: strdup failed");
		return -1;
	}

	pthread_rwlock_wrlock(&st->host_lock);
	status = store_obj(NULL, st->hosts,
			SDB_HOST, cname, last_update, NULL);
	pthread_rwlock_unlock(&st->host_lock);

	free(cname);
	return status;
} /* store_host */

static int
store_service_attr(const char *hostname, const char *service,
		const char *key, const sdb_data_t *value, sdb_time_t last_update,
		sdb_object_t *user_data)
{
	sdb_store_t *st = SDB_STORE(user_data);

	sdb_host_t *host;
	sdb_service_t *svc;
	sdb_avltree_t *services;
	int status = 0;

	if ((! hostname) || (! service) || (! key))
		return -1;

	pthread_rwlock_wrlock(&st->host_lock);
	host = lookup_host(st, hostname, /* canonicalize = */ 1);
	services = get_host_children(host, SDB_SERVICE);
	sdb_object_deref(SDB_OBJ(host));
	if (! services) {
		sdb_log(SDB_LOG_ERR, "store: Failed to store attribute '%s' "
				"for service '%s' - host '%ss' not found",
				key, service, hostname);
		pthread_rwlock_unlock(&st->host_lock);
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
	pthread_rwlock_unlock(&st->host_lock);

	return status;
} /* store_service_attr */

static int
store_service(const char *hostname, const char *name,
		sdb_time_t last_update, sdb_object_t *user_data)
{
	sdb_store_t *st = SDB_STORE(user_data);

	sdb_host_t *host;
	sdb_avltree_t *services;
	sdb_data_t d;

	int status = 0;

	if ((! hostname) || (! name))
		return -1;

	pthread_rwlock_wrlock(&st->host_lock);
	host = lookup_host(st, hostname, /* canonicalize = */ 1);
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
	pthread_rwlock_unlock(&st->host_lock);

	if (status)
		return status;

	/* record the hostname as an attribute */
	d.type = SDB_TYPE_STRING;
	d.data.string = SDB_OBJ(host)->name;
	if (store_service_attr(hostname, name, "hostname", &d, last_update, user_data))
		status = -1;
	return status;
} /* store_service */

static int
store_metric_attr(const char *hostname, const char *metric,
		const char *key, const sdb_data_t *value, sdb_time_t last_update,
		sdb_object_t *user_data)
{
	sdb_store_t *st = SDB_STORE(user_data);

	sdb_avltree_t *metrics;
	sdb_host_t *host;
	sdb_metric_t *m;
	int status = 0;

	if ((! hostname) || (! metric) || (! key))
		return -1;

	pthread_rwlock_wrlock(&st->host_lock);
	host = lookup_host(st, hostname, /* canonicalize = */ 1);
	metrics = get_host_children(host, SDB_METRIC);
	sdb_object_deref(SDB_OBJ(host));
	if (! metrics) {
		sdb_log(SDB_LOG_ERR, "store: Failed to store attribute '%s' "
				"for metric '%s' - host '%s' not found",
				key, metric, hostname);
		pthread_rwlock_unlock(&st->host_lock);
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
	pthread_rwlock_unlock(&st->host_lock);

	return status;
} /* store_metric_attr */

static int
store_metric(const char *hostname, const char *name,
		sdb_metric_store_t *store, sdb_time_t last_update,
		sdb_object_t *user_data)
{
	sdb_store_t *st = SDB_STORE(user_data);

	sdb_store_obj_t *obj = NULL;
	sdb_host_t *host;
	sdb_metric_t *metric;
	sdb_data_t d;

	sdb_avltree_t *metrics;

	int status = 0;

	if ((! hostname) || (! name))
		return -1;

	if (store) {
		if ((store->type != NULL) != (store->id != NULL))
			return -1;
		else if (! store->type)
			store = NULL;
	}

	pthread_rwlock_wrlock(&st->host_lock);
	host = lookup_host(st, hostname, /* canonicalize = */ 1);
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

	if (status) {
		pthread_rwlock_unlock(&st->host_lock);
		return status;
	}

	assert(obj);
	metric = METRIC(obj);

	if (store)
		if (store_metric_store(metric, store))
			status = -1;
	pthread_rwlock_unlock(&st->host_lock);

	/* record the hostname as an attribute */
	d.type = SDB_TYPE_STRING;
	d.data.string = SDB_OBJ(host)->name;
	if (store_metric_attr(hostname, name, "hostname", &d, last_update, user_data))
		status = -1;
	return status;
} /* store_metric */

sdb_store_writer_t sdb_store_writer = {
	store_host, store_service, store_metric,
	store_attribute, store_service_attr, store_metric_attr,
};

/*
 * TODO: let prepare and execute accept a store object as their user_data
 * object
 */

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
	return store_host(name, last_update, SDB_OBJ(store));
} /* sdb_store_host */

int
sdb_store_service(sdb_store_t *store, const char *hostname, const char *name,
		sdb_time_t last_update)
{
	return store_service(hostname, name, last_update, SDB_OBJ(store));
} /* sdb_store_service */

int
sdb_store_metric(sdb_store_t *store, const char *hostname, const char *name,
		sdb_metric_store_t *metric_store, sdb_time_t last_update)
{
	return store_metric(hostname, name, metric_store, last_update, SDB_OBJ(store));
} /* sdb_store_metric */

int
sdb_store_attribute(sdb_store_t *store, const char *hostname,
		const char *key, const sdb_data_t *value, sdb_time_t last_update)
{
	return store_attribute(hostname, key, value, last_update, SDB_OBJ(store));
} /* sdb_store_attribute */

int
sdb_store_service_attr(sdb_store_t *store, const char *hostname,
		const char *service, const char *key, const sdb_data_t *value,
		sdb_time_t last_update)
{
	return store_service_attr(hostname, service, key, value,
			last_update, SDB_OBJ(store));
} /* sdb_store_service_attr */

int
sdb_store_metric_attr(sdb_store_t *store, const char *hostname,
		const char *metric, const char *key, const sdb_data_t *value,
		sdb_time_t last_update)
{
	return store_metric_attr(hostname, metric, key, value,
			last_update, SDB_OBJ(store));
} /* sdb_store_metric_attr */

sdb_store_obj_t *
sdb_store_get_host(sdb_store_t *store, const char *name)
{
	sdb_host_t *host;

	if ((! store) || (! name))
		return NULL;

	host = lookup_host(store, name, /* canonicalize = */ 0);
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
	sdb_host_t *host;
	sdb_metric_t *m;

	sdb_timeseries_t *ts;

	int status = 0;

	if ((! store) || (! hostname) || (! metric) || (! opts) || (! buf))
		return -1;

	pthread_rwlock_rdlock(&store->host_lock);
	host = lookup_host(store, hostname, /* canonicalize = */ 1);
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

