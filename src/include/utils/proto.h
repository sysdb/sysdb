/*
 * SysDB - src/include/utils/proto.h
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

#ifndef SDB_UTILS_PROTO_H
#define SDB_UTILS_PROTO_H 1

#include "utils/strbuf.h"

#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	SDB_PROTO_SELECTIN = 0,
	SDB_PROTO_SELECTOUT,
	SDB_PROTO_SELECTERR,
};

/*
 * sdb_proto_select:
 * Wait for a file-descriptor to become ready for I/O operations of the
 * specified type. This is a simple wrapper around the select() system call.
 * The type argument may be any of the SDB_PROTO_SELECT* constants.
 *
 * Returns:
 *  - the number of file descriptors ready for I/O
 *  - a negative value on error
 */
int
sdb_proto_select(int fd, int type);

ssize_t
sdb_proto_send(int fd, size_t msg_len, const char *msg);

ssize_t
sdb_proto_send_msg(int fd, uint32_t code,
		uint32_t msg_len, const char *msg);

uint32_t
sdb_proto_get_int(sdb_strbuf_t *buf, size_t offset);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_UTILS_PROTO_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

