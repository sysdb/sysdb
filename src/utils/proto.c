/*
 * SysDB - src/utils/proto.c
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

#include "utils/error.h"
#include "utils/proto.h"

#include <arpa/inet.h>
#include <errno.h>

#include <limits.h>

#include <string.h>
#include <unistd.h>

#include <sys/select.h>

/*
 * public API
 */

ssize_t
sdb_proto_marshal(char *buf, size_t buf_len, uint32_t code,
		uint32_t msg_len, const char *msg)
{
	size_t len = 2 * sizeof(uint32_t) + msg_len;
	uint32_t tmp;

	if (buf_len < 2 * sizeof(uint32_t))
		return -1;
	if (buf_len < len) /* crop message */
		msg_len -= (uint32_t)(len - buf_len);

	tmp = htonl(code);
	memcpy(buf, &tmp, sizeof(tmp));
	tmp = htonl(msg_len);
	memcpy(buf + sizeof(tmp), &tmp, sizeof(tmp));

	if (msg_len)
		memcpy(buf + 2 * sizeof(tmp), msg, msg_len);
	return len;
} /* sdb_proto_marshal */

int
sdb_proto_unmarshal_header(const char *buf, size_t buf_len,
		uint32_t *code, uint32_t *msg_len)
{
	uint32_t tmp;

	if (buf_len < 2 * sizeof(uint32_t))
		return -1;

	tmp = sdb_proto_unmarshal_int(buf, buf_len);
	if (code)
		*code = tmp;
	tmp = sdb_proto_unmarshal_int(buf + sizeof(uint32_t),
			buf_len - sizeof(uint32_t));
	if (msg_len)
		*msg_len = tmp;
	return 0;
} /* sdb_proto_unmarshal_header */

uint32_t
sdb_proto_unmarshal_int(const char *buf, size_t buf_len)
{
	uint32_t n;

	/* not enough data to read */
	if (buf_len < sizeof(n))
		return UINT32_MAX;

	memcpy(&n, buf, sizeof(n));
	return ntohl(n);
} /* sdb_proto_unmarshal_int */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

