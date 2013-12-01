/*
 * SysDB - t/core/object_test.c
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

#include "core/object.h"
#include "libsysdb_test.h"

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

START_TEST(test_obj_create)
{
	struct noop {
		sdb_object_t super;
		int data;
	};
	sdb_type_t noop_type = {
		/* size = */ sizeof(struct noop),
		/* init = */ obj_init_noop,
		/* destroy = */ obj_destroy_noop,
	};

	sdb_object_t *obj;
	const char *name = "test-object";

	init_noop_called = 0;
	init_noop_retval = 0;
	destroy_noop_called = 0;
	obj = sdb_object_create(name, noop_type);
	fail_unless(obj != NULL,
			"sdb_object_create() = NULL; expected: a new object");
	fail_unless(obj->type.size == noop_type.size,
			"after sdb_object_create(): type size mismatch; got: %zu; "
			"expected: %zu", obj->type.size, noop_type.size);
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
	/* the memory address at 'obj' is no longer valid but usually this check
	 * should still work */
	fail_unless(obj->ref_cnt == 0,
			"after sdb_object_deref(): obj->ref_cnt = %d; expected: 0",
			obj->ref_cnt);
	fail_unless(destroy_noop_called == 1,
			"sdb_object_deref() did not call object's destroy function");

	init_noop_called = 0;
	init_noop_retval = -1;
	destroy_noop_called = 0;
	obj = sdb_object_create(name, noop_type);
	fail_unless(obj == NULL,
			"sdb_object_create() = %p; expected NULL (init returned -1)",
			obj);
	fail_unless(init_noop_called == 1,
			"sdb_object_create() did not call object's init function");
	fail_unless(destroy_noop_called == 1,
			"sdb_object_create() did not call object's destroy function "
			"after init failure");

	noop_type.size = 1;
	init_noop_called = 0;
	init_noop_retval = 0;
	destroy_noop_called = 0;
	obj = sdb_object_create(name, noop_type);
	fail_unless(obj == NULL,
			"sdb_object_create() = %p; expected NULL (type's size too small)",
			obj);
	fail_unless(init_noop_called == 0,
			"sdb_object_create() called object's init function "
			"when size was too small");
	fail_unless(destroy_noop_called == 0,
			"sdb_object_create() called object's destroy function "
			"when size was too small");

	noop_type.size = sizeof(struct noop);
	init_noop_retval = 0;
	noop_type.init = NULL;
	obj = sdb_object_create(name, noop_type);
	fail_unless(obj != NULL,
			"sdb_object_create() fails without init callback");
	sdb_object_deref(obj);

	noop_type.destroy = NULL;
	obj = sdb_object_create(name, noop_type);
	fail_unless(obj != NULL,
			"sdb_object_create() fails without destroy callback");
	sdb_object_deref(obj);
}
END_TEST

Suite *
core_object_suite(void)
{
	Suite *s = suite_create("core::object");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_test(tc, test_obj_create);
	suite_add_tcase(s, tc);

	return s;
} /* core_object_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

