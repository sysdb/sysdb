/*
 * SysDB - src/utils/llist.c
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

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "utils/llist.h"

#include <assert.h>
#include <stdlib.h>
#include <strings.h>

#include <pthread.h>

/*
 * private data types
 */

struct sdb_llist_elem;
typedef struct sdb_llist_elem sdb_llist_elem_t;

struct sdb_llist_elem {
	sdb_object_t *obj;

	sdb_llist_elem_t *next;
	sdb_llist_elem_t *prev;
};

struct sdb_llist {
	pthread_rwlock_t lock;

	sdb_llist_elem_t *head;
	sdb_llist_elem_t *tail;

	size_t length;
};

struct sdb_llist_iter {
	sdb_llist_t *list;
	sdb_llist_elem_t *elem;
};

/*
 * private helper functions
 */

/* Insert a new element after 'elem'. If 'elem' is NULL, insert at the head of
 * the list. */
static int
llist_insert_after(sdb_llist_t *list, sdb_llist_elem_t *elem,
		sdb_object_t *obj)
{
	sdb_llist_elem_t *new;

	assert(list);

	new = malloc(sizeof(*new));
	if (! new)
		return -1;

	new->obj  = obj;
	if (elem)
		new->next = elem->next;
	else if (list->head)
		new->next = list->head;
	else
		new->next = NULL;
	new->prev = elem;

	if (elem) {
		if (elem->next)
			elem->next->prev = new;
		else
			list->tail = new;
		elem->next = new;
	}
	else {
		/* new entry will be new head */
		if (list->head)
			list->head->prev = new;

		list->head = new;
		if (! list->tail)
			list->tail = new;
	}

	assert(list->head && list->tail);
	if (! list->length) {
		assert(list->head == list->tail);
	}

	sdb_object_ref(obj);
	++list->length;
	return 0;
} /* llist_insert_after */

static sdb_llist_elem_t *
llist_search(sdb_llist_t *list,
		sdb_llist_lookup_cb lookup, const void *user_data)
{
	sdb_llist_elem_t *elem;

	assert(list && lookup);

	for (elem = list->head; elem; elem = elem->next)
		if (! lookup(elem->obj, user_data))
			break;
	return elem;
} /* llist_search */

static sdb_object_t *
llist_remove_elem(sdb_llist_t *list, sdb_llist_elem_t *elem)
{
	sdb_object_t *obj;

	assert(list && elem);

	obj = elem->obj;

	if (elem->prev)
		elem->prev->next = elem->next;
	else {
		assert(elem == list->head);
		list->head = elem->next;
	}

	if (elem->next)
		elem->next->prev = elem->prev;
	else {
		assert(elem == list->tail);
		list->tail = elem->prev;
	}

	elem->prev = elem->next = NULL;
	free(elem);

	--list->length;
	return obj;
} /* llist_remove_elem */

/*
 * public API
 */

sdb_llist_t *
sdb_llist_create(void)
{
	sdb_llist_t *list;

	list = malloc(sizeof(*list));
	if (! list)
		return NULL;

	pthread_rwlock_init(&list->lock, /* attr = */ NULL);

	list->head = list->tail = NULL;
	list->length = 0;
	return list;
} /* sdb_llist_create */

sdb_llist_t *
sdb_llist_clone(sdb_llist_t *list)
{
	sdb_llist_t *new;
	sdb_llist_elem_t *elem;

	if (! list)
		return NULL;

	new = sdb_llist_create();
	if (! new)
		return NULL;

	if (! list->length) {
		assert((! list->head) && (! list->tail));
		return new;
	}

	for (elem = list->head; elem; elem = elem->next) {
		if (sdb_llist_append(new, elem->obj)) {
			sdb_llist_destroy(new);
			return NULL;
		}
	}
	return new;
} /* sdb_llist_clone */

void
sdb_llist_destroy(sdb_llist_t *list)
{
	sdb_llist_elem_t *elem;

	if (! list)
		return;

	pthread_rwlock_wrlock(&list->lock);

	elem = list->head;
	while (elem) {
		sdb_llist_elem_t *tmp = elem->next;

		sdb_object_deref(elem->obj);
		free(elem);

		elem = tmp;
	}

	list->head = list->tail = NULL;
	list->length = 0;

	pthread_rwlock_unlock(&list->lock);
	pthread_rwlock_destroy(&list->lock);
	free(list);
} /* sdb_llist_destroy */

int
sdb_llist_append(sdb_llist_t *list, sdb_object_t *obj)
{
	int status;

	if ((! list) || (! obj))
		return -1;

	pthread_rwlock_wrlock(&list->lock);
	status = llist_insert_after(list, list->tail, obj);
	pthread_rwlock_unlock(&list->lock);
	return status;
} /* sdb_llist_append */

int
sdb_llist_insert(sdb_llist_t *list, sdb_object_t *obj, size_t idx)
{
	sdb_llist_elem_t *prev;
	sdb_llist_elem_t *next;

	int status;

	size_t i;

	if ((! list) || (! obj) || (idx > list->length))
		return -1;

	pthread_rwlock_wrlock(&list->lock);

	prev = NULL;
	next = list->head;

	for (i = 0; i < idx; ++i) {
		prev = next;
		next = next->next;
	}
	status = llist_insert_after(list, prev, obj);
	pthread_rwlock_unlock(&list->lock);
	return status;
} /* sdb_llist_insert */

