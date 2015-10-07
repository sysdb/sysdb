/*
 * SysDB - src/core/store_json.c
 * Copyright (C) 2013-2015 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

/*
 * This module implements JSON support.
 */

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "sysdb.h"
#include "core/store-private.h"
#include "utils/error.h"

#include <assert.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/*
 * private data types
 */

struct sdb_store_json_formatter {
	sdb_object_t super;

	/* The string buffer to write to */
	sdb_strbuf_t *buf;

	/* The context describes the state of the formatter through
	 * the path pointing to the current object */
	int context[8];
	size_t current;

	int type;
	int flags;
};
#define F(obj) ((sdb_store_json_formatter_t *)(obj))

static int
formatter_init(sdb_object_t *obj, va_list ap)
{
	sdb_store_json_formatter_t *f = F(obj);

	f->buf = va_arg(ap, sdb_strbuf_t *);
	if (! f->buf)
		return -1;

	f->type = va_arg(ap, int);
	if ((f->type != SDB_HOST) && (f->type != SDB_SERVICE) && (f->type != SDB_METRIC))
		return -1;

	f->flags = va_arg(ap, int);

	f->context[0] = 0;
	f->current = 0;
	return 0;
} /* formatter_init */

static sdb_type_t formatter_type = {
	/* size = */ sizeof(sdb_store_json_formatter_t),
	/* init = */ formatter_init,
	/* destroy = */ NULL,
};

/* A generic representation of a stored object. */
typedef struct {
	/* identifier */
	int type;
	const char *name;

	/* attribute value (optional) */
	sdb_data_t *value;

	/* metric's timeseries (optional)
	 * -1: unset
	 *  0: false
	 *  1: true */
	int timeseries;

	/* generic meta-data */
	sdb_time_t last_update;
	sdb_time_t interval;
	size_t backends_num;
	const char * const *backends;
} obj_t;

/*
 * private helper functions
 */

static void
escape_string(const char *src, char *dest)
{
	size_t i = 1;
	dest[0] = '"';
	for ( ; *src; ++src) {
		char c = *src;
		if ((c == '"') || (c == '\\') || iscntrl((int)c)) {
			dest[i] = '\\';
			++i;
		}
		switch (c) {
			case '\a': dest[i] = 'a'; break;
			case '\b': dest[i] = 'b'; break;
			case '\t': dest[i] = 't'; break;
			case '\n': dest[i] = 'n'; break;
			case '\v': dest[i] = 'v'; break;
			case '\f': dest[i] = 'f'; break;
			case '\r': dest[i] = 'r'; break;
			default: dest[i] = c; break;
		}
		++i;
	}
	dest[i] = '"';
	dest[i + 1] = '\0';
} /* escape_string */

/* handle_new_object takes care of all maintenance logic related to adding a
 * new object. That is, it manages context information and emit the prefix and
 * suffix of an object. */
static int
handle_new_object(sdb_store_json_formatter_t *f, int type)
{
	/* first top-level object */
	if (! f->context[0]) {
		if ((type != f->type) && (type != SDB_HOST)) {
			sdb_log(SDB_LOG_ERR, "store: Unexpected object of type %s "
					"as the first element during %s JSON serialization",
					SDB_STORE_TYPE_TO_NAME(type),
					SDB_STORE_TYPE_TO_NAME(f->type));
			return -1;
		}
		if (f->flags & SDB_WANT_ARRAY)
			sdb_strbuf_append(f->buf, "[");
		assert(f->current == 0);
		f->context[f->current] = type;
		return 0;
	}

	if ((f->context[f->current] != SDB_HOST)
			&& (type != SDB_ATTRIBUTE)) {
		/* new entry of the same type or a parent object;
		 * rewind to the right state */
		while ((f->current > 0)
				&& (f->context[f->current] != type)) {
			sdb_strbuf_append(f->buf, "}]");
			--f->current;
		}
	}

	if (type == f->context[f->current]) {
		/* new entry of the same type */
		sdb_strbuf_append(f->buf, "},");
	}
	else if ((f->context[f->current] == SDB_HOST)
			|| (type == SDB_ATTRIBUTE)) {
		assert(type != SDB_HOST);
		/* all object types may be children of a host;
		 * attributes may be children of any type */
		sdb_strbuf_append(f->buf, ", \"%ss\": [",
				SDB_STORE_TYPE_TO_NAME(type));
		++f->current;
	}
	else {
		sdb_log(SDB_LOG_ERR, "store: Unexpected object of type %s "
				"on level %zu during JSON serialization",
				SDB_STORE_TYPE_TO_NAME(type), f->current);
		return -1;
	}

	assert(f->current < SDB_STATIC_ARRAY_LEN(f->context));
	f->context[f->current] = type;
	return 0;
} /* handle_new_object */

