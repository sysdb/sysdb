/*
 * SysDB - t/unit/core/object_test.c
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

#if HAVE_CONFIG_H
#	include "config.h"
#endif

#include "core/object.h"
#include "testutils.h"

#include <check.h>

/*
 * private data types
 */

static int init_noop_called = 0;
static int init_noop_retval = 0;
static int
obj_init_noop(sdb_object_t *obj, va_list __attribute__((unused)) ap)
{
	++init_noop_called;
	fail_unless(obj != NULL, "obj init function: received obj == NULL");
	return init_noop_retval;
} /* obj_init_noop */

static int destroy_noop_called = 0;
static void
obj_destroy_noop(sdb_object_t *obj)
{
	++destroy_noop_called;
	fail_unless(obj != NULL, "obj destroy function: received obj == NULL");
} /* obj_destroy_noop */

struct noop {
	sdb_object_t super;
	int data;
};
static sdb_type_t noop_type = {
	/* size = */ sizeof(struct noop),
	/* init = */ obj_init_noop,
	/* destroy = */ obj_destroy_noop,
};

static void *wrapped = (void *)0x42;

static int destroy_wrapper_called = 0;
static void
wrapper_destroy(void *obj)
{
	++destroy_wrapper_called;
	fail_unless(obj == wrapped,
			"wrapper_destroy received unexpected obj %p; expected: %p",
			obj, wrapped);
} /* wrapper_destroy */

START_TEST(test_obj_create)
{
	sdb_object_t *obj;
	sdb_type_t test_type = noop_type;

	const char *name = "test-object";

	init_noop_called = 0;
	init_noop_retval = 0;
	destroy_noop_called = 0;
	obj = sdb_object_create(name, test_type);
	fail_unless(obj != NULL,
			"sdb_object_create() = NULL; expected: a new object");
	fail_unless(obj->type.size == test_type.size,
			"after sdb_object_create(): type size mismatch; got: %zu; "
			"expected: %zu", obj->type.size, test_type.size);
	fail_unless(obj->type.init == obj_init_noop,
			"after sdb_object_create(): type init = %p; exptected: %p",
			obj->type.init, obj_init_noop);
	fail_unless(obj->type.destroy == obj_destroy_noop,
			"after sdb_object_create(): type destroy = %p; exptected: %p",
			obj->type.destroy, obj_destroy_noop);
	fail_unless(obj->ref_cnt == 1,
			"after sdb_object_create(): obj->ref_cnt = %d; expected: 1",
			obj->ref_cnt);
	fail_unless(!strcmp(obj->name, "test-object"),
			"after sdb_object_create(): obj->name = '%s'; expected: '%s'",
			obj->name, name);
	fail_unless(obj->name != name,
			"after sdb_object_create(): obj->name was not strdup()'ed");

	fail_unless(init_noop_called == 1,
			"sdb_object_create() did not call object's init function");
	fail_unless(destroy_noop_called == 0,
			"sdb_object_create() called object's destroy function");
	fail_unless(((struct noop *)obj)->data == 0,
			"sdb_object_create() did not initialize data to zero");

	sdb_object_deref(obj);
	fail_unless(destroy_noop_called == 1,
			"sdb_object_deref() did not call object's destroy function");

	init_noop_called = 0;
	init_noop_retval = -1;
	destroy_noop_called = 0;
	obj = sdb_object_create(name, test_type);
	fail_unless(obj == NULL,
			"sdb_object_create() = %p; expected NULL (init returned -1)",
			obj);
	fail_unless(init_noop_called == 1,
			"sdb_object_create() did not call object's init function");
	fail_unless(destroy_noop_called == 1,
			"sdb_object_create() did not call object's destroy function "
			"after init failure");

	test_type.size = 1;
	init_noop_called = 0;
	init_noop_retval = 0;
	destroy_noop_called = 0;
	obj = sdb_object_create(name, test_type);
	fail_unless(obj == NULL,
			"sdb_object_create() = %p; expected NULL (type's size too small)",
			obj);
	fail_unless(init_noop_called == 0,
			"sdb_object_create() called object's init function "
			"when size was too small");
	fail_unless(destroy_noop_called == 0,
			"sdb_object_create() called object's destroy function "
			"when size was too small");

	test_type.size = sizeof(struct noop);
	init_noop_retval = 0;
	test_type.init = NULL;
	obj = sdb_object_create(name, test_type);
	fail_unless(obj != NULL,
			"sdb_object_create() fails without init callback");
	sdb_object_deref(obj);

	test_type.destroy = NULL;
	obj = sdb_object_create(name, test_type);
	fail_unless(obj != NULL,
			"sdb_object_create() fails without destroy callback");
	sdb_object_deref(obj);

	init_noop_called = 0;
	obj = sdb_object_create_simple(name, sizeof(struct noop), NULL);
	fail_unless(obj != NULL,
			"sdb_object_create_simple() = NULL; expected: <obj>");
	fail_unless(obj->type.size == sizeof(struct noop),
			"sdb_object_create_simple() created object of size %zu; "
			"expected: %zu", obj->type.size, sizeof(struct noop));
	fail_unless(obj->type.init == NULL,
			"sdb_object_create_simple() did not set init=NULL");
	fail_unless(obj->type.destroy == NULL,
			"sdb_object_create_simple() did not set destroy=NULL");
	fail_unless(init_noop_called == 0,
			"sdb_object_create_simple() unexpectedly called noop's init");
	sdb_object_deref(obj);

	obj = sdb_object_create_T(NULL, struct noop);
	fail_unless(obj != NULL,
			"sdb_object_create_simple() = NULL; expected: <obj>");
	fail_unless(obj->type.size == sizeof(struct noop),
			"sdb_object_create_simple() created object of size %zu; "
			"expected: %zu", obj->type.size, sizeof(struct noop));
	fail_unless(obj->type.init == NULL,
			"sdb_object_create_simple() did not set init=NULL");
	fail_unless(obj->type.destroy == NULL,
			"sdb_object_create_simple() did not set destroy=NULL");
	fail_unless(init_noop_called == 0,
			"sdb_object_create_simple() unexpectedly called noop's init");
	sdb_object_deref(obj);
}
END_TEST

