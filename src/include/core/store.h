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
#include "parser/ast.h"
#include "utils/strbuf.h"

#include <stdbool.h>
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
 * sdb_store_t represents an in-memory store. It inherits from sdb_object_t
 * and may safely be case to a generic object.
 */
struct sdb_store;
typedef struct sdb_store sdb_store_t;
#define SDB_STORE(obj) ((sdb_store_t *)(obj))

/*
 * sdb_store_obj_t represents the super-class of any object stored in the
 * database. It inherits from sdb_object_t and may safely be cast to a generic
 * object to access its name.
 */
struct sdb_store_obj;
typedef struct sdb_store_obj sdb_store_obj_t;

/*
 * A metric store describes how to access a metric's data.
 */
typedef struct {
	const char *type;
	const char *id;
} sdb_metric_store_t;

/*
 * Expressions represent arithmetic expressions based on stored objects and
 * their various attributes.
 *
 * An expression object inherits from sdb_object_t and, thus, may safely be
 * cast to a generic object.
 */
struct sdb_store_expr;
typedef struct sdb_store_expr sdb_store_expr_t;
#define SDB_STORE_EXPR(obj) ((sdb_store_expr_t *)(obj))

/*
 * An expression iterator iterates over the values of an iterable expression
 * (see sdb_store_expr_iterable).
 */
struct sdb_store_expr_iter;
typedef struct sdb_store_expr_iter sdb_store_expr_iter_t;

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
	int (*store_host)(const char *name, sdb_time_t last_update,
			sdb_object_t *user_data);

	/*
	 * store_service:
	 * Add/update a service in the store. If the service, identified by its
	 * name, already exists for the specified host, it will be updated
	 * according to the specified name and timestamp. If the referenced host
	 * does not exist, an error will be reported. Else, a new entry will be
	 * created in the store.
	 */
	int (*store_service)(const char *hostname, const char *name,
			sdb_time_t last_update, sdb_object_t *user_data);

	/*
	 * store_metric:
	 * Add/update a metric in the store. If the metric, identified by its
	 * name, already exists for the specified host, it will be updated
	 * according to the specified attributes. If the referenced host does not
	 * exist, an error will be reported. Else, a new entry will be created in
	 * the store.
	 */
	int (*store_metric)(const char *hostname, const char *name,
			sdb_metric_store_t *store, sdb_time_t last_update,
			sdb_object_t *user_data);

	/*
	 * store_attribute:
	 * Add/update a host's attribute in the store. If the attribute,
	 * identified by its key, already exists for the specified host, it will
	 * be updated to the specified values. If the referenced host does not
	 * exist, an error will be reported. Else, a new entry will be created in
	 * the store.
	 */
	int (*store_attribute)(const char *hostname,
			const char *key, const sdb_data_t *value, sdb_time_t last_update,
			sdb_object_t *user_data);

	/*
	 * store_service_attr:
	 * Add/update a service's attribute in the store. If the attribute,
	 * identified by its key, already exists for the specified service, it
	 * will be updated to the specified value. If the references service (for
	 * the specified host) does not exist, an error will be reported.
	 */
	int (*store_service_attr)(const char *hostname, const char *service,
			const char *key, const sdb_data_t *value, sdb_time_t last_update,
			sdb_object_t *user_data);

	/*
	 * store_metric_attr:
	 * Add/update a metric's attribute in the store. If the attribute,
	 * identified by its key, already exists for the specified metric, it will
	 * be updated to the specified value. If the references metric (for the
	 * specified host) does not exist, an error will be reported.
	 */
	int (*store_metric_attr)(const char *hostname, const char *metric,
			const char *key, const sdb_data_t *value, sdb_time_t last_update,
			sdb_object_t *user_data);
} sdb_store_writer_t;

/*
 * sdb_store_writer:
 * A store writer implementation that provides an in-memory object store. It
 * expects a store object as its user-data argument.
 */
extern sdb_store_writer_t sdb_store_writer;

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
	 * this function.
	 *
	 * TODO: Instead of letting the executor write directly to a string buffer
	 *       (which cannot easily be merged with other results), let it hand
	 *       all objects to a store-writer.
	 */
	int (*execute_query)(sdb_object_t *q,
			sdb_strbuf_t *buf, sdb_strbuf_t *errbuf,
			sdb_object_t *user_data);
} sdb_store_reader_t;

