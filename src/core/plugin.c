/*
 * SysDB - src/core/plugin.c
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
#include "core/plugin.h"
#include "utils/error.h"
#include "utils/llist.h"
#include "utils/time.h"

#include <assert.h>

#include <errno.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <ltdl.h>

#include <pthread.h>

/*
 * private data types
 */

struct sdb_plugin_info {
	char *name;

	char *description;
	char *copyright;
	char *license;

	int   version;
	int   plugin_version;
};
#define SDB_PLUGIN_INFO_INIT { "no name set", "no description set", \
	/* copyright */ "", /* license */ "", \
	/* version */ -1, /* plugin_version */ -1 }

typedef struct {
	sdb_object_t super;
	char cb_name[64];
	void *cb_callback;
	sdb_object_t *cb_user_data;
	sdb_plugin_ctx_t cb_ctx;
} sdb_plugin_cb_t;
#define SDB_PLUGIN_CB_INIT { SDB_OBJECT_INIT, /* name = */ "", \
	/* callback = */ NULL, /* user_data = */ NULL, \
	SDB_PLUGIN_CTX_INIT }

typedef struct {
	sdb_plugin_cb_t super;
#define ccb_name super.cb_name
#define ccb_callback super.cb_callback
#define ccb_user_data super.cb_user_data
#define ccb_ctx super.cb_ctx
	sdb_time_t ccb_interval;
	sdb_time_t ccb_next_update;
} sdb_plugin_collector_cb_t;

#define SDB_PLUGIN_CB(obj) ((sdb_plugin_cb_t *)(obj))
#define SDB_PLUGIN_CCB(obj) ((sdb_plugin_collector_cb_t *)(obj))

/*
 * private variables
 */

static sdb_plugin_ctx_t  plugin_default_ctx = SDB_PLUGIN_CTX_INIT;

static pthread_key_t    plugin_ctx_key;
static _Bool            plugin_ctx_key_initialized = 0;

static sdb_llist_t      *config_list = NULL;
static sdb_llist_t      *init_list = NULL;
static sdb_llist_t      *collector_list = NULL;
static sdb_llist_t      *shutdown_list = NULL;

/*
 * private helper functions
 */

static void
sdb_plugin_ctx_destructor(void *ctx)
{
	if (! ctx)
		return;
	free(ctx);
} /* sdb_plugin_ctx_destructor */

static void
sdb_plugin_ctx_init(void)
{
	if (plugin_ctx_key_initialized)
		return;

	pthread_key_create(&plugin_ctx_key, sdb_plugin_ctx_destructor);
	plugin_ctx_key_initialized = 1;
} /* sdb_plugin_ctx_init */

static sdb_plugin_ctx_t *
sdb_plugin_ctx_create(void)
{
	sdb_plugin_ctx_t *ctx;

	ctx = malloc(sizeof(*ctx));
	if (! ctx)
		return NULL;

	*ctx = plugin_default_ctx;

	if (! plugin_ctx_key_initialized)
		sdb_plugin_ctx_init();
	pthread_setspecific(plugin_ctx_key, ctx);
	return ctx;
} /* sdb_plugin_ctx_create */

static int
sdb_plugin_cmp_name(const sdb_object_t *a, const sdb_object_t *b)
{
	const sdb_plugin_cb_t *cb1 = (const sdb_plugin_cb_t *)a;
	const sdb_plugin_cb_t *cb2 = (const sdb_plugin_cb_t *)b;

	assert(cb1 && cb2);
	return strcasecmp(cb1->cb_name, cb2->cb_name);
} /* sdb_plugin_cmp_name */

static int
sdb_plugin_cmp_next_update(const sdb_object_t *a, const sdb_object_t *b)
{
	const sdb_plugin_collector_cb_t *ccb1
		= (const sdb_plugin_collector_cb_t *)a;
	const sdb_plugin_collector_cb_t *ccb2
		= (const sdb_plugin_collector_cb_t *)b;

	assert(ccb1 && ccb2);

	return (ccb1->ccb_next_update > ccb2->ccb_next_update)
		? 1 : (ccb1->ccb_next_update < ccb2->ccb_next_update)
		? -1 : 0;
} /* sdb_plugin_cmp_next_update */

