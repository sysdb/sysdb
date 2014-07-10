/*
 * SysDB - src/include/utils/avltree.h
 * Copyright (C) 2014 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#ifndef SDB_UTILS_AVLTREE_H
#define SDB_UTILS_AVLTREE_H 1

#include "core/object.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * An AVL tree implements Adelson-Velskii and Landis' self-balancing search
 * tree. It supports search, insert, and delete operations in average and
 * worst-case time-complexity O(log n).
 */
struct sdb_avltree;
typedef struct sdb_avltree sdb_avltree_t;

struct sdb_avltree_iter;
typedef struct sdb_avltree_iter sdb_avltree_iter_t;

/*
 * sdb_avltree_create:
 * Creates an AVL tree. Insert and lookup operations will use the specified
 * compare function to determine the location of an object in the tree. If no
 * function has been specified, it defaults to sdb_object_cmp_by_name, that
 * is, objects will be compared by their names.
 */
sdb_avltree_t *
sdb_avltree_create(sdb_object_cmp_cb cmp);

/*
 * sdb_avltree_destroy:
 * Destroy the specified AVL tree and release all included objects (decrement
 * the ref-count).
 */
void
sdb_avltree_destroy(sdb_avltree_t *tree);

/*
 * sdb_avltree_clear:
 * Remove all nodes from the tree, releasing the included objects (decrement
 * the ref-count).
 */
void
sdb_avltree_clear(sdb_avltree_t *tree);

/*
 * sdb_avltree_insert:
 * Insert a new node into the tree (using the tree's compare function to find
 * the right location). Each object must be unique (based on the compare
 * function). This operation may change the structure of the tree by
 * rebalancing subtrees which no longer comply with the rules of AVL.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_avltree_insert(sdb_avltree_t *tree, sdb_object_t *obj);

/*
 * sdb_avltree_lookup:
 * Lookup an object from a tree. The object matching the specified reference
 * object (using the tree's compare function) will be returned.
 *
 * Returns:
 *  - the requested object
 *  - NULL if no such object exists
 */
sdb_object_t *
sdb_avltree_lookup(sdb_avltree_t *tree, const sdb_object_t *ref);

/*
 * sdb_avltree_get_iter, sdb_avltree_iter_has_next, sdb_avltree_iter_get_next,
 * sdb_avltree_iter_destroy:
 * Iterate through all nodes of the tree. The iterator will start at the
 * smallest element (based on the tree's compare function) and then iterate
 * through the sorted sequence of all nodes.
 *
 * sdb_avltree_iter_get_next returns NULL if there is no next element.
 */
sdb_avltree_iter_t *
sdb_avltree_get_iter(sdb_avltree_t *tree);
void
sdb_avltree_iter_destroy(sdb_avltree_iter_t *iter);

_Bool
sdb_avltree_iter_has_next(sdb_avltree_iter_t *iter);
sdb_object_t *
sdb_avltree_iter_get_next(sdb_avltree_iter_t *iter);

/*
 * sdb_avltree_size:
 * Returns the number of nodes in the tree.
 */
size_t
sdb_avltree_size(sdb_avltree_t *tree);

/*
 * sdb_avltree_valid:
 * Validate a tree, checking if all rules of AVL are met. All errors will be
 * reported through the logging sub-system. This function is mainly intended
 * for debugging and (unit) testing.
 *
 * Returns:
 *  - true if the tree is valid
 *  - false else
 */
_Bool
sdb_avltree_valid(sdb_avltree_t *tree);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_UTILS_AVLTREE_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

