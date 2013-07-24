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

static sdb_llist_t *host_list = NULL;
static pthread_rwlock_t host_lock = PTHREAD_RWLOCK_INITIALIZER;

/*
 * public types
 */

static int
sdb_host_init(sdb_object_t *obj, va_list __attribute__((unused)) ap)
{
	SDB_HOST(obj)->_last_update = sdb_gettime();
	/* ignore errors -> last_update will be updated later */

	SDB_HOST(obj)->attributes = sdb_llist_create();
	if (! SDB_HOST(obj)->attributes)
		return -1;
	SDB_HOST(obj)->services = sdb_llist_create();
	if (! SDB_HOST(obj)->services)
		return -1;
	return 0;
} /* sdb_host_init */

static void
sdb_host_destroy(sdb_object_t *obj)
{
	assert(obj);

	if (SDB_HOST(obj)->attributes)
		sdb_llist_destroy(SDB_HOST(obj)->attributes);
	if (SDB_HOST(obj)->services)
		sdb_llist_destroy(SDB_HOST(obj)->services);
} /* sdb_host_destroy */

static sdb_object_t *
sdb_host_do_clone(const sdb_object_t *obj)
{
	const sdb_host_t *host = (const sdb_host_t *)obj;
	sdb_host_t *new;

	new = SDB_HOST(sdb_object_create(obj->name, sdb_host_type));
	if (! new)
		return NULL;

	/* make sure these are initialized; else sdb_object_deref() might access
	 * arbitrary memory in case of an error */
	new->services = new->attributes = NULL;

	if (host->attributes) {
		new->attributes = sdb_llist_clone(host->attributes);
		if (! new->attributes) {
			sdb_object_deref(SDB_OBJ(new));
			return NULL;
		}
	}

	new->_last_update = host->_last_update;
	if (host->services) {
		new->services = sdb_llist_clone(host->services);
		if (! new->services) {
			sdb_object_deref(SDB_OBJ(new));
			return NULL;
		}
	}
	return SDB_OBJ(new);
} /* sdb_host_do_clone */

static int
sdb_attr_init(sdb_object_t *obj, va_list ap)
{
	const char *hostname = va_arg(ap, const char *);
	const char *value = va_arg(ap, const char *);

	SDB_ATTR(obj)->hostname = strdup(hostname);
	SDB_ATTR(obj)->attr_value = strdup(value);
	if ((! SDB_ATTR(obj)->hostname) || (! SDB_ATTR(obj)->attr_value))
		return -1;

	SDB_ATTR(obj)->_last_update = sdb_gettime();
	return 0;
} /* sdb_attr_init */

static void
sdb_attr_destroy(sdb_object_t *obj)
{
	assert(obj);

	if (SDB_ATTR(obj)->hostname)
		free(SDB_ATTR(obj)->hostname);
	if (SDB_ATTR(obj)->attr_value)
		free(SDB_ATTR(obj)->attr_value);
} /* sdb_attr_destroy */

static sdb_object_t *
sdb_attr_clone(const sdb_object_t *obj)
{
	const sdb_attribute_t *attr = (const sdb_attribute_t *)obj;
	sdb_attribute_t *new;

	new = SDB_ATTR(sdb_object_create(obj->name, sdb_attribute_type,
				attr->hostname, attr->attr_value));
	if (! new)
		return NULL;

	new->_last_update = attr->_last_update;
	return SDB_OBJ(new);
} /* sdb_attr_clone */

static int
sdb_svc_init(sdb_object_t *obj, va_list ap)
{
	const char *hostname = va_arg(ap, const char *);

	SDB_SVC(obj)->hostname = strdup(hostname);
	if (! SDB_SVC(obj)->hostname)
		return -1;

	SDB_SVC(obj)->_last_update = sdb_gettime();
	/* ignore errors -> last_update will be updated later */
	return 0;
} /* sdb_svc_init */

static void
sdb_svc_destroy(sdb_object_t *obj)
{
	assert(obj);

	if (SDB_SVC(obj)->hostname)
		free(SDB_SVC(obj)->hostname);
} /* sdb_svc_destroy */

static sdb_object_t *
sdb_svc_clone(const sdb_object_t *obj)
{
	const sdb_service_t *svc = (const sdb_service_t *)obj;
	sdb_service_t *new;

	new = sdb_service_create(svc->hostname, obj->name);
	if (! new)
		return NULL;

	new->_last_update = svc->_last_update;
	return SDB_OBJ(new);
} /* sdb_svc_clone */

const sdb_type_t sdb_host_type = {
	sizeof(sdb_host_t),

	sdb_host_init,
	sdb_host_destroy,
	sdb_host_do_clone
};

