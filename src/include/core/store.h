/*
 * SysDB - src/include/core/store.h
 * Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>
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
#include "core/timeseries.h"
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
	SDB_ATTRIBUTE,
};
#define SDB_STORE_TYPE_TO_NAME(t) \
	(((t) == SDB_HOST) ? "host" \
		: ((t) == SDB_SERVICE) ? "service" \
		: ((t) == SDB_METRIC) ? "metric" \
		: ((t) == SDB_ATTRIBUTE) ? "attribute" : "unknown")

/*
 * sdb_store_obj_t represents the super-class of any object stored in the
 * database. It inherits from sdb_object_t and may safely be cast to a generic
 * object to access its name.
 */
struct sdb_store_obj;
typedef struct sdb_store_obj sdb_store_obj_t;

/*
 * Queryable fields of a stored object.
 */
enum {
	SDB_FIELD_LAST_UPDATE = 1, /* datetime */
	SDB_FIELD_AGE,             /* datetime */
	SDB_FIELD_INTERVAL,        /* datetime */
	SDB_FIELD_BACKEND,         /* string */
};

#define SDB_FIELD_TO_NAME(f) \
	(((f) == SDB_FIELD_LAST_UPDATE) ? "last-update" \
		: ((f) == SDB_FIELD_AGE) ? "age" \
		: ((f) == SDB_FIELD_INTERVAL) ? "interval" \
		: ((f) == SDB_FIELD_BACKEND) ? "backend" : "unknown")

/*
 * sdb_store_clear:
 * Clear the entire store and remove all stored objects.
 */
void
sdb_store_clear(void);

/*
 * sdb_store_host:
 * Add/update a host in the store. If the host, identified by its
 * canonicalized name, already exists, it will be updated according to the
 * specified name and timestamp. Else, a new entry will be created in the
 * store. Any memory required for storing the entry will be allocated an
 * managed by the store itself.
 *
 * Returns:
 *  - 0 on success
 *  - a positive value if the new entry is older than the currently stored
 *    entry (in this case, no update will happen)
 *  - a negative value on error
 */
int
sdb_store_host(const char *name, sdb_time_t last_update);

/*
 * sdb_store_has_host:
 * sdb_store_get_host:
 * Query the store for a host by its (canonicalized) name.
 *
 * sdb_store_get_host increments the ref count of the host object. The caller
 * needs to deref it when no longer using it.
 */
_Bool
sdb_store_has_host(const char *name);

sdb_store_obj_t *
sdb_store_get_host(const char *name);

/*
 * sdb_store_attribute:
 * Add/update a host's attribute in the store. If the attribute, identified by
 * its key, already exists for the specified host, it will be updated to the
 * specified values. If the referenced host does not exist, an error will be
 * reported. Else, a new entry will be created in the store. Any memory
 * required for storing the entry will be allocated and managed by the store
 * itself.
 *
 * Returns:
 *  - 0 on success
 *  - a positive value if the new entry is older than the currently stored
 *    entry (in this case, no update will happen)
 *  - a negative value on error
 */
int
sdb_store_attribute(const char *hostname,
		const char *key, const sdb_data_t *value,
		sdb_time_t last_update);

/*
 * sdb_store_service:
 * Add/update a service in the store. If the service, identified by its name,
 * already exists for the specified host, it will be updated according to the
 * specified 'service' object. If the referenced host does not exist, an error
 * will be reported. Else, a new entry will be created in the store. Any
 * memory required for storing the entry will be allocated an managed by the
 * store itself.
 *
 * Returns:
 *  - 0 on success
 *  - a positive value if the new entry is older than the currently stored
 *    entry (in this case, no update will happen)
 *  - a negative value on error
 */
int
sdb_store_service(const char *hostname, const char *name,
		sdb_time_t last_update);

/*
 * sdb_store_service_attr:
 * Add/update a service's attribute in the store. If the attribute, identified
 * by its key, already exists for the specified service, it will be updated to
 * the specified value. If the references service (for the specified host)
 * does not exist, an error will be reported. Any memory required for storing
 * the entry will be allocated and managed by the store itself.
 *
 * Returns:
 *  - 0 on success
 *  - a positive value if the new entry is older than the currently stored
 *    entry (in this case, no update will happen)
 *  - a negative value on error
 */