static sdb_plugin_cb_t *
sdb_plugin_find_by_name(sdb_llist_t *list, const char *name)
{
	sdb_plugin_cb_t tmp = SDB_PLUGIN_CB_INIT;

	sdb_object_t *obj;
	assert(name);

	if (! list)
		return NULL;

	snprintf(tmp.cb_name, sizeof(tmp.cb_name), "%s", name);
	tmp.cb_name[sizeof(tmp.cb_name) - 1] = '\0';
	obj = sdb_llist_search(list, SDB_OBJ(&tmp), sdb_plugin_cmp_name);
	if (! obj)
		return NULL;
	return SDB_PLUGIN_CB(obj);
} /* sdb_plugin_find_by_name */

static int
sdb_plugin_cb_init(sdb_object_t *obj, va_list ap)
{
	sdb_llist_t **list = va_arg(ap, sdb_llist_t **);
	const char  *type = va_arg(ap, const char *);
	const char  *name = va_arg(ap, const char *);
	void    *callback = va_arg(ap, void *);
	sdb_object_t   *ud = va_arg(ap, sdb_object_t *);

	assert(list);
	assert(type);
	assert(obj);

	if (sdb_plugin_find_by_name(*list, name)) {
		sdb_log(SDB_LOG_WARNING, "plugin: %s callback '%s' "
				"has already been registered. Ignoring newly "
				"registered version.\n", type, name);
		return -1;
	}

	snprintf(SDB_PLUGIN_CB(obj)->cb_name,
			sizeof(SDB_PLUGIN_CB(obj)->cb_name),
			"%s", name);
	SDB_PLUGIN_CB(obj)->cb_name[sizeof(SDB_PLUGIN_CB(obj)->cb_name) - 1] = '\0';
	SDB_PLUGIN_CB(obj)->cb_callback = callback;
	SDB_PLUGIN_CB(obj)->cb_ctx      = sdb_plugin_get_ctx();

	sdb_object_ref(ud);
	SDB_PLUGIN_CB(obj)->cb_user_data = ud;
	return 0;
} /* sdb_plugin_cb_init */

static void
sdb_plugin_cb_destroy(sdb_object_t *obj)
{
	assert(obj);
	sdb_object_deref(SDB_PLUGIN_CB(obj)->cb_user_data);
} /* sdb_plugin_cb_destroy */

static int
sdb_plugin_add_callback(sdb_llist_t **list, const char *type,
		const char *name, void *callback, sdb_object_t *user_data)
{
	sdb_object_t *obj;

	if ((! name) || (! callback))
		return -1;

	assert(list);

	if (! *list)
		*list = sdb_llist_create();
	if (! *list)
		return -1;

	obj = sdb_object_create(sizeof(sdb_plugin_cb_t), sdb_plugin_cb_init,
			sdb_plugin_cb_destroy, list, type, name, callback, user_data);
	if (! obj)
		return -1;

	if (sdb_llist_append(*list, obj)) {
		sdb_object_deref(obj);
		return -1;
	}

	/* pass control to the list */
	sdb_object_deref(obj);

	sdb_log(SDB_LOG_INFO, "plugin: Registered %s callback '%s'.\n",
			type, name);
	return 0;
} /* sdb_plugin_add_callback */

/*
 * public API
 */

