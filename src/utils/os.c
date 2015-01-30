/*
 * SysDB - src/utils/os.c
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
#endif /* HAVE_CONFIG_H */

#include "utils/os.h"
#include "utils/error.h"

#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <dirent.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <libgen.h>
#include <netdb.h>
#include <pwd.h>

/*
 * public API
 */

char *
sdb_get_homedir(void)
{
	char *username = sdb_get_current_user();

	struct passwd pw_entry;
	struct passwd *result = NULL;

	char buf[4096];

	int status;

	if (username) {
		memset(&pw_entry, 0, sizeof(pw_entry));
		status = getpwnam_r(username, &pw_entry, buf, sizeof(buf), &result);
	}
	else
		status = -1;

	if (status || (! result)) {
		char errbuf[1024];
		sdb_log(SDB_LOG_WARNING, "os: Failed to determine home directory "
				"for user %s: %s", username,
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		free(username);
		return NULL;
	}
	free(username);
	return strdup(result->pw_dir);
} /* sdb_get_homedir */

char *
sdb_realpath(const char *path)
{
	if (! path)
		return NULL;

	if ((strlen(path) >= 2) && (path[0] == '~') && (path[1] == '/')) {
		char *homedir = sdb_get_homedir();
		char tmp[(homedir ? strlen(homedir) : 0) + strlen(path)];
		char *ret;

		if (! homedir)
			return NULL;

		snprintf(tmp, sizeof(tmp), "%s/%s", homedir, path + 2);
		ret = realpath(tmp, NULL);
		free(homedir);
		return ret;
	}

	return realpath(path, NULL);
} /* sdb_realpath */

int
sdb_mkdir_all(const char *pathname, mode_t mode)
{
	struct stat st;
	char *pathname_copy;
	char *base_dir;

	int status = 0;

	if ((! pathname) || (! *pathname)) {
		errno = EINVAL;
		return -1;
	}

	memset(&st, 0, sizeof(st));
	if (! stat(pathname, &st)) {
		if (! S_ISDIR(st.st_mode)) {
			errno = ENOTDIR;
			return -1;
		}
		return 0;
	}

	if (errno != ENOENT)
		/* pathname exists but we cannot access it */
		return -1;

	pathname_copy = strdup(pathname);
	if (! pathname_copy)
		return -1;
	base_dir = dirname(pathname_copy);

	status = sdb_mkdir_all(base_dir, mode);
	if (! status)
		status = mkdir(pathname, mode);

	free(pathname_copy);
	return status;
} /* sdb_mkdir_all */

int
sdb_remove_all(const char *pathname)
{
	struct stat st;

	if ((! pathname) || (! *pathname)) {
		errno = EINVAL;
		return -1;
	}

	memset(&st, 0, sizeof(st));
	if (stat(pathname, &st))
		return -1;

	if (S_ISDIR(st.st_mode)) {
		DIR *d = opendir(pathname);

		if (! d)
			return -1;

		while (42) {
			struct dirent de;
			struct dirent *res = NULL;

			char filename[strlen(pathname) + sizeof(de.d_name) + 2];

			memset(&de, 0, sizeof(de));
			if (readdir_r(d, &de, &res)) {
				closedir(d);
				return -1;
			}

			if (! res)
				break;

			if ((de.d_name[0] == '.') && ((de.d_name[1] == '\0')
						|| ((de.d_name[1] == '.')&& (de.d_name[2] == '\0'))))
				continue;

			snprintf(filename, sizeof(filename),
					"%s/%s", pathname, de.d_name);
			if (sdb_remove_all(filename)) {
				closedir(d);
				return -1;
			}
		}
		closedir(d);
	}
	return remove(pathname);
} /* sdb_remove_all */

char *
sdb_get_current_user(void)
{
	struct passwd pw_entry;
	struct passwd *result = NULL;

	uid_t uid;

	char buf[4096];
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
	return strdup(result->pw_name);
} /* sdb_get_current_user */

int
sdb_select(int fd, int type)
{
	fd_set fds;
	fd_set *readfds = NULL;
	fd_set *writefds = NULL;
	fd_set *exceptfds = NULL;

	if (fd < 0) {
		errno = EBADF;
		return -1;
	}

	FD_ZERO(&fds);

	switch (type) {
		case SDB_SELECTIN:
			readfds = &fds;
			break;
		case SDB_SELECTOUT:
			writefds = &fds;
			break;
		case SDB_SELECTERR:
			exceptfds = &fds;
			break;
		default:
			errno = EINVAL;
			return -1;
	}

	FD_SET(fd, &fds);

	while (42) {
		int n;
		errno = 0;
		n = select(fd + 1, readfds, writefds, exceptfds, NULL);

		if ((n < 0) && (errno != EINTR))
			return n;
		if (n > 0)
			break;
	}
	return 0;
} /* sdb_select */

ssize_t
sdb_write(int fd, size_t msg_len, const void *msg)
{
	const char *buf;
	size_t len;

	if ((fd < 0) || (msg_len && (! msg)))
		return -1;
	if (! msg_len)
		return 0;

	buf = msg;
	len = msg_len;
	while (len > 0) {
		ssize_t status;

		if (sdb_select(fd, SDB_SELECTOUT))
			return -1;

		errno = 0;
		status = write(fd, buf, len);
		if (status < 0) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
				continue;
			if (errno == EINTR)
				continue;

			return status;
		}

		len -= (size_t)status;
		buf += status;
	}

	return (ssize_t)msg_len;
} /* sdb_write */

int
sdb_resolve(int network, const char *address, struct addrinfo **res)
{
	struct addrinfo ai_hints;
	const char *host;
	char *port;
	int status;

	if (! res) {
		errno = EINVAL;
		return EAI_SYSTEM;
	}

	if (address) {
		host = address;
		port = strchr(host, ':');
		if (port) {
			*port = '\0';
			++port;
		}
		if (! *host)
			host = NULL;
	}
	else {
		host = NULL;
		port = NULL;
	}

	memset(&ai_hints, 0, sizeof(ai_hints));
	ai_hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
	if (network & SDB_NET_V4)
		ai_hints.ai_family = AF_INET;
	else if (network & SDB_NET_V6)
		ai_hints.ai_family = AF_INET6;
	else
		ai_hints.ai_family = AF_UNSPEC;

	if ((network & SDB_NET_IP) == SDB_NET_IP) {
		ai_hints.ai_socktype = 0;
		ai_hints.ai_protocol = 0;
	}
	else if (network & SDB_NET_TCP) {
		ai_hints.ai_socktype = SOCK_STREAM;
		ai_hints.ai_protocol = IPPROTO_TCP;
	}
	else if (network & SDB_NET_UDP) {
		ai_hints.ai_socktype = SOCK_DGRAM;
		ai_hints.ai_protocol = IPPROTO_UDP;
	}

	status = getaddrinfo(host, port, &ai_hints, res);
	if (port) {
		--port;
		*port = ':';
	}
	return status;
} /* sdb_resolve */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