int
sdb_store_service_attr(const char *hostname, const char *service,
		const char *key, const sdb_data_t *value, sdb_time_t last_update);

/*
 * A metric store describes how to access a metric's data.
 */
typedef struct {
	const char *type;
	const char *id;
} sdb_metric_store_t;

/*
 * sdb_store_metric:
 * Add/update a metric in the store. If the metric, identified by its name,
 * already exists for the specified host, it will be updated according to the
 * specified 'metric' object. If the referenced host does not exist, an error
 * will be reported. Else, a new entry will be created in the store. Any
 * memory required for storing the entry will be allocated an managed by the
 * store itself.
 *
 * If specified, the metric store describes where to access the metric's data.
 *
 * Returns:
 *  - 0 on success
 *  - a positive value if the new entry is older than the currently stored
 *    entry (in this case, no update will happen)
 *  - a negative value on error
 */
int
sdb_store_metric(const char *hostname, const char *name,
		sdb_metric_store_t *store, sdb_time_t last_update);

/*
 * sdb_store_metric_attr:
 * Add/update a metric's attribute in the store. If the attribute, identified
 * by its key, already exists for the specified metric, it will be updated to
 * the specified value. If the references metric (for the specified host)
 * does not exist, an error will be reported. Any memory required for storing
 * the entry will be allocated and managed by the store itself.
 *
 * Returns:
 *  - 0 on success
 *  - a positive value if the new entry is older than the currently stored
 *    entry (in this case, no update will happen)
 *  - a negative value on error
 */
int
sdb_store_metric_attr(const char *hostname, const char *metric,
		const char *key, const sdb_data_t *value, sdb_time_t last_update);

/*
 * sdb_store_fetch_timeseries:
 * Fetch the time-series described by the specified host's metric and
 * serialize it as JSON into the provided string buffer.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_store_fetch_timeseries(const char *hostname, const char *metric,
		sdb_timeseries_opts_t *opts, sdb_strbuf_t *buf);

/*
 * sdb_store_get_field:
 * Get the value of a stored object's queryable field. The caller is
 * responsible for freeing any dynamically allocated memory possibly stored in
 * the returned value.
 *
 * Note: Retrieving the backend this way is not currently supported.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_store_get_field(sdb_store_obj_t *obj, int field, sdb_data_t *res);

/*
 * Expressions specify arithmetic expressions.
 *
 * A expression object inherits from sdb_object_t and, thus, may safely be
 * cast to a generic object.
 */
struct sdb_store_expr;
typedef struct sdb_store_expr sdb_store_expr_t;
#define SDB_STORE_EXPR(obj) ((sdb_store_expr_t *)(obj))

/*
 * sdb_store_expr_create:
 * Creates an arithmetic expression implementing the specified operator on the
 * specified left and right operand.
 *
 * Returns:
 *  - an expression object on success
 *  - NULL else
 */
sdb_store_expr_t *
sdb_store_expr_create(int op, sdb_store_expr_t *left, sdb_store_expr_t *right);

/*
 * sdb_store_expr_fieldvalue:
 * Creates an expression which evaluates to the value of the specified
 * queryable field of a stored object.
 *
 * Returns:
 *  - an expression object on success
 *  - NULL else
 */
sdb_store_expr_t *
sdb_store_expr_fieldvalue(int field);

/*
 * sdb_store_expr_constvalue:
 * Creates an expression which evaluates to the specified constant value.
 *
 * Returns:
 *  - an expression object on success
 *  - NULL else
 */
sdb_store_expr_t *
sdb_store_expr_constvalue(const sdb_data_t *value);

/*
 * sdb_store_expr_eval:
 * Evaluate an expression for the specified stored object and stores the
 * result in 'res'. The result's value will be allocated dynamically if
 * necessary and, thus, should be free'd by the caller (e.g. using
 * sdb_data_free_datum). The object may be NULL, in which case the expression
 * needs to evaluate to a constant value.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_store_expr_eval(sdb_store_expr_t *expr, sdb_store_obj_t *obj,
		sdb_data_t *res);

/*
 * Conditionals may be used to lookup hosts from the store based on a
 * conditional expression.
 *
 * A conditional object inherits from sdb_object_t and, thus, may safely be
 * cast to a generic object.
 */
