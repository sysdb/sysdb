/*
 * SysDB - src/parser/ast.c
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

#include "core/store.h"

#include "parser/ast.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/*
 * data types
 */

static void
op_destroy(sdb_object_t *obj)
{
	sdb_ast_op_t *op = SDB_AST_OP(obj);
	sdb_object_deref(SDB_OBJ(op->left));
	sdb_object_deref(SDB_OBJ(op->right));
	op->left = op->right = NULL;
} /* op_destroy */

static void
iter_destroy(sdb_object_t *obj)
{
	sdb_ast_iter_t *iter = SDB_AST_ITER(obj);
	sdb_object_deref(SDB_OBJ(iter->iter));
	sdb_object_deref(SDB_OBJ(iter->expr));
	iter->iter = iter->expr = NULL;
} /* iter_destroy */

static void
typed_destroy(sdb_object_t *obj)
{
	sdb_ast_typed_t *typed = SDB_AST_TYPED(obj);
	sdb_object_deref(SDB_OBJ(typed->expr));
	typed->expr = NULL;
} /* typed_destroy */

static void
const_destroy(sdb_object_t *obj)
{
	sdb_ast_const_t *c = SDB_AST_CONST(obj);
	sdb_data_free_datum(&c->value);
} /* const_destroy */

static void
value_destroy(sdb_object_t *obj)
{
	sdb_ast_value_t *value = SDB_AST_VALUE(obj);
	if (value->name)
		free(value->name);
	value->name = NULL;
} /* value_destroy */

static void
fetch_destroy(sdb_object_t *obj)
{
	sdb_ast_fetch_t *fetch = SDB_AST_FETCH(obj);
	if (fetch->hostname)
		free(fetch->hostname);
	if (fetch->name)
		free(fetch->name);
	fetch->hostname = fetch->name = NULL;

	sdb_object_deref(SDB_OBJ(fetch->filter));
	fetch->filter = NULL;
} /* fetch_destroy */

static void
list_destroy(sdb_object_t *obj)
{
	sdb_ast_list_t *list = SDB_AST_LIST(obj);
	sdb_object_deref(SDB_OBJ(list->filter));
	list->filter = NULL;
} /* list_destroy */

static void
lookup_destroy(sdb_object_t *obj)
{
	sdb_ast_lookup_t *lookup = SDB_AST_LOOKUP(obj);
	sdb_object_deref(SDB_OBJ(lookup->matcher));
	sdb_object_deref(SDB_OBJ(lookup->filter));
	lookup->matcher = lookup->filter = NULL;
} /* lookup_destroy */

static void
store_destroy(sdb_object_t *obj)
{
	sdb_ast_store_t *store = SDB_AST_STORE(obj);
	if (store->hostname)
		free(store->hostname);
	if (store->parent)
		free(store->parent);
	if (store->name)
		free(store->name);
	store->hostname = store->parent = store->name = NULL;

	if (store->store_type)
		free(store->store_type);
	if (store->store_id)
		free(store->store_id);
	store->store_type = store->store_id = NULL;

	sdb_data_free_datum(&store->value);
} /* store_destroy */

static void
timeseries_destroy(sdb_object_t *obj)
{
	sdb_ast_timeseries_t *timeseries = SDB_AST_TIMESERIES(obj);
	if (timeseries->hostname)
		free(timeseries->hostname);
	if (timeseries->metric)
		free(timeseries->metric);
	timeseries->hostname = timeseries->metric = NULL;
} /* timeseries_destroy */

static sdb_type_t op_type = {
	/* size */ sizeof(sdb_ast_op_t),
	/* init */ NULL,
	/* destroy */ op_destroy,
};

static sdb_type_t iter_type = {
	/* size */ sizeof(sdb_ast_iter_t),
	/* init */ NULL,
	/* destroy */ iter_destroy,
};

static sdb_type_t typed_type = {
	/* size */ sizeof(sdb_ast_typed_t),
	/* init */ NULL,
	/* destroy */ typed_destroy,
};

static sdb_type_t const_type = {
	/* size */ sizeof(sdb_ast_const_t),
	/* init */ NULL,
	/* destroy */ const_destroy,
};

static sdb_type_t value_type = {
	/* size */ sizeof(sdb_ast_value_t),
	/* init */ NULL,
	/* destroy */ value_destroy,
};

static sdb_type_t fetch_type = {
	/* size */ sizeof(sdb_ast_fetch_t),
	/* init */ NULL,
	/* destroy */ fetch_destroy,
};

static sdb_type_t list_type = {
	/* size */ sizeof(sdb_ast_list_t),
	/* init */ NULL,
	/* destroy */ list_destroy,
};

static sdb_type_t lookup_type = {
	/* size */ sizeof(sdb_ast_lookup_t),
	/* init */ NULL,
	/* destroy */ lookup_destroy,
};

static sdb_type_t st_type = {
	/* size */ sizeof(sdb_ast_store_t),
	/* init */ NULL,
	/* destroy */ store_destroy,
};

static sdb_type_t ts_type = {
	/* size */ sizeof(sdb_ast_timeseries_t),
	/* init */ NULL,
	/* destroy */ timeseries_destroy,
};

/*
 * public API
 */

sdb_ast_node_t *
sdb_ast_op_create(int kind, sdb_ast_node_t *left, sdb_ast_node_t *right)
{
	sdb_ast_op_t *op;
	op = SDB_AST_OP(sdb_object_create(SDB_AST_OP_TO_STRING(kind), op_type));
	if (! op)
		return NULL;

	op->super.type = SDB_AST_TYPE_OPERATOR;

	op->kind = kind;
	op->left = left;
	op->right = right;
	return SDB_AST_NODE(op);
} /* sdb_ast_op_create */

