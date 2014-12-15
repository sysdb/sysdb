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
#include <sys/stat.h>

#include <dirent.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <libgen.h>
#include <pwd.h>

/*
 * public API
 */

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

	char buf[1024];
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

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