const sdb_type_t sdb_attribute_type = {
	sizeof(sdb_attribute_t),

	sdb_attr_init,
	sdb_attr_destroy,
	sdb_attr_clone
};

const sdb_type_t sdb_service_type = {
	sizeof(sdb_service_t),

	sdb_svc_init,
	sdb_svc_destroy,
	sdb_svc_clone
};

/*
 * public API
 */

int
sdb_store_host(const char *name, sdb_time_t last_update)
{
	sdb_host_t *old;

	char *cname;
	int status = 0;

	if (! name)
		return -1;

	cname = sdb_plugin_cname(strdup(name));
	if (! cname) {
		sdb_log(SDB_LOG_ERR, "store: strdup failed");
		return -1;
	}

	last_update = last_update;
	if (last_update <= 0)
		last_update = sdb_gettime();

	pthread_rwlock_wrlock(&host_lock);

	if (! host_list) {
		if (! (host_list = sdb_llist_create())) {
			pthread_rwlock_unlock(&host_lock);
			return -1;
		}
	}

	old = SDB_HOST(sdb_llist_search_by_name(host_list, cname));

	if (old) {
		if (old->_last_update > last_update) {
			sdb_log(SDB_LOG_DEBUG, "store: Cannot update host '%s' - "
					"value too old (%"PRIscTIME" < %"PRIscTIME")",
					cname, last_update, old->_last_update);
			/* don't report an error; the host may be updated by multiple
			 * backends */
			status = 1;
		}
		else {
			old->_last_update = last_update;
		}
	}
	else {
		sdb_host_t *new = SDB_HOST(sdb_object_create(name, sdb_host_type));
		if (! new) {
			char errbuf[1024];
			sdb_log(SDB_LOG_ERR, "store: Failed to clone host object: %s",
					sdb_strerror(errno, errbuf, sizeof(errbuf)));
			pthread_rwlock_unlock(&host_lock);
			return -1;
		}

		free(SDB_OBJ(new)->name);
		SDB_OBJ(new)->name = cname;

		if (! new->attributes) {
			if (! (new->attributes = sdb_llist_create())) {
				char errbuf[1024];
				sdb_log(SDB_LOG_ERR, "store: Failed to initialize "
						"host object '%s': %s", SDB_OBJ(new)->name,
						sdb_strerror(errno, errbuf, sizeof(errbuf)));
				sdb_object_deref(SDB_OBJ(new));
				pthread_rwlock_unlock(&host_lock);
				return -1;
			}
		}

		if (! new->services) {
			if (! (new->services = sdb_llist_create())) {
				char errbuf[1024];
				sdb_log(SDB_LOG_ERR, "store: Failed to initialize "
						"host object '%s': %s", SDB_OBJ(new)->name,
						sdb_strerror(errno, errbuf, sizeof(errbuf)));
				sdb_object_deref(SDB_OBJ(new));
				pthread_rwlock_unlock(&host_lock);
				return -1;
			}
		}

		status = sdb_llist_insert_sorted(host_list, SDB_OBJ(new),
				sdb_object_cmp_by_name);

		/* pass control to the list or destroy in case of an error */
		sdb_object_deref(SDB_OBJ(new));
	}

	pthread_rwlock_unlock(&host_lock);
	return status;
} /* sdb_store_host */

_Bool
sdb_store_has_host(const char *name)
{
	sdb_host_t *host;

	if (! name)
		return NULL;

	host = SDB_HOST(sdb_llist_search_by_name(host_list, name));
	return host != NULL;
} /* sdb_store_has_host */

int
sdb_store_attribute(const sdb_attribute_t *attr)
{
	sdb_host_t *host;
	sdb_attribute_t *old;
	sdb_time_t last_update;

	int status = 0;

	if (! attr)
		return -1;

	last_update = attr->_last_update;
	if (last_update <= 0)
		last_update = sdb_gettime();

	if (! host_list)
		return -1;

	pthread_rwlock_wrlock(&host_lock);

	host = SDB_HOST(sdb_llist_search_by_name(host_list, attr->hostname));
	if (! host) {
		pthread_rwlock_unlock(&host_lock);
		return -1;
	}

	old = SDB_ATTR(sdb_llist_search_by_name(host->attributes,
				SDB_CONST_OBJ(attr)->name));
	if (old) {
		if (old->_last_update > last_update) {
			sdb_log(SDB_LOG_DEBUG, "store: Cannot update attribute "
					"'%s/%s' - value too old (%"PRIscTIME" < %"PRIscTIME")",
					attr->hostname, SDB_CONST_OBJ(attr)->name, last_update,
					old->_last_update);
			status = 1;
		}
		else {
			old->_last_update = last_update;
		}
	}
	else {
		sdb_attribute_t *new = SDB_ATTR(sdb_object_clone(SDB_CONST_OBJ(attr)));
		if (! new) {
			char errbuf[1024];
			sdb_log(SDB_LOG_ERR, "store: Failed to clone attribute "
					"object: %s", sdb_strerror(errno, errbuf, sizeof(errbuf)));
			pthread_rwlock_unlock(&host_lock);
			return -1;
		}

		status = sdb_llist_insert_sorted(host->attributes, SDB_OBJ(new),
				sdb_object_cmp_by_name);

		/* pass control to the list or destroy in case of an error */
		sdb_object_deref(SDB_OBJ(new));
	}

	pthread_rwlock_unlock(&host_lock);
	return status;
} /* sdb_store_attribute */

