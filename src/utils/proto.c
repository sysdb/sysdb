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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/select.h>

/*
 * private helper functions
 */

/* swap endianess of the specified 64-bit value */
static uint64_t
endian_swap64(uint64_t v)
{
	return
		  (((v & 0xff00000000000000LL) >> 56) & 0x00000000000000ffLL)
		| (((v & 0x00ff000000000000LL) >> 40) & 0x000000000000ff00LL)
		| (((v & 0x0000ff0000000000LL) >> 24) & 0x0000000000ff0000LL)
		| (((v & 0x000000ff00000000LL) >> 8)  & 0x00000000ff000000LL)
		| (((v & 0x00000000ff000000LL) << 8)  & 0x000000ff00000000LL)
		| (((v & 0x0000000000ff0000LL) << 24) & 0x0000ff0000000000LL)
		| (((v & 0x000000000000ff00LL) << 40) & 0x00ff000000000000LL)
		| (((v & 0x00000000000000ffLL) << 56) & 0xff00000000000000LL);
} /* endian_swap64 */

static unsigned char *
memdup(const unsigned char *d, size_t length)
{
	unsigned char *v = malloc(length);
	if (! v)
		return NULL;
	memcpy(v, d, length);
	return v;
} /* memdup */

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
		v = endian_swap64(v);
#endif
		memcpy(buf, &v, sizeof(v));
	}
	return sizeof(v);
} /* marshal_int64 */

static ssize_t
unmarshal_int64(const char *buf, size_t len, int64_t *v)
{
	if (len < sizeof(*v))
		return -1;
	if (v) {
		memcpy(v, buf, sizeof(*v));
#if __BYTE_ORDER != __BIG_ENDIAN
		*v = endian_swap64(*v);
#endif
	}
	return sizeof(*v);
} /* unmarshal_int64 */

static ssize_t
marshal_double(char *buf, size_t buf_len, double v)
{
	uint64_t t = 0;
	assert(sizeof(v) == sizeof(t));
	if (buf_len >= sizeof(t)) {
		memcpy(&t, &v, sizeof(v));
#if IEEE754_DOUBLE_BYTE_ORDER != IEEE754_DOUBLE_BIG_ENDIAN
		t = endian_swap64(t);
#endif
		memcpy(buf, &t, sizeof(t));
	}
	return sizeof(t);
} /* marshal_double */

static ssize_t
unmarshal_double(const char *buf, size_t len, double *v)
{
	uint64_t t;
	assert(sizeof(*v) == sizeof(t));
	if (len < sizeof(*v))
		return -1;
	if (v) {
		memcpy(&t, buf, sizeof(t));
#if IEEE754_DOUBLE_BYTE_ORDER != IEEE754_DOUBLE_BIG_ENDIAN
		t = endian_swap64(t);
#endif
		memcpy(v, &t, sizeof(t));
	}
	return sizeof(*v);
} /* unmarshal_double */

static ssize_t
marshal_datetime(char *buf, size_t buf_len, sdb_time_t v)
{
	return marshal_int64(buf, buf_len, (int64_t)v);
} /* marshal_datetime */

static ssize_t
unmarshal_datetime(const char *buf, size_t len, sdb_time_t *v)
{
	return unmarshal_int64(buf, len, (int64_t *)v);
} /* unmarshal_datetime */

static ssize_t
marshal_binary(char *buf, size_t buf_len, size_t len, const unsigned char *v)
{
	uint32_t tmp;
	if (buf_len >= sizeof(tmp) + len) {
		tmp = htonl((uint32_t)len);
		memcpy(buf, &tmp, sizeof(tmp));
		memcpy(buf + sizeof(tmp), v, len);
	}
	return sizeof(tmp) + len;
} /* marshal_binary */

static ssize_t
unmarshal_binary(const char *buf, size_t len,
		size_t *v_len, const unsigned char **v)
{
	uint32_t l;
	ssize_t n;

	if ((n = sdb_proto_unmarshal_int32(buf, len, &l)) < 0)
		return -1;
	buf += n; len -= n;
	if (len < (size_t)l)
		return -1;

	if (v_len)
		*v_len = (size_t)l;
	if (v && (l > 0))
		*v = (const unsigned char *)buf;
	else if (v)
		*v = NULL;
	return sizeof(l) + (ssize_t)l;
} /* unmarshal_binary */