START_TEST(test_obj_wrapper)
{
	sdb_object_t *obj;
	const char *name = "wrapped-object";

	destroy_wrapper_called = 0;
	obj = sdb_object_create_wrapper(name, wrapped, wrapper_destroy);
	fail_unless(obj != NULL,
			"sdb_object_create_wrapper() = NULL; expected: wrapper object");
	fail_unless(obj->ref_cnt == 1,
			"after sdb_object_create_wrapper(); obj->ref_cnt = %d; "
			"expected: 1", obj->ref_cnt);
	fail_unless(!strcmp(obj->name, name),
			"after sdb_object_create_wrapper(); obj->name = %s; expected: %s",
			obj->name, name);
	fail_unless(obj->name != name,
			"sdb_object_create_wrapper() did not copy object name");
	fail_unless(SDB_OBJ_WRAPPER(obj)->data == wrapped,
			"wrapped object wraps unexpected data %p; expected: %p",
			SDB_OBJ_WRAPPER(obj)->data, wrapped);

	fail_unless(destroy_wrapper_called == 0,
			"sdb_object_create_wrapper() called object's destructor");

	sdb_object_deref(obj);
	fail_unless(destroy_wrapper_called == 1,
			"sdb_object_deref() did not call wrapped object's destructor");
}
END_TEST

