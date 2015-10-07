/*
 * SysDB - src/include/core/store.h
 * Copyright (C) 2012-2015 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#ifndef SDB_CORE_STORE_H
#define SDB_CORE_STORE_H 1

#include "sysdb.h"
#include "core/object.h"
#include "core/data.h"
#include "core/time.h"
#include "parser/ast.h"
#include "utils/strbuf.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Store object types.
 */
enum {
	SDB_HOST = 1,
	SDB_SERVICE,
	SDB_METRIC,

	SDB_ATTRIBUTE = 1 << 4,

	/*
	 * Queryable fields of a stored object.
	 */
	SDB_FIELD_NAME = 1 << 8, /* type: string */
	SDB_FIELD_LAST_UPDATE,   /* type: datetime */
	SDB_FIELD_AGE,           /* type: datetime */
	SDB_FIELD_INTERVAL,      /* type: datetime */
	SDB_FIELD_BACKEND,       /* type: array of strings */
	SDB_FIELD_VALUE,         /* attributes only;  type: type of the value */
	SDB_FIELD_TIMESERIES,    /* metrics only;  type: boolean */
};
#define SDB_STORE_TYPE_TO_NAME(t) \
	(((t) == SDB_HOST) ? "host" \
		: ((t) == SDB_SERVICE) ? "service" \
		: ((t) == SDB_METRIC) ? "metric" \
		: ((t) == SDB_ATTRIBUTE) ? "attribute" \
		: ((t) == (SDB_ATTRIBUTE | SDB_HOST)) ? "host attribute" \
		: ((t) == (SDB_ATTRIBUTE | SDB_SERVICE)) ? "service attribute" \
		: ((t) == (SDB_ATTRIBUTE | SDB_METRIC)) ? "metric attribute" \
		: "unknown")

#define SDB_FIELD_TO_NAME(f) \
	(((f) == SDB_FIELD_NAME) ? "name" \
		: ((f) == SDB_FIELD_LAST_UPDATE) ? "last-update" \
		: ((f) == SDB_FIELD_AGE) ? "age" \
		: ((f) == SDB_FIELD_INTERVAL) ? "interval" \
		: ((f) == SDB_FIELD_BACKEND) ? "backend" \
		: ((f) == SDB_FIELD_VALUE) ? "value" \
		: ((f) == SDB_FIELD_TIMESERIES) ? "timeseries" \
		: "unknown")

#define SDB_FIELD_TYPE(f) \
	(((f) == SDB_FIELD_NAME) ? SDB_TYPE_STRING \
		: ((f) == SDB_FIELD_LAST_UPDATE) ? SDB_TYPE_DATETIME \
		: ((f) == SDB_FIELD_AGE) ? SDB_TYPE_DATETIME \
		: ((f) == SDB_FIELD_INTERVAL) ? SDB_TYPE_DATETIME \
		: ((f) == SDB_FIELD_BACKEND) ? (SDB_TYPE_ARRAY | SDB_TYPE_STRING) \
		: ((f) == SDB_FIELD_VALUE) ? -1 /* unknown */ \
		: ((f) == SDB_FIELD_TIMESERIES) ? SDB_TYPE_BOOLEAN \
		: -1)

/*
 * sdb_store_host_t represents the meta-data of a stored host object.
 */
typedef struct {
	const char *name;

	sdb_time_t last_update;
	sdb_time_t interval;
	const char * const *backends;
	size_t backends_num;
} sdb_store_host_t;
#define SDB_STORE_HOST_INIT { NULL, 0, 0, NULL, 0 }

/*
 * sdb_store_service_t represents the meta-data of a stored service object.
 */
typedef struct {
	const char *hostname;
	const char *name;

	sdb_time_t last_update;
	sdb_time_t interval;
	const char * const *backends;
	size_t backends_num;
} sdb_store_service_t;
#define SDB_STORE_SERVICE_INIT { NULL, NULL, 0, 0, NULL, 0 }

/*
 * sdb_metric_store_t specifies how to access a metric's data.
 */
typedef struct {
	const char *type;
	const char *id;
} sdb_metric_store_t;

/*
 * sdb_store_metric_t represents the meta-data of a stored metric object.
 */
typedef struct {
	const char *hostname;
	const char *name;
	struct {
		const char *type;
		const char *id;
	} store;

	sdb_time_t last_update;
	sdb_time_t interval;
	const char * const *backends;
	size_t backends_num;
} sdb_store_metric_t;
#define SDB_STORE_METRIC_INIT { NULL, NULL, { NULL, NULL }, 0, 0, NULL, 0 }

