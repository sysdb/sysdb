/*
 * syscollector - src/core/store.c
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

#include "syscollector.h"
#include "core/store.h"
#include "utils/llist.h"
#include "utils/string.h"

#include <assert.h>

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

/*
 * private data types
 */

/* type used for looking up named objects */
typedef struct {
	sc_object_t parent;
	const char *obj_name;
} sc_store_lookup_obj_t;
#define SC_STORE_LOOKUP_OBJ_INIT { SC_OBJECT_INIT, NULL }

/*
 * private variables
 */

static sc_llist_t *host_list = NULL;
static pthread_rwlock_t host_lock = PTHREAD_RWLOCK_INITIALIZER;

/*
 * private helper functions
 */

static int
sc_store_obj_cmp_by_name(const sc_object_t *a, const sc_object_t *b)
{
	const sc_store_obj_t *h1 = (const sc_store_obj_t *)a;
	const sc_store_obj_t *h2 = (const sc_store_obj_t *)b;

	assert(h1 && h2);
	return strcasecmp(h1->name, h2->name);
} /* sc_store_obj_cmp_by_name */

static int
sc_cmp_store_obj_with_name(const sc_object_t *a, const sc_object_t *b)
{
	const sc_store_obj_t *obj = (const sc_store_obj_t *)a;
	const sc_store_lookup_obj_t *lookup = (const sc_store_lookup_obj_t *)b;

	assert(obj && lookup);
	return strcasecmp(obj->name, lookup->obj_name);
} /* sc_cmp_store_obj_with_name */

static int
sc_host_init(sc_object_t *obj, va_list ap)
{
	const char *name = va_arg(ap, const char *);

	SC_HOST(obj)->host_name = strdup(name);
	if (! SC_HOST(obj)->host_name)
		return -1;

	SC_HOST(obj)->host_last_update = sc_gettime();
	/* ignore errors -> last_update will be updated later */

	SC_HOST(obj)->services = sc_llist_create();
	if (! SC_HOST(obj)->services)
		return -1;
	return 0;
} /* sc_host_init */

static void
sc_host_destroy(sc_object_t *obj)
{
	assert(obj);

	if (SC_HOST(obj)->host_name)
		free(SC_HOST(obj)->host_name);

	if (SC_HOST(obj)->services)
		sc_llist_destroy(SC_HOST(obj)->services);
} /* sc_host_destroy */

static int
sc_svc_init(sc_object_t *obj, va_list ap)
{
	const char *hostname = va_arg(ap, const char *);
	const char *name = va_arg(ap, const char *);

	SC_SVC(obj)->hostname = strdup(hostname);
	SC_SVC(obj)->svc_name = strdup(name);
	if ((! SC_SVC(obj)->hostname) || (! SC_SVC(obj)->svc_name))
		return -1;

	SC_SVC(obj)->svc_last_update = sc_gettime();
	/* ignore errors -> last_update will be updated later */
	return 0;
} /* sc_svc_init */

static void
sc_svc_destroy(sc_object_t *obj)
{
	assert(obj);

	if (SC_SVC(obj)->hostname)
		free(SC_SVC(obj)->hostname);
	if (SC_SVC(obj)->svc_name)
		free(SC_SVC(obj)->svc_name);
} /* sc_svc_destroy */

/*
 * public API
 */

sc_host_t *
sc_host_create(const char *name)
{
	sc_object_t *obj;

	if (! name)
		return NULL;

	obj = sc_object_create(sizeof(sc_host_t), sc_host_init,
			sc_host_destroy, name);
	if (! obj)
		return NULL;
	return SC_HOST(obj);
} /* sc_host_create */

sc_host_t *
sc_host_clone(const sc_host_t *host)
{
	sc_host_t *clone;

	clone = sc_host_create(host->host_name);
	if (! clone)
		return NULL;

	clone->host_last_update = host->host_last_update;
	if (host->services) {
		clone->services = sc_llist_clone(host->services);
		if (! clone->services) {
			sc_object_deref(SC_OBJ(clone));
			return NULL;
		}
	}
	else
		clone->services = NULL;
	return clone;
} /* sc_host_clone */