static int
json_emit(sdb_store_json_formatter_t *f, obj_t *obj)
{
	char time_str[64];
	char interval_str[64];
	char name[2 * strlen(obj->name) + 3];
	size_t i;

	assert(f && obj);

	handle_new_object(f, obj->type);

	escape_string(obj->name, name);
	sdb_strbuf_append(f->buf, "{\"name\": %s, ", name);
	if ((obj->type == SDB_ATTRIBUTE) && (obj->value)) {
		char tmp[sdb_data_strlen(obj->value) + 1];
		char val[2 * sizeof(tmp) + 3];
		if (! sdb_data_format(obj->value, tmp, sizeof(tmp),
					SDB_DOUBLE_QUOTED))
			snprintf(tmp, sizeof(tmp), "<error>");

		if (tmp[0] == '"') {
			/* a string; escape_string handles quoting */
			tmp[strlen(tmp) - 1] = '\0';
			escape_string(tmp + 1, val);
			sdb_strbuf_append(f->buf, "\"value\": %s, ", val);
		}
		else
			sdb_strbuf_append(f->buf, "\"value\": %s, ", tmp);
	}
	else if ((obj->type == SDB_METRIC) && (obj->timeseries >= 0)) {
		if (obj->timeseries)
			sdb_strbuf_append(f->buf, "\"timeseries\": true, ");
		else
			sdb_strbuf_append(f->buf, "\"timeseries\": false, ");
	}

	/* TODO: make time and interval formats configurable */
	if (! sdb_strftime(time_str, sizeof(time_str), obj->last_update))
		snprintf(time_str, sizeof(time_str), "<error>");
	time_str[sizeof(time_str) - 1] = '\0';

	if (! sdb_strfinterval(interval_str, sizeof(interval_str),
				obj->interval))
		snprintf(interval_str, sizeof(interval_str), "<error>");
	interval_str[sizeof(interval_str) - 1] = '\0';

	sdb_strbuf_append(f->buf, "\"last_update\": \"%s\", "
			"\"update_interval\": \"%s\", \"backends\": [",
			time_str, interval_str);

	for (i = 0; i < obj->backends_num; ++i) {
		sdb_strbuf_append(f->buf, "\"%s\"", obj->backends[i]);
		if (i < obj->backends_num - 1)
			sdb_strbuf_append(f->buf, ",");
	}
	sdb_strbuf_append(f->buf, "]");
	return 0;
} /* json_emit */

static int
emit_host(sdb_store_host_t *host, sdb_object_t *user_data)
{
	sdb_store_json_formatter_t *f = F(user_data);

	if ((! host) || (! user_data))
		return -1;

	{
		obj_t o = {
			SDB_HOST,
			host->name,

			/* value */ NULL,
			/* timeseries */ -1,

			host->last_update,
			host->interval,
			host->backends_num,
			(const char * const *)host->backends,
		};

		return json_emit(f, &o);
	}
} /* emit_host */

static int
emit_service(sdb_store_service_t *service, sdb_object_t *user_data)
{
	sdb_store_json_formatter_t *f = F(user_data);

	if ((! service) || (! user_data))
		return -1;

	{
		obj_t o = {
			SDB_SERVICE,
			service->name,

			/* value */ NULL,
			/* timeseries */ -1,

			service->last_update,
			service->interval,
			service->backends_num,
			(const char * const *)service->backends,
		};

		return json_emit(f, &o);
	}
} /* emit_service */

static int
emit_metric(sdb_store_metric_t *metric, sdb_object_t *user_data)
{
	sdb_store_json_formatter_t *f = F(user_data);

	if ((! metric) || (! user_data))
		return -1;

	{
		obj_t o = {
			SDB_METRIC,
			metric->name,

			/* value */ NULL,
			/* timeseries */ metric->store.type != NULL,

			metric->last_update,
			metric->interval,
			metric->backends_num,
			(const char * const *)metric->backends,
		};

		return json_emit(f, &o);
	}
} /* emit_metric */

static int
emit_attribute(sdb_store_attribute_t *attr, sdb_object_t *user_data)
{
	sdb_store_json_formatter_t *f = F(user_data);

	if ((! attr) || (! user_data))
		return -1;

	{
		obj_t o = {
			SDB_ATTRIBUTE,
			attr->key,

			/* value */ &attr->value,
			/* timeseries */ -1,

			attr->last_update,
			attr->interval,
			attr->backends_num,
			(const char * const *)attr->backends,
		};

		return json_emit(f, &o);
	}
} /* emit_attribute */

/*
 * public API
 */