int
sdb_llist_insert_sorted(sdb_llist_t *list,
		sdb_object_t *obj, sdb_llist_cmp_cb compare)
{
	sdb_llist_elem_t *prev;
	sdb_llist_elem_t *next;

	int status;

	if ((! list) || (! obj) || (! compare))
		return -1;

	pthread_rwlock_wrlock(&list->lock);

	prev = NULL;
	next = list->head;

	while (next) {
		if (compare(obj, next->obj) < 0)
			break;

		prev = next;
		next = next->next;
	}
	status = llist_insert_after(list, prev, obj);
	pthread_rwlock_unlock(&list->lock);
	return status;
} /* sdb_llist_insert_sorted */

sdb_object_t *
sdb_llist_get(sdb_llist_t *list, size_t i)
{
	sdb_llist_elem_t *elem;
	size_t j;

	if ((! list) || (i >= list->length))
		return NULL;

	for (elem = list->head, j = 0; j < i; elem = elem->next, ++j)
		/* iterate */;

	assert(elem);
	sdb_object_ref(elem->obj);
	return elem->obj;
} /* sdb_llist_get */

sdb_object_t *
sdb_llist_search(sdb_llist_t *list,
		sdb_llist_lookup_cb lookup, const void *user_data)
{
	sdb_llist_elem_t *elem;

	if ((! list) || (! lookup))
		return NULL;

	pthread_rwlock_rdlock(&list->lock);
	elem = llist_search(list, lookup, user_data);
	pthread_rwlock_unlock(&list->lock);

	if (elem)
		return elem->obj;
	return NULL;
} /* sdb_llist_search */

sdb_object_t *
sdb_llist_search_by_name(sdb_llist_t *list, const char *key)
{
	sdb_llist_elem_t *elem;

	if (! list)
		return NULL;

	pthread_rwlock_rdlock(&list->lock);

	for (elem = list->head; elem; elem = elem->next)
		if (! strcasecmp(elem->obj->name, key))
			break;

	pthread_rwlock_unlock(&list->lock);

	if (elem)
		return elem->obj;
	return NULL;
} /* sdb_llist_search_by_name */

sdb_object_t *
sdb_llist_remove(sdb_llist_t *list,
		sdb_llist_lookup_cb lookup, const void *user_data)
{
	sdb_llist_elem_t *elem;
	sdb_object_t *obj = NULL;

	if ((! list) || (! lookup))
		return NULL;

	pthread_rwlock_wrlock(&list->lock);
	elem = llist_search(list, lookup, user_data);
	if (elem)
		obj = llist_remove_elem(list, elem);
	pthread_rwlock_unlock(&list->lock);

	return obj;
} /* sdb_llist_remove */

sdb_object_t *
sdb_llist_remove_by_name(sdb_llist_t *list, const char *key)
{
	sdb_llist_elem_t *elem;
	sdb_object_t *obj = NULL;

	if (! list)
		return NULL;

	pthread_rwlock_rdlock(&list->lock);

	for (elem = list->head; elem; elem = elem->next)
		if ((key == elem->obj->name)
				|| (! strcasecmp(elem->obj->name, key)))
			break;

	if (elem)
		obj = llist_remove_elem(list, elem);
	pthread_rwlock_unlock(&list->lock);

	return obj;
} /* sdb_llist_remove_by_name */

sdb_object_t *
sdb_llist_shift(sdb_llist_t *list)
{
	sdb_object_t *obj;

	if ((! list) || (! list->head))
		return NULL;

	pthread_rwlock_wrlock(&list->lock);
	obj = llist_remove_elem(list, list->head);
	pthread_rwlock_unlock(&list->lock);
	return obj;
} /* sdb_llist_shift */

sdb_llist_iter_t *
sdb_llist_get_iter(sdb_llist_t *list)
{
	sdb_llist_iter_t *iter;

	if (! list)
		return NULL;

	iter = malloc(sizeof(*iter));
	if (! iter)
		return NULL;

	pthread_rwlock_rdlock(&list->lock);

	iter->list = list;
	iter->elem = list->head;

	/* XXX: keep lock until destroying the iterator? */
	pthread_rwlock_unlock(&list->lock);
	return iter;
} /* sdb_llist_get_iter */

void
sdb_llist_iter_destroy(sdb_llist_iter_t *iter)
{
	if (! iter)
		return;

	iter->list = NULL;
	iter->elem = NULL;
	free(iter);
} /* sdb_llist_iter_destroy */

_Bool
sdb_llist_iter_has_next(sdb_llist_iter_t *iter)
{
	if (! iter)
		return 0;
	return iter->elem != NULL;
} /* sdb_llist_iter_has_next */

sdb_object_t *
sdb_llist_iter_get_next(sdb_llist_iter_t *iter)
{
	sdb_object_t *obj;

	if ((! iter) || (! iter->elem))
		return NULL;

	pthread_rwlock_rdlock(&iter->list->lock);

	/* XXX: increment ref-cnt for this object?
	 *      also: when letting an element take ownership of next and prev
	 *      elements, this might be a fairly cheap way to implement a weak
	 *      type of snapshotting */

	obj = iter->elem->obj;
	iter->elem = iter->elem->next;

	pthread_rwlock_unlock(&iter->list->lock);
	return obj;
} /* sdb_llist_iter_get_next */

int
sdb_llist_iter_remove_current(sdb_llist_iter_t *iter)
{
	sdb_llist_elem_t *elem;

	if ((! iter) || (! iter->list))
		return -1;

	pthread_rwlock_wrlock(&iter->list->lock);

	if (! iter->elem) /* reached end of list */
		elem = iter->list->tail;
	else
		elem = iter->elem->prev;
	if (elem)
		llist_remove_elem(iter->list, elem);

	pthread_rwlock_unlock(&iter->list->lock);

	if (! elem)
		return -1;
	return 0;
} /* sdb_llist_iter_remove */

size_t
sdb_llist_len(sdb_llist_t *list)
{
	if (! list)
		return 0;
	return list->length;
} /* sdb_llist_len */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