int
sc_store_host(const sc_host_t *host)
{
	sc_store_lookup_obj_t lookup = SC_STORE_LOOKUP_OBJ_INIT;
	sc_time_t last_update;

	sc_host_t *old;
	int status = 0;

	if ((! host) || (! host->host_name))
		return -1;

	last_update = host->host_last_update;
	if (last_update <= 0)
		last_update = sc_gettime();

	pthread_rwlock_wrlock(&host_lock);

	if (! host_list) {
		if (! (host_list = sc_llist_create())) {
			pthread_rwlock_unlock(&host_lock);
			return -1;
		}
	}

	lookup.obj_name = host->host_name;
	old = SC_HOST(sc_llist_search(host_list, (const sc_object_t *)&lookup,
				sc_cmp_store_obj_with_name));

	if (old) {
		if (old->host_last_update > last_update) {
			fprintf(stderr, "store: Cannot update host '%s' - "
					"value too old (%"PRIscTIME" < %"PRIscTIME")\n",
					host->host_name, last_update, old->host_last_update);
			/* don't report an error; the host may be updated by multiple
			 * backends */
			status = 1;
		}
		else {
			old->host_last_update = last_update;
		}
	}
	else {
		sc_host_t *new = sc_host_clone(host);
		if (! new) {
			char errbuf[1024];
			fprintf(stderr, "store: Failed to clone host object: %s\n",
					sc_strerror(errno, errbuf, sizeof(errbuf)));
			return -1;
		}

		if (! new->services) {
			if (! (new->services = sc_llist_create())) {
				char errbuf[1024];
				fprintf(stderr, "store: Failed to initialize "
						"host object '%s': %s\n", host->host_name,
						sc_strerror(errno, errbuf, sizeof(errbuf)));
				sc_object_deref(SC_OBJ(new));
				return -1;
			}
		}

		status = sc_llist_insert_sorted(host_list, SC_OBJ(new),
				sc_store_obj_cmp_by_name);

		/* pass control to the list or destroy in case of an error */
		sc_object_deref(SC_OBJ(new));
	}

	pthread_rwlock_unlock(&host_lock);
	return status;
} /* sc_store_host */

const sc_host_t *
sc_store_get_host(const char *name)
{
	sc_store_lookup_obj_t lookup = SC_STORE_LOOKUP_OBJ_INIT;
	sc_host_t *host;

	if (! name)
		return NULL;

	lookup.obj_name = name;
	host = SC_HOST(sc_llist_search(host_list, (const sc_object_t *)&lookup,
				sc_cmp_store_obj_with_name));

	if (! host)
		return NULL;
	return host;
} /* sc_store_get_host */

sc_service_t *
sc_service_create(const char *hostname, const char *name)
{
	sc_object_t *obj;

	if ((! hostname) || (! name))
		return NULL;

	obj = sc_object_create(sizeof(sc_service_t), sc_svc_init,
			sc_svc_destroy, hostname, name);
	if (! obj)
		return NULL;
	return SC_SVC(obj);
} /* sc_service_create */

sc_service_t *
sc_service_clone(const sc_service_t *svc)
{
	sc_service_t *clone;

	clone = sc_service_create(svc->hostname, svc->svc_name);
	if (! clone)
		return NULL;

	clone->svc_last_update = svc->svc_last_update;
	return clone;
} /* sc_service_clone */

