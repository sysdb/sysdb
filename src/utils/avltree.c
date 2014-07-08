/*
 * SysDB - src/utils/avltree.c
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

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "utils/avltree.h"

#include <stdlib.h>

#include <pthread.h>

/*
 * private data types
 */

struct node;
typedef struct node node_t;

struct node {
	sdb_object_t *obj;

	node_t *parent;
	node_t *left;
	node_t *right;
};

struct sdb_avltree {
	pthread_rwlock_t lock;

	sdb_object_cmp_cb cmp;

	node_t *root;
	size_t size;
};

struct sdb_avltree_iter {
	sdb_avltree_t *tree;
	node_t *node;
};

/*
 * private helper functions
 */

static void
node_destroy(node_t *n)
{
	sdb_object_deref(n->obj);
	n->obj = NULL;
	n->parent = n->left = n->right = NULL;
	free(n);
} /* node_destroy */

static node_t *
node_create(sdb_object_t *obj)
{
	node_t *n = malloc(sizeof(*n));
	if (! n)
		return NULL;

	sdb_object_ref(obj);
	n->obj = obj;
	n->parent = n->left = n->right = NULL;
	return n;
} /* node_create */

static node_t *
node_next(node_t *n)
{
	node_t *next;

	if (! n)
		return NULL;

	/* descend into the right tree */
	if (n->right) {
		next = n->right;
		while (next->left)
			next = next->left;
		return next;
	}

	/* visit the parent if we're in its left tree */
	if (n->parent && (n->parent->left == n))
		return n->parent;

	/* find the parent of the sibling sub-tree on the right */
	next = n->parent;
	next = NULL;
	while (n->parent && (n->parent->right == n)) {
		n = n->parent;
		next = n;
	}
	if (next)
		next = next->parent;
	return next;
} /* node_next */

static void
tree_clear(sdb_avltree_t *tree)
{
	node_t *n;

	n = tree->root;
	while (n) {
		node_t *tmp = node_next(n);

		node_destroy(n);
		n = tmp;
	}

	tree->root = NULL;
	tree->size = 0;
} /* tree_clear */

/*
 * public API
 */

sdb_avltree_t *
sdb_avltree_create(sdb_object_cmp_cb cmp)
{
	sdb_avltree_t *tree;

	tree = malloc(sizeof(*tree));
	if (! tree)
		return NULL;

	if (! cmp)
		cmp = sdb_object_cmp_by_name;

	pthread_rwlock_init(&tree->lock, /* attr = */ NULL);
	tree->cmp = cmp;

	tree->root = NULL;
	tree->size = 0;
	return tree;
} /* sdb_avltree_create */

void
sdb_avltree_destroy(sdb_avltree_t *tree)
{
	if (! tree)
		return;

	pthread_rwlock_wrlock(&tree->lock);
	tree_clear(tree);
	pthread_rwlock_unlock(&tree->lock);
	pthread_rwlock_destroy(&tree->lock);
	free(tree);
} /* sdb_avltree_destroy */

void
sdb_avltree_clear(sdb_avltree_t *tree)
{
	if (! tree)
		return;

	pthread_rwlock_wrlock(&tree->lock);
	tree_clear(tree);
	pthread_rwlock_unlock(&tree->lock);
} /* sdb_avltree_clear */

int
sdb_avltree_insert(sdb_avltree_t *tree, sdb_object_t *obj)
{
	node_t *parent;
	node_t *n;

	int diff = -1;

	if (! tree)
		return -1;

	n = node_create(obj);
	if (! n)
		return -1;

	parent = tree->root;
	while (parent) {
		diff = tree->cmp(obj, parent->obj);
		if (! diff) {
			node_destroy(n);
			return -1;
		}

		if (diff < 0) {
			if (! parent->left) {
				parent->left = n;
				break;
			}
			parent = parent->left;
		}
		else {
			if (! parent->right) {
				parent->right = n;
				break;
			}
			parent = parent->right;
		}
	}

	if (! parent) {
		/* new root */
		if (diff < 0)
			n->right = tree->root;
		else
			n->left = tree->root;
		tree->root = n;
	}

	n->parent = parent;
	++tree->size;

	/* XXX: rebalance */
	return 0;
} /* sdb_avltree_insert */

sdb_avltree_iter_t *
sdb_avltree_get_iter(sdb_avltree_t *tree)
{
	sdb_avltree_iter_t *iter;

	if (! tree)
		return NULL;

	iter = malloc(sizeof(*iter));
	if (! iter)
		return NULL;

	pthread_rwlock_rdlock(&tree->lock);

	iter->tree = tree;
	iter->node = tree->root->left;
	while (iter->node && iter->node->left)
		iter->node = iter->node->left;

	pthread_rwlock_unlock(&tree->lock);
	return iter;
} /* sdb_avltree_get_iter */

void
sdb_avltree_iter_destroy(sdb_avltree_iter_t *iter)
{
	if (! iter)
		return;

	iter->tree = NULL;
	iter->node = NULL;
	free(iter);
} /* sdb_avltree_iter_destroy */

_Bool
sdb_avltree_iter_has_next(sdb_avltree_iter_t *iter)
{
	if (! iter)
		return 0;

	return iter->node != NULL;
} /* sdb_avltree_iter_has_next */

sdb_object_t *
sdb_avltree_iter_get_next(sdb_avltree_iter_t *iter)
{
	node_t *n;

	if (! iter)
		return NULL;

	n = iter->node;
	iter->node = node_next(iter->node);
	return n ? n->obj : NULL;
} /* sdb_avltree_iter_get_next */

size_t
sdb_avltree_size(sdb_avltree_t *tree)
{
	return tree ? tree->size : 0;
} /* sdb_avltree_size */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

