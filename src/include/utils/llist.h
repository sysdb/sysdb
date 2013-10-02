/*
 * SysDB - src/include/utils/llist.h
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

#ifndef SDB_UTILS_LLIST_H
#define SDB_UTILS_LLIST_H 1

#include "core/object.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sdb_llist;
typedef struct sdb_llist sdb_llist_t;

struct sdb_llist_iter;
typedef struct sdb_llist_iter sdb_llist_iter_t;

typedef int (*sdb_llist_cmp_cb)(const sdb_object_t *, const sdb_object_t *);
typedef int (*sdb_llist_lookup_cb)(const sdb_object_t *, const void *user_data);

/*
 * sdb_llist_create, sdb_llist_destroy:
 * Create and destroy a doubly linked list object.
 *
 * sdb_llist_create returns NULL on error.
 * sdb_llist_destroy will also destroy all remaining elements, thus releasing
 * the included objects (decrement the ref-count).
 */
sdb_llist_t *
sdb_llist_create(void);
void
sdb_llist_destroy(sdb_llist_t *list);

/*
 * sdb_llist_clone:
 * Clone an existing list. The objects stored in the list will not be copied
 * but rather their reference count incremented.
 *
 * Returns:
 *  - the copied list on success
 *  - NULL else
 */
sdb_llist_t *
sdb_llist_clone(sdb_llist_t *list);

/*
 * sdb_llist_append:
 * Append the given 'obj' to the end of 'list'. The list will take ownership
 * of the object, that is, increment the reference count by one. In case the
 * caller does not longer use the object for other purposes, it should thus
 * deref it.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value on failure
 */
int
sdb_llist_append(sdb_llist_t *list, sdb_object_t *obj);

/*
 * sdb_llist_insert:
 * Insert the new element at the specified position (zero being the head of
 * the list; the length of the list being the tail)). If the index is greater
 * than the length of the list (i.e. past the tail of the list), an error is
 * returned. The list will take ownership of the object, that is, increment
 * the reference count by one. In case the caller does not longer use the
 * object for other purposes, it should thus deref it.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value on failure
 */
int
sdb_llist_insert(sdb_llist_t *list, sdb_object_t *obj, size_t idx);

/*
 * sdb_llist_insert_sorted:
 * Insert the given 'obj' in the 'list' using a sort order as determined by
 * the 'compare' function. The function will insert the new entry before the
 * first entry which sorts later than the new entry. It will not ensure that
 * the rest of the list is sorted. The list will take ownership of the object,
 * that is, increment the reference count by one. In case the caller does not
 * longer use the object for other purposes, it should thus deref it.
 *
 * The 'compare' function should return less than zero, zero, greater than
 * zero if the first argument sorts less than, equal or greater than the
 * second argument respectively.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value on failure
 */
int
sdb_llist_insert_sorted(sdb_llist_t *list,
		sdb_object_t *obj, sdb_llist_cmp_cb);

/*
 * sdb_llist_search:
 * Search for a object in the given 'list'. The function will return the first
 * entry for which the 'lookup' callback returns 0. The 'user_data' is passed
 * on to the lookup function on each invocation.
 *
 * Returns:
 *  - a pointer to the first matching object
 *  - NULL else
 */
sdb_object_t *
sdb_llist_search(sdb_llist_t *list,
		sdb_llist_lookup_cb lookup, const void *user_data);

/*
 * sdb_llist_search_by_name:
 * Search for an object named 'key' in the given 'list'. The function will
 * return the first entry whose name matches the specified 'key' ignoring the
 * case of the characters.
 *
 * Returns:
 *  - a pointer to the first matching object
 *  - NULL else
 */
sdb_object_t *
sdb_llist_search_by_name(sdb_llist_t *list, const char *key);

/*
 * sdb_llist_remove:
 * Removes and returns the first matchin element of the list. The ref-count of
 * the item will not be changed, that is, if the element will not be used any
 * further, it should be de-referenced by the caller.
 *
 * Returns:
 *  - a pointer to the first matching object
 *  - NULL else
 */
sdb_object_t *
sdb_llist_remove(sdb_llist_t *list,
		sdb_llist_lookup_cb lookup, const void *user_data);

/*
 * sdb_llist_shift:
 * Removes and returns the first element of the list. The ref-count of the
 * item will not be changed, that is, if the element will not be used any
 * further, it should be de-referenced by the caller.
 *
 * Returns:
 *  - the former first element of the list
 *  - NULL if the list is empty
 */
sdb_object_t *
sdb_llist_shift(sdb_llist_t *list);

/* sdb_llist_get_iter, sdb_llist_iter_has_next, sdb_llist_iter_get_next:
 * Iterate through the list, element by element.
 *
 * sdb_llist_iter_get_next returns NULL if there is no next element.
 */
sdb_llist_iter_t *
sdb_llist_get_iter(sdb_llist_t *list);
void
sdb_llist_iter_destroy(sdb_llist_iter_t *iter);

_Bool
sdb_llist_iter_has_next(sdb_llist_iter_t *iter);
sdb_object_t *
sdb_llist_iter_get_next(sdb_llist_iter_t *iter);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_UTILS_LLIST_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

