/*
 * SysDB - src/include/core/memstore.h
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

#ifndef SDB_CORE_MEMSTORE_H
#define SDB_CORE_MEMSTORE_H 1

#include "sysdb.h"
#include "core/object.h"
#include "core/data.h"
#include "core/store.h"
#include "core/time.h"
#include "parser/ast.h"
#include "utils/strbuf.h"

#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sdb_memstore_t represents an in-memory store. It inherits from sdb_object_t
 * and may safely be case to a generic object.
 */
struct sdb_memstore;
typedef struct sdb_memstore sdb_memstore_t;
#define SDB_MEMSTORE(obj) ((sdb_memstore_t *)(obj))

/*
 * sdb_memstore_obj_t represents the super-class of any stored object. It
 * inherits from sdb_object_t and may safely be cast to a generic object to
 * access its name.
 */
struct sdb_memstore_obj;
typedef struct sdb_memstore_obj sdb_memstore_obj_t;

/*
 * Expressions represent arithmetic expressions based on stored objects and
 * their various attributes.
 *
 * An expression object inherits from sdb_object_t and, thus, may safely be
 * cast to a generic object.
 */
struct sdb_memstore_expr;
typedef struct sdb_memstore_expr sdb_memstore_expr_t;
#define SDB_MEMSTORE_EXPR(obj) ((sdb_memstore_expr_t *)(obj))

/*
 * An expression iterator iterates over the values of an iterable expression.
 */
struct sdb_memstore_expr_iter;
typedef struct sdb_memstore_expr_iter sdb_memstore_expr_iter_t;

/*
 * Store matchers may be used to lookup hosts from the store based on their
 * various attributes. Service and attribute matchers are applied to a host's
 * services and attributes and evaluate to true if *any* service or attribute
 * matches.
 *
 * A store matcher object inherits from sdb_object_t and, thus, may safely be
 * cast to a generic object.
 */
struct sdb_memstore_matcher;
typedef struct sdb_memstore_matcher sdb_memstore_matcher_t;
#define SDB_MEMSTORE_MATCHER(obj) ((sdb_memstore_matcher_t *)(obj))

/*
 * sdb_memstore_writer:
 * A store writer implementation that provides an in-memory object store. It
 * expects a store object as its user-data argument.
 */
extern sdb_store_writer_t sdb_memstore_writer;

/*
 * sdb_memstore_reader:
 * A store reader implementation that uses an in-memory object store. It
 * expects a store object as its user-data argument.
 */
extern sdb_store_reader_t sdb_memstore_reader;

/*
 * sdb_memstore_create:
 * Allocate a new in-memory store.
 */
sdb_memstore_t *
sdb_memstore_create(void);

/*
 * sdb_memstore_host, sdb_memstore_service, sdb_memstore_metric,
 * sdb_memstore_attribute, sdb_memstore_metric_attr:
 * Store an object in the specified store. The hostname is expected to be
 * canonical.
 */
int
sdb_memstore_host(sdb_memstore_t *store, const char *name,
		sdb_time_t last_update, sdb_time_t interval);
int
sdb_memstore_service(sdb_memstore_t *store, const char *hostname, const char *name,
		sdb_time_t last_update, sdb_time_t interval);
int
sdb_memstore_metric(sdb_memstore_t *store, const char *hostname, const char *name,
		sdb_metric_store_t *metric_store,
		sdb_time_t last_update, sdb_time_t interval);
int
sdb_memstore_attribute(sdb_memstore_t *store, const char *hostname,
		const char *key, const sdb_data_t *value,
		sdb_time_t last_update, sdb_time_t interval);
int
sdb_memstore_service_attr(sdb_memstore_t *store, const char *hostname,
		const char *service, const char *key, const sdb_data_t *value,
		sdb_time_t last_update, sdb_time_t interval);
