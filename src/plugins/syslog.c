/*
 * SysDB - src/plugins/syslog.c
 * Copyright (C) 2013 Sebastian 'tokkee' Harl <sh@tokkee.org>
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
#include "core/error.h"

#include <assert.h>
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

/*
 * plugin API
 */

static int
sdb_syslog_init(sdb_object_t __attribute__((unused)) *user_data)
{
	openlog("sysdbd", LOG_NDELAY | LOG_NOWAIT | LOG_PID, LOG_DAEMON);
	return 0;
} /* sdb_syslog_init */

static int
sdb_syslog_log(int prio, const char *msg,
		sdb_object_t __attribute__((unused)) *user_data)
{
	syslog(SDB_LOG_PRIO_TO_SYSLOG(prio), "%s", msg);
	return 0;
} /* sdb_syslog_log */

static int
sdb_syslog_shutdown(sdb_object_t __attribute__((unused)) *user_data)
{
	closelog();
	return 0;
} /* sdb_syslog_shutdown */

int
sdb_module_init(sdb_plugin_info_t *info)
{
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_NAME, "syslog");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_DESC,
			"log messages to the system logger");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_COPYRIGHT,
			"Copyright (C) 2013 Sebastian 'tokkee' Harl <sh@tokkee.org>");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_LICENSE, "BSD");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_VERSION, SDB_VERSION);
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_PLUGIN_VERSION, SDB_VERSION);

	sdb_plugin_register_init("syslog", sdb_syslog_init, NULL);
	sdb_plugin_register_log("syslog", sdb_syslog_log, NULL);
	sdb_plugin_register_shutdown("syslog", sdb_syslog_shutdown, NULL);
	return 0;
} /* sdb_module_init */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