int
sdb_plugin_load(const char *name)
{
	char filename[1024];

	lt_dlhandle lh;

	int (*mod_init)(sdb_plugin_info_t *);
	sdb_plugin_info_t plugin_info = SDB_PLUGIN_INFO_INIT;

	int status;

	snprintf(filename, sizeof(filename), "%s/%s.so",
			PKGLIBDIR, name);
	filename[sizeof(filename) - 1] = '\0';

	if (access(filename, R_OK)) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "plugin: Failed to load plugin '%s': %s\n",
				name, sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return -1;
	}

	lt_dlinit();
	lt_dlerror();

	lh = lt_dlopen(filename);
	if (! lh) {
		sdb_log(SDB_LOG_ERR, "plugin: Failed to load plugin '%s': %s\n"
				"The most common cause for this problem are missing "
				"dependencies.\n", name, lt_dlerror());
		return -1;
	}

	mod_init = (int (*)(sdb_plugin_info_t *))lt_dlsym(lh, "sdb_module_init");
	if (! mod_init) {
		sdb_log(SDB_LOG_ERR, "plugin: Failed to load plugin '%s': "
				"could not find symbol 'sdb_module_init'\n", name);
		return -1;
	}

	status = mod_init(&plugin_info);
	if (status) {
		sdb_log(SDB_LOG_ERR, "plugin: Failed to initialize "
				"plugin '%s'\n", name);
		return -1;
	}

	/* compare minor version */
	if ((plugin_info.version < 0)
			|| ((int)(plugin_info.version / 100) != (int)(SDB_VERSION / 100)))
		sdb_log(SDB_LOG_WARNING, "plugin: WARNING: version of "
				"plugin '%s' (%i.%i.%i) does not match our version "
				"(%i.%i.%i); this might cause problems\n",
				name, SDB_VERSION_DECODE(plugin_info.version),
				SDB_VERSION_DECODE(SDB_VERSION));

	sdb_log(SDB_LOG_INFO, "plugin: Successfully loaded "
			"plugin '%s' v%i (%s)\n\t%s\n",
			plugin_info.name, plugin_info.plugin_version,
			plugin_info.description, plugin_info.copyright);
	return 0;
} /* sdb_plugin_load */

int
sdb_plugin_set_info(sdb_plugin_info_t *info, int type, ...)
{
	va_list ap;

	if (! info)
		return -1;

	va_start(ap, type);

	switch (type) {
		case SDB_PLUGIN_INFO_NAME:
			{
				char *name = va_arg(ap, char *);
				info->name = name;
			}
			break;
		case SDB_PLUGIN_INFO_DESC:
			{
				char *desc = va_arg(ap, char *);
				info->description = desc;
			}
			break;
		case SDB_PLUGIN_INFO_COPYRIGHT:
			{
				char *copyright = va_arg(ap, char *);
				info->copyright = copyright;
			}
			break;
		case SDB_PLUGIN_INFO_LICENSE:
			{
				char *license = va_arg(ap, char *);
				info->license = license;
			}
			break;
		case SDB_PLUGIN_INFO_VERSION:
			{
				int version = va_arg(ap, int);
				info->version = version;
			}
			break;
		case SDB_PLUGIN_INFO_PLUGIN_VERSION:
			{
				int version = va_arg(ap, int);
				info->plugin_version = version;
			}
			break;
		default:
			va_end(ap);
			return -1;
	}

	va_end(ap);
	return 0;
} /* sdb_plugin_set_info */

int
sdb_plugin_register_config(const char *name, sdb_plugin_config_cb callback)
{
	return sdb_plugin_add_callback(&config_list, "init", name,
			callback, NULL);
} /* sdb_plugin_register_config */

int
sdb_plugin_register_init(const char *name, sdb_plugin_init_cb callback,
		sdb_object_t *user_data)
{
	return sdb_plugin_add_callback(&init_list, "init", name,
			callback, user_data);
} /* sdb_plugin_register_init */

int
sdb_plugin_register_shutdown(const char *name, sdb_plugin_shutdown_cb callback,
		sdb_object_t *user_data)
{
	return sdb_plugin_add_callback(&shutdown_list, "shutdown", name,
			callback, user_data);
} /* sdb_plugin_register_shutdown */

