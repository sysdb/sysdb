/*
 * SysDB - t/unit/utils/avltree_test.c
 * Copyright (C) 2015 Sebastian 'tokkee' Harl <sh@tokkee.org>
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
#endif

#include "utils/avltree.h"
#include "testutils.h"

#include <check.h>

static sdb_avltree_t *tree;

static void
setup(void)
{
	tree = sdb_avltree_create();
	fail_unless(tree != NULL,
			"sdb_avltree_create() = NULL; expected AVL-tree object");
} /* setup */

static void
teardown(void)
{
	sdb_avltree_destroy(tree);
	tree = NULL;
} /* teardown */

/* 'a' thru 'o' */
static sdb_object_t test_data[] = {
	SDB_OBJECT_STATIC("h"),
	SDB_OBJECT_STATIC("j"),
	SDB_OBJECT_STATIC("i"),
	SDB_OBJECT_STATIC("f"),
	SDB_OBJECT_STATIC("e"),
	SDB_OBJECT_STATIC("g"),
	SDB_OBJECT_STATIC("k"),
	SDB_OBJECT_STATIC("l"),
	SDB_OBJECT_STATIC("m"),
	SDB_OBJECT_STATIC("n"),
	SDB_OBJECT_STATIC("o"),
	SDB_OBJECT_STATIC("d"),
	SDB_OBJECT_STATIC("c"),
	SDB_OBJECT_STATIC("b"),
	SDB_OBJECT_STATIC("a"),
};

static char *unused_names[] = { "x", "y", "z" };

static void
populate(void)
{
	size_t i;
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(test_data); ++i)
		sdb_avltree_insert(tree, &test_data[i]);
} /* populate */

START_TEST(test_null)
{
	sdb_object_t o1 = SDB_OBJECT_STATIC("obj");
	sdb_object_t *o2;
	sdb_avltree_iter_t *iter;
	int check;

	/* all functions should work even when passed null values */
	sdb_avltree_destroy(NULL);
	sdb_avltree_clear(NULL);

	check = sdb_avltree_insert(NULL, NULL);
	fail_unless(check < 0,
			"sdb_avltree_insert(NULL, NULL) = %d; expected: <0", check);
	check = sdb_avltree_insert(NULL, &o1);
	fail_unless(check < 0,
			"sdb_avltree_insert(NULL, <obj>) = %d; expected: <0", check);
	fail_unless(o1.ref_cnt == 1,
			"sdb_avltree_insert(NULL, <obj>) incremented ref-cnt");
	/* it's acceptable to insert NULL */

	iter = sdb_avltree_get_iter(NULL);
	fail_unless(iter == NULL,
			"sdb_avltree_get_iter(NULL) = %p; expected: NULL", iter);

	check = sdb_avltree_iter_has_next(NULL) != 0;
	fail_unless(check == 0,
			"sdb_avltree_iter_has_next(NULL) = %d; expected: 0", check);
	o2 = sdb_avltree_iter_get_next(NULL);
	fail_unless(o2 == NULL,
			"sdb_avltree_iter_get_next(NULL) = %p; expected: NULL", o2);

	sdb_avltree_iter_destroy(NULL);

	check = (int)sdb_avltree_size(NULL);
	fail_unless(check == 0,
			"sdb_avltree_size(NULL) = %d; expected: 0", check);
}
END_TEST