static ssize_t
marshal_string(char *buf, size_t buf_len, const char *v)
{
	/* The actual string including the terminating null byte. */
	size_t len = strlen(v) + 1;
	if (buf_len >= len)
		memcpy(buf, v, len);
	return len;
} /* marshal_string */

static ssize_t
unmarshal_string(const char *buf, size_t len, const char **v)
{
	size_t l = 0;

	for (l = 0; l < len; ++l)
		if (buf[l] == '\0')
			break;
	if (l == len)
		return -1;
	if (v)
		*v = buf;
	return l + 1;
} /* unmarshal_string */

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
sdb_proto_marshal_data(char *buf, size_t buf_len, const sdb_data_t *datum)
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

ssize_t
sdb_proto_marshal_attribute(char *buf, size_t buf_len,
		const sdb_proto_attribute_t *attr)
{
	size_t len;
	ssize_t n;

	if ((! attr) || (! attr->parent) || (! attr->key) || (! attr->value)
			|| ((attr->parent_type != SDB_HOST) && (! attr->hostname))
			|| ((attr->parent_type != SDB_HOST)
				&& (attr->parent_type != SDB_SERVICE)
				&& (attr->parent_type != SDB_METRIC)))
		return -1;

	n = sdb_proto_marshal_data(NULL, 0, attr->value);
	if (n < 0)
		return -1;

	len = OBJ_HEADER_LEN
		+ strlen(attr->parent) + strlen(attr->key) + 2 + (size_t)n;
	if (attr->parent_type != SDB_HOST)
		len += strlen(attr->hostname) + 1;
	if (buf_len < len)
		return len;

	n = marshal_obj_header(buf, buf_len,
			attr->parent_type | SDB_ATTRIBUTE, attr->last_update);
	buf += n; buf_len -= n;
	if (attr->parent_type != SDB_HOST) {
		n = marshal_string(buf, buf_len, attr->hostname);
		buf += n; buf_len -= n;
	}
	n = marshal_string(buf, buf_len, attr->parent);
	buf += n; buf_len -= n;
	n = marshal_string(buf, buf_len, attr->key);
	buf += n; buf_len -= n;
	sdb_proto_marshal_data(buf, buf_len, attr->value);
	return len;
} /* sdb_proto_marshal_attribute */

ssize_t
sdb_proto_unmarshal_header(const char *buf, size_t buf_len,
		uint32_t *code, uint32_t *msg_len)
{
	uint32_t tmp;
	ssize_t n;

	if (buf_len < 2 * sizeof(uint32_t))
		return -1;

	n = sdb_proto_unmarshal_int32(buf, buf_len, &tmp);
	if (code)
		*code = tmp;
	buf += n; buf_len -= n;
	sdb_proto_unmarshal_int32(buf, buf_len, &tmp);
	if (msg_len)
		*msg_len = tmp;
	return 2 * sizeof(uint32_t);
} /* sdb_proto_unmarshal_header */

ssize_t
sdb_proto_unmarshal_int32(const char *buf, size_t buf_len, uint32_t *v)
{
	uint32_t n;

	/* not enough data to read */
	if (buf_len < sizeof(n))
		return -1;

	memcpy(&n, buf, sizeof(n));
	if (v)
		*v = ntohl(n);
	return sizeof(n);
} /* sdb_proto_unmarshal_int32 */