struct sdb_store_cond;
typedef struct sdb_store_cond sdb_store_cond_t;
#define SDB_STORE_COND(obj) ((sdb_store_cond_t *)(obj))

/*
 * sdb_store_attr_cond:
 * Creates a conditional based on attribute values. The value of stored
 * attributes is compared against the value the expression evaluates to. See
 * sdb_data_cmp for details about the comparison.
 */
sdb_store_cond_t *
sdb_store_attr_cond(const char *name, sdb_store_expr_t *expr);

/*
 * sdb_store_obj_cond:
 * Creates a conditional based on queryable object fields. The respective
 * field of *any* object type is compared against the value the expression
 * evaluates to.
 */
sdb_store_cond_t *
sdb_store_obj_cond(int field, sdb_store_expr_t *expr);

/*
 * Store matchers may be used to lookup hosts from the store based on their
 * various attributes. Service and attribute matchers are applied to a host's
 * services and attributes and evaluate to true if *any* service or attribute
 * matches.
 *
 * A store matcher object inherits from sdb_object_t and, thus, may safely be
 * cast to a generic object.
 */
struct sdb_store_matcher;
typedef struct sdb_store_matcher sdb_store_matcher_t;
#define SDB_STORE_MATCHER(obj) ((sdb_store_matcher_t *)(obj))

/*
 * sdb_store_name_matcher:
 * Creates a matcher matching by the specified object type's name. If 're' is
 * true, the specified name is treated as a POSIX extended regular expression.
 * Else, the exact name has to match (case-insensitive).
 */
sdb_store_matcher_t *
sdb_store_name_matcher(int type, const char *name, _Bool re);

/*
 * sdb_store_attr_matcher:
 * Creates a matcher matching attributes based on their value. If 're' is
 * true, the specified name is treated as a POSIX extended regular expression.
 * Else, the exact name has to match (case-insensitive).
 */
sdb_store_matcher_t *
sdb_store_attr_matcher(const char *name, const char *value, _Bool re);

/*
 * sdb_store_isnull_matcher:
 * Creates a matcher matching "missing" attributes.
 */
sdb_store_matcher_t *
sdb_store_isnull_matcher(const char *attr_name);

/*
 * sdb_store_lt_matcher, sdb_store_le_matcher, sdb_store_eq_matcher,
 * sdb_store_ge_matcher, sdb_store_gt_matcher:
 * Creates a matcher based on a conditional. The matcher matches objects for
 * which the conditional evaluates the object to compare less than, less or
 * equal, equal, greater or equal, or greater than the conditional's value
 * repsectively.
 */
sdb_store_matcher_t *
sdb_store_lt_matcher(sdb_store_cond_t *cond);
sdb_store_matcher_t *
sdb_store_le_matcher(sdb_store_cond_t *cond);
sdb_store_matcher_t *
sdb_store_eq_matcher(sdb_store_cond_t *cond);
sdb_store_matcher_t *
sdb_store_ge_matcher(sdb_store_cond_t *cond);
sdb_store_matcher_t *
sdb_store_gt_matcher(sdb_store_cond_t *cond);

/*
 * sdb_store_parse_field_name:
 * Parse the name of a stored object's queryable field.
 *
 * Returns:
 *  - the field id on success
 *  - a negative value else
 */
int
sdb_store_parse_field_name(const char *name);

/*
 * sdb_store_matcher_parse_cmp:
 * Parse a simple compare expression (<obj_type>.<attr> <op> <expression>).
 *
 * Returns:
 *  - a matcher object on success
 *  - NULL else
 */
sdb_store_matcher_t *
sdb_store_matcher_parse_cmp(const char *obj_type, const char *attr,
		const char *op, sdb_store_expr_t *expr);

/*
 * sdb_store_matcher_parse_field_cmp:
 * Parse a simple compare expression for queryable object fields (<field> <op>
 * <expression>).
 *
 * Returns:
 *  - a matcher object on success
 *  - NULL else
 */
sdb_store_matcher_t *
sdb_store_matcher_parse_field_cmp(const char *name, const char *op,
		sdb_store_expr_t *expr);

/*
 * sdb_store_dis_matcher:
 * Creates a matcher matching the disjunction (logical OR) of two matchers.
 */
