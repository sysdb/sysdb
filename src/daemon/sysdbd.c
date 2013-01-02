/*
 * SysDB - src/sysdbd.c
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

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "sysdb.h"
#include "core/plugin.h"
#include "core/store.h"
#include "utils/string.h"

#include "daemon/config.h"

#if HAVE_LIBGEN_H
#	include <libgen.h>
#else /* HAVE_LIBGEN_H */
#	define basename(path) (path)
#endif /* ! HAVE_LIBGEN_H */

#include <errno.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <signal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#ifndef CONFIGFILE
#	define CONFIGFILE SYSCONFDIR"/sysdb/sysdbd.conf"
#endif

static sdb_plugin_loop_t plugin_main_loop = SDB_PLUGIN_LOOP_INIT;

static void
sigintterm_handler(int __attribute__((unused)) signo)
{
	plugin_main_loop.do_loop = 0;
} /* sigintterm_handler */

static void
exit_usage(char *name, int status)
{
	printf(
"Usage: %s <options>\n"

"\nOptions:\n"
"  -C FILE   the main configuration file\n"
"            default: "CONFIGFILE"\n"
"  -d        run in background (daemonize)\n"
"\n"
"  -h        display this help and exit\n"
"  -V        display the version number and copyright\n"

"\nSysDB daemon "SDB_VERSION_STRING SDB_VERSION_EXTRA", "PACKAGE_URL"\n",
basename(name));
	exit(status);
} /* exit_usage */

static void
exit_version(void)
{
	printf("SysDBd version "SDB_VERSION_STRING SDB_VERSION_EXTRA", "
			"built "BUILD_DATE"\n"
			"using libsysdb verion %s%s\n"
			"Copyright (C) 2012 "PACKAGE_MAINTAINER"\n"

			"\nThis is free software under the terms of the BSD license, see "
			"the source for\ncopying conditions. There is NO WARRANTY; not "
			"even for MERCHANTABILITY or\nFITNESS FOR A PARTICULAR "
			"PURPOSE.\n", sdb_version_string(), sdb_version_extra());
	exit(0);
} /* exit_version */

static int
daemonize(void)
{
	pid_t pid;

	if ((pid = fork()) < 0) {
		char errbuf[1024];
		fprintf(stderr, "Failed to fork to background: %s\n",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return errno;
	}
	else if (pid != 0) {
		/* parent */
		exit(0);
	}

	if (chdir("/")) {
		char errbuf[1024];
		fprintf(stderr, "Failed to change working directory to /: %s\n",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return errno;
	}

	/* detach from session */
	setsid();

	close(0);
	if (open("/dev/null", O_RDWR)) {
		char errbuf[1024];
		fprintf(stderr, "Failed to connect stdin to '/dev/null': %s\n",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return errno;
	}

	close(1);
	if (dup(0) != 1) {
		char errbuf[1024];
		fprintf(stderr, "Could not connect stdout to '/dev/null': %s\n",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return errno;
	}

	close(2);
	if (dup(0) != 2) {
		char errbuf[1024];
		fprintf(stdout, "Could not connect stderr to '/dev/null': %s\n",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return errno;
	}
	return 0;
} /* daemonize */

int
main(int argc, char **argv)
{
	char *config_filename = NULL;
	_Bool daemon = 0;

	struct sigaction sa_intterm;

	while (42) {
		int opt = getopt(argc, argv, "C:dhV");

		if (-1 == opt)
			break;

		switch (opt) {
			case 'C':
				config_filename = optarg;
				break;
			case 'd':
				daemon = 1;
				break;

			case 'h':
				exit_usage(argv[0], 0);
				break;
			case 'V':
				exit_version();
				break;
			default:
				exit_usage(argv[0], 1);
		}
	}

	if (optind < argc)
		exit_usage(argv[0], 1);

	if (! config_filename)
		config_filename = CONFIGFILE;

	if (daemon_parse_config(config_filename)) {
		fprintf(stderr, "Failed to parse configuration file.\n");
		exit(1);
	}

	memset(&sa_intterm, 0, sizeof(sa_intterm));
	sa_intterm.sa_handler = sigintterm_handler;
	sa_intterm.sa_flags = 0;

	if (sigaction(SIGINT, &sa_intterm, /* old action */ NULL)) {
		char errbuf[1024];
		fprintf(stderr, "Failed to install signal handler for SIGINT: %s\n",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		exit(1);
	}
	if (sigaction(SIGTERM, &sa_intterm, /* old action */ NULL)) {
		char errbuf[1024];
		fprintf(stderr, "Failed to install signal handler for SIGTERM: %s\n",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		exit(1);
	}

	if (daemon)
		if (daemonize())
			exit(1);

	fprintf(stderr, "SysDB daemon "SDB_VERSION_STRING SDB_VERSION_EXTRA
			" (pid %i) initialized successfully\n", (int)getpid());

	sdb_plugin_init_all();
	sdb_plugin_collector_loop(&plugin_main_loop);

	fprintf(stderr, "Shutting down SysDB daemon "SDB_VERSION_STRING
			SDB_VERSION_EXTRA" (pid %i)\n", (int)getpid());

	fprintf(stderr, "Store dump:\n");
	sdb_store_dump(stderr);
	return 0;
} /* main */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

