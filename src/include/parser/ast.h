/*
 * SysDB - src/include/parser/ast.h
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
 * The SysDB query language AST describes the parse-tree of an SysQL query.
 */

#ifndef SDB_PARSER_AST_H
#define SDB_PARSER_AST_H 1

#include "core/data.h"
#include "core/time.h"
#include "core/object.h"

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sdb_ast_node_type_t describes the type of an AST node.
 */
typedef enum {
	/* command nodes */
	SDB_AST_TYPE_FETCH      = 1,
	SDB_AST_TYPE_LIST       = 2,
	SDB_AST_TYPE_LOOKUP     = 3,
	SDB_AST_TYPE_STORE      = 4,
	SDB_AST_TYPE_TIMESERIES = 5,

	/* generic expressions */
	SDB_AST_TYPE_OPERATOR   = 100,
	SDB_AST_TYPE_ITERATOR   = 101,

	/* values */
	SDB_AST_TYPE_CONST      = 200,
	SDB_AST_TYPE_VALUE      = 201,

	SDB_AST_TYPE_TYPED      = 210,
} sdb_ast_node_type_t;

/*
 * sdb_ast_operator_t describes the type of an operator.
 */
typedef enum {
	/* logical and comparison operators */
#define SDB_AST_IS_LOGICAL(n) \
	((((n)->type == SDB_AST_TYPE_OPERATOR) \
			&& ((SDB_AST_AND <= SDB_AST_OP(n)->kind) \
				&& (SDB_AST_OP(n)->kind <= SDB_AST_IN))) \
		|| (((n)->type == SDB_AST_TYPE_ITERATOR) \
			&& ((SDB_AST_ALL <= SDB_AST_ITER(n)->kind) \
				&& (SDB_AST_ITER(n)->kind <= SDB_AST_ANY))))
	SDB_AST_AND    = 1000,
	SDB_AST_OR     = 1001,
	SDB_AST_NOT    = 1002,

	SDB_AST_LT     = 1010,
	SDB_AST_LE     = 1011,
	SDB_AST_EQ     = 1012,
	SDB_AST_NE     = 1013,
	SDB_AST_GE     = 1014,
	SDB_AST_GT     = 1015,
	SDB_AST_REGEX  = 1016,
	SDB_AST_NREGEX = 1017,
	SDB_AST_ISNULL = 1018,
	SDB_AST_IN     = 1019,

	/* arithmetic expressions */
#define SDB_AST_IS_ARITHMETIC(n) \
	(((n)->type == SDB_AST_TYPE_CONST) \
		|| ((n)->type == SDB_AST_TYPE_VALUE) \
		|| ((n)->type == SDB_AST_TYPE_TYPED) \
		|| (((n)->type == SDB_AST_TYPE_OPERATOR) \
			&& ((SDB_AST_ADD <= SDB_AST_OP(n)->kind) \
				&& (SDB_AST_OP(n)->kind <= SDB_AST_CONCAT))))
	SDB_AST_ADD    = 2000,
	SDB_AST_SUB    = 2001,
	SDB_AST_MUL    = 2002,
	SDB_AST_DIV    = 2003,
	SDB_AST_MOD    = 2004,
	SDB_AST_CONCAT = 2005,

	/* iterators */
#define SDB_AST_IS_ITERATOR(n) \
	(((n)->type == SDB_AST_TYPE_ITERATOR) \
		&& ((SDB_AST_ALL <= SDB_AST_ITER(n)->kind) \
			&& (SDB_AST_ITER(n)->kind <= SDB_AST_ANY)))
	SDB_AST_ALL    = 3000,
	SDB_AST_ANY    = 3001,
} sdb_ast_operator_t;