sdb_store_matcher_t *
sdb_store_dis_matcher(sdb_store_matcher_t *left, sdb_store_matcher_t *right);

/*
 * sdb_store_con_matcher:
 * Creates a matcher matching the conjunction (logical AND) of two matchers.
 */
sdb_store_matcher_t *
sdb_store_con_matcher(sdb_store_matcher_t *left, sdb_store_matcher_t *right);

/*
 * sdb_store_con_matcher::
 * Creates a matcher matching the inverse (logical NOT) of a matcher.
 */
sdb_store_matcher_t *
sdb_store_inv_matcher(sdb_store_matcher_t *m);

/*
 * sdb_store_matcher_matches:
 * Check whether the specified matcher matches the specified store object. If
 * specified, the filter will be used to preselect objects for further
 * evaluation. It is applied to any object that's used during the evaluation
 * of the matcher. Only those objects matching the filter will be considered.
 *
 * Note that the filter is applied to all object types (hosts, service,
 * metric, attribute). Thus, any object-specific matchers are mostly unsuited
 * for this purpose and, if used, may result in unexpected behavior.
 *
 * Returns:
 *  - 1 if the object matches
 *  - 0 else
 */
int
sdb_store_matcher_matches(sdb_store_matcher_t *m, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter);

/*
 * sdb_store_matcher_tostring:
 * Format a matcher object as string. This is meant for logging or debugging
 * purposes.
 */
char *
sdb_store_matcher_tostring(sdb_store_matcher_t *m, char *buf, size_t buflen);

/*
 * sdb_store_lookup_cb:
 * Lookup callback. It is called for each matching object when looking up data
 * in the store. The lookup aborts if the callback returns non-zero.
 */
typedef int (*sdb_store_lookup_cb)(sdb_store_obj_t *obj, void *user_data);

/*
 * sdb_store_scan:
 * Look up objects in the store. The specified callback function is called for
 * each object in the store matching 'm'. The function performs a full scan of
 * all hosts stored in the database. If specified, the filter will be used to
 * preselect objects for further evaluation. See the description of
 * 'sdb_store_matcher_matches' for details.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_store_scan(sdb_store_matcher_t *m, sdb_store_matcher_t *filter,
		sdb_store_lookup_cb cb, void *user_data);

/*
 * Flags for serialization functions.
 *
 * By default, the full object will be included in the serialized output. When
 * specifying any of the flags, the respective information will be left out.
 */
enum {
	SDB_SKIP_ATTRIBUTES         = 1 << 0,
	SDB_SKIP_SERVICES           = 1 << 1,
	SDB_SKIP_METRICS            = 1 << 2,
	SDB_SKIP_SERVICE_ATTRIBUTES = 1 << 3,

	SDB_SKIP_ALL                = 0xffff,
};

/*
 * sdb_store_tojson:
 * Serialize the entire store to JSON and append the result to the specified
 * buffer. If specified, only objects matching the filter will be included in
 * the result (see sdb_store_host_tojson for details).
 *
 * Returns:
 *  - 0 on success
 *  - a negative value on error
 */
int
sdb_store_tojson(sdb_strbuf_t *buf, sdb_store_matcher_t *filter, int flags);

/*
 * sdb_store_host_tojson:
 * Serialize a host object to JSON and append the result to the specified
 * buffer. If specified, only objects matching the filter will be included in
 * the result. The filter is applied to each object individually and, thus,
 * should not be of any object-type specific kind. The filter is never applied
 * to the specified host object; the caller is responsible for this and for
 * correctly handling skipped hosts.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value on error
 */
int
sdb_store_host_tojson(sdb_store_obj_t *host, sdb_strbuf_t *buf,
		sdb_store_matcher_t *filter, int flags);

/*
 * sdb_store_iter_cb:
 * Store iterator callback. Iteration stops if the callback returns non-zero.
 */
typedef int (*sdb_store_iter_cb)(sdb_store_obj_t *obj, void *user_data);

/*
 * sdb_store_iterate:
 * Iterate the entire store, calling the specified callback for each object.
 * The user_data pointer is passed on to each call of the callback.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_store_iterate(sdb_store_iter_cb cb, void *user_data);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_CORE_STORE_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