START_TEST(test_obj_ref)
{
	sdb_object_t *obj;
	sdb_type_t test_type = noop_type;

	init_noop_called = 0;
	init_noop_retval = 0;
	destroy_noop_called = 0;

	obj = sdb_object_create("test-object", test_type);
	fail_unless(obj != NULL,
			"sdb_object_create() = NULL; expected: valid object");

	sdb_object_ref(obj);
	fail_unless(obj->ref_cnt == 2,
			"after db_object_ref(): obj->ref_cnt = %d; expected: 2",
			obj->ref_cnt);

	sdb_object_ref(obj);
	fail_unless(obj->ref_cnt == 3,
			"after db_object_ref(): obj->ref_cnt = %d; expected: 3",
			obj->ref_cnt);

	obj->ref_cnt = 42;
	sdb_object_ref(obj);
	fail_unless(obj->ref_cnt == 43,
			"after db_object_ref(): obj->ref_cnt = %d; expected: 43",
			obj->ref_cnt);

	fail_unless(init_noop_called == 1,
			"after some sdb_object_ref(); object's init called %d times; "
			"expected: 1", init_noop_called);
	fail_unless(destroy_noop_called == 0,
			"after some sdb_object_ref(); object's destroy called %d time%s; "
			"expected: 0", destroy_noop_called == 1 ? "" : "2",
			destroy_noop_called);

	sdb_object_deref(obj);
	fail_unless(obj->ref_cnt == 42,
			"after db_object_deref(): obj->ref_cnt = %d; expected: 42",
			obj->ref_cnt);

	obj->ref_cnt = 23;
	sdb_object_deref(obj);
	fail_unless(obj->ref_cnt == 22,
			"after db_object_deref(): obj->ref_cnt = %d; expected: 22",
			obj->ref_cnt);

	fail_unless(init_noop_called == 1,
			"after some sdb_object_{de,}ref(); object's init called %d times; "
			"expected: 1", init_noop_called);
	fail_unless(destroy_noop_called == 0,
			"after some sdb_object_{de,}ref(); object's destroy called "
			"%d time%s; expected: 0", destroy_noop_called == 1 ? "" : "2",
			destroy_noop_called);

	obj->ref_cnt = 1;
	sdb_object_deref(obj);
	fail_unless(init_noop_called == 1,
			"after some sdb_object_{de,}ref(); object's init called %d times; "
			"expected: 1", init_noop_called);
	fail_unless(destroy_noop_called == 1,
			"after some sdb_object_{de,}ref(); object's destroy called "
			"%d times; expected: 1", destroy_noop_called);

	/* this should work */
	sdb_object_deref(NULL);
}
END_TEST

START_TEST(test_obj_cmp)
{
	sdb_object_t *obj1, *obj2, *obj3, *obj4;
	int status;

	obj1 = sdb_object_create("a", noop_type);
	obj2 = sdb_object_create("b", noop_type);
	obj3 = sdb_object_create("B", noop_type);
	obj4 = sdb_object_create("c", noop_type);

	status = sdb_object_cmp_by_name(obj1, obj2);
	fail_unless(status == -1,
			"sdb_object_cmp_by_name('a', 'b') = %d; expected: -1", status);
	status = sdb_object_cmp_by_name(obj2, obj3);
	fail_unless(status == 0,
			"sdb_object_cmp_by_name('b', 'B') = %d; expected: 0", status);
	status = sdb_object_cmp_by_name(obj4, obj3);
	fail_unless(status == 1,
			"sdb_object_cmp_by_name('c', 'B') = %d; expected: 1", status);
	status = sdb_object_cmp_by_name(obj1, obj1);
	fail_unless(status == 0,
			"sdb_object_cmp_by_name('a', 'a') = %d; expected: 0", status);

	sdb_object_deref(obj1);
	sdb_object_deref(obj2);
	sdb_object_deref(obj3);
	sdb_object_deref(obj4);
}
END_TEST

TEST_MAIN("core::object")
{
	TCase *tc = tcase_create("core");
	tcase_add_test(tc, test_obj_create);
	tcase_add_test(tc, test_obj_wrapper);
	tcase_add_test(tc, test_obj_ref);
	tcase_add_test(tc, test_obj_cmp);
	ADD_TCASE(tc);
}
TEST_MAIN_END

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

