/*
 * SysDB - src/core/memstore.c
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
#include "core/memstore-private.h"
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
 * private types
 */

struct sdb_memstore {
	sdb_object_t super;

	/* hosts are the top-level entries and
	 * reference everything else */
	sdb_avltree_t *hosts;
	pthread_rwlock_t host_lock;
};

/* internal representation of a to-be-stored object */
typedef struct {
	sdb_memstore_obj_t *parent;
	sdb_avltree_t *parent_tree;
	int type;
	const char *name;
	sdb_time_t last_update;
	const char * const *backends;
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
	if (! (SDB_MEMSTORE(obj)->hosts = sdb_avltree_create()))
		return -1;
	if ((err = pthread_rwlock_init(&SDB_MEMSTORE(obj)->host_lock,
					/* attr = */ NULL))) {
		char errbuf[128];
		sdb_log(SDB_LOG_ERR, "memstore: Failed to initialize lock: %s",
				sdb_strerror(err, errbuf, sizeof(errbuf)));
		return -1;
	}
	return 0;
} /* store_init */

static void
store_destroy(sdb_object_t *obj)
{
	int err;
	if ((err = pthread_rwlock_destroy(&SDB_MEMSTORE(obj)->host_lock))) {
		char errbuf[128];
		sdb_log(SDB_LOG_ERR, "memstore: Failed to destroy lock: %s",
				sdb_strerror(err, errbuf, sizeof(errbuf)));
		return;
	}
	sdb_avltree_destroy(SDB_MEMSTORE(obj)->hosts);
	SDB_MEMSTORE(obj)->hosts = NULL;
} /* store_destroy */

