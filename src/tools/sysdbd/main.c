/*
 * SysDB - src/tools/sysdbd/main.c
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
#include "utils/error.h"

#include "frontend/connection.h"
#include "frontend/sock.h"

#include "tools/sysdbd/configfile.h"

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

#include <pthread.h>

#ifndef CONFIGFILE
#	define CONFIGFILE SYSCONFDIR"/sysdb/sysdbd.conf"
#endif

#ifndef DEFAULT_SOCKET
#	define DEFAULT_SOCKET "unix:"LOCALSTATEDIR"/run/sysdbd.sock"
#endif

static sdb_plugin_loop_t plugin_main_loop = SDB_PLUGIN_LOOP_INIT;
static sdb_fe_loop_t frontend_main_loop = SDB_FE_LOOP_INIT;

static char *config_filename = NULL;
static int reconfigure = 0;

static char *default_listen_addresses[] = {
	DEFAULT_SOCKET,
};

static void
sigintterm_handler(int __attribute__((unused)) signo)
{
	frontend_main_loop.do_loop = 0;
} /* sigintterm_handler */

static void
sighup_handler(int __attribute__((unused)) signo)
{
	/* (temporarily) terminate the plugin loop ... */
	frontend_main_loop.do_loop = 0;
	/* ... and tell the main loop to reconfigure the daemon */
	reconfigure = 1;
} /* sighup_handler */

static void
exit_usage(char *name, int status)
{
	printf(
"Usage: %s <options>\n"

"\nOptions:\n"
"  -C FILE   the main configuration file\n"
"            default: "CONFIGFILE"\n"
"  -D        do not run in background (daemonize)\n"
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
			"using libsysdb version %s%s\n"
			"Copyright (C) 2012-2014 "PACKAGE_MAINTAINER"\n"

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
		sdb_log(SDB_LOG_ERR, "Failed to fork to background: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return errno;
	}
	else if (pid != 0) {
		/* parent */
		exit(0);
	}

	if (chdir("/")) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "Failed to change working directory to "
				"the root directory: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return errno;
	}

	/* detach from session */
	setsid();

	close(0);
	if (open("/dev/null", O_RDWR)) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "Failed to connect stdin to '/dev/null': %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return errno;
	}

	close(1);
	if (dup(0) != 1) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "Could not connect stdout to '/dev/null': %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return errno;
	}

	close(2);
	if (dup(0) != 2) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "Could not connect stderr to '/dev/null': %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return errno;
	}
	return 0;
} /* daemonize */

static int
configure(void)
{
	int status;

	if ((status = daemon_parse_config(config_filename))) {
		if (status > 0)
			sdb_log(SDB_LOG_ERR, "Failed to parse configuration file.");
		else
			sdb_log(SDB_LOG_ERR, "Failed to load configuration file.\n"
					"\tCheck other error messages for details.");
		return 1;
	}

	if (! listen_addresses) {
		listen_addresses = default_listen_addresses;
		listen_addresses_num = SDB_STATIC_ARRAY_LEN(default_listen_addresses);
	}
	return 0;
} /* configure */

static int
do_reconfigure(void)
{
	int status;

	sdb_log(SDB_LOG_INFO, "Reconfiguring SysDB daemon");

	if (listen_addresses != default_listen_addresses)
		daemon_free_listen_addresses();
	listen_addresses = NULL;

	sdb_plugin_reconfigure_init();
	if ((status = configure()))
		return status;
	sdb_plugin_init_all();
	sdb_plugin_reconfigure_finish();
	sdb_connection_enable_logging();
	return 0;
} /* do_reconfigure */

static void *
backend_handler(void __attribute__((unused)) *data)
{
	sdb_plugin_collector_loop(&plugin_main_loop);
	sdb_log(SDB_LOG_INFO, "Shutting down backend thread");
	return NULL;
} /* backend_handler */

