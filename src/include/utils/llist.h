/*
 * syscollector - src/include/utils/llist.h
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

#ifndef SC_UTILS_LLIST_H
#define SC_UTILS_LLIST_H 1

#include "core/object.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sc_llist;
typedef struct sc_llist sc_llist_t;

struct sc_llist_iter;
typedef struct sc_llist_iter sc_llist_iter_t;

/*
 * sc_llist_create, sc_llist_destroy:
 * Create and destroy a doubly linked list object.
 *
 * sc_llist_create returns NULL on error.
 * sc_llist_destroy will also destroy all remaining elements, thus releasing
 * the included objects (decrement the ref-count).
 */
sc_llist_t *
sc_llist_create(void);
void
sc_llist_destroy(sc_llist_t *list);

/*
 * sc_llist_clone:
 * Clone an existing list. The objects stored in the list will not be copied
 * but rather their reference count incremented.
 *
 * Returns:
 *  - the copied list on success
 *  - NULL else
 */
sc_llist_t *
sc_llist_clone(sc_llist_t *list);

/*
 * sc_llist_append:
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
sc_llist_append(sc_llist_t *list, sc_object_t *obj);

/*
 * sc_llist_insert:
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
sc_llist_insert(sc_llist_t *list, sc_object_t *obj, size_t index);

/*
 * sc_llist_insert_sorted:
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
sc_llist_insert_sorted(sc_llist_t *list, sc_object_t *obj,
		int (*compare)(const sc_object_t *, const sc_object_t *));

/* sc_llist_search:
 * Search for a 'key' in the given 'list'. The function will return the first
 * entry that matches the specified 'key'. For that purpose, the 'compare'
 * function is used. It should return 0 iff the two arguments compare equal.
 *
 * Returns:
 *  - a pointer the sc_object_t containing the matching entry
 *  - NULL else
 */
sc_object_t *
sc_llist_search(sc_llist_t *list, const sc_object_t *key,
		int (*compare)(const sc_object_t *, const sc_object_t *));

/*
 * sc_llist_shift:
 * Removes and returns the first element of the list. The ref-count of the
 * item will not be changed, that is, if the element will not be used any
 * further, it should be re-referenced by the caller.
 *
 * Returns:
 *  - the former first element of the list
 *  - NULL if the list is empty
 */
sc_object_t *
sc_llist_shift(sc_llist_t *list);

/* sc_llist_get_iter, sc_llist_iter_has_next, sc_llist_iter_get_next:
 * Iterate through the list, element by element.
 *
 * sc_llist_iter_get_next returns NULL if there is no next element.
 */
sc_llist_iter_t *
sc_llist_get_iter(sc_llist_t *list);
void
sc_llist_iter_destroy(sc_llist_iter_t *iter);

_Bool
sc_llist_iter_has_next(sc_llist_iter_t *iter);
sc_object_t *
sc_llist_iter_get_next(sc_llist_iter_t *iter);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SC_UTILS_LLIST_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