int
sdb_memstore_metric_attr(sdb_memstore_t *store, const char *hostname,
		const char *metric, const char *key, const sdb_data_t *value,
		sdb_time_t last_update, sdb_time_t interval);

/*
 * sdb_memstore_get_host:
 * Query the specified store for a host by its (canonicalized) name.
 *
 * The function increments the ref count of the host object. The caller needs
 * to deref it when no longer using it.
 */
sdb_memstore_obj_t *
sdb_memstore_get_host(sdb_memstore_t *store, const char *name);

/*
 * sdb_memstore_get_child:
 * Retrieve an object's child object of the specified type and name. The
 * reference count of the child object will be incremented before returning
 * it. The caller is responsible for releasing the object once it's no longer
 * used.
 *
 * Returns:
 *  - the child object on success
 *  - NULL else
 */
sdb_memstore_obj_t *
sdb_memstore_get_child(sdb_memstore_obj_t *obj, int type, const char *name);

/*
 * sdb_memstore_get_field:
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
sdb_memstore_get_field(sdb_memstore_obj_t *obj, int field, sdb_data_t *res);

/*
 * sdb_memstore_get_attr:
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
sdb_memstore_get_attr(sdb_memstore_obj_t *obj, const char *name, sdb_data_t *res,
		sdb_memstore_matcher_t *filter);

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
 * sdb_memstore_query_t:
 * A parsed query readily prepared for execution.
 */
struct sdb_memstore_query;
typedef struct sdb_memstore_query sdb_memstore_query_t;

/*
 * sdb_memstore_query_prepare:
 * Prepare the query described by 'ast' for execution in a store.
 *
 * Returns:
 *  - a store query on success
 *  - NULL else
 */
sdb_memstore_query_t *
sdb_memstore_query_prepare(sdb_ast_node_t *ast);

/*
 * sdb_memstore_query_prepare_matcher:
 * Prepare the logical expression described by 'ast' for execution as a store
 * matcher.
 *
 * Returns:
 *  - a matcher on success
 *  - NULL else
 */
sdb_memstore_matcher_t *
sdb_memstore_query_prepare_matcher(sdb_ast_node_t *ast);

/*
 * sdb_memstore_query_execute:
 * Execute a previously prepared query in the specified store. The query
 * result will be written to 'buf' and any errors to 'errbuf'.
 *
 * Returns:
 *  - the result type (to be used by the server reply)
 *  - a negative value on error
 */
int
sdb_memstore_query_execute(sdb_memstore_t *store, sdb_memstore_query_t *m,
		sdb_store_writer_t *w, sdb_object_t *wd, sdb_strbuf_t *errbuf);

/*
 * sdb_memstore_expr_create:
 * Creates an arithmetic expression implementing the specified operator on the
 * specified left and right operand.
 *
 * Returns:
 *  - an expression object on success
 *  - NULL else
 */
sdb_memstore_expr_t *
sdb_memstore_expr_create(int op,
		sdb_memstore_expr_t *left, sdb_memstore_expr_t *right);

/*
 * sdb_memstore_expr_typed:
 * Creates an expression which evaluates in the context of an object's sibling
 * as specified by the given type.
 *
 * Returns:
 *  - an expression object on success
 *  - NULL else
 */
sdb_memstore_expr_t *
sdb_memstore_expr_typed(int typ, sdb_memstore_expr_t *expr);

/*
 * sdb_memstore_expr_fieldvalue:
 * Creates an expression which evaluates to the value of the specified
 * queryable field of a stored object.
 *
 * Returns:
 *  - an expression object on success
 *  - NULL else
 */
sdb_memstore_expr_t *
sdb_memstore_expr_fieldvalue(int field);

/*
 * sdb_memstore_expr_attrvalue:
 * Creates an expression which evaluates to the value of the specified
 * attribute of a stored object. Evaluates to a NULL value if the attribute
 * does not exist.
 *
 * Returns:
 *  - an expression object on success
 *  - NULL else
 */