sdb_ast_node_t *
sdb_ast_iter_create(int kind, sdb_ast_node_t *iter, sdb_ast_node_t *expr)
{
	sdb_ast_iter_t *i;
	i = SDB_AST_ITER(sdb_object_create(SDB_AST_OP_TO_STRING(kind), iter_type));
	if (! i)
		return NULL;

	i->super.type = SDB_AST_TYPE_ITERATOR;

	i->kind = kind;
	i->iter = iter;
	i->expr = expr;
	return SDB_AST_NODE(i);
} /* sdb_ast_iter_create */

sdb_ast_node_t *
sdb_ast_typed_create(int type, sdb_ast_node_t *expr)
{
	char name[32];
	sdb_ast_typed_t *typed;
	size_t i;

	strncpy(name, SDB_STORE_TYPE_TO_NAME(type), sizeof(name));
	for (i = 0; i < strlen(name); ++i)
		name[i] = (char)toupper((int)name[i]);
	typed = SDB_AST_TYPED(sdb_object_create(name, typed_type));
	if (! typed)
		return NULL;

	typed->super.type = SDB_AST_TYPE_TYPED;

	typed->type = type;
	typed->expr = expr;
	return SDB_AST_NODE(typed);
} /* sdb_ast_typed_create */

sdb_ast_node_t *
sdb_ast_const_create(sdb_data_t value)
{
	sdb_ast_const_t *c;
	c = SDB_AST_CONST(sdb_object_create("CONST", const_type));
	if (! c)
		return NULL;

	c->super.type = SDB_AST_TYPE_CONST;

	c->value = value;
	return SDB_AST_NODE(c);
} /* sdb_ast_const_create */

sdb_ast_node_t *
sdb_ast_value_create(int type, char *name)
{
	sdb_ast_value_t *value;
	value = SDB_AST_VALUE(sdb_object_create("VALUE", value_type));
	if (! value)
		return NULL;

	value->super.type = SDB_AST_TYPE_VALUE;

	value->type = type;
	value->name = name;
	return SDB_AST_NODE(value);
} /* sdb_ast_value_create */

sdb_ast_node_t *
sdb_ast_fetch_create(int obj_type, char *hostname, char *name, bool full,
		sdb_ast_node_t *filter)
{
	sdb_ast_fetch_t *fetch;
	fetch = SDB_AST_FETCH(sdb_object_create("FETCH", fetch_type));
	if (! fetch)
		return NULL;

	fetch->super.type = SDB_AST_TYPE_FETCH;

	fetch->obj_type = obj_type;
	fetch->hostname = hostname;
	fetch->name = name;
	fetch->full = full;
	fetch->filter = filter;
	return SDB_AST_NODE(fetch);
} /* sdb_ast_fetch_create */

sdb_ast_node_t *
sdb_ast_list_create(int obj_type, sdb_ast_node_t *filter)
{
	sdb_ast_list_t *list;
	list = SDB_AST_LIST(sdb_object_create("LIST", list_type));
	if (! list)
		return NULL;

	list->super.type = SDB_AST_TYPE_LIST;

	list->obj_type = obj_type;
	list->filter = filter;
	return SDB_AST_NODE(list);
} /* sdb_ast_list_create */

sdb_ast_node_t *
sdb_ast_lookup_create(int obj_type, sdb_ast_node_t *matcher,
		sdb_ast_node_t *filter)
{
	sdb_ast_lookup_t *lookup;
	lookup = SDB_AST_LOOKUP(sdb_object_create("LOOKUP", lookup_type));
	if (! lookup)
		return NULL;

	lookup->super.type = SDB_AST_TYPE_LOOKUP;

	lookup->obj_type = obj_type;
	lookup->matcher = matcher;
	lookup->filter = filter;
	return SDB_AST_NODE(lookup);
} /* sdb_ast_lookup_create */

sdb_ast_node_t *
sdb_ast_store_create(int obj_type, char *hostname,
		int parent_type, char *parent, char *name, sdb_time_t last_update,
		char *store_type, char *store_id, sdb_data_t value)
{
	sdb_ast_store_t *store;
	store = SDB_AST_STORE(sdb_object_create("STORE", st_type));
	if (! store)
		return NULL;

	store->super.type = SDB_AST_TYPE_STORE;

	store->obj_type = obj_type;
	store->hostname = hostname;
	store->parent_type = parent_type;
	store->parent = parent;
	store->name = name;
	store->last_update = last_update;
	store->store_type = store_type;
	store->store_id = store_id;
	store->value = value;
	return SDB_AST_NODE(store);
} /* sdb_ast_store_create */

sdb_ast_node_t *
sdb_ast_timeseries_create(char *hostname, char *metric,
		sdb_time_t start, sdb_time_t end)
{
	sdb_ast_timeseries_t *timeseries;
	timeseries = SDB_AST_TIMESERIES(sdb_object_create("TIMESERIES", ts_type));
	if (! timeseries)
		return NULL;

	timeseries->super.type = SDB_AST_TYPE_TIMESERIES;

	timeseries->hostname = hostname;
	timeseries->metric = metric;
	timeseries->start = start;
	timeseries->end = end;
	return SDB_AST_NODE(timeseries);
} /* sdb_ast_timeseries_create */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */
