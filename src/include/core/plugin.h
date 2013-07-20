/*
 * SysDB - src/include/core/plugin.h
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

#ifndef SDB_CORE_PLUGIN_H
#define SDB_CORE_PLUGIN_H 1

#include "sysdb.h"
#include "core/object.h"
#include "core/time.h"

#include "liboconfig/oconfig.h"

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	sdb_time_t interval;
} sdb_plugin_ctx_t;
#define SDB_PLUGIN_CTX_INIT { 0 }

struct sdb_plugin_info;
typedef struct sdb_plugin_info sdb_plugin_info_t;

/* this should be used in the header of a plugin to avoid
 * missing prototype warnings/errors for the plugin init
 * function */
#define SDB_PLUGIN_MAGIC \
	int sdb_module_init(sdb_plugin_info_t *info);

typedef struct {
	_Bool do_loop;
	sdb_time_t default_interval;
} sdb_plugin_loop_t;
#define SDB_PLUGIN_LOOP_INIT { 1, 0 }

/*
 * sdb_plugin_load:
 * Load (any type of) plugin by loading the shared object file and calling the
 * sdb_module_init function.
 */
int
sdb_plugin_load(const char *name);

/*
 * sdb_plugin_set_info:
 * Fill in the fields of the sdb_plugin_info_t object passed to the
 * sdb_module_init function. This information is used to identify the plugin
 * and also to provide additional information to the user.
 */
enum {
	SDB_PLUGIN_INFO_NAME,          /* plugin name: string */
	SDB_PLUGIN_INFO_DESC,          /* plugin description: string */
	SDB_PLUGIN_INFO_COPYRIGHT,     /* plugin copyright: string */
	SDB_PLUGIN_INFO_LICENSE,       /* plugin license: string */
	SDB_PLUGIN_INFO_VERSION,       /* libsysdb version: integer */
	SDB_PLUGIN_INFO_PLUGIN_VERSION /* plugin version: integer */
};

int
sdb_plugin_set_info(sdb_plugin_info_t *info, int type, ...);

/*
 * plugin callback functions
 */

typedef int (*sdb_plugin_config_cb)(oconfig_item_t *ci);
typedef int (*sdb_plugin_init_cb)(sdb_object_t *user_data);
typedef int (*sdb_plugin_collector_cb)(sdb_object_t *user_data);
typedef int (*sdb_plugin_shutdown_cb)(sdb_object_t *user_data);
typedef int (*sdb_plugin_log_cb)(int prio, const char *msg,
		sdb_object_t *user_data);

/*
 * sdb_plugin_register_config:
 * Register a "config" function. This will be used to pass on the
 * configuration for a plugin. The plugin has to make sure that the function
 * can be called multiple times in order to process multiple <Plugin> blocks
 * specified in the configuration file(s).
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_plugin_register_config(const char *name, sdb_plugin_config_cb callback);

/*
 * sdb_plugin_register_init:
 * Register an "init" function. All "init" functions will be called after
 * finishing the config parsing and before starting any other work. The
 * functions will be called in the same order as they have been registered,
 * that is, functions of different plugins will be called in the same order as
 * the appropriate "Load" statements in the config file.
 *
 * If the "init" function returns a non-zero value, *all* callbacks of the
 * plugin will be unloaded.
 *
 * Arguments:
 *  - user_data: If specified, this will be passed on to each call of the
 *    callback. The function will take ownership of the object, that is,
 *    increment the reference count by one. In case the caller does not longer
 *    use the object for other purposes, it should thus deref it.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_plugin_register_init(const char *name, sdb_plugin_init_cb callback,
		sdb_object_t *user_data);

/*
 * sdb_plugin_register_collector:
 * Register a "collector" function. This is where a backend is doing its main
 * work. This function will be called whenever an update of a backend has been
 * requested (either by regular interval or by user request). The backend
 * should then query the appropriate data-source and submit all values to the
 * core.
 *
 * Arguments:
 *  - interval: Specifies the regular interval at which to update the backend.
 *    If this is NULL, global settings will be used.
 *  - user_data: If specified, this will be passed on to each call of the
 *    callback. The function will take ownership of the object, that is,
 *    increment the reference count by one. In case the caller does not longer
 *    use the object for other purposes, it should thus deref it.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_plugin_register_collector(const char *name,
		sdb_plugin_collector_cb callback,
		const sdb_time_t *interval, sdb_object_t *user_data);

/*
 * sdb_plugin_register_shutdown:
 * Register a "shutdown" function to be called after stopping all update
 * processes and before shutting down the daemon.
 *
 * Arguments:
 *  - user_data: If specified, this will be passed on to each call of the
 *    callback. The function will take ownership of the object, that is,
 *    increment the reference count by one. In case the caller does not longer
 *    use the object for other purposes, it should thus deref it.
 */
int
sdb_plugin_register_shutdown(const char *name,
		sdb_plugin_shutdown_cb callback,
		sdb_object_t *user_data);

/*
 * sdb_plugin_register_log:
 * Register a "log" function to be called whenever logging is to be done.
 *
 * Arguments:
 *  - user_data: If specified, this will be passed on to each call of the
 *    callback. The function will take ownership of the object, that is,
 *    increment the reference count by one. In case the caller does not longer
 *    use the object for other purposes, it should thus deref it.
 */
int
sdb_plugin_register_log(const char *name, sdb_plugin_log_cb callback,
		sdb_object_t *user_data);

/*
 * sdb_plugin_get_ctx, sdb_plugin_set_ctx:
 * The plugin context defines a set of settings that are available whenever a
 * plugin has been called. It may be used to pass around various information
 * between the different component of the library without having each and
 * every plugin care about it.
 */
sdb_plugin_ctx_t
sdb_plugin_get_ctx(void);
sdb_plugin_ctx_t
sdb_plugin_set_ctx(sdb_plugin_ctx_t ctx);

/*
 * sdb_plugin_configure:
 * Configure the plugin called 'name' (according to the registered config
 * callback) using the config tree 'ci'.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_plugin_configure(const char *name, oconfig_item_t *ci);

/*
 * sdb_plugin_init_all:
 * Initialize all plugins using their registered "init" function.
 */
int
sdb_plugin_init_all(void);

/*
 * sdb_plugin_collector_loop:
 * Loop until loop->do_loop is false, calling the next collector function on
 * each iteration and once its next update interval is passed.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_plugin_collector_loop(sdb_plugin_loop_t *loop);

/*
 * sdb_plugin_log:
 * Log the specified message using all registered log callbacks. The message
 * will be logged with the specified priority.
 */
int
sdb_plugin_log(int prio, const char *msg);

/*
 * sdb_plugin_logf:
 * Log a formatted message. See sdb_plugin_log for more information.
 */
int
sdb_plugin_vlogf(int prio, const char *fmt, va_list ap);
int
sdb_plugin_logf(int prio, const char *fmt, ...);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_CORE_PLUGIN_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