START_TEST(test_insert)
{
	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(test_data); ++i) {
		int check;

		check = sdb_avltree_insert(tree, &test_data[i]);
		fail_unless(check == 0,
				"sdb_avltree_insert(<tree>, <%s>) = %d; expected: 0",
				test_data[i].name, check);

		check = (int)sdb_avltree_size(tree);
		fail_unless(check == (int)i + 1,
				"sdb_avltree_size(<tree>) = %d; expected: %zu",
				check, i + 1);

		fail_unless(sdb_avltree_valid(tree),
				"sdb_avltree_insert(<tree>, <%s>) left behind invalid tree",
				test_data[i].name);
	}

	/* and again ... now reporting errors because of duplicates */
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(test_data); ++i) {
		int check;

		check = sdb_avltree_insert(tree, &test_data[i]);
		fail_unless(check < 0,
				"sdb_avltree_insert(<tree>, <%s>) = %d (redo); expected: <0",
				test_data[i].name, check);

		check = (int)sdb_avltree_size(tree);
		fail_unless(check == SDB_STATIC_ARRAY_LEN(test_data),
				"sdb_avltree_size(<tree>) = %d; expected: %zu",
				check, SDB_STATIC_ARRAY_LEN(test_data));
	}
}
END_TEST

START_TEST(test_lookup)
{
	size_t i;

	populate();

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(test_data); ++i) {
		sdb_object_t *obj;

		obj = sdb_avltree_lookup(tree, test_data[i].name);
		fail_unless(obj != NULL,
				"sdb_avltree_lookup(<tree>, %s) = NULL; "
				"expected: <obj>", test_data[i].name);
		fail_unless(obj == &test_data[i],
				"sdb_avltree_lookup(<tree>, %s) = %p (%s); "
				"expected: %p, (%s)", test_data[i].name, obj, obj->name,
				&test_data[i], test_data[i].name);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(unused_names); ++i) {
		sdb_object_t *obj;

		obj = sdb_avltree_lookup(tree, unused_names[i]);
		fail_unless(obj == NULL,
				"sdb_avltree_lookup(<tree>, %s) = %p (%s); "
				"expected: NULL", unused_names[i],
				obj, obj ? obj->name : "<nil>");
	}
}
END_TEST

START_TEST(test_iter)
{
	sdb_avltree_iter_t *iter;
	sdb_object_t *obj;

	size_t check, i;

	populate();
	check = sdb_avltree_size(tree);
	fail_unless(check == SDB_STATIC_ARRAY_LEN(test_data),
			"INTERNAL ERROR: AVL tree size (after populate) = %zu; "
			"expected: %zu", check, SDB_STATIC_ARRAY_LEN(test_data));

	iter = sdb_avltree_get_iter(tree);
	fail_unless(iter != NULL,
			"sdb_avltree_get_iter(<tree>) = NULL; expected: <iter>");

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(test_data); ++i) {
		char expected_name[] = { (char)('a' + (int)i), '\0' };

		_Bool c = sdb_avltree_iter_has_next(iter);
		fail_unless(c, "sdb_avltree_iter_has_next(<iter[%zu]>) = false; "
				"expected: true", i);

		obj = sdb_avltree_iter_get_next(iter);
		fail_unless(obj != NULL,
				"sdb_avltree_iter_get_next(<iter[%zu]>) = NULL; "
				"expected: <obj>", i);
		fail_unless(!strcmp(obj->name, expected_name),
				"sdb_avltree_iter[%zu] = %s; expected: %s",
				i, obj->name, expected_name);
	}

	check = sdb_avltree_iter_has_next(iter) != 0;
	fail_unless(check == 0, "sdb_avltree_iter_has_next(<iter>) = true; "
			"expected: false");
	obj = sdb_avltree_iter_get_next(iter);
	fail_unless(obj == NULL,
			"sdb_avltree_iter_get_next(<iter>) = <obj>; expected: NULL");

	sdb_avltree_iter_destroy(iter);

	sdb_avltree_clear(tree);
	check = sdb_avltree_size(tree);
	fail_unless(check == 0,
			"sdb_avltree_clear(<tree>) left %zu nodes in the tree; "
			"expected: 0", check);
}
END_TEST

TEST_MAIN("utils::avltree")
{
	TCase *tc = tcase_create("core");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_null);
	tcase_add_test(tc, test_insert);
	tcase_add_test(tc, test_lookup);
	tcase_add_test(tc, test_iter);
	ADD_TCASE(tc);
}
TEST_MAIN_END

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

