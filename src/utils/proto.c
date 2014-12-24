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

#if HAVE_CONFIG_H
#	include "config.h"
#endif

#include "core/data.h"
#include "core/store.h"
#include "core/time.h"
#include "utils/error.h"
#include "utils/proto.h"

#include <assert.h>
#include <errno.h>

#include <arpa/inet.h>
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
marshal_int32(char *buf, size_t buf_len, uint32_t v)
{
	if (buf_len >= sizeof(v)) {
		v = htonl(v);
		memcpy(buf, &v, sizeof(v));
	}
	return sizeof(v);
} /* marshal_int32 */

static ssize_t
marshal_int64(char *buf, size_t buf_len, int64_t v)
{
	if (buf_len >= sizeof(v)) {
#if __BYTE_ORDER != __BIG_ENDIAN
		v = (((int64_t)ntohl((int32_t)v)) << 32)
			+ ((int64_t)ntohl((int32_t)(v >> 32)));
#endif
		memcpy(buf, &v, sizeof(v));
	}
	return sizeof(v);
} /* marshal_int64 */

static ssize_t
marshal_double(char *buf, size_t buf_len, double v)
{
	uint64_t t = 0;
	assert(sizeof(v) == sizeof(t));
	memcpy(&t, &v, sizeof(v));
#if IEEE754_DOUBLE_BYTE_ORDER != IEEE754_DOUBLE_BIG_ENDIAN
	t = (((int64_t)ntohl((int32_t)t)) << 32)
		+ ((int64_t)ntohl((int32_t)(t >> 32)));
#endif
	if (buf_len >= sizeof(t))
		memcpy(buf, &t, sizeof(t));
	return sizeof(t);
} /* marshal_double */

static ssize_t
marshal_datetime(char *buf, size_t buf_len, sdb_time_t v)
{
	return marshal_int64(buf, buf_len, (int64_t)v);
} /* marshal_datetime */

static ssize_t
marshal_binary(char *buf, size_t buf_len, size_t len, const unsigned char *v)
{
	uint32_t tmp = htonl((uint32_t)len);
	if (buf_len >= sizeof(tmp) + len) {
		memcpy(buf, &tmp, sizeof(tmp));
		memcpy(buf + sizeof(tmp), v, len);
	}
	return sizeof(tmp) + len;
} /* marshal_binary */

static ssize_t
marshal_string(char *buf, size_t buf_len, const char *v)
{
	/* The actual string including the terminating null byte. */
	size_t len = strlen(v) + 1;
	if (buf_len >= len)
		memcpy(buf, v, len);
	return len;
} /* marshal_string */

