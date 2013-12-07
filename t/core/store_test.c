/*
 * SysDB - t/core/store_test.c
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

#include "core/store.h"
#include "libsysdb_test.h"

#include <check.h>

START_TEST(test_store_host)
{
	struct {
		const char *name;
		sdb_time_t  last_update;
		int         expected;
	} golden_data[] = {
		{ "a", 2, 0 },
		{ "a", 3, 0 },
		{ "a", 1, 1 },
		{ "b", 2, 0 },
		{ "b", 1, 1 },
		{ "A", 1, 1 }, /* case-insensitive */
		{ "A", 4, 0 },
	};

	struct {
		const char *name;
		_Bool       has;
	} golden_hosts[] = {
		{ "a", 1 == 1 },
		{ "b", 1 == 1 },
		{ "c", 0 == 1 },
		{ "A", 1 == 1 },
	};

	size_t i;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		int status;

		status = sdb_store_host(golden_data[i].name,
				golden_data[i].last_update);
		fail_unless(status == golden_data[i].expected,
				"sdb_store_host(%s, %d) = %d; expected: %d",
				golden_data[i].name, (int)golden_data[i].last_update,
				status, golden_data[i].expected);
	}

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_hosts); ++i) {
		_Bool has;

		has = sdb_store_has_host(golden_hosts[i].name);
		fail_unless(has == golden_hosts[i].has,
				"sdb_store_has_host(%s) = %d; expected: %d",
				golden_hosts[i].name, has, golden_hosts[i].has);
	}
}
END_TEST

START_TEST(test_store_attr)
{
	struct {
		const char *host;
		const char *key;
		const char *value;
		sdb_time_t  last_update;
		int         expected;
	} golden_data[] = {
		{ "k", "k", "v", 1, -1 },
		{ "k", "k", "v", 1, -1 }, /* retry to ensure the host is not created */
		{ "l", "k1", "v1", 1, 0 },
		{ "l", "k1", "v2", 2, 0 },
		{ "l", "k1", "v3", 1, 1 },
		{ "l", "k2", "v1", 1, 0 },
		{ "m", "k", "v1", 2, 0 },
		{ "m", "k", "v2", 1, 1 },
	};

	size_t i;

	sdb_store_host("l", 1);
	sdb_store_host("m", 1);
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		int status;

		status = sdb_store_attribute(golden_data[i].host,
				golden_data[i].key, golden_data[i].value,
				golden_data[i].last_update);
		fail_unless(status == golden_data[i].expected,
				"sdb_store_attribute(%s, %s, %s, %d) = %d; expected: %d",
				golden_data[i].host, golden_data[i].key, golden_data[i].value,
				golden_data[i].last_update, status, golden_data[i].expected);
	}
}
END_TEST

START_TEST(test_store_service)
{
	struct {
		const char *host;
		const char *svc;
		sdb_time_t  last_update;
		int         expected;
	} golden_data[] = {
		{ "k", "s", 1, -1 },
		{ "k", "s", 1, -1 }, /* retry to ensure the host is not created */
		{ "l", "s1", 1, 0 },
		{ "l", "s1", 2, 0 },
		{ "l", "s1", 1, 1 },
		{ "l", "s2", 1, 0 },
		{ "m", "s", 2, 0 },
		{ "m", "s", 1, 1 },
	};

	size_t i;

	sdb_store_host("m", 1);
	sdb_store_host("l", 1);
	for (i = 0; i < SDB_STATIC_ARRAY_LEN(golden_data); ++i) {
		int status;

		status = sdb_store_service(golden_data[i].host,
				golden_data[i].svc, golden_data[i].last_update);
		fail_unless(status == golden_data[i].expected,
				"sdb_store_attribute(%s, %s, %d) = %d; expected: %d",
				golden_data[i].host, golden_data[i].svc,
				golden_data[i].last_update, status, golden_data[i].expected);
	}
}
END_TEST

Suite *
core_store_suite(void)
{
	Suite *s = suite_create("core::store");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_test(tc, test_store_host);
	tcase_add_test(tc, test_store_attr);
	tcase_add_test(tc, test_store_service);
	suite_add_tcase(s, tc);

	return s;
} /* core_store_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

