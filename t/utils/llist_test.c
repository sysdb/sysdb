/*
 * SysDB - t/utils/llist_test.c
 * Copyright (C) 2013 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include "utils/llist.h"
#include "libsysdb_test.h"

#include <check.h>

/*
 * private data types
 */

static sdb_object_t golden_data[] = {
	SSTRING_OBJ("abc"),
	SSTRING_OBJ("bcd"),
	SSTRING_OBJ("cde"),
	SSTRING_OBJ("def"),
	SSTRING_OBJ("efg"),
	SSTRING_OBJ("fgh"),
	SSTRING_OBJ("ghi")
};

static char *unused_names[] = {
	"xyz",
	"yza",
	"zab"
};

static sdb_llist_t *list;

static void
setup(void)
{
	list = sdb_llist_create();
	fail_unless(list != NULL,
			"sdb_llist_create() = NULL; expected list object");
} /* setup */

static void
teardown(void)
{
	sdb_llist_destroy(list);
	list = NULL;
} /* teardown */

/* populate the list with the golden data in the specified order */
static void
populate(void)
{
	size_t i;
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		int check = sdb_llist_append(list, &golden_data[i]);
		fail_unless(check == 0,
				"sdb_llist_append(%s) = %i; expected: 0",
				golden_data[i].name, check);
	}
} /* populate */

START_TEST(test_llist_clone)
{
	sdb_llist_t *clone;
	size_t i;

	populate();

	clone = sdb_llist_clone(list);
	fail_unless(clone != NULL,
			"sdb_llist_clone() = NULL; expected: cloned list object");

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		fail_unless(golden_data[i].ref_cnt == 3,
				"sdb_llist_clone() did not take ownership");
	}

	sdb_llist_destroy(clone);
}
END_TEST

START_TEST(test_llist_destroy)
{
	size_t i;
	populate();
	sdb_llist_destroy(list);
	list = NULL;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		fail_unless(golden_data[i].ref_cnt == 1,
				"sdb_llist_destroy() did not deref element %s",
				golden_data[i].name);
	}
}
END_TEST

START_TEST(test_llist_append)
{
	size_t i;

	fail_unless(sdb_llist_len(list) == 0,
			"sdb_llist_len(<empty list>) = %zu; expected: 0",
			sdb_llist_len(list));

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		int check = sdb_llist_append(list, &golden_data[i]);
		fail_unless(check == 0,
				"sdb_llist_append(%s) = %i; expected: 0",
				golden_data[i].name, check);
		fail_unless(golden_data[i].ref_cnt == 2,
				"sdb_llist_append(%s) did not take ownership",
				golden_data[i].name);
		fail_unless(sdb_llist_len(list) == i + 1,
				"sdb_llist_len(<empty list>) = %zu; expected: zu",
				sdb_llist_len(list), i + 1);
	}
}
END_TEST

START_TEST(test_llist_insert)
{
	size_t i;
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		int check = sdb_llist_insert(list, &golden_data[i], 0);
		fail_unless(check == 0,
				"sdb_llist_insert(%s, 0) = %i; expected: 0",
				golden_data[i].name, check);
		fail_unless(golden_data[i].ref_cnt == 2,
				"sdb_llist_insert(%s, 0) did not take ownership",
				golden_data[i].name);
	}
}
END_TEST

START_TEST(test_validate_insert)
{
	size_t i;
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		/* none of these operations will succeed
		 * => 1 is invalid for each case */
		int check = sdb_llist_insert(list, &golden_data[i], 1);
		fail_unless(check == -1,
				"sdb_llist_insert(%s, 1) = %i; expected: -1",
				golden_data[i].name, check);
		fail_unless(golden_data[i].ref_cnt == 1,
				"sdb_llist_insert(%s, 1) took ownership",
				golden_data[i].name);
	}
}
END_TEST

START_TEST(test_llist_get)
{
	size_t i;
	populate();
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_object_t *check = sdb_llist_get(list, i);
		fail_unless(check == &golden_data[i],
				"sdb_llist_get() = %p; expected: %p",
				check, &golden_data[i]);
		fail_unless(check->ref_cnt == 3,
				"sdb_llist_get() didn't increment reference count; got: %i; "
				"expected: 3", check->ref_cnt);
		sdb_object_deref(check);
	}
}
END_TEST

START_TEST(test_remove_by_name)
{
	/* "random" indexes */
	int indexes[] = { 4, 5, 3, 6, 2, 0, 1 };
	size_t i;

	populate();

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(indexes); ++i) {
		sdb_object_t *check;

		fail_unless((size_t)indexes[i] < SDB_STATIC_ARRAY_LEN(golden_data),
				"INTERNAL ERROR: invalid index %i", indexes[i]);

		check = sdb_llist_remove_by_name(list, golden_data[indexes[i]].name);
		fail_unless(check == &golden_data[indexes[i]],
				"sdb_llist_remove_by_name() = %p; expected: %p",
				check, &golden_data[indexes[i]]);
		fail_unless(check->ref_cnt == 2,
				"sdb_llist_remove_by_name() returned unexpected reference "
				"count; got: %i; expected: 2", check->ref_cnt);

		check = sdb_llist_remove_by_name(list, golden_data[indexes[i]].name);
		fail_unless(check == NULL,
				"sdb_llist_remove_by_name() did not remove the element");
	}
}
END_TEST