ssize_t
sdb_proto_unmarshal_data(const char *buf, size_t len, sdb_data_t *datum)
{
	sdb_data_t d = SDB_DATA_INIT;
	ssize_t l = 0, n;
	uint32_t tmp;
	size_t i;

	if ((n = sdb_proto_unmarshal_int32(buf, len, &tmp)) < 0)
		return -1;
	d.type = (int)tmp;
	if (d.type == SDB_TYPE_NULL)
		return sizeof(tmp);
	buf += n; len -= n; l += n;

/* Don't populate 'd' if 'datum' is NULL. */
#define D(field) (datum ? &d.data.field : NULL)
	if (d.type == SDB_TYPE_INTEGER)
		n = unmarshal_int64(buf, len, D(integer));
	else if (d.type == SDB_TYPE_DECIMAL)
		n = unmarshal_double(buf, len, D(decimal));
	else if (d.type == SDB_TYPE_STRING) {
		const char *str = NULL;
		n = unmarshal_string(buf, len, &str);
		if ((n > 0) && datum)
			if (! (d.data.string = strdup(str)))
				n = -1;
	}
	else if (d.type == SDB_TYPE_DATETIME)
		n = unmarshal_datetime(buf, len, D(datetime));
	else if (d.type == SDB_TYPE_BINARY) {
		const unsigned char *data = NULL;
		n = unmarshal_binary(buf, len, D(binary.length), &data);
		if ((n > 0) && datum)
			if (! (d.data.binary.datum = memdup(data, d.data.binary.length)))
				n = -1;
	}
	else if (d.type == SDB_TYPE_REGEX) {
		if (datum) {
			const char *str = NULL;
			n = unmarshal_string(buf, len, &str);
			if (sdb_data_parse(str, SDB_TYPE_REGEX, &d))
				n = -1;
		}
		else
			n = unmarshal_string(buf, len, NULL);
	}
	else
		n = 0;
#undef D

	if (n < 0)
		return n;
	else if (n > 0) {
		if (datum)
			*datum = d;
		return l + n;
	}

	if (! (d.type & SDB_TYPE_ARRAY)) {
		errno = EINVAL;
		return -1;
	}

	/* arrays */
	if ((n = sdb_proto_unmarshal_int32(buf, len, &tmp)) < 0)
		return -1;
	buf += n; len -= n; l += n;
	d.data.array.length = (size_t)tmp;

#define V(field) (datum ? &v[i]field : NULL)
	if (datum)
		d.data.array.values = calloc(d.data.array.length,
				sdb_data_sizeof(d.type & 0xff));
	for (i = 0; i < d.data.array.length; ++i) {
		if ((d.type & 0xff) == SDB_TYPE_INTEGER) {
			int64_t *v = d.data.array.values;
			n = unmarshal_int64(buf, len, V());
		}
		else if ((d.type & 0xff) == SDB_TYPE_DECIMAL) {
			double *v = d.data.array.values;
			n = unmarshal_double(buf, len, V());
		}
		else if ((d.type & 0xff) == SDB_TYPE_STRING) {
			char **v = d.data.array.values;
			const char *str = NULL;
			n = unmarshal_string(buf, len, &str);
			if ((n > 0) && datum)
				if (! (v[i] = strdup(str)))
					n = -1;
		}
		else if ((d.type & 0xff) == SDB_TYPE_DATETIME) {
			sdb_time_t *v = d.data.array.values;
			n = unmarshal_datetime(buf, len, V());
		}
		else if ((d.type & 0xff) == SDB_TYPE_BINARY) {
			struct {
				size_t length;
				unsigned char *datum;
			} *v = d.data.array.values;
			const unsigned char *data = NULL;

			n = unmarshal_binary(buf, len, V(.length), &data);
			if ((n > 0) && datum)
				if (! (v[i].datum = memdup(data, v[i].length)))
					n = -1;
		}
		else if ((d.type & 0xff) == SDB_TYPE_REGEX) {
			struct {
				char *raw;
				regex_t regex;
			} *v = d.data.array.values;
			if (datum) {
				sdb_data_t t = SDB_DATA_INIT;
				const char *str = NULL;
				n = unmarshal_string(buf, len, &str);
				if (! sdb_data_parse(str, SDB_TYPE_REGEX, &t)) {
					v[i].raw = t.data.re.raw;
					v[i].regex = t.data.re.regex;
				}
				else
					n = -1;
			}
			else
				n = unmarshal_string(buf, len, NULL);
		}
		else {
			if (datum)
				sdb_data_free_datum(&d);
			errno = EINVAL;
			return -1;
		}

		if (n < 0) {
			if (datum)
				sdb_data_free_datum(&d);
			return -1;
		}
		if (len >= (size_t)n) {
			buf += n;
			len -= n;
			l += n;
		}
		else
			return -1;
	}
#undef V

	if (datum)
		*datum = d;
	return l;
} /* sdb_proto_unmarshal_data */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

