/*
 * SysDB - t/core/store_lookup_test.c
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

#include "core/store.h"
#include "libsysdb_test.h"

#include <check.h>
#include <string.h>

static void
populate(void)
{
	const char *hosts[] = { "a", "b", "c" };

	struct {
		const char *host;
		const char *service;
	} services[] = {
		{ "a", "s1" },
		{ "a", "s2" },
		{ "b", "s1" },
		{ "b", "s3" },
	};

	struct {
		const char *host;
		const char *name;
		sdb_data_t  value;
	} attrs[] = {
		{ "a", "k1", { SDB_TYPE_STRING, { .string = "v1" } } },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(hosts); ++i) {
		int status = sdb_store_host(hosts[i], 1);
		fail_unless(status == 0,
				"sdb_store_host(%s, 1) = %d; expected: 0",
				hosts[i], status);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(services); ++i) {
		int status = sdb_store_service(services[i].host,
				services[i].service, 1);
		fail_unless(status == 0,
				"sdb_store_service(%s, %s, 1) = %d; expected: 0",
				services[i].host, services[i].service, status);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(attrs); ++i) {
		int status = sdb_store_attribute(attrs[i].host,
				attrs[i].name, &attrs[i].value, 1);
		fail_unless(status == 0,
				"sdb_store_attribute(%s, %s, <val>, 1) = %d; expected: 0",
				attrs[i].host, attrs[i].name, status);
	}
} /* populate */

START_TEST(test_store_match)
{
	sdb_store_base_t *obj;

	struct {
		const char *hostname;
		const char *hostname_re;

		const char *service_name;
		const char *service_name_re;

		const char *attr_name;
		const char *attr_name_re;
		const char *attr_value;
		const char *attr_value_re;

		int expected;
	} golden_data[] = {
		{
			/* host */ NULL, NULL,
			/* svc  */ NULL, NULL,
			/* attr */ NULL, NULL, NULL, NULL, 0
		},
		{
			/* host */ "a", NULL,
			/* svc  */ NULL, NULL,
			/* attr */ NULL, NULL, NULL, NULL, 0
		},
		{
			/* host */ "b", NULL,
			/* svc  */ NULL, NULL,
			/* attr */ NULL, NULL, NULL, NULL, -1
		},
		{
			/* host */ NULL, "^a$",
			/* svc  */ NULL, NULL,
			/* attr */ NULL, NULL, NULL, NULL, 0
		},
		{
			/* host */ NULL, "^b$",
			/* svc  */ NULL, NULL,
			/* attr */ NULL, NULL, NULL, NULL, -1
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ NULL, NULL,
			/* attr */ NULL, NULL, NULL, NULL, 0
		},
		{
			/* host */ "a", "^b$",
			/* svc  */ NULL, NULL,
			/* attr */ NULL, NULL, NULL, NULL, -1
		},
		{
			/* host */ "b", "^a$",
			/* svc  */ NULL, NULL,
			/* attr */ NULL, NULL, NULL, NULL, -1
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "s1", NULL,
			/* attr */ NULL, NULL, NULL, NULL, 0
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ NULL, "^s1$",
			/* attr */ NULL, NULL, NULL, NULL, 0
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "s1", "^s1$",
			/* attr */ NULL, NULL, NULL, NULL, 0
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "x1", NULL,
			/* attr */ NULL, NULL, NULL, NULL, -1
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ NULL, "x",
			/* attr */ NULL, NULL, NULL, NULL, -1
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "x1", "x",
			/* attr */ NULL, NULL, NULL, NULL, -1
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "s1", "x",
			/* attr */ NULL, NULL, NULL, NULL, -1
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "x1", "s",
			/* attr */ NULL, NULL, NULL, NULL, -1
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "s1", "^s1$",
			/* attr */ "k1", NULL, NULL, NULL, 0
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "s1", "^s1$",
			/* attr */ NULL, "^k", NULL, NULL, 0
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "s1", "^s1$",
			/* attr */ NULL, NULL, "v1", NULL, 0
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "s1", "^s1$",
			/* attr */ NULL, NULL, NULL, "^v1$", 0
		},
		{
			/* host */ "a", "^a$",
			/* svc  */ "s1", "^s1$",
			/* attr */ "k1", "1", "v1", "1", 0
		},
	};

	size_t i;

	obj = sdb_store_get_host("a");
	fail_unless(obj != NULL,
			"sdb_store_get_host(a) = NULL; expected: <host>");

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		sdb_store_matcher_t *h, *s, *a;
		int status;

		s = sdb_store_service_matcher(golden_data[i].service_name,
				golden_data[i].service_name_re, NULL);
		fail_unless(s != NULL,
				"sdb_store_service_matcher() = NULL; expected: <matcher>");

		a = sdb_store_attr_matcher(golden_data[i].attr_name,
				golden_data[i].attr_name_re, golden_data[i].attr_value,
				golden_data[i].attr_value_re);
		fail_unless(a != NULL,
				"sdb_store_attr_matcher() = NULL; expected: <matcher>");

		h = sdb_store_host_matcher(golden_data[i].hostname,
				golden_data[i].hostname_re, s, a);
		fail_unless(h != NULL,
				"sdb_store_host_matcher() = NULL: expected: <matcher>");
		/* pass ownership to the host matcher */
		sdb_object_deref(SDB_OBJ(s));
		sdb_object_deref(SDB_OBJ(a));

		status = sdb_store_matcher_matches(h, obj);
		fail_unless(status == golden_data[i].expected,
				"sdb_store_matcher_matches({{%s, %s},{%s, %s},"
				"{%s, %s, %s, %s}}, <host a>) = %d; expected: %d",
				golden_data[i].hostname, golden_data[i].hostname_re,
				golden_data[i].service_name, golden_data[i].service_name_re,
				golden_data[i].attr_name, golden_data[i].attr_name_re,
				golden_data[i].attr_value, golden_data[i].attr_value_re,
				status, golden_data[i].expected);

		sdb_object_deref(SDB_OBJ(h));
	}
}
END_TEST

Suite *
core_store_lookup_suite(void)
{
	Suite *s = suite_create("core::store_lookup");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_checked_fixture(tc, populate, sdb_store_clear);
	tcase_add_test(tc, test_store_match);
	suite_add_tcase(s, tc);

	return s;
} /* core_store_lookup_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