#define SDB_AST_OP_TO_STRING(op) \
	(((op) == SDB_AST_AND) ? "AND" \
		: ((op) == SDB_AST_OR) ? "OR" \
		: ((op) == SDB_AST_NOT) ? "NOT" \
		: ((op) == SDB_AST_LT) ? "LT" \
		: ((op) == SDB_AST_LE) ? "LE" \
		: ((op) == SDB_AST_EQ) ? "EQ" \
		: ((op) == SDB_AST_NE) ? "NE" \
		: ((op) == SDB_AST_GE) ? "GE" \
		: ((op) == SDB_AST_GT) ? "GT" \
		: ((op) == SDB_AST_REGEX) ? "REGEX" \
		: ((op) == SDB_AST_NREGEX) ? "NREGEX" \
		: ((op) == SDB_AST_ISNULL) ? "ISNULL" \
		: ((op) == SDB_AST_IN) ? "IN" \
		: ((op) == SDB_AST_ADD) ? "ADD" \
		: ((op) == SDB_AST_SUB) ? "SUB" \
		: ((op) == SDB_AST_MUL) ? "MUL" \
		: ((op) == SDB_AST_DIV) ? "DIV" \
		: ((op) == SDB_AST_MOD) ? "MOD" \
		: ((op) == SDB_AST_CONCAT) ? "CONCAT" \
		: ((op) == SDB_AST_ALL) ? "ALL" \
		: ((op) == SDB_AST_ANY) ? "ANY" \
		: "UNKNOWN")

#define SDB_AST_TYPE_TO_STRING(n) \
	(((n)->type == SDB_AST_TYPE_FETCH) ? "FETCH" \
		: ((n)->type == SDB_AST_TYPE_LIST) ? "LIST" \
		: ((n)->type == SDB_AST_TYPE_LOOKUP) ? "LOOKUP" \
		: ((n)->type == SDB_AST_TYPE_STORE) ? "STORE" \
		: ((n)->type == SDB_AST_TYPE_TIMESERIES) ? "TIMESERIES" \
		: ((n)->type == SDB_AST_TYPE_OPERATOR) \
			? SDB_AST_OP_TO_STRING(SDB_AST_OP(n)->kind) \
		: ((n)->type == SDB_AST_TYPE_ITERATOR) ? "ITERATOR" \
		: ((n)->type == SDB_AST_TYPE_CONST) ? "CONSTANT" \
		: ((n)->type == SDB_AST_TYPE_VALUE) ? "VALUE" \
		: ((n)->type == SDB_AST_TYPE_TYPED) ? "TYPED VALUE" \
		: "UNKNOWN")

/*
 * sdb_ast_node_t is the interface for AST nodes. The first field of any
 * actual implementation of the interface is of type sdb_ast_node_t to
 * fascilitate casting between the interface and implementation types.
 *
 * It inherits from sdb_object_t and instances may safely be cast to a generic
 * object as well.
 */
typedef struct {
	sdb_object_t super;

	/* type describes the type of the actual node */
	int type;
} sdb_ast_node_t;
#define SDB_AST_NODE(obj) ((sdb_ast_node_t *)(obj))

/*
 * sdb_ast_op_t represents a simple operation.
 */
typedef struct {
	sdb_ast_node_t super;
	int kind;
	/* left operand is NULL for unary expressions */
	sdb_ast_node_t *left;
	sdb_ast_node_t *right;
} sdb_ast_op_t;
#define SDB_AST_OP(obj) ((sdb_ast_op_t *)(obj))
#define SDB_AST_OP_INIT \
	{ { SDB_OBJECT_INIT, SDB_AST_TYPE_OPERATOR }, -1, NULL, NULL }

/*
 * sdb_ast_iter_t represents an iterator.
 */
typedef struct {
	sdb_ast_node_t super;
	int kind;
	sdb_ast_node_t *iter;
	/* exactly one operand of the expression has to be unset and will be
	 * filled in by the iterator value */
	sdb_ast_node_t *expr;
} sdb_ast_iter_t;
#define SDB_AST_ITER(obj) ((sdb_ast_iter_t *)(obj))
#define SDB_AST_ITER_INIT \
	{ { SDB_OBJECT_INIT, SDB_AST_TYPE_ITERATOR }, -1, NULL, NULL }