static int
store_obj_init(sdb_object_t *obj, va_list ap)
{
	sdb_memstore_obj_t *sobj = STORE_OBJ(obj);

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
	sdb_memstore_obj_t *sobj = STORE_OBJ(obj);
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
	metric_t *sobj = METRIC(obj);
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
	metric_t *sobj = METRIC(obj);
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
	/* size = */ sizeof(sdb_memstore_t),
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
	/* size = */ sizeof(metric_t),
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
record_backends(sdb_memstore_obj_t *obj,
		const char * const *backends, size_t backends_num)
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
store_obj(store_obj_t *obj, sdb_memstore_obj_t **updated_obj)
{
	sdb_memstore_obj_t *old, *new;
	int status = 0;

	assert(obj->parent_tree);

	if (obj->last_update <= 0)
		obj->last_update = sdb_gettime();

	old = STORE_OBJ(sdb_avltree_lookup(obj->parent_tree, obj->name));
	if (old) {
		if (old->last_update > obj->last_update) {
			sdb_log(SDB_LOG_DEBUG, "memstore: Cannot update %s '%s' - "
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
			sdb_log(SDB_LOG_ERR, "memstore: Failed to create %s '%s': %s",
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
store_metric_store(metric_t *metric, sdb_store_metric_t *m)
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
 * store writer API
 */

static int
store_attribute(sdb_store_attribute_t *attr, sdb_object_t *user_data)
{
	sdb_memstore_t *st = SDB_MEMSTORE(user_data);
	store_obj_t obj = STORE_OBJ_INIT;
	sdb_memstore_obj_t *new = NULL;
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
		sdb_log(SDB_LOG_ERR, "memstore: Failed to store attribute '%s' - "
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
			sdb_log(SDB_LOG_ERR, "memstore: Failed to store attribute '%s' - "
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
	sdb_memstore_t *st = SDB_MEMSTORE(user_data);
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
	sdb_memstore_t *st = SDB_MEMSTORE(user_data);
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
		sdb_log(SDB_LOG_ERR, "memstore: Failed to store service '%s' - "
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
	sdb_memstore_t *st = SDB_MEMSTORE(user_data);
	store_obj_t obj = STORE_OBJ_INIT;
	sdb_memstore_obj_t *new = NULL;
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
		sdb_log(SDB_LOG_ERR, "memstore: Failed to store metric '%s' - "
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

sdb_store_writer_t sdb_memstore_writer = {
	store_host, store_service, store_metric, store_attribute,
};

/*
 * store query API
 */

static sdb_object_t *
prepare_query(sdb_ast_node_t *ast,
		sdb_strbuf_t __attribute__((unused)) *errbuf,
		sdb_object_t __attribute__((unused)) *user_data)
{
	return SDB_OBJ(sdb_memstore_query_prepare(ast));
} /* prepare_query */

static int
execute_query(sdb_object_t *q,
		sdb_store_writer_t *w, sdb_object_t *wd, sdb_strbuf_t *errbuf,
		sdb_object_t *user_data)
{
	return sdb_memstore_query_execute(SDB_MEMSTORE(user_data),
			QUERY(q), w, wd, errbuf);
} /* execute_query */

sdb_store_reader_t sdb_memstore_reader = {
	prepare_query, execute_query,
};

/*
 * public API
 */

sdb_memstore_t *
sdb_memstore_create(void)
{
	return SDB_MEMSTORE(sdb_object_create("memstore", store_type));
} /* sdb_memstore_create */

int
sdb_memstore_host(sdb_memstore_t *store, const char *name, sdb_time_t last_update)
{
	sdb_store_host_t host = {
		name, last_update, 0, NULL, 0,
	};
	return store_host(&host, SDB_OBJ(store));
} /* sdb_memstore_host */

int
sdb_memstore_service(sdb_memstore_t *store, const char *hostname, const char *name,
		sdb_time_t last_update)
{
	sdb_store_service_t service = {
		hostname, name, last_update, 0, NULL, 0,
	};
	return store_service(&service, SDB_OBJ(store));
} /* sdb_memstore_service */

int
sdb_memstore_metric(sdb_memstore_t *store, const char *hostname, const char *name,
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
} /* sdb_memstore_metric */

int
sdb_memstore_attribute(sdb_memstore_t *store, const char *hostname,
		const char *key, const sdb_data_t *value, sdb_time_t last_update)
{
	sdb_store_attribute_t attr = {
		NULL, SDB_HOST, hostname, key, SDB_DATA_INIT, last_update, 0, NULL, 0,
	};
	if (value) {
		attr.value = *value;
	}
	return store_attribute(&attr, SDB_OBJ(store));
} /* sdb_memstore_attribute */

int
sdb_memstore_service_attr(sdb_memstore_t *store, const char *hostname,
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
} /* sdb_memstore_service_attr */

int
sdb_memstore_metric_attr(sdb_memstore_t *store, const char *hostname,
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
} /* sdb_memstore_metric_attr */

sdb_memstore_obj_t *
sdb_memstore_get_host(sdb_memstore_t *store, const char *name)
{
	host_t *host;

	if ((! store) || (! name))
		return NULL;

	host = HOST(sdb_avltree_lookup(store->hosts, name));
	if (! host)
		return NULL;

	return STORE_OBJ(host);
} /* sdb_memstore_get_host */

sdb_memstore_obj_t *
sdb_memstore_get_child(sdb_memstore_obj_t *host, int type, const char *name)
{
	sdb_avltree_t *children;

	if ((! host) || (host->type != SDB_HOST) || (! name))
		return NULL;

	children = get_host_children(HOST(host), type);
	if (! children)
		return NULL;
	return STORE_OBJ(sdb_avltree_lookup(children, name));
} /* sdb_memstore_get_child */

int
sdb_memstore_get_field(sdb_memstore_obj_t *obj, int field, sdb_data_t *res)
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
} /* sdb_memstore_get_field */

int
sdb_memstore_get_attr(sdb_memstore_obj_t *obj, const char *name, sdb_data_t *res,
		sdb_memstore_matcher_t *filter)
{
	sdb_avltree_t *tree = NULL;
	sdb_memstore_obj_t *attr;

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
	if (filter && (! sdb_memstore_matcher_matches(filter, attr, NULL))) {
		sdb_object_deref(SDB_OBJ(attr));
		return -1;
	}

	assert(STORE_OBJ(attr)->type == SDB_ATTRIBUTE);
	if (res)
		sdb_data_copy(res, &ATTR(attr)->value);
	sdb_object_deref(SDB_OBJ(attr));
	return 0;
} /* sdb_memstore_get_attr */

int
sdb_memstore_scan(sdb_memstore_t *store, int type,
		sdb_memstore_matcher_t *m, sdb_memstore_matcher_t *filter,
		sdb_memstore_lookup_cb cb, void *user_data)
{
	sdb_avltree_iter_t *host_iter = NULL;
	int status = 0;

	if ((! store) || (! cb))
		return -1;

	if ((type != SDB_HOST) && (type != SDB_SERVICE) && (type != SDB_METRIC)) {
		sdb_log(SDB_LOG_ERR, "memstore: Cannot scan objects of type %d", type);
		return -1;
	}

	pthread_rwlock_rdlock(&store->host_lock);
	host_iter = sdb_avltree_get_iter(store->hosts);
	if (! host_iter)
		status = -1;

	/* has_next returns false if the iterator is NULL */
	while (sdb_avltree_iter_has_next(host_iter)) {
		sdb_memstore_obj_t *host;
		sdb_avltree_iter_t *iter = NULL;

		host = STORE_OBJ(sdb_avltree_iter_get_next(host_iter));
		assert(host);

		if (! sdb_memstore_matcher_matches(filter, host, NULL))
			continue;

		if (type == SDB_SERVICE)
			iter = sdb_avltree_get_iter(HOST(host)->services);
		else if (type == SDB_METRIC)
			iter = sdb_avltree_get_iter(HOST(host)->metrics);

		if (iter) {
			while (sdb_avltree_iter_has_next(iter)) {
				sdb_memstore_obj_t *obj;
				obj = STORE_OBJ(sdb_avltree_iter_get_next(iter));
				assert(obj);

				if (sdb_memstore_matcher_matches(m, obj, filter)) {
					if (cb(obj, filter, user_data)) {
						sdb_log(SDB_LOG_ERR, "memstore: Callback returned "
								"an error while scanning");
						status = -1;
						break;
					}
				}
			}
		}
		else if (sdb_memstore_matcher_matches(m, host, filter)) {
			if (cb(host, filter, user_data)) {
				sdb_log(SDB_LOG_ERR, "memstore: Callback returned "
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
} /* sdb_memstore_scan */

int
sdb_memstore_emit(sdb_memstore_obj_t *obj, sdb_store_writer_t *w, sdb_object_t *wd)
{
	if ((! obj) || (! w))
		return -1;

	switch (obj->type) {
	case SDB_HOST:
		{
			sdb_store_host_t host = {
				obj->_name,
				obj->last_update,
				obj->interval,
				(const char * const *)obj->backends,
				obj->backends_num,
			};
			if (! w->store_host)
				return -1;
			return w->store_host(&host, wd);
		}
	case SDB_SERVICE:
		{
			sdb_store_service_t service = {
				obj->parent ? obj->parent->_name : NULL,
				obj->_name,
				obj->last_update,
				obj->interval,
				(const char * const *)obj->backends,
				obj->backends_num,
			};
			if (! w->store_service)
				return -1;
			return w->store_service(&service, wd);
		}
	case SDB_METRIC:
		{
			sdb_store_metric_t metric = {
				obj->parent ? obj->parent->_name : NULL,
				obj->_name,
				{
					METRIC(obj)->store.type,
					METRIC(obj)->store.id,
				},
				obj->last_update,
				obj->interval,
				(const char * const *)obj->backends,
				obj->backends_num,
			};
			if (! w->store_metric)
				return -1;
			return w->store_metric(&metric, wd);
		}
	case SDB_ATTRIBUTE:
		{
			sdb_store_attribute_t attr = {
				NULL,
				obj->parent ? obj->parent->type : 0,
				obj->parent ? obj->parent->_name : NULL,
				obj->_name,
				ATTR(obj)->value,
				obj->last_update,
				obj->interval,
				(const char * const *)obj->backends,
				obj->backends_num,
			};
			if (obj->parent && (obj->parent->type != SDB_HOST)
					&& obj->parent->parent)
				attr.hostname = obj->parent->parent->_name;
			if (! w->store_attribute)
				return -1;
			return w->store_attribute(&attr, wd);
		}
	}

	return -1;
} /* sdb_memstore_emit */

int
sdb_memstore_emit_full(sdb_memstore_obj_t *obj, sdb_memstore_matcher_t *filter,
		sdb_store_writer_t *w, sdb_object_t *wd)
{
	sdb_avltree_t *trees[] = { NULL, NULL, NULL };
	size_t i;

	if (sdb_memstore_emit(obj, w, wd))
		return -1;

	if (obj->type == SDB_HOST) {
		trees[0] = HOST(obj)->attributes;
		trees[1] = HOST(obj)->metrics;
		trees[2] = HOST(obj)->services;
	}
	else if (obj->type == SDB_SERVICE)
		trees[0] = SVC(obj)->attributes;
	else if (obj->type == SDB_METRIC)
		trees[0] = METRIC(obj)->attributes;
	else if (obj->type == SDB_ATTRIBUTE)
		return 0;
	else
		return -1;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(trees); ++i) {
		sdb_avltree_iter_t *iter;

		if (! trees[i])
			continue;

		iter = sdb_avltree_get_iter(trees[i]);
		while (sdb_avltree_iter_has_next(iter)) {
			sdb_memstore_obj_t *child;
			child = STORE_OBJ(sdb_avltree_iter_get_next(iter));

			if (filter && (! sdb_memstore_matcher_matches(filter, child, NULL)))
				continue;

			if (sdb_memstore_emit_full(child, filter, w, wd)) {
				sdb_avltree_iter_destroy(iter);
				return -1;
			}
		}
		sdb_avltree_iter_destroy(iter);
	}
	return 0;
} /* sdb_memstore_emit_full */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