/*
 * sdb_store_reader:
 * A store reader implementation that uses an in-memory object store. It
 * expects a store object as its user-data argument.
 */
extern sdb_store_reader_t sdb_store_reader;

/*
 * sdb_store_create:
 * Allocate a new in-memory store.
 */
sdb_store_t *
sdb_store_create(void);

/*
 * sdb_store_init:
 * Initialize the store sub-system. This function has to be called before
 * doing any other store operations.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_store_init(void);

/*
 * sdb_store_clear:
 * Clear the entire store and remove all stored objects.
 */
void
sdb_store_clear(void);

/*
 * sdb_store_host, sdb_store_service, sdb_store_metric, sdb_store_attribute,
 * sdb_store_metric_attr:
 * Store an object in the specified store.
 */
int
sdb_store_host(sdb_store_t *store, const char *name, sdb_time_t last_update);
int
sdb_store_service(sdb_store_t *store, const char *hostname, const char *name,
		sdb_time_t last_update);
int
sdb_store_metric(sdb_store_t *store, const char *hostname, const char *name,
		sdb_metric_store_t *metric_store, sdb_time_t last_update);
int
sdb_store_attribute(sdb_store_t *store, const char *hostname,
		const char *key, const sdb_data_t *value, sdb_time_t last_update);
int
sdb_store_service_attr(sdb_store_t *store, const char *hostname,
		const char *service, const char *key, const sdb_data_t *value,
		sdb_time_t last_update);
int
sdb_store_metric_attr(sdb_store_t *store, const char *hostname,
		const char *metric, const char *key, const sdb_data_t *value,
		sdb_time_t last_update);

/*
 * sdb_store_get_host:
 * Query the store for a host by its (canonicalized) name.
 *
 * The function increments the ref count of the host object. The caller needs
 * to deref it when no longer using it.
 */
sdb_store_obj_t *
sdb_store_get_host(const char *name);

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
 * sdb_store_get_child:
 * Retrieve a host's child object of the specified type and name. The
 * reference count of the child object will be incremented before returning
 * it. The caller is responsible for releasing the object once it's no longer
 * used.
 *
 * Returns:
 *  - the child object on success
 *  - NULL else
 */
sdb_store_obj_t *
sdb_store_get_child(sdb_store_obj_t *host, int type, const char *name);

/*
 * sdb_store_get_field:
 * Get the value of a stored object's queryable field. The caller is
 * responsible for freeing any dynamically allocated memory possibly stored in
 * the returned value. If 'res' is NULL, the function will return whether the
 * field exists.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_store_get_field(sdb_store_obj_t *obj, int field, sdb_data_t *res);

/*
 * sdb_store_get_attr:
 * Get the value of a stored object's attribute. The caller is responsible for
 * freeing any dynamically allocated memory possibly stored in the returned
 * value. If 'res' is NULL, the function will return whether the attribute
 * exists. If specified, only attributes matching the filter will be
 * considered.
 *
 * Returns:
 *  - 0 if the attribute exists
 *  - a negative value else
 */
int
sdb_store_get_attr(sdb_store_obj_t *obj, const char *name, sdb_data_t *res,
		sdb_store_matcher_t *filter);

/*
 * Querying a store:
 *
 *  - Query interface: A query is a formal description of an interaction with
 *    the store. It can be used, both, for read and write access. The query is
 *    described by its abstract syntax tree (AST). The parser package provides
 *    means to parse a string (SysQL) representation of the query into an AST.
 *
 *  - Matcher / expression interface: This low-level interface provides direct
 *    control over how to access the store. It is used by the query
 *    implementation internally and can only be used for read access.
 */

/*
 * sdb_store_query_t:
 * A parsed query readily prepared for execution.
 */
struct sdb_store_query;
typedef struct sdb_store_query sdb_store_query_t;

/*
 * sdb_store_query_prepare:
 * Prepare the query described by 'ast' for execution in a store.
 *
 * Returns:
 *  - a store query on success
 *  - NULL else
 */