sdb_memstore_expr_t *
sdb_memstore_expr_attrvalue(const char *name);

/*
 * sdb_memstore_expr_constvalue:
 * Creates an expression which evaluates to the specified constant value.
 *
 * Returns:
 *  - an expression object on success
 *  - NULL else
 */
sdb_memstore_expr_t *
sdb_memstore_expr_constvalue(const sdb_data_t *value);

/*
 * sdb_memstore_expr_eval:
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
sdb_memstore_expr_eval(sdb_memstore_expr_t *expr, sdb_memstore_obj_t *obj,
		sdb_data_t *res, sdb_memstore_matcher_t *filter);

/*
 * sdb_memstore_expr_iter:
 * Iterate over the elements of an iterable expression. sdb_memstore_expr_iter
 * returns NULL if the expression is not iterable (for the specified object).
 *
 * sdb_memstore_expr_iter_get_next returns NULL if there is no next element.
 */
sdb_memstore_expr_iter_t *
sdb_memstore_expr_iter(sdb_memstore_expr_t *expr, sdb_memstore_obj_t *obj,
		sdb_memstore_matcher_t *filter);
void
sdb_memstore_expr_iter_destroy(sdb_memstore_expr_iter_t *iter);

bool
sdb_memstore_expr_iter_has_next(sdb_memstore_expr_iter_t *iter);
sdb_data_t
sdb_memstore_expr_iter_get_next(sdb_memstore_expr_iter_t *iter);

/*
 * sdb_memstore_dis_matcher:
 * Creates a matcher matching the disjunction (logical OR) of two matchers.
 */
sdb_memstore_matcher_t *
sdb_memstore_dis_matcher(sdb_memstore_matcher_t *left, sdb_memstore_matcher_t *right);

/*
 * sdb_memstore_con_matcher:
 * Creates a matcher matching the conjunction (logical AND) of two matchers.
 */
sdb_memstore_matcher_t *
sdb_memstore_con_matcher(sdb_memstore_matcher_t *left, sdb_memstore_matcher_t *right);

/*
 * sdb_memstore_inv_matcher:
 * Creates a matcher matching the inverse (logical NOT) of a matcher.
 */
sdb_memstore_matcher_t *
sdb_memstore_inv_matcher(sdb_memstore_matcher_t *m);

/*
 * sdb_memstore_any_matcher:
 * Creates a matcher iterating over values of the first expression (which has
 * to be iterable). It matches if *any* of those elements match 'm'. 'm' has
 * to be an ary operation with the left operand unset.
 */
sdb_memstore_matcher_t *
sdb_memstore_any_matcher(sdb_memstore_expr_t *iter, sdb_memstore_matcher_t *m);

/*
 * sdb_memstore_all_matcher:
 * Creates a matcher iterating over values of the first expression (which has
 * to be iterable). It matches if *all* of those elements match 'm'. 'm' has
 * to be an ary operation with the left operand unset.
 */
sdb_memstore_matcher_t *
sdb_memstore_all_matcher(sdb_memstore_expr_t *iter, sdb_memstore_matcher_t *m);

/*
 * sdb_memstore_in_matcher:
 * Creates a matcher which matches if the right value evaluates to an array
 * value and the left value is included in that array. See sdb_data_inarray
 * for more details.
 */
sdb_memstore_matcher_t *
sdb_memstore_in_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right);

/*
 * sdb_memstore_lt_matcher, sdb_memstore_le_matcher, sdb_memstore_eq_matcher,
 * sdb_memstore_ge_matcher, sdb_memstore_gt_matcher:
 * Create conditional matchers comparing the values of two expressions. The
 * matcher matches if the left expression compres less than, less or equal
 * than, equal to, not equal to, greater or equal than, or greater than the
 * right expression.
 */
