/*
 * SysDB - src/plugins/backend/store/memory.c
 * Copyright (C) 2015 Sebastian 'tokkee' Harl <sh@tokkee.org>
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
#endif

#include "sysdb.h"
#include "core/plugin.h"
#include "core/store.h"
#include "utils/error.h"

SDB_PLUGIN_MAGIC;

/*
 * plugin API
 */

static int
mem_init(sdb_object_t *user_data)
{
	sdb_memstore_t *store = SDB_MEMSTORE(user_data);

	if (! store) {
		sdb_log(SDB_LOG_ERR, "store: Failed to allocate store");
		return -1;
	}
	if (sdb_plugin_register_writer("memstore",
				&sdb_memstore_writer, SDB_OBJ(store))) {
		sdb_object_deref(SDB_OBJ(store));
		return -1;
	}
	if (sdb_plugin_register_reader("memstore",
				&sdb_memstore_reader, SDB_OBJ(store))) {
		sdb_object_deref(SDB_OBJ(store));
		return -1;
	}
	return 0;
} /* mem_init */

static int
mem_shutdown(sdb_object_t *user_data)
{
	sdb_object_deref(user_data);
	return 0;
} /* mem_shutdown */

int
sdb_module_init(sdb_plugin_info_t *info)
{
	/* store singleton */
	static sdb_memstore_t *store;

	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_DESC, "in-memory object store");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_COPYRIGHT,
			"Copyright (C) 2015 Sebastian 'tokkee' Harl <sh@tokkee.org>");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_LICENSE, "BSD");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_VERSION, SDB_VERSION);
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_PLUGIN_VERSION, SDB_VERSION);

	if (! store) {
		if (! (store = sdb_memstore_create())) {
			sdb_log(SDB_LOG_ERR, "store::memory plugin: "
					"Failed to create store object");
			return -1;
		}
	}

	sdb_plugin_register_init("main", mem_init, SDB_OBJ(store));
	sdb_plugin_register_shutdown("main", mem_shutdown, SDB_OBJ(store));
	return 0;
} /* sdb_module_init */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