sdb_store_query_t *
sdb_store_query_prepare(sdb_ast_node_t *ast);

/*
 * sdb_store_query_prepare_matcher:
 * Prepare the logical expression described by 'ast' for execution as a store
 * matcher.
 *
 * Returns:
 *  - a matcher on success
 *  - NULL else
 */
sdb_store_matcher_t *
sdb_store_query_prepare_matcher(sdb_ast_node_t *ast);

/*
 * sdb_store_query_execute:
 * Execute a previously prepared query. The query result will be written to
 * 'buf' and any errors to 'errbuf'.
 *
 * Returns:
 *  - the result type (to be used by the server reply)
 *  - a negative value on error
 */
int
sdb_store_query_execute(sdb_store_query_t *m,
		sdb_strbuf_t *buf, sdb_strbuf_t *errbuf);

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
 * sdb_store_expr_typed:
 * Creates an expression which evaluates in the context of an object's sibling
 * as specified by the given type.
 *
 * Returns:
 *  - an expression object on success
 *  - NULL else
 */
sdb_store_expr_t *
sdb_store_expr_typed(int typ, sdb_store_expr_t *expr);

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
 * sdb_store_expr_attrvalue:
 * Creates an expression which evaluates to the value of the specified
 * attribute of a stored object. Evaluates to a NULL value if the attribute
 * does not exist.
 *
 * Returns:
 *  - an expression object on success
 *  - NULL else
 */
sdb_store_expr_t *
sdb_store_expr_attrvalue(const char *name);

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
 * needs to evaluate to a constant value. If specified, only objects matching
 * the filter will be used during the evaluation.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_store_expr_eval(sdb_store_expr_t *expr, sdb_store_obj_t *obj,
		sdb_data_t *res, sdb_store_matcher_t *filter);

/*
 * sdb_store_expr_iterable:
 * Check whether an expression, evaluated in the specified context (HOST,
 * SERVICE, METRIC) is iterable, that is, if it may evaluate to multiple
 * values.
 */
bool
sdb_store_expr_iterable(sdb_store_expr_t *expr, int context);

/*
 * sdb_store_expr_iter:
 * Iterate over the elements of an iterable expression. sdb_store_expr_iter
 * returns NULL if the expression is not iterable (for the specified object).
 * See also sdb_store_expr_iterable.
 *
 * sdb_store_expr_iter_get_next returns NULL if there is no next element.
 */
sdb_store_expr_iter_t *
sdb_store_expr_iter(sdb_store_expr_t *expr, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter);
void
sdb_store_expr_iter_destroy(sdb_store_expr_iter_t *iter);

bool
sdb_store_expr_iter_has_next(sdb_store_expr_iter_t *iter);
sdb_data_t
sdb_store_expr_iter_get_next(sdb_store_expr_iter_t *iter);

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
 * sdb_store_inv_matcher::
 * Creates a matcher matching the inverse (logical NOT) of a matcher.
 */
sdb_store_matcher_t *
sdb_store_inv_matcher(sdb_store_matcher_t *m);

/*
 * sdb_store_any_matcher:
 * Creates a matcher iterating over values of the first expression (which has
 * to be iterable). It matches if *any* of those elements match 'm'. 'm' has
 * to be an ary operation with the left operand unset.
 */
sdb_store_matcher_t *
sdb_store_any_matcher(sdb_store_expr_t *iter, sdb_store_matcher_t *m);

/*
 * sdb_store_all_matcher:
 * Creates a matcher iterating over values of the first expression (which has
 * to be iterable). It matches if *all* of those elements match 'm'. 'm' has
 * to be an ary operation with the left operand unset.
 */
sdb_store_matcher_t *
sdb_store_all_matcher(sdb_store_expr_t *iter, sdb_store_matcher_t *m);

/*
 * sdb_store_in_matcher:
 * Creates a matcher which matches if the right value evaluates to an array
 * value and the left value is included in that array. See sdb_data_inarray
 * for more details.
 */
sdb_store_matcher_t *
sdb_store_in_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right);