sdb_service_t *
sdb_service_create(const char *hostname, const char *name)
{
	sdb_object_t *obj;

	if ((! hostname) || (! name))
		return NULL;

	obj = sdb_object_create(name, sdb_service_type, hostname);
	if (! obj)
		return NULL;
	return SDB_SVC(obj);
} /* sdb_service_create */

int
sdb_store_service(const sdb_service_t *svc)
{
	sdb_host_t *host;
	sdb_service_t *old;
	sdb_time_t last_update;

	int status = 0;

	if (! svc)
		return -1;

	last_update = svc->_last_update;
	if (last_update <= 0)
		last_update = sdb_gettime();

	if (! host_list)
		return -1;

	pthread_rwlock_wrlock(&host_lock);

	host = SDB_HOST(sdb_llist_search_by_name(host_list, svc->hostname));
	if (! host) {
		pthread_rwlock_unlock(&host_lock);
		return -1;
	}

	old = SDB_SVC(sdb_llist_search_by_name(host->services,
				SDB_CONST_OBJ(svc)->name));
	if (old) {
		if (old->_last_update > last_update) {
			sdb_log(SDB_LOG_DEBUG, "store: Cannot update service "
					"'%s/%s' - value too old (%"PRIscTIME" < %"PRIscTIME")",
					svc->hostname, SDB_CONST_OBJ(svc)->name, last_update,
					old->_last_update);
			status = 1;
		}
		else {
			old->_last_update = last_update;
		}
	}
	else {
		sdb_service_t *new = SDB_SVC(sdb_object_clone(SDB_CONST_OBJ(svc)));
		if (! new) {
			char errbuf[1024];
			sdb_log(SDB_LOG_ERR, "store: Failed to clone service "
					"object: %s", sdb_strerror(errno, errbuf, sizeof(errbuf)));
			pthread_rwlock_unlock(&host_lock);
			return -1;
		}

		status = sdb_llist_insert_sorted(host->services, SDB_OBJ(new),
				sdb_object_cmp_by_name);

		/* pass control to the list or destroy in case of an error */
		sdb_object_deref(SDB_OBJ(new));
	}

	pthread_rwlock_unlock(&host_lock);
	return status;
} /* sdb_store_service */

const sdb_service_t *
sdb_store_get_service(const sdb_host_t *host, const char *name)
{
	sdb_service_t *svc;

	if ((! host) || (! name))
		return NULL;

	svc = SDB_SVC(sdb_llist_search_by_name(host->services, name));
	if (! svc)
		return NULL;
	return svc;
} /* sdb_store_get_service */

int
sdb_store_dump(FILE *fh)
{
	sdb_llist_iter_t *host_iter;

	if (! fh)
		return -1;

	pthread_rwlock_rdlock(&host_lock);

	host_iter = sdb_llist_get_iter(host_list);
	if (! host_iter) {
		pthread_rwlock_unlock(&host_lock);
		return -1;
	}

	while (sdb_llist_iter_has_next(host_iter)) {
		sdb_host_t *host = SDB_HOST(sdb_llist_iter_get_next(host_iter));
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
					SDB_OBJ(attr)->name, attr->attr_value, time_str);
		}

		sdb_llist_iter_destroy(attr_iter);

		svc_iter = sdb_llist_get_iter(host->services);
		if (! svc_iter) {
			char errbuf[1024];
			fprintf(fh, "Failed to retrieve services: %s\n",
					sdb_strerror(errno, errbuf, sizeof(errbuf)));
			continue;
		}

		while (sdb_llist_iter_has_next(svc_iter)) {
			sdb_service_t *svc = SDB_SVC(sdb_llist_iter_get_next(svc_iter));
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
	pthread_rwlock_unlock(&host_lock);
	return 0;
} /* sdb_store_dump */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