sdb_store_writer_t sdb_store_json_writer = {
	emit_host, emit_service, emit_metric, emit_attribute,
};

sdb_store_json_formatter_t *
sdb_store_json_formatter(sdb_strbuf_t *buf, int type, int flags)
{
	return F(sdb_object_create("json-formatter", formatter_type,
				buf, type, flags));
} /* sdb_store_json_formatter */

/* TODO: Move sdb_store_emit* somewhere else. */

int
sdb_memstore_emit(sdb_memstore_obj_t *obj, sdb_store_writer_t *w, sdb_object_t *wd)
{
	if ((! obj) || (! w))
		return -1;

	switch (obj->type) {
	case SDB_HOST:
		{
			sdb_store_host_t host = {
				obj->_name,
				obj->last_update,
				obj->interval,
				(const char * const *)obj->backends,
				obj->backends_num,
			};
			if (! w->store_host)
				return -1;
			return w->store_host(&host, wd);
		}
	case SDB_SERVICE:
		{
			sdb_store_service_t service = {
				obj->parent ? obj->parent->_name : NULL,
				obj->_name,
				obj->last_update,
				obj->interval,
				(const char * const *)obj->backends,
				obj->backends_num,
			};
			if (! w->store_service)
				return -1;
			return w->store_service(&service, wd);
		}
	case SDB_METRIC:
		{
			sdb_store_metric_t metric = {
				obj->parent ? obj->parent->_name : NULL,
				obj->_name,
				{
					METRIC(obj)->store.type,
					METRIC(obj)->store.id,
				},
				obj->last_update,
				obj->interval,
				(const char * const *)obj->backends,
				obj->backends_num,
			};
			if (! w->store_metric)
				return -1;
			return w->store_metric(&metric, wd);
		}
	case SDB_ATTRIBUTE:
		{
			sdb_store_attribute_t attr = {
				NULL,
				obj->parent ? obj->parent->type : 0,
				obj->parent ? obj->parent->_name : NULL,
				obj->_name,
				ATTR(obj)->value,
				obj->last_update,
				obj->interval,
				(const char * const *)obj->backends,
				obj->backends_num,
			};
			if (obj->parent && (obj->parent->type != SDB_HOST)
					&& obj->parent->parent)
				attr.hostname = obj->parent->parent->_name;
			if (! w->store_attribute)
				return -1;
			return w->store_attribute(&attr, wd);
		}
	}

	return -1;
} /* sdb_memstore_emit */

int
sdb_memstore_emit_full(sdb_memstore_obj_t *obj, sdb_memstore_matcher_t *filter,
		sdb_store_writer_t *w, sdb_object_t *wd)
{
	sdb_avltree_t *trees[] = { NULL, NULL, NULL };
	size_t i;

	if (sdb_memstore_emit(obj, w, wd))
		return -1;

	if (obj->type == SDB_HOST) {
		trees[0] = HOST(obj)->attributes;
		trees[1] = HOST(obj)->metrics;
		trees[2] = HOST(obj)->services;
	}
	else if (obj->type == SDB_SERVICE)
		trees[0] = SVC(obj)->attributes;
	else if (obj->type == SDB_METRIC)
		trees[0] = METRIC(obj)->attributes;
	else if (obj->type == SDB_ATTRIBUTE)
		return 0;
	else
		return -1;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(trees); ++i) {
		sdb_avltree_iter_t *iter;

		if (! trees[i])
			continue;

		iter = sdb_avltree_get_iter(trees[i]);
		while (sdb_avltree_iter_has_next(iter)) {
			sdb_memstore_obj_t *child;
			child = STORE_OBJ(sdb_avltree_iter_get_next(iter));

			if (filter && (! sdb_memstore_matcher_matches(filter, child, NULL)))
				continue;

			if (sdb_memstore_emit_full(child, filter, w, wd)) {
				sdb_avltree_iter_destroy(iter);
				return -1;
			}
		}
		sdb_avltree_iter_destroy(iter);
	}
	return 0;
} /* sdb_memstore_emit_full */

int
sdb_store_json_finish(sdb_store_json_formatter_t *f)
{
	if (! f)
		return -1;

	if (! f->context[0]) {
		/* no content */
		if (f->flags & SDB_WANT_ARRAY)
			sdb_strbuf_append(f->buf, "[]");
		return 0;
	}

	while (f->current > 0) {
		sdb_strbuf_append(f->buf, "}]");
		--f->current;
	}
	sdb_strbuf_append(f->buf, "}");

	if (f->flags & SDB_WANT_ARRAY)
		sdb_strbuf_append(f->buf, "]");
	return 0;
} /* sdb_store_json_finish */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

