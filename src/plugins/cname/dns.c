/*
 * SysDB - src/plugins/cname/dns.c
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

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "sysdb.h"
#include "core/plugin.h"
#include "utils/error.h"

#include <assert.h>
#include <string.h>
#include <syslog.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

/* make it a no-op if necessary */
#ifndef AI_IDN
#	define AI_IDN 0
#endif

SDB_PLUGIN_MAGIC;

/*
 * plugin API
 */

static char *
sdb_dns_cname(const char *name,
		sdb_object_t __attribute__((unused)) *user_data)
{
	struct addrinfo hints;
	struct addrinfo *ret = NULL;
	char *cname;
	int err;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_CANONNAME | AI_IDN;

	err = getaddrinfo(name, /* service = */ NULL, &hints, &ret);
	if (err) {
		/* XXX: what to do about EAI_AGAIN? */
		sdb_plugin_logf(SDB_LOG_ERR, "cname::dns: Failed to resolve '%s': %s",
				name, gai_strerror(err));
		return NULL;
	}

	if ((! ret) || (! ret->ai_canonname))
		return NULL;

	cname = strdup(ret->ai_canonname);
	freeaddrinfo(ret);
	return cname;
} /* sdb_dns_cname */

int
sdb_module_init(sdb_plugin_info_t *info)
{
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_DESC,
			"canonicalize hostnames by querying DNS");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_COPYRIGHT,
			"Copyright (C) 2013 Sebastian 'tokkee' Harl <sh@tokkee.org>");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_LICENSE, "BSD");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_VERSION, SDB_VERSION);
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_PLUGIN_VERSION, SDB_VERSION);

	sdb_plugin_register_cname("main", sdb_dns_cname, NULL);
	return 0;
} /* sdb_module_init */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