/*
 * sdb_store_attribute_t represents a stored attribute.
 */
typedef struct {
	const char *hostname; /* optional */
	int parent_type;
	const char *parent;
	const char *key;
	sdb_data_t value;

	sdb_time_t last_update;
	sdb_time_t interval;
	const char * const *backends;
	size_t backends_num;
} sdb_store_attribute_t;
#define SDB_STORE_ATTRIBUTE_INIT { NULL, 0, NULL, NULL, SDB_DATA_INIT, 0, 0, NULL, 0 }

/*
 * A JSON formatter converts stored objects into the JSON format.
 * See http://www.ietf.org/rfc/rfc4627.txt
 *
 * A JSON formatter object inherits from sdb_object_t and, thus, may safely be
 * cast to a generic object.
 */
struct sdb_store_json_formatter;
typedef struct sdb_store_json_formatter sdb_store_json_formatter_t;

/*
 * A store writer describes the interface for plugins implementing a store.
 *
 * Any of the call-back functions shall return:
 *  - 0 on success
 *  - a positive value if the new entry is older than the currently stored
 *    entry (in this case, no update will happen)
 *  - a negative value on error
 */
typedef struct {
	/*
	 * store_host:
	 * Add/update a host in the store. If the host, identified by its
	 * canonicalized name, already exists, it will be updated according to the
	 * specified name and timestamp. Else, a new entry will be created in the
	 * store.
	 */
	int (*store_host)(sdb_store_host_t *host, sdb_object_t *user_data);

	/*
	 * store_service:
	 * Add/update a service in the store. If the service, identified by its
	 * name, already exists for the specified host, it will be updated
	 * according to the specified name and timestamp. If the referenced host
	 * does not exist, an error will be reported. Else, a new entry will be
	 * created in the store.
	 */
	int (*store_service)(sdb_store_service_t *service, sdb_object_t *user_data);

	/*
	 * store_metric:
	 * Add/update a metric in the store. If the metric, identified by its
	 * name, already exists for the specified host, it will be updated
	 * according to the specified attributes. If the referenced host does not
	 * exist, an error will be reported. Else, a new entry will be created in
	 * the store.
	 */
	int (*store_metric)(sdb_store_metric_t *metric, sdb_object_t *user_data);

	/*
	 * store_attribute:
	 * Add/update a host's attribute in the store. If the attribute,
	 * identified by its key, already exists for the specified host, it will
	 * be updated to the specified values. If the referenced host does not
	 * exist, an error will be reported. Else, a new entry will be created in
	 * the store.
	 */
	int (*store_attribute)(sdb_store_attribute_t *attr, sdb_object_t *user_data);
} sdb_store_writer_t;

/*
 * A store reader describes the interface to query a store implementation.
 */
typedef struct {
	/*
	 * prepare_query:
	 * Prepare the query described by 'ast' for execution.
	 */
	sdb_object_t *(*prepare_query)(sdb_ast_node_t *ast,
			sdb_strbuf_t *errbuf, sdb_object_t *user_data);

	/*
	 * execute_query:
	 * Execute a previously prepared query. The callback may expect that only
	 * queries prepared by its respective prepare callback will be passed to
	 * this function. The query result will be passed back via the specified
	 * store writer.
	 */
	int (*execute_query)(sdb_object_t *q,
			sdb_store_writer_t *w, sdb_object_t *wd,
			sdb_strbuf_t *errbuf, sdb_object_t *user_data);
} sdb_store_reader_t;

/*
 * Flags for JSON formatting.
 */
enum {
	SDB_WANT_ARRAY = 1 << 0,
};

/*
 * sdb_store_json_formatter:
 * Create a JSON formatter for the specified object types writing to the
 * specified buffer.
 */
sdb_store_json_formatter_t *
sdb_store_json_formatter(sdb_strbuf_t *buf, int type, int flags);

/*
 * sdb_store_json_finish:
 * Finish the JSON output. This function has to be called once after emiting
 * all objects.
 */
int
sdb_store_json_finish(sdb_store_json_formatter_t *f);

/*
 * sdb_store_json_writer:
 * A store writer implementation that generates JSON output. It expects a
 * store JSON formatter as its user-data argument.
 */
extern sdb_store_writer_t sdb_store_json_writer;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_CORE_STORE_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

