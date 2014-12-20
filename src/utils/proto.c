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

#include "core/data.h"
#include "core/time.h"
#include "utils/error.h"
#include "utils/proto.h"

#include <arpa/inet.h>
#include <errno.h>

#include <limits.h>

#include <string.h>
#include <unistd.h>

#include <sys/select.h>

/*
 * private helper functions
 */

/* In case there's not enough buffer space, the marshal functions have to
 * return the number of bytes that would have been written if enough space had
 * been available. */

static ssize_t
marshal_int(char *buf, size_t buf_len, int64_t v)
{
	if (buf_len >= sizeof(v)) {
#if __BYTE_ORDER != __BIG_ENDIAN
		v = (((int64_t)ntohl((int32_t)v)) << 32)
			+ ((int64_t)ntohl((int32_t)(v >> 32)));
#endif
		memcpy(buf, &v, sizeof(v));
	}
	return sizeof(v);
} /* marshal_int */

static ssize_t
marshal_double(char __attribute__((unused)) *buf,
		size_t __attribute__((unused)) buf_len,
		double __attribute__((unused)) v)
{
	/* XXX: find a good network representation */
	errno = ENOTSUP;
	return -1;
} /* marshal_double */

static ssize_t
marshal_datetime(char *buf, size_t buf_len, sdb_time_t v)
{
	return marshal_int(buf, buf_len, (int64_t)v);
} /* marshal_datetime */

static ssize_t
marshal_binary(char *buf, size_t buf_len, size_t len, const unsigned char *v)
{
	uint32_t tmp = htonl((uint32_t)len);
	if (buf_len >= len) {
		memcpy(buf, &tmp, sizeof(tmp));
		memcpy(buf + sizeof(tmp), v, len);
	}
	return sizeof(tmp) + len;
} /* marshal_binary */

static ssize_t
marshal_string(char *buf, size_t buf_len, const char *v)
{
	/* The actual string including the terminating null byte. */
	return marshal_binary(buf, buf_len,
			strlen(v) + 1, (const unsigned char *)v);
} /* marshal_string */

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

ssize_t
sdb_proto_marshal_data(char *buf, size_t buf_len, sdb_data_t *datum)
{
	ssize_t len = 0, n = 0;
	uint32_t tmp;
	size_t i;
	int type;

	if (buf_len >= sizeof(tmp)) {
		tmp = htonl((uint32_t)datum->type);
		memcpy(buf, &tmp, sizeof(tmp));
		buf += sizeof(tmp);
		buf_len -= sizeof(tmp);
	}
	else
		buf_len = 0;
	len += sizeof(tmp);

	if (datum->type == SDB_TYPE_NULL)
		return len;

	if (datum->type == SDB_TYPE_INTEGER)
		n = marshal_int(buf, buf_len, datum->data.integer);
	else if (datum->type == SDB_TYPE_DECIMAL)
		n = marshal_double(buf, buf_len, datum->data.decimal);
	else if (datum->type == SDB_TYPE_STRING)
		n = marshal_string(buf, buf_len, datum->data.string);
	else if (datum->type == SDB_TYPE_DATETIME)
		n = marshal_datetime(buf, buf_len, datum->data.datetime);
	else if (datum->type == SDB_TYPE_BINARY)
		n = marshal_binary(buf, buf_len,
				datum->data.binary.length, datum->data.binary.datum);

	if (n < 0)
		return n;
	else if (n > 0)
		return len + n;

	if (! (datum->type & SDB_TYPE_ARRAY)) {
		errno = EINVAL;
		return -1;
	}

	/* arrays */
	if (buf_len >= sizeof(tmp)) {
		tmp = htonl((uint32_t)datum->data.array.length);
		memcpy(buf, &tmp, sizeof(tmp));
		buf += sizeof(tmp);
		buf_len -= sizeof(tmp);
	}
	else
		buf_len = 0;
	len += sizeof(tmp);

	type = datum->type & 0xff;
	for (i = 0; i < datum->data.array.length; ++i) {
		if (type == SDB_TYPE_INTEGER) {
			int64_t *v = datum->data.array.values;
			n = marshal_int(buf, buf_len, v[i]);
		}
		else if (type == SDB_TYPE_DECIMAL) {
			double *v = datum->data.array.values;
			n = marshal_double(buf, buf_len, v[i]);
		}
		else if (type == SDB_TYPE_STRING) {
			char **v = datum->data.array.values;
			n = marshal_string(buf, buf_len, v[i]);
		}
		else {
			errno = ENOTSUP;
			return -1;
		}

		if (n < 0)
			return -1;
		if (buf_len >= (size_t)n) {
			buf += n;
			buf_len -= n;
		}
		else
			buf_len = 0;
		len += n;
	}
	return len;
} /* sdb_proto_marshal_data */

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

