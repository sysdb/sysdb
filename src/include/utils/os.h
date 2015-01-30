/*
 * SysDB - src/include/utils/os.h
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

#ifndef SDB_UTILS_OS_H
#define SDB_UTILS_OS_H 1

#include <sys/types.h>
#include <netdb.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sdb_get_homedir:
 * Returns the home directory of the current user. The buffer to hold the
 * return value is allocated dynamically and has to be freed by the caller.
 *
 * Returns:
 *  - the current user's home directory on success
 *  - NULL else
 */
char *
sdb_get_homedir(void);

/*
 * sysdb_mkdir_all:
 * Recursively create the directory 'pathname' (similar to 'mkdir -p' on the
 * command line) using file permissions as specified by 'mode'.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_mkdir_all(const char *pathname, mode_t mode);

/*
 * sdb_remove_all:
 * Recursively deletes the specified path from the filesystem.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_remove_all(const char *pathname);

/*
 * sdb_get_current_user:
 * Returns the name of the current user. The string is allocated dynamically
 * and has to be freed by the caller.
 *
 * Returns:
 *  - the username on success
 *  - NULL else
 */
char *
sdb_get_current_user(void);

enum {
	SDB_SELECTIN = 0,
	SDB_SELECTOUT,
	SDB_SELECTERR,
};

/*
 * sdb_select:
 * Wait for a file-descriptor to become ready for I/O operations of the
 * specified type. This is a simple wrapper around the select() system call.
 * The type argument may be any of the SDB_SELECT* constants.
 *
 * Returns:
 *  - the number of file descriptors ready for I/O
 *  - a negative value on error
 */
int
sdb_select(int fd, int type);

/*
 * sdb_write:
 * Write a message to a file-descriptor. This is a simple wrapper around the
 * write() system call ensuring that all data is written on success.
 *
 * Returns:
 *  - the number of bytes written
 *  - a negative value on error
 */
ssize_t
sdb_write(int fd, size_t msg_len, const void *msg);

enum {
	SDB_NET_TCP = 1 << 0,
	SDB_NET_UDP = 1 << 1,
	SDB_NET_IP  = SDB_NET_TCP | SDB_NET_UDP,

	SDB_NET_V4  = 1 << 2,
	SDB_NET_V6  = 1 << 3,
};

/*
 * sdb_resolve:
 * Resolve the specified address on the specified network which may be a
 * bitwise OR of the SDB_NET constants. The network addresses are returned in
 * the list pointed to by 'res'. The list is allocated dynamically and has to
 * be freed using freeaddrinfo().
 *
 * Returns:
 *  - zero on success
 *  - an error code else; use gai_strerror() to translate the error into a
 *    human readable string
 */
int
sdb_resolve(int network, const char *address, struct addrinfo **res);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_UTILS_OS_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

