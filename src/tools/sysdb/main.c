/*
 * SysDB - src/tools/sysdb/main.c
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

#include "tools/sysdb/input.h"

#include "client/sysdb.h"
#include "client/sock.h"
#include "utils/error.h"
#include "utils/strbuf.h"

#include <errno.h>

#if HAVE_LIBGEN_H
#	include <libgen.h>
#else /* HAVE_LIBGEN_H */
#	define basename(path) (path)
#endif /* ! HAVE_LIBGEN_H */

#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <sys/types.h>

#include <pwd.h>

#if HAVE_EDITLINE_READLINE_H
#	include <editline/readline.h>
#	if HAVE_EDITLINE_HISTORY_H
#		include <editline/history.h>
#	endif
#elif HAVE_READLINE_READLINE_H
#	include <readline/readline.h>
#	if HAVE_READLINE_HISTORY_H
#		include <readline/history.h>
#	endif
#elif HAVE_READLINE_H
#	include <readline.h>
#	if HAVE_HISTORY_H
#		include <history.h>
#	endif
#endif /* READLINEs */

#ifndef DEFAULT_SOCKET
#	define DEFAULT_SOCKET "unix:"LOCALSTATEDIR"/run/sysdbd.sock"
#endif

static void
exit_usage(char *name, int status)
{
	printf(
"Usage: %s <options>\n"

"\nOptions:\n"
"  -h        display this help and exit\n"
"  -V        display the version number and copyright\n"

"\nSysDB client "SDB_CLIENT_VERSION_STRING SDB_CLIENT_VERSION_EXTRA", "
PACKAGE_URL"\n", basename(name));
	exit(status);
} /* exit_usage */

static void
exit_version(void)
{
	printf("SysDB version "SDB_CLIENT_VERSION_STRING
			SDB_CLIENT_VERSION_EXTRA", built "BUILD_DATE"\n"
			"using libsysdbclient version %s%s\n"
			"Copyright (C) 2012-2014 "PACKAGE_MAINTAINER"\n"

			"\nThis is free software under the terms of the BSD license, see "
			"the source for\ncopying conditions. There is NO WARRANTY; not "
			"even for MERCHANTABILITY or\nFITNESS FOR A PARTICULAR "
			"PURPOSE.\n", sdb_client_version_string(),
			sdb_client_version_extra());
	exit(0);
} /* exit_version */

static const char *
get_current_user(void)
{
	struct passwd pw_entry;
	struct passwd *result = NULL;

	uid_t uid;

	/* needs to be static because we return a pointer into this buffer
	 * to the caller */
	static char buf[1024];

	int status;

	uid = geteuid();

	memset(&pw_entry, 0, sizeof(pw_entry));
	status = getpwuid_r(uid, &pw_entry, buf, sizeof(buf), &result);

	if (status || (! result)) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "Failed to determine current username: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return NULL;
	}
	return result->pw_name;
} /* get_current_user */

static const char *
get_homedir(const char *username)
{
	struct passwd pw_entry;
	struct passwd *result = NULL;

	/* needs to be static because we return a pointer into this buffer
	 * to the caller */
	static char buf[1024];

	int status;

	memset(&pw_entry, 0, sizeof(pw_entry));
	status = getpwnam_r(username, &pw_entry, buf, sizeof(buf), &result);

	if (status || (! result)) {
		char errbuf[1024];
		sdb_log(SDB_LOG_WARNING, "Failed to determine home directory "
				"for user %s: %s", username,
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return NULL;
	}
	return result->pw_dir;
} /* get_homedir */

int
main(int argc, char **argv)
{
	const char *host = NULL;
	const char *user = NULL;

	const char *homedir;
	char hist_file[1024] = "";

	sdb_input_t input = SDB_INPUT_INIT;

	while (42) {
		int opt = getopt(argc, argv, "H:U:hV");

		if (-1 == opt)
			break;

		switch (opt) {
			case 'H':
				host = optarg;
				break;
			case 'U':
				user = optarg;
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

	if (! host)
		host = DEFAULT_SOCKET;
	if (! user) {
		user = get_current_user();
		if (! user)
			exit(1);
	}

	input.client = sdb_client_create(host);
	if (! input.client) {
		sdb_log(SDB_LOG_ERR, "Failed to create client object");
		exit(1);
	}
	if (sdb_client_connect(input.client, user)) {
		sdb_log(SDB_LOG_ERR, "Failed to connect to SysDBd");
		sdb_client_destroy(input.client);
		exit(1);
	}

	sdb_log(SDB_LOG_INFO, "SysDB client "SDB_CLIENT_VERSION_STRING
			SDB_CLIENT_VERSION_EXTRA"\n");

	using_history();

	if ((homedir = get_homedir(user))) {
		snprintf(hist_file, sizeof(hist_file) - 1,
				"%s/.sysdb_history", homedir);
		hist_file[sizeof(hist_file) - 1] = '\0';

		errno = 0;
		if (read_history(hist_file) && (errno != ENOENT)) {
			char errbuf[1024];
			sdb_log(SDB_LOG_WARNING, "Failed to load history (%s): %s",
					hist_file, sdb_strerror(errno, errbuf, sizeof(errbuf)));
		}
	}

	input.input = sdb_strbuf_create(2048);
	sdb_input_init(&input);
	sdb_input_mainloop();

	if (hist_file[0] != '\0') {
		errno = 0;
		if (write_history(hist_file)) {
			char errbuf[1024];
			sdb_log(SDB_LOG_WARNING, "Failed to store history (%s): %s",
					hist_file, sdb_strerror(errno, errbuf, sizeof(errbuf)));
		}
	}

	sdb_client_destroy(input.client);
	sdb_strbuf_destroy(input.input);
	return 0;
} /* main */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