/*
 * sdb_store_lt_matcher, sdb_store_le_matcher, sdb_store_eq_matcher,
 * sdb_store_ge_matcher, sdb_store_gt_matcher:
 * Create conditional matchers comparing the values of two expressions. The
 * matcher matches if the left expression compres less than, less or equal
 * than, equal to, not equal to, greater or equal than, or greater than the
 * right expression.
 */
sdb_store_matcher_t *
sdb_store_lt_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right);
sdb_store_matcher_t *
sdb_store_le_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right);
sdb_store_matcher_t *
sdb_store_eq_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right);
sdb_store_matcher_t *
sdb_store_ne_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right);
sdb_store_matcher_t *
sdb_store_ge_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right);
sdb_store_matcher_t *
sdb_store_gt_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right);

/*
 * sdb_store_regex_matcher:
 * Creates a matcher which matches the string value the left expression
 * evaluates to against the regular expression the right expression evaluates
 * to. The right expression may either be a constant value regular expression
 * or string or a dynamic value evaluating to a string. In the latter case,
 * the string is compiled to a regex every time the matcher is executed.
 */
sdb_store_matcher_t *
sdb_store_regex_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right);

/*
 * sdb_store_nregex_matcher:
 * Creates a regex matcher just like sdb_store_regex_matcher except that it
 * matches in case the regular expression does not match.
 */
sdb_store_matcher_t *
sdb_store_nregex_matcher(sdb_store_expr_t *left, sdb_store_expr_t *right);

/*
 * sdb_store_isnull_matcher:
 * Creates a matcher matching NULL values.
 */
sdb_store_matcher_t *
sdb_store_isnull_matcher(sdb_store_expr_t *expr);

/*
 * sdb_store_istrue_matcher, sdb_store_isfalse_matcher:
 * Creates a matcher matching boolean values.
 */
sdb_store_matcher_t *
sdb_store_istrue_matcher(sdb_store_expr_t *expr);
sdb_store_matcher_t *
sdb_store_isfalse_matcher(sdb_store_expr_t *expr);

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
 * sdb_store_matcher_op_cb:
 * Callback constructing a matcher operator.
 */
typedef sdb_store_matcher_t *(*sdb_store_matcher_op_cb)
	(sdb_store_expr_t *, sdb_store_expr_t *);

/*
 * sdb_store_lookup_cb:
 * Lookup callback. It is called for each matching object when looking up data
 * in the store passing on the lookup filter and the specified user-data. The
 * lookup aborts early if the callback returns non-zero.
 */
typedef int (*sdb_store_lookup_cb)(sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter, void *user_data);

/*
 * sdb_store_scan:
 * Look up objects of the specified type in the store. The specified callback
 * function is called for each object in the store matching 'm'. The function
 * performs a full scan of all objects stored in the database. If specified,
 * the filter will be used to preselect objects for further evaluation. See
 * the description of 'sdb_store_matcher_matches' for details.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_store_scan(int type, sdb_store_matcher_t *m, sdb_store_matcher_t *filter,
		sdb_store_lookup_cb cb, void *user_data);

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
 * sdb_store_json_emit:
 * Serialize a single object to JSON adding it to the string buffer associated
 * with the formatter object. The serialized object will not include
 * attributes or any child objects. Instead, call the function again for each
 * of those objects. All attributes have to be emitted before any other
 * children types. Use sdb_store_json_emit_full() to emit a full (filtered)
 * object.
 *
 * Note that the output might not be valid JSON before calling
 * sdb_store_json_finish().
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_store_json_emit(sdb_store_json_formatter_t *f, sdb_store_obj_t *obj);

/*
 * sdb_store_json_emit_full:
 * Serialize a single object including it's attributes and all children to
 * JSON, adding it to the string buffer associated with the formatter object.
 * The filter, if specified, is applied to each attribute and child object.
 * Only matching objects will be included in the output.
 *
 * Note that the output might not be valid JSON before calling
 * sdb_store_json_finish().
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_store_json_emit_full(sdb_store_json_formatter_t *f, sdb_store_obj_t *obj,
		sdb_store_matcher_t *filter);

/*
 * sdb_store_json_finish:
 * Finish the JSON output. This function has to be called once after emiting
 * all objects.
 */
int
sdb_store_json_finish(sdb_store_json_formatter_t *f);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_CORE_STORE_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

