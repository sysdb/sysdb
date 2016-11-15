/*
 * SysDB - src/plugins/backend/facter.cc
 * Copyright (C) 2014 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include <facter/facts/collection.hpp>
#include <iostream>
#include <sstream>

static const char *hostname;
static sdb_time_t now;

static bool
fact_iter(std::string const &k, facter::facts::value const *v)
{
	/* Don't ignore hidden values for now; they also provide the old,
	 * non-structured facts. */

	std::stringstream ss;
	v->write(ss, false);
	std::string s = ss.str();
	char *str = const_cast<char *>(s.c_str());

	/* Ignore non-structured facts for now. */
	if (str[0] == '{')
		return true;

	sdb_data_t value = { SDB_TYPE_STRING, { .string = str } };
	sdb_plugin_store_attribute(hostname, k.c_str(), &value, now);
	return true;
} /* fact_iter */

/* SysDB interface */
extern "C" {

	SDB_PLUGIN_MAGIC;

	static int
	facter_collect(sdb_object_t __attribute__((unused)) *user_data)
	{
		facter::facts::collection facts;

		/* XXX: this may execute other programs; can we be sure that works
		 * reasonably well in a multi-threaded program? */
		facts.add_default_facts();
		facts.add_external_facts();

		now = sdb_gettime();
		facter::facts::value const *v = facts["fqdn"];
		std::stringstream ss;
		v->write(ss, false);
		std::string s = ss.str();
		hostname = s.c_str();

		sdb_plugin_store_host(hostname, now);
		facts.each(fact_iter);
		sdb_log(SDB_LOG_DEBUG, "Processed %zu facts for host '%s'",
				facts.size(), hostname);
		return 0;
	} /* main */

	int
	sdb_module_init(sdb_plugin_info_t *info)
	{
		sdb_plugin_set_info(info, SDB_PLUGIN_INFO_DESC,
				"backend retrieving local facter facts");
		sdb_plugin_set_info(info, SDB_PLUGIN_INFO_COPYRIGHT,
				"Copyright (C) 2014 Sebastian 'tokkee' Harl <sh@tokkee.org>");
		sdb_plugin_set_info(info, SDB_PLUGIN_INFO_LICENSE, "BSD");
		sdb_plugin_set_info(info, SDB_PLUGIN_INFO_VERSION, SDB_VERSION);
		sdb_plugin_set_info(info, SDB_PLUGIN_INFO_PLUGIN_VERSION, SDB_VERSION);

		sdb_plugin_register_collector("main", facter_collect,
				/* interval */ NULL, /* user_data */ NULL);
		return 0;
	} /* sdb_module_init */

} /* extern C */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