int
sc_store_service(const sc_service_t *svc)
{
	sc_store_lookup_obj_t lookup = SC_STORE_LOOKUP_OBJ_INIT;
	sc_host_t *host;

	sc_service_t *old;

	sc_time_t last_update;

	int status = 0;

	if (! svc)
		return -1;

	last_update = svc->svc_last_update;
	if (last_update <= 0)
		last_update = sc_gettime();

	if (! host_list)
		return -1;

	pthread_rwlock_wrlock(&host_lock);

	lookup.obj_name = svc->hostname;
	host = SC_HOST(sc_llist_search(host_list, (const sc_object_t *)&lookup,
				sc_cmp_store_obj_with_name));

	if (! host)
		return -1;

	lookup.obj_name = svc->svc_name;
	old = SC_SVC(sc_llist_search(host->services, (const sc_object_t *)&lookup,
				sc_cmp_store_obj_with_name));

	if (old) {
		if (old->host_last_update > last_update) {
			fprintf(stderr, "store: Cannot update service '%s/%s' - "
					"value too old (%"PRIscTIME" < %"PRIscTIME")\n",
					svc->hostname, svc->svc_name, last_update,
					old->host_last_update);
			status = 1;
		}
		else {
			old->svc_last_update = last_update;
		}
	}
	else {
		sc_service_t *new = sc_service_clone(svc);
		if (! new) {
			char errbuf[1024];
			fprintf(stderr, "store: Failed to clone service object: %s\n",
					sc_strerror(errno, errbuf, sizeof(errbuf)));
			return -1;
		}

		status = sc_llist_insert_sorted(host->services, SC_OBJ(new),
				sc_store_obj_cmp_by_name);

		/* pass control to the list or destroy in case of an error */
		sc_object_deref(SC_OBJ(new));
	}

	pthread_rwlock_unlock(&host_lock);
	return status;
} /* sc_store_service */

const sc_service_t *
sc_store_get_service(const sc_host_t *host, const char *name)
{
	sc_store_lookup_obj_t lookup = SC_STORE_LOOKUP_OBJ_INIT;
	sc_service_t *svc;

	if ((! host) || (! name))
		return NULL;

	lookup.obj_name = name;
	svc = SC_SVC(sc_llist_search(host->services, (const sc_object_t *)&lookup,
				sc_cmp_store_obj_with_name));

	if (! svc)
		return NULL;
	return svc;
} /* sc_store_get_service */

int
sc_store_dump(FILE *fh)
{
	sc_llist_iter_t *host_iter;

	if (! fh)
		return -1;

	pthread_rwlock_rdlock(&host_lock);

	host_iter = sc_llist_get_iter(host_list);
	if (! host_iter)
		return -1;

	while (sc_llist_iter_has_next(host_iter)) {
		sc_host_t *host = SC_HOST(sc_llist_iter_get_next(host_iter));
		sc_llist_iter_t *svc_iter;

		char time_str[64];

		assert(host);

		if (! sc_strftime(time_str, sizeof(time_str),
					"%F %T %z", host->host_last_update))
			snprintf(time_str, sizeof(time_str), "<error>");
		time_str[sizeof(time_str) - 1] = '\0';

		fprintf(fh, "Host '%s' (last updated: %s):\n",
				host->host_name, time_str);

		svc_iter = sc_llist_get_iter(host->services);
		if (! svc_iter) {
			char errbuf[1024];
			fprintf(fh, "Failed to retrieve services: %s\n",
					sc_strerror(errno, errbuf, sizeof(errbuf)));
			continue;
		}

		while (sc_llist_iter_has_next(svc_iter)) {
			sc_service_t *svc = SC_SVC(sc_llist_iter_get_next(svc_iter));
			assert(svc);

			if (! sc_strftime(time_str, sizeof(time_str),
						"%F %T %z", svc->svc_last_update))
				snprintf(time_str, sizeof(time_str), "<error>");
			time_str[sizeof(time_str) - 1] = '\0';

			fprintf(fh, "\tService '%s' (last updated: %s)\n",
					svc->svc_name, time_str);
		}

		sc_llist_iter_destroy(svc_iter);
	}

	sc_llist_iter_destroy(host_iter);
	return 0;
} /* sc_store_dump */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