#define OBJ_HEADER_LEN (sizeof(uint32_t) + sizeof(sdb_time_t))
static ssize_t
marshal_obj_header(char *buf, size_t buf_len,
		int type, sdb_time_t last_update)
{
	ssize_t n;

	if (buf_len < OBJ_HEADER_LEN)
		return OBJ_HEADER_LEN;

	n = marshal_int32(buf, buf_len, (uint32_t)type);
	buf += n; buf_len -= n;
	marshal_datetime(buf, buf_len, last_update);
	return OBJ_HEADER_LEN;
} /* marshal_obj_header */

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
		n = marshal_int64(buf, buf_len, datum->data.integer);
	else if (datum->type == SDB_TYPE_DECIMAL)
		n = marshal_double(buf, buf_len, datum->data.decimal);
	else if (datum->type == SDB_TYPE_STRING)
		n = marshal_string(buf, buf_len, datum->data.string);
	else if (datum->type == SDB_TYPE_DATETIME)
		n = marshal_datetime(buf, buf_len, datum->data.datetime);
	else if (datum->type == SDB_TYPE_BINARY)
		n = marshal_binary(buf, buf_len,
				datum->data.binary.length, datum->data.binary.datum);
	else if (datum->type == SDB_TYPE_REGEX)
		n = marshal_string(buf, buf_len, datum->data.re.raw);

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
			n = marshal_int64(buf, buf_len, v[i]);
		}
		else if (type == SDB_TYPE_DECIMAL) {
			double *v = datum->data.array.values;
			n = marshal_double(buf, buf_len, v[i]);
		}
		else if (type == SDB_TYPE_STRING) {
			char **v = datum->data.array.values;
			n = marshal_string(buf, buf_len, v[i]);
		}
		else if (type == SDB_TYPE_DATETIME) {
			sdb_time_t *v = datum->data.array.values;
			n = marshal_datetime(buf, buf_len, v[i]);
		}
		else if (type == SDB_TYPE_BINARY) {
			struct {
				size_t length;
				unsigned char *datum;
			} *v = datum->data.array.values;
			n = marshal_binary(buf, buf_len, v[i].length, v[i].datum);
		}
		else if (type == SDB_TYPE_REGEX) {
			struct {
				char *raw;
				regex_t regex;
			} *v = datum->data.array.values;
			n = marshal_string(buf, buf_len, v[i].raw);
		}
		else {
			errno = EINVAL;
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

ssize_t
sdb_proto_marshal_host(char *buf, size_t buf_len,
		const sdb_proto_host_t *host)
{
	size_t len;
	ssize_t n;

	if ((! host) || (! host->name))
		return -1;

	len = OBJ_HEADER_LEN + strlen(host->name) + 1;
	if (buf_len < len)
		return len;

	n = marshal_obj_header(buf, buf_len, SDB_HOST, host->last_update);
	buf += n; buf_len -= n;
	marshal_string(buf, buf_len, host->name);
	return len;
} /* sdb_proto_marshal_host */

ssize_t
sdb_proto_marshal_service(char *buf, size_t buf_len,
		const sdb_proto_service_t *svc)
{
	size_t len;
	ssize_t n;

	if ((! svc) || (! svc->hostname) || (! svc->name))
		return -1;

	len = OBJ_HEADER_LEN + strlen(svc->hostname) + strlen(svc->name) + 2;
	if (buf_len < len)
		return len;

	n = marshal_obj_header(buf, buf_len, SDB_SERVICE, svc->last_update);
	buf += n; buf_len -= n;
	n = marshal_string(buf, buf_len, svc->hostname);
	buf += n; buf_len -= n;
	marshal_string(buf, buf_len, svc->name);
	return len;
} /* sdb_proto_marshal_service */

ssize_t
sdb_proto_marshal_metric(char *buf, size_t buf_len,
		const sdb_proto_metric_t *metric)
{
	size_t len;
	ssize_t n;

	if ((! metric) || (! metric->hostname) || (! metric->name))
		return -1;

	len = OBJ_HEADER_LEN + strlen(metric->hostname) + strlen(metric->name) + 2;
	if (metric->store_type && metric->store_id)
		len += strlen(metric->store_type) + strlen(metric->store_id) + 2;
	if (buf_len < len)
		return len;

	n = marshal_obj_header(buf, buf_len, SDB_METRIC, metric->last_update);
	buf += n; buf_len -= n;
	n = marshal_string(buf, buf_len, metric->hostname);
	buf += n; buf_len -= n;
	n = marshal_string(buf, buf_len, metric->name);
	buf += n; buf_len -= n;
	if (metric->store_type && metric->store_id) {
		n = marshal_string(buf, buf_len, metric->store_type);
		buf += n; buf_len -= n;
		marshal_string(buf, buf_len, metric->store_id);
	}
	return len;
} /* sdb_proto_marshal_metric */

int
sdb_proto_unmarshal_header(const char *buf, size_t buf_len,
		uint32_t *code, uint32_t *msg_len)
{
	uint32_t tmp;

	if (buf_len < 2 * sizeof(uint32_t))
		return -1;

	tmp = sdb_proto_unmarshal_int32(buf, buf_len);
	if (code)
		*code = tmp;
	tmp = sdb_proto_unmarshal_int32(buf + sizeof(uint32_t),
			buf_len - sizeof(uint32_t));
	if (msg_len)
		*msg_len = tmp;
	return 0;
} /* sdb_proto_unmarshal_header */

uint32_t
sdb_proto_unmarshal_int32(const char *buf, size_t buf_len)
{
	uint32_t n;

	/* not enough data to read */
	if (buf_len < sizeof(n))
		return UINT32_MAX;

	memcpy(&n, buf, sizeof(n));
	return ntohl(n);
} /* sdb_proto_unmarshal_int32 */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