/*
 * sdb_ast_typed_t represents a typed value.
 */
typedef struct {
	sdb_ast_node_t super;
	int type;
	sdb_ast_node_t *expr;
} sdb_ast_typed_t;
#define SDB_AST_TYPED(obj) ((sdb_ast_typed_t *)(obj))
#define SDB_AST_TYPED_INIT \
	{ { SDB_OBJECT_INIT, SDB_AST_TYPE_TYPED }, -1, NULL }

/*
 * sdb_ast_const_t represents a constant value.
 */
typedef struct {
	sdb_ast_node_t super;
	sdb_data_t value;
} sdb_ast_const_t;
#define SDB_AST_CONST(obj) ((sdb_ast_const_t *)(obj))
#define SDB_AST_CONST_INIT \
	{ { SDB_OBJECT_INIT, SDB_AST_TYPE_CONST }, SDB_DATA_INIT }

/*
 * sdb_ast_value_t represents an object-specific value: sibling nodes,
 * attributes, or field values.
 */
typedef struct {
	sdb_ast_node_t super;
	int type; /* attribute or field */
	char *name; /* object name; optional */
} sdb_ast_value_t;
#define SDB_AST_VALUE(obj) ((sdb_ast_value_t *)(obj))
#define SDB_AST_VALUE_INIT \
	{ { SDB_OBJECT_INIT, SDB_AST_TYPE_VALUE }, -1, NULL }

/*
 * sdb_ast_fetch_t represents a FETCH command.
 */
typedef struct {
	sdb_ast_node_t super;
	int obj_type;
	char *hostname; /* optional */
	char *name;
	sdb_ast_node_t *filter; /* optional */
} sdb_ast_fetch_t;
#define SDB_AST_FETCH(obj) ((sdb_ast_fetch_t *)(obj))
#define SDB_AST_FETCH_INIT \
	{ { SDB_OBJECT_INIT, SDB_AST_TYPE_FETCH }, -1, NULL, NULL, NULL }

/*
 * sdb_ast_list_t represents a LIST command.
 */
typedef struct {
	sdb_ast_node_t super;
	int obj_type;
	sdb_ast_node_t *filter; /* optional */
} sdb_ast_list_t;
#define SDB_AST_LIST(obj) ((sdb_ast_list_t *)(obj))
#define SDB_AST_LIST_INIT \
	{ { SDB_OBJECT_INIT, SDB_AST_TYPE_LIST }, -1, NULL }

/*
 * sdb_ast_lookup_t represents a LOOKUP command.
 */
typedef struct {
	sdb_ast_node_t super;
	int obj_type;
	sdb_ast_node_t *matcher; /* optional */
	sdb_ast_node_t *filter; /* optional */
} sdb_ast_lookup_t;
#define SDB_AST_LOOKUP(obj) ((sdb_ast_lookup_t *)(obj))
#define SDB_AST_LOOKUP_INIT \
	{ { SDB_OBJECT_INIT, SDB_AST_TYPE_LOOKUP }, -1, NULL, NULL }

/*
 * sdb_ast_store_t represents a STORE command.
 */
typedef struct {
	sdb_ast_node_t super;
	int obj_type;
	char *hostname;  /* optional */
	int parent_type; /* optional */
	char *parent;    /* optional */
	char *name;
	sdb_time_t last_update;

	/* metric specific */
	char *store_type;
	char *store_id;

	/* attribute specific */
	sdb_data_t value;
} sdb_ast_store_t;
#define SDB_AST_STORE(obj) ((sdb_ast_store_t *)(obj))
#define SDB_AST_STORE_INIT \
	{ { SDB_OBJECT_INIT, SDB_AST_TYPE_STORE }, \
		-1, NULL, -1, NULL, NULL, 0, NULL, NULL, SDB_DATA_INIT }

