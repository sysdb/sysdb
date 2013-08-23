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

#include <check.h>

#include "libsysdb_test.h"
#include "utils/llist.h"

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
	int i;
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		int check = sdb_llist_append(list, &golden_data[i]);
		fail_unless(check == 0,
				"sdb_llist_append(%s) = %i; expected: 0",
				golden_data[i].name, check);
	}
} /* populate */

START_TEST(test_append)
{
	int i;
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		int check = sdb_llist_append(list, &golden_data[i]);
		fail_unless(check == 0,
				"sdb_llist_append(%s) = %i; expected: 0",
				golden_data[i].name, check);
		fail_unless(golden_data[i].ref_cnt == 2,
				"sdb_llist_append(%s) did not take ownership",
				golden_data[i].name);
	}
}
END_TEST

START_TEST(test_insert)
{
	int i;
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

START_TEST(test_insert_invalid)
{
	int i;
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

START_TEST(test_search)
{
	int i;
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
}
END_TEST

START_TEST(test_shift)
{
	int i;
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

START_TEST(test_iter)
{
	sdb_llist_iter_t *iter;
	int i;

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

Suite *
util_llist_suite(void)
{
	Suite *s = suite_create("utils::llist");

	TCase *tc_core = tcase_create("core");
	tcase_add_checked_fixture(tc_core, setup, teardown);
	tcase_add_test(tc_core, test_append);
	tcase_add_test(tc_core, test_insert);
	tcase_add_test(tc_core, test_insert_invalid);
	tcase_add_test(tc_core, test_search);
	tcase_add_test(tc_core, test_shift);
	tcase_add_test(tc_core, test_iter);
	suite_add_tcase(s, tc_core);

	return s;
} /* util_llist_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