sdb_memstore_matcher_t *
sdb_memstore_lt_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right);
sdb_memstore_matcher_t *
sdb_memstore_le_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right);
sdb_memstore_matcher_t *
sdb_memstore_eq_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right);
sdb_memstore_matcher_t *
sdb_memstore_ne_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right);
sdb_memstore_matcher_t *
sdb_memstore_ge_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right);
sdb_memstore_matcher_t *
sdb_memstore_gt_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right);

/*
 * sdb_memstore_regex_matcher:
 * Creates a matcher which matches the string value the left expression
 * evaluates to against the regular expression the right expression evaluates
 * to. The right expression may either be a constant value regular expression
 * or string or a dynamic value evaluating to a string. In the latter case,
 * the string is compiled to a regex every time the matcher is executed.
 */
sdb_memstore_matcher_t *
sdb_memstore_regex_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right);

/*
 * sdb_memstore_nregex_matcher:
 * Creates a regex matcher just like sdb_memstore_regex_matcher except that it
 * matches in case the regular expression does not match.
 */
sdb_memstore_matcher_t *
sdb_memstore_nregex_matcher(sdb_memstore_expr_t *left, sdb_memstore_expr_t *right);

/*
 * sdb_memstore_isnull_matcher:
 * Creates a matcher matching NULL values.
 */
sdb_memstore_matcher_t *
sdb_memstore_isnull_matcher(sdb_memstore_expr_t *expr);

/*
 * sdb_memstore_istrue_matcher, sdb_memstore_isfalse_matcher:
 * Creates a matcher matching boolean values.
 */
sdb_memstore_matcher_t *
sdb_memstore_istrue_matcher(sdb_memstore_expr_t *expr);
sdb_memstore_matcher_t *
sdb_memstore_isfalse_matcher(sdb_memstore_expr_t *expr);

/*
 * sdb_memstore_matcher_matches:
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
sdb_memstore_matcher_matches(sdb_memstore_matcher_t *m, sdb_memstore_obj_t *obj,
		sdb_memstore_matcher_t *filter);

/*
 * sdb_memstore_matcher_op_cb:
 * Callback constructing a matcher operator.
 */
typedef sdb_memstore_matcher_t *(*sdb_memstore_matcher_op_cb)
	(sdb_memstore_expr_t *, sdb_memstore_expr_t *);

/*
 * sdb_memstore_lookup_cb:
 * Lookup callback. It is called for each matching object when looking up data
 * in the store passing on the lookup filter and the specified user-data. The
 * lookup aborts early if the callback returns non-zero.
 */
typedef int (*sdb_memstore_lookup_cb)(sdb_memstore_obj_t *obj,
		sdb_memstore_matcher_t *filter, void *user_data);

/*
 * sdb_memstore_scan:
 * Look up objects of the specified type in the specified store. The specified
 * callback function is called for each object in the store matching 'm'. The
 * function performs a full scan of all stored objects. If specified, the
 * filter will be used to preselect objects for further evaluation. See the
 * description of 'sdb_memstore_matcher_matches' for details.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_memstore_scan(sdb_memstore_t *store, int type,
		sdb_memstore_matcher_t *m, sdb_memstore_matcher_t *filter,
		sdb_memstore_lookup_cb cb, void *user_data);

/*
 * sdb_memstore_emit:
 * Send a single object to the specified store writer. Attributes or any child
 * objects are not included. Use sdb_memstore_emit_full() to emit a full
 * (filtered) object.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_memstore_emit(sdb_memstore_obj_t *obj, sdb_store_writer_t *w, sdb_object_t *wd);

/*
 * sdb_memstore_emit_full:
 * Send a single object and its attributes and all children to the specified
 * store writer. The filter, if specified, is applied to each attribute and
 * child object. Only matching objects will be emitted.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_memstore_emit_full(sdb_memstore_obj_t *obj, sdb_memstore_matcher_t *filter,
		sdb_store_writer_t *w, sdb_object_t *wd);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_CORE_MEMSTORE_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