static int
main_loop(void)
{
	pthread_t backend_thread;

	while (42) {
		size_t i;

		plugin_main_loop.do_loop = 1;
		frontend_main_loop.do_loop = 1;

		memset(&backend_thread, 0, sizeof(backend_thread));
		if (pthread_create(&backend_thread, /* attr = */ NULL,
					backend_handler, /* arg = */ NULL)) {
			char buf[1024];
			sdb_log(SDB_LOG_ERR, "Failed to create backend handler thread: %s",
					sdb_strerror(errno, buf, sizeof(buf)));

			plugin_main_loop.do_loop = 0;
			break;
		}

		sdb_fe_socket_t *sock = sdb_fe_sock_create();
		for (i = 0; i < listen_addresses_num; ++i)
			if (sdb_fe_sock_add_listener(sock, listen_addresses[i]))
				break;

		/* break on error */
		if (i >= listen_addresses_num)
			sdb_fe_sock_listen_and_serve(sock, &frontend_main_loop);

		sdb_log(SDB_LOG_INFO, "Waiting for backend thread to terminate");
		plugin_main_loop.do_loop = 0;
		/* send a signal to interrupt the sleep call
		 * and make the thread shut down faster */
		pthread_kill(backend_thread, SIGINT);
		pthread_join(backend_thread, NULL);
		sdb_fe_sock_destroy(sock);

		if (! reconfigure)
			break;

		reconfigure = 0;
		if (do_reconfigure()) {
			sdb_log(SDB_LOG_ERR, "Reconfiguration failed");
			break;
		}
	}
	return 0;
} /* main_loop */

int
main(int argc, char **argv)
{
	_Bool do_daemonize = 1;

	struct sigaction sa_intterm;
	struct sigaction sa_hup;
	int status;

	sdb_error_set_logger(sdb_plugin_log);

	while (42) {
		int opt = getopt(argc, argv, "C:DhV");

		if (-1 == opt)
			break;

		switch (opt) {
			case 'C':
				config_filename = optarg;
				break;
			case 'D':
				do_daemonize = 0;
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
	if ((status = configure()))
		exit(status);

	memset(&sa_intterm, 0, sizeof(sa_intterm));
	sa_intterm.sa_handler = sigintterm_handler;
	sa_intterm.sa_flags = 0;

	if (sigaction(SIGINT, &sa_intterm, /* old action */ NULL)) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "Failed to install signal handler for "
				"SIGINT: %s", sdb_strerror(errno, errbuf, sizeof(errbuf)));
		exit(1);
	}
	if (sigaction(SIGTERM, &sa_intterm, /* old action */ NULL)) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "Failed to install signal handler for "
				"SIGTERM: %s", sdb_strerror(errno, errbuf, sizeof(errbuf)));
		exit(1);
	}

	if (do_daemonize)
		if (daemonize())
			exit(1);

	sdb_log(SDB_LOG_INFO, "SysDB daemon "SDB_VERSION_STRING
			SDB_VERSION_EXTRA " (pid %i) initialized successfully",
			(int)getpid());

	sdb_plugin_init_all();
	plugin_main_loop.default_interval = SECS_TO_SDB_TIME(60);

	memset(&sa_hup, 0, sizeof(sa_hup));
	sa_hup.sa_handler = sighup_handler;
	sa_hup.sa_flags = 0;

	if (sigaction(SIGHUP, &sa_hup, /* old action */ NULL)) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "Failed to install signal handler for "
				"SIGHUP: %s", sdb_strerror(errno, errbuf, sizeof(errbuf)));
		exit(1);
	}

	/* ignore, we see this, for example, if a client disconnects without
	 * closing the connection cleanly */
	signal(SIGPIPE, SIG_IGN);

	sdb_connection_enable_logging();
	status = main_loop();

	sdb_log(SDB_LOG_INFO, "Shutting down SysDB daemon "SDB_VERSION_STRING
			SDB_VERSION_EXTRA" (pid %i)", (int)getpid());
	return status;
} /* main */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

