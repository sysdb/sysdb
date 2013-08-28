/*
 * SysDB - t/utils/dbi_test.c
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

#include "libsysdb_test.h"
#include "utils/dbi.h"

#include <check.h>
#include <dbi/dbi.h>

#define TEST_MAGIC ((void *)0x1337)

/*
 * private variables
 */

static sdb_dbi_client_t *client;

/*
 * mocked functions
 */

/* dbi_driver, dbi_conn, dbi_result are void pointers */

dbi_driver
dbi_driver_open(const char *name)
{
	if (strcmp(name, "mockdriver"))
		return NULL;
	return (dbi_driver)strdup(name);
} /* dbi_driver_open */

dbi_driver
dbi_driver_list(dbi_driver curr)
{
	if (!curr)
		return "mockdriver";
	return NULL;
} /* dbi_driver_list */

const char *
dbi_driver_get_name(dbi_driver driver)
{
	return (const char *)driver;
} /* dbi_driver_get_name */

int
dbi_conn_set_option(dbi_conn __attribute__((unused)) conn,
		const char __attribute__((unused)) *key,
		const char __attribute__((unused)) *value)
{
	return 0;
} /* dbi_conn_set_option */

const char *
dbi_conn_get_option_list(dbi_conn __attribute__((unused)) conn,
		const char __attribute__((unused)) *key)
{
	return NULL;
} /* dbi_conn_get_option_list */

dbi_conn
dbi_conn_open(dbi_driver driver)
{
	if (strcmp((const char *)driver, "mockdriver"))
		return NULL;
	return (dbi_conn)"mockconnection";
} /* dbi_conn_open */

static int dbi_conn_connect_called = 0;
int
dbi_conn_connect(dbi_conn conn)
{
	++dbi_conn_connect_called;
	if (strcmp((const char *)conn, "mockconnection"))
		return DBI_ERROR_NOCONN;
	return 0;
} /* dbi_conn_connect */

int
dbi_conn_ping(dbi_conn conn)
{
	if (strcmp((const char *)conn, "mockconnection"))
		return 0;
	return 1;
} /* dbi_conn_connect */

void
dbi_conn_close(dbi_conn __attribute__((unused)) conn)
{
	return;
} /* dbi_conn_close */

static int dbi_conn_query_called = 0;
dbi_result
dbi_conn_query(dbi_conn conn, const char __attribute__((unused)) *statement)
{
	++dbi_conn_query_called;
	if (strcmp((const char *)conn, "mockconnection"))
		return NULL;
	return (dbi_result)"mockresult";
} /* dbi_conn_query */

unsigned long long
dbi_result_get_numrows(dbi_result res)
{
	if (strcmp((const char *)res, "mockresult"))
		return DBI_ROW_ERROR;
	return 5;
} /* dbi_result_get_numrows */

static int dbi_result_free_called = 0;
int
dbi_result_free(dbi_result res)
{
	++dbi_result_free_called;
	if (strcmp((const char *)res, "mockresult"))
		return -1;
	return 0;
} /* dbi_result_free */

unsigned int
dbi_result_get_numfields(dbi_result res)
{
	if (strcmp((const char *)res, "mockresult"))
		return DBI_FIELD_ERROR;
	return 0;
} /* dbi_result_get_numfields */

unsigned short
dbi_result_get_field_type_idx(dbi_result res,
		unsigned int __attribute__((unused)) i)
{
	if (strcmp((const char *)res, "mockresult"))
		return DBI_TYPE_ERROR;
	return DBI_TYPE_ERROR;
} /* dbi_result_get_field_type_idx */

const char *
dbi_result_get_field_name(dbi_result res,
		unsigned int __attribute__((unused)) i)
{
	if (strcmp((const char *)res, "mockresult"))
		return NULL;
	return NULL;
} /* dbi_result_get_field_name */

/*
 * private helper functions
 */