/*
 * sdb_ast_timeseries_t represents a TIMESERIES command.
 */
typedef struct {
	sdb_ast_node_t super;
	char *hostname;
	char *metric;
	sdb_time_t start;
	sdb_time_t end;
} sdb_ast_timeseries_t;
#define SDB_AST_TIMESERIES(obj) ((sdb_ast_timeseries_t *)(obj))
#define SDB_AST_TIMESERIES_INIT \
	{ { SDB_OBJECT_INIT, SDB_AST_TYPE_TIMESERIES }, NULL, NULL, 0, 0 }

/*
 * AST constructors:
 * Newly created nodes take ownership of any dynamically allocated objects
 * (node objects, dynamically allocated constant values, strings). The memory
 * will be freed when destroying the node using sdb_object_deref.
 *
 * The constructors do not verify any arguments. The analyzer has to be used
 * for that purpose.
 */

/*
 * sdb_ast_op_create:
 * Creates an AST node representing a simple (ary or unary) operation. The
 * newly created node takes ownership of the left and right nodes.
 */
sdb_ast_node_t *
sdb_ast_op_create(int kind, sdb_ast_node_t *left, sdb_ast_node_t *right);

/*
 * sdb_ast_iter_create:
 * Creates an AST node representing an iterator. The newly created node takes
 * ownership of the iter and expr nodes.
 */
sdb_ast_node_t *
sdb_ast_iter_create(int kind, sdb_ast_node_t *iter, sdb_ast_node_t *expr);

/*
 * sdb_ast_typed_create:
 * Creates an AST node representing a typed expression. Thew newly created
 * node takes ownership of the expr node.
 */
sdb_ast_node_t *
sdb_ast_typed_create(int type, sdb_ast_node_t *expr);

/*
 * sdb_ast_const_create:
 * Creates an AST node representing a constant value. The newly created node
 * takes ownership of the value object.
 */
sdb_ast_node_t *
sdb_ast_const_create(sdb_data_t value);

/*
 * sdb_ast_value_create:
 * Creates an AST node representing an object-specific value (sibling nodes,
 * attributes, or field values). The newly created node takes ownership of the
 * string value.
 */
sdb_ast_node_t *
sdb_ast_value_create(int type, char *name);

/*
 * sdb_ast_fetch_create:
 * Creates an AST node representing a FETCH command. The newly created node
 * takes ownership of the strings and the filter node.
 */
sdb_ast_node_t *
sdb_ast_fetch_create(int obj_type, char *hostname, char *name,
		sdb_ast_node_t *filter);

/*
 * sdb_ast_list_create:
 * Creates an AST node representing a LIST command. The newly created node
 * takes ownership of the filter node.
 */
sdb_ast_node_t *
sdb_ast_list_create(int obj_type, sdb_ast_node_t *filter);

/*
 * sdb_ast_lookup_create:
 * Creates an AST node representing a LOOKUP command. The newly created node
 * takes ownership of the matcher and filter nodes.
 */
sdb_ast_node_t *
sdb_ast_lookup_create(int obj_type, sdb_ast_node_t *matcher,
		sdb_ast_node_t *filter);

/*
 * sdb_ast_store_create:
 * Creates an AST node representing a STORE command. Thew newly created node
 * takes ownership of all strings and the value object.
 */
sdb_ast_node_t *
sdb_ast_store_create(int obj_type, char *hostname,
		int parent_type, char *parent, char *name, sdb_time_t last_update,
		char *store_type, char *store_id, sdb_data_t value);

/*
 * sdb_ast_timeseries_create:
 * Creates an AST node representing a TIMESERIES command. The newly created
 * node takes ownership of the strings.
 */
sdb_ast_node_t *
sdb_ast_timeseries_create(char *hostname, char *metric,
		sdb_time_t start, sdb_time_t end);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_PARSER_AST_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