static int
dummy_lookup(const sdb_object_t __attribute__((unused)) *obj,
		const void __attribute__((unused)) *user_data)
{
	return 0;
} /* dummy_lookup */

START_TEST(test_llist_search)
{
	size_t i;
	populate();
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_object_t *check = sdb_llist_search_by_name(list,
				golden_data[i].name);
		fail_unless(check == &golden_data[i],
				"sdb_llist_search_by_name(%s) = NULL; expected: %p",
				golden_data[i].name, &golden_data[i]);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(unused_names); ++i) {
		sdb_object_t *check = sdb_llist_search_by_name(list,
				unused_names[i]);
		fail_unless(check == NULL,
				"sdb_llist_search_by_name(%s) = %p; expected: NULL",
				unused_names[i], check);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		/* dummy_lookup always return 0, thus, this will always return the
		 * first element */
		sdb_object_t *check = sdb_llist_search(list, dummy_lookup, NULL);
		fail_unless(check == &golden_data[i],
				"sdb_llist_search() = %p (%s); expected: %p (%s)",
				check, check->name, &golden_data[i], golden_data[i].name);

		/* => remove the first element */
		check = sdb_llist_remove(list, dummy_lookup, NULL);
		fail_unless(check == &golden_data[i],
				"sdb_llist_remove() = %p (%s); expected: %p (%s)",
				check, check->name, &golden_data[i], golden_data[i].name);
		fail_unless(check->ref_cnt == 2,
				"sdb_llist_remove() changed reference count; got: %i; "
				"expected: 2", check->ref_cnt);
	}
	/* should now be empty */
	fail_unless(sdb_llist_len(list) == 0,
			"Still have %i elements in the list; expected: 0",
			sdb_llist_len(list));
}
END_TEST

START_TEST(test_llist_shift)
{
	size_t i;
	populate();
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_object_t *check = sdb_llist_shift(list);
		fail_unless(check == &golden_data[i],
				"sdb_llist_shift() = NULL; expected: %p",
				&golden_data[i]);
		fail_unless(check->ref_cnt == 2,
				"sdb_llist_shift() changed reference count; got: %i; "
				"expected: 2", check->ref_cnt);
	}

	/* must be empty now */
	fail_unless(sdb_llist_shift(list) == NULL,
			"sdb_llist_shift() returned value; expected: NULL");
}
END_TEST

START_TEST(test_llist_iter)
{
	sdb_llist_iter_t *iter;
	size_t i;

	populate();

	iter = sdb_llist_get_iter(list);
	fail_unless(iter != NULL,
			"sdb_llist_get_iter() did not return an iterator");

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_object_t *check;
		fail_unless(sdb_llist_iter_has_next(iter),
				"sdb_llist_iter_has_next() = FALSE; expected: TRUE");
		check = sdb_llist_iter_get_next(iter);
		fail_unless(check == &golden_data[i],
				"sdb_llist_iter_get_next() = %p; expected: %p",
				check, &golden_data[i]);
	}

	fail_unless(!sdb_llist_iter_has_next(iter),
			"sdb_llist_iter_has_next() = TRUE; expected: FALSE");
	fail_unless(sdb_llist_iter_get_next(iter) == NULL,
			"sdb_llist_iter_get_next() returned value; expected: NULL");
	sdb_llist_iter_destroy(iter);
}
END_TEST

START_TEST(test_llist_iter_remove)
{
	sdb_llist_iter_t *iter;
	sdb_object_t *check;
	size_t i;

	populate();

	iter = sdb_llist_get_iter(list);
	fail_unless(iter != NULL,
			"sdb_llist_get_iter() did not return an iterator");

	i = 0;
	while (sdb_llist_iter_has_next(iter)) {
		check = sdb_llist_iter_get_next(iter);
		fail_unless(check == &golden_data[i],
				"sdb_llist_iter_get_next() = %p; expected: %p",
				check, &golden_data[i]);

		sdb_llist_iter_remove_current(iter);
		++i;
	}
	sdb_llist_iter_destroy(iter);

	fail_unless(i == (size_t)SDB_STATIC_ARRAY_LEN(golden_data),
			"iterated for %zu steps; expected: %i",
			i, SDB_STATIC_ARRAY_LEN(golden_data));

	/* all elements should be removed */
	check = sdb_llist_shift(list);
	fail_unless(check == NULL,
			"sdb_llist_shift() = %p; expected: NULL", check);
}
END_TEST

Suite *
util_llist_suite(void)
{
	Suite *s = suite_create("utils::llist");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_llist_clone);
	tcase_add_test(tc, test_llist_destroy);
	tcase_add_test(tc, test_llist_append);
	tcase_add_test(tc, test_llist_insert);
	tcase_add_test(tc, test_validate_insert);
	tcase_add_test(tc, test_llist_get);
	tcase_add_test(tc, test_remove_by_name);
	tcase_add_test(tc, test_llist_search);
	tcase_add_test(tc, test_llist_shift);
	tcase_add_test(tc, test_llist_iter);
	tcase_add_test(tc, test_llist_iter_remove);
	suite_add_tcase(s, tc);

	return s;
} /* util_llist_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