static void
setup(void)
{
	client = sdb_dbi_client_create("mockdriver", "mockdatabase");
	fail_unless(client != NULL,
			"sdb_dbi_client_create() = NULL; expected client object");
} /* setup */

static void
connect(void)
{
	int check = sdb_dbi_client_connect(client);
	fail_unless(check == 0,
			"sdb_dbi_client_connect() = %i; expected: 0", check);
} /* connect */

static void
teardown(void)
{
	sdb_dbi_client_destroy(client);
	client = NULL;
} /* teardown */

static int test_query_callback_called = 0;
static int
test_query_callback(sdb_dbi_client_t *c,
		size_t n, sdb_data_t *data, sdb_object_t *user_data)
{
	++test_query_callback_called;

	fail_unless(c == client,
			"query callback received unexpected client = %p; "
			"expected: %p", c, client);
	fail_unless(n == 0,
			"query callback received n = %i; expected: 0", n);
	fail_unless(data == NULL,
			"query callback received data = %p; expected: NULL", data);
	fail_unless(user_data == TEST_MAGIC,
			"query callback received user_data = %p; expected: %p",
			user_data, TEST_MAGIC);
	return 0;
} /* test_query_callback */

/*
 * tests
 */

START_TEST(test_client_connect)
{
	int check = sdb_dbi_client_connect(client);
	fail_unless(check == 0,
			"sdb_dbi_client_connect() = %i; expected: 0", check);

	fail_unless(dbi_conn_connect_called == 1,
			"sdb_dbi_client_create() called dbi_conn_connect %i times; "
			"expected: 1", dbi_conn_connect_called);
}
END_TEST

START_TEST(test_client_check_conn)
{
	int check = sdb_dbi_client_check_conn(client);
	fail_unless(check == 0,
			"sdb_dbi_client_check_conn() = %i; expected: 0", check);

	/* the first call will actually connect to the database */
	fail_unless(dbi_conn_connect_called == 1,
			"sdb_dbi_client_check_conn() called dbi_conn_connect %i times; "
			"expected: 1", dbi_conn_connect_called);

	check = sdb_dbi_client_check_conn(client);
	fail_unless(check == 0,
			"sdb_dbi_client_check_conn() = %i; expected: 0", check);

	/* should not reconnect */
	fail_unless(dbi_conn_connect_called == 1,
			"sdb_dbi_client_check_conn() called dbi_conn_connect %i time(s); "
			"expected: 0", dbi_conn_connect_called - 1);
}
END_TEST

START_TEST(test_exec_query)
{
	int check = sdb_dbi_exec_query(client, "mockquery", test_query_callback,
			/* user_data = */ TEST_MAGIC, /* n = */ 0);
	/* not connected yet */
	fail_unless(check < 0,
			"sdb_dbi_exec_query() = %i; expected: < 0", check);

	connect();

	check = sdb_dbi_exec_query(client, "mockquery", test_query_callback,
			/* user_data = */ TEST_MAGIC, /* n = */ 0);
	fail_unless(check == 0,
			"sdb_dbi_exec_query() = %i; expected: 0", check);

	fail_unless(dbi_conn_query_called == 1,
			"sdb_dbi_exec_query() called dbi_conn_query %i times; "
			"expected: 1", dbi_conn_query_called);
	fail_unless(test_query_callback_called == 0,
			"sdb_dbi_exec_query() did not call the registered callback "
			"for each result row; got %i call%s; expected: 0",
			test_query_callback_called,
			(test_query_callback_called == 1) ? "" : "s");

	fail_unless(dbi_result_free_called == 1,
			"sdb_dbi_exec_query() did not free the query result object");
}
END_TEST

/*
 * test API
 */

Suite *
util_dbi_suite(void)
{
	Suite *s = suite_create("utils::dbi");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_client_connect);
	tcase_add_test(tc, test_client_check_conn);
	tcase_add_test(tc, test_exec_query);
	suite_add_tcase(s, tc);

	return s;
} /* util_llist_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