int
sdb_plugin_register_collector(const char *name, sdb_plugin_collector_cb callback,
		const sdb_time_t *interval, sdb_object_t *user_data)
{
	sdb_object_t *obj;

	if ((! name) || (! callback))
		return -1;

	if (! collector_list)
		collector_list = sdb_llist_create();
	if (! collector_list)
		return -1;

	obj = sdb_object_create(sizeof(sdb_plugin_collector_cb_t),
			sdb_plugin_cb_init, sdb_plugin_cb_destroy,
			&collector_list, "collector", name, callback, user_data);
	if (! obj)
		return -1;

	if (interval)
		SDB_PLUGIN_CCB(obj)->ccb_interval = *interval;
	else {
		sdb_time_t tmp = sdb_plugin_get_ctx().interval;

		if (tmp > 0)
			SDB_PLUGIN_CCB(obj)->ccb_interval = tmp;
		else
			SDB_PLUGIN_CCB(obj)->ccb_interval = 0;
	}

	if (! (SDB_PLUGIN_CCB(obj)->ccb_next_update = sdb_gettime())) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "plugin: Failed to determine current "
				"time: %s\n", sdb_strerror(errno, errbuf, sizeof(errbuf)));
		sdb_object_deref(obj);
		return -1;
	}

	if (sdb_llist_insert_sorted(collector_list, obj,
				sdb_plugin_cmp_next_update)) {
		sdb_object_deref(obj);
		return -1;
	}

	/* pass control to the list */
	sdb_object_deref(obj);

	sdb_log(SDB_LOG_INFO, "plugin: Registered collector callback '%s' "
			"(interval = %.3fs).\n", name,
			SDB_TIME_TO_DOUBLE(SDB_PLUGIN_CCB(obj)->ccb_interval));
	return 0;
} /* sdb_plugin_register_collector */

sdb_plugin_ctx_t
sdb_plugin_get_ctx(void)
{
	sdb_plugin_ctx_t *ctx;

	if (! plugin_ctx_key_initialized)
		sdb_plugin_ctx_init();
	ctx = pthread_getspecific(plugin_ctx_key);

	if (! ctx)
		ctx = sdb_plugin_ctx_create();
	if (! ctx)
		return plugin_default_ctx;
	return *ctx;
} /* sdb_plugin_get_ctx */

sdb_plugin_ctx_t
sdb_plugin_set_ctx(sdb_plugin_ctx_t ctx)
{
	sdb_plugin_ctx_t *tmp;
	sdb_plugin_ctx_t old;

	if (! plugin_ctx_key_initialized)
		sdb_plugin_ctx_init();
	tmp = pthread_getspecific(plugin_ctx_key);

	if (! tmp)
		tmp = sdb_plugin_ctx_create();
	if (! tmp)
		return plugin_default_ctx;

	old = *tmp;
	*tmp = ctx;
	return old;
} /* sdb_plugin_set_ctx */

int
sdb_plugin_configure(const char *name, oconfig_item_t *ci)
{
	sdb_plugin_cb_t *plugin;
	sdb_plugin_config_cb callback;

	sdb_plugin_ctx_t old_ctx;

	int status;

	if ((! name) || (! ci))
		return -1;

	plugin = sdb_plugin_find_by_name(config_list, name);
	if (! plugin) {
		/* XXX: check if any such plugin has been loaded */
		sdb_log(SDB_LOG_ERR, "plugin: Plugin '%s' did not register "
				"a config callback.\n", name);
		errno = ENOENT;
		return -1;
	}

	old_ctx = sdb_plugin_set_ctx(plugin->cb_ctx);
	callback = plugin->cb_callback;
	status = callback(ci);
	sdb_plugin_set_ctx(old_ctx);
	return status;
} /* sdb_plugin_configure */

