/*
 * SysDB - src/plugins/syslog.c
 * Copyright (C) 2013, 2015 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include "liboconfig/utils.h"

#include <assert.h>
#include <strings.h>
#include <syslog.h>

SDB_PLUGIN_MAGIC;

#if (SDB_LOG_EMERG != LOG_EMERG) \
		|| (SDB_LOG_ERR != LOG_ERR) \
		|| (SDB_LOG_WARNING != LOG_WARNING) \
		|| (SDB_LOG_NOTICE != LOG_NOTICE) \
		|| (SDB_LOG_INFO != LOG_INFO) \
		|| (SDB_LOG_DEBUG != LOG_DEBUG)
#	define SDB_LOG_PRIO_TO_SYSLOG(prio) \
		(((prio) == SDB_LOG_EMERG) ? LOG_EMERG \
			: ((prio) == SDB_LOG_ERR) ? LOG_ERR \
			: ((prio) == SDB_LOG_WARNING) ? LOG_WARNING \
			: ((prio) == SDB_LOG_NOTICE) ? LOG_NOTICE \
			: ((prio) == SDB_LOG_INFO) ? LOG_INFO \
			: ((prio) == SDB_LOG_DEBUG) ? LOG_DEBUG : LOG_ERR)
#else
#	define SDB_LOG_PRIO_TO_SYSLOG(prio) (prio)
#endif

static int loglevel = SDB_DEFAULT_LOGLEVEL;

/*
 * plugin API
 */

static int
syslog_log(int prio, const char *msg,
		sdb_object_t __attribute__((unused)) *user_data)
{
	if (prio > loglevel)
		return 0;
	syslog(SDB_LOG_PRIO_TO_SYSLOG(prio), "%s", msg);
	return 0;
} /* syslog_log */

static int
syslog_shutdown(sdb_object_t __attribute__((unused)) *user_data)
{
	closelog();
	return 0;
} /* syslog_shutdown */

static int
syslog_config(oconfig_item_t *ci)
{
	int i;

	if (! ci) {
		/* reset loglevel on deconfigure */
		loglevel = SDB_DEFAULT_LOGLEVEL;
		return 0;
	}

	for (i = 0; i < ci->children_num; ++i) {
		oconfig_item_t *child = ci->children + i;

		if (! strcasecmp(child->key, "LogLevel")) {
			char *level = NULL;
			if (oconfig_get_string(child, &level)) {
				sdb_log(SDB_LOG_ERR, "LogLevel requires a single string argument\n"
						"\tUsage: Loglevel LEVEL");
				return -1;
			}
			loglevel = sdb_error_parse_priority(level);
			if (loglevel < 0) {
				loglevel = SDB_DEFAULT_LOGLEVEL;
				sdb_log(SDB_LOG_ERR, "Invalid loglevel: '%s'", level);
				return -1;
			}
			sdb_log(SDB_LOG_INFO, "Log-level set to %s",
					SDB_LOG_PRIO_TO_STRING(loglevel));
		}
		else
			sdb_log(SDB_LOG_WARNING, "Ignoring unknown config option '%s'.", child->key);
	}
	return 0;
} /* syslog_config */

int
sdb_module_init(sdb_plugin_info_t *info)
{
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_DESC,
			"log messages to the system logger");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_COPYRIGHT,
			"Copyright (C) 2013 Sebastian 'tokkee' Harl <sh@tokkee.org>");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_LICENSE, "BSD");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_VERSION, SDB_VERSION);
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_PLUGIN_VERSION, SDB_VERSION);

	if (info)
		openlog("sysdbd", LOG_NDELAY | LOG_NOWAIT | LOG_PID, LOG_DAEMON);

	sdb_plugin_register_log("main", syslog_log, NULL);
	sdb_plugin_register_config(syslog_config);
	sdb_plugin_register_shutdown("main", syslog_shutdown, NULL);
	return 0;
} /* sdb_module_init */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