int
sdb_plugin_init_all(void)
{
	sdb_llist_iter_t *iter;

	iter = sdb_llist_get_iter(init_list);
	while (sdb_llist_iter_has_next(iter)) {
		sdb_plugin_init_cb callback;
		sdb_plugin_ctx_t old_ctx;

		sdb_object_t *obj = sdb_llist_iter_get_next(iter);
		assert(obj);

		callback = SDB_PLUGIN_CB(obj)->cb_callback;

		old_ctx = sdb_plugin_set_ctx(SDB_PLUGIN_CB(obj)->cb_ctx);
		if (callback(SDB_PLUGIN_CB(obj)->cb_user_data)) {
			/* XXX: unload plugin */
		}
		sdb_plugin_set_ctx(old_ctx);
	}
	return 0;
} /* sdb_plugin_init_all */

int
sdb_plugin_collector_loop(sdb_plugin_loop_t *loop)
{
	if ((! collector_list) || (! loop))
		return -1;

	while (loop->do_loop) {
		sdb_plugin_collector_cb callback;
		sdb_plugin_ctx_t old_ctx;

		sdb_time_t interval, now;

		sdb_object_t *obj = sdb_llist_shift(collector_list);
		if (! obj)
			return -1;

		callback = SDB_PLUGIN_CCB(obj)->ccb_callback;

		if (! (now = sdb_gettime())) {
			char errbuf[1024];
			sdb_log(SDB_LOG_ERR, "plugin: Failed to determine current "
					"time: %s\n", sdb_strerror(errno, errbuf, sizeof(errbuf)));
			now = SDB_PLUGIN_CCB(obj)->ccb_next_update;
		}

		if (now < SDB_PLUGIN_CCB(obj)->ccb_next_update) {
			interval = SDB_PLUGIN_CCB(obj)->ccb_next_update - now;

			errno = 0;
			while (loop->do_loop && sdb_sleep(interval, &interval)) {
				if (errno != EINTR) {
					char errbuf[1024];
					sdb_log(SDB_LOG_ERR, "plugin: Failed to sleep: %s\n",
							sdb_strerror(errno, errbuf, sizeof(errbuf)));
					return -1;
				}
				errno = 0;
			}

			if (! loop->do_loop)
				return 0;
		}

		old_ctx = sdb_plugin_set_ctx(SDB_PLUGIN_CCB(obj)->ccb_ctx);
		if (callback(SDB_PLUGIN_CCB(obj)->ccb_user_data)) {
			/* XXX */
		}
		sdb_plugin_set_ctx(old_ctx);

		interval = SDB_PLUGIN_CCB(obj)->ccb_interval;
		if (! interval)
			interval = loop->default_interval;
		if (! interval) {
			sdb_log(SDB_LOG_WARNING, "plugin: No interval configured "
					"for plugin '%s'; skipping any further "
					"iterations.\n", SDB_PLUGIN_CCB(obj)->ccb_name);
			sdb_object_deref(obj);
			continue;
		}

		SDB_PLUGIN_CCB(obj)->ccb_next_update += interval;

		if (! (now = sdb_gettime())) {
			char errbuf[1024];
			sdb_log(SDB_LOG_ERR, "plugin: Failed to determine current "
					"time: %s\n", sdb_strerror(errno, errbuf, sizeof(errbuf)));
			now = SDB_PLUGIN_CCB(obj)->ccb_next_update;
		}

		if (now > SDB_PLUGIN_CCB(obj)->ccb_next_update) {
			sdb_log(SDB_LOG_WARNING, "plugin: Plugin '%s' took too "
					"long; skipping iterations to keep up.\n",
					SDB_PLUGIN_CCB(obj)->ccb_name);
			SDB_PLUGIN_CCB(obj)->ccb_next_update = now;
		}

		if (sdb_llist_insert_sorted(collector_list, obj,
					sdb_plugin_cmp_next_update)) {
			sdb_log(SDB_LOG_ERR, "plugin: Failed to re-insert "
					"plugin '%s' into collector list. Unable to further "
					"use the plugin.\n",
					SDB_PLUGIN_CCB(obj)->ccb_name);
			sdb_object_deref(obj);
			return -1;
		}

		/* pass control back to the list */
		sdb_object_deref(obj);
	}
	return 0;
} /* sdb_plugin_read_loop */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

