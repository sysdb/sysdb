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
 * private data-types
 */
typedef union {
	long long   integer;
	double      decimal;
	const char *string;
	time_t      datetime;
	struct {
		size_t length;
		const unsigned char *datum;
	} binary;
} mock_data_t;

typedef struct {
	const char *name;
	unsigned long long nrows;
	unsigned long long current_row;
	unsigned int    nfields;
	unsigned short *field_types;
	char          **field_names;
} mock_query_t;

/*
 * private variables
 */

static sdb_dbi_client_t *client;

/*
 * mock queries
 */

/* field definitions */
static struct {
	unsigned short field_types[1];
	char          *field_names[1];
} rows1[] = {
	{ { DBI_TYPE_INTEGER }, { "field0" }, },
};

static mock_data_t golden_data[][1] = {
	{ { .integer = 1234 } },
	{ { .integer = 2345 } },
	{ { .integer = 3456 } },
	{ { .integer = 4567 } },
	{ { .integer = 5678 } },
};

/* query definitions */
static mock_query_t mock_queries[] = {
	{ "mockquery0", 5, 1, 0, NULL, NULL },
	{ "mockquery1", 0, 0, 1, rows1[0].field_types, rows1[0].field_names },
	{ "mockquery2", 1, 1, 1, rows1[0].field_types, rows1[0].field_names },
	{ "mockquery3", 5, 1, 1, rows1[0].field_types, rows1[0].field_names },
};

static mock_query_t *current_query = NULL;

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

static unsigned long long dbi_conn_connect_called = 0;
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

int
dbi_conn_error(dbi_conn conn, const char **errmsg)
{
	if ((! conn) || (strcmp((const char *)conn, "mockconnection")))
		return DBI_ERROR_BADOBJECT;
	if (errmsg)
		*errmsg = "mockerror";
	return DBI_ERROR_UNSUPPORTED;
} /* dbi_conn_error */

static unsigned long long dbi_conn_query_called = 0;
dbi_result
dbi_conn_query(dbi_conn conn, const char __attribute__((unused)) *statement)
{
	size_t i;

	++dbi_conn_query_called;
	if (strcmp((const char *)conn, "mockconnection"))
		return NULL;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(mock_queries); ++i) {
		if (!strcmp(mock_queries[i].name, statement)) {
			current_query = &mock_queries[i];
			return (dbi_result)current_query;
		}
	}
	return NULL;
} /* dbi_conn_query */

unsigned long long
dbi_result_get_numrows(dbi_result res)
{
	mock_query_t *q = res;
	if (! q)
		return DBI_ROW_ERROR;
	return q->nrows;
} /* dbi_result_get_numrows */

unsigned int
dbi_result_get_numfields(dbi_result res)
{
	mock_query_t *q = res;
	if (! q)
		return DBI_FIELD_ERROR;
	return q->nfields;
} /* dbi_result_get_numfields */

unsigned short
dbi_result_get_field_type_idx(dbi_result res, unsigned int i)
{
	mock_query_t *q = res;
	if ((! q) || (i > q->nfields))
		return DBI_TYPE_ERROR;
	return q->field_types[i - 1];
} /* dbi_result_get_field_type_idx */

const char *
dbi_result_get_field_name(dbi_result res, unsigned int i)
{
	mock_query_t *q = res;
	if ((! q) || (i > q->nfields))
		return NULL;
	return q->field_names[i - 1];
} /* dbi_result_get_field_name */

int
dbi_result_seek_row(dbi_result res, unsigned long long n)
{
	mock_query_t *q = res;
	if ((! q) || (n > q->nrows))
		return 0;

	q->current_row = n;
	return 1;
} /* dbi_result_seek_row */

static mock_data_t
get_golden_data(dbi_result res, unsigned int i) {
	mock_query_t *q = res;
	fail_unless(q != NULL, "dbi_result_get_*_idx() called with "
			"NULL result data; expected valid result object");

	/* this information is managed by seek_row and, thus,
	 * should never be invalid */
	fail_unless(q->current_row && q->current_row <= q->nrows,
			"INTERNAL ERROR: current row out of range");

	fail_unless(i && i <= q->nfields,
			"dbi_result_get_*_idx() called with index out of range; "
			"got: %u; expected [1, %u]", i, q->nfields);
	return golden_data[q->current_row - 1][i];
} /* get_golden_data */

long long
dbi_result_get_longlong_idx(dbi_result res, unsigned int i)
{
	fail_unless(current_query->field_types[i] != DBI_TYPE_INTEGER,
			"dbi_result_get_longlong_idx() called for non-integer "
			"column type %u", current_query->field_types[i]);
	return get_golden_data(res, i).integer;
} /* dbi_result_get_longlong_idx */

double
dbi_result_get_double_idx(dbi_result res, unsigned int i)
{
	fail_unless(current_query->field_types[i] != DBI_TYPE_DECIMAL,
			"dbi_result_get_double_idx() called for non-integer "
			"column type %u", current_query->field_types[i]);
	return get_golden_data(res, i).decimal;
} /* dbi_result_get_double_idx */

const char *
dbi_result_get_string_idx(dbi_result res, unsigned int i)
{
	fail_unless(current_query->field_types[i] != DBI_TYPE_STRING,
			"dbi_result_get_string_idx() called for non-integer "
			"column type %u", current_query->field_types[i]);
	return get_golden_data(res, i).string;
} /* dbi_result_get_string_idx */

time_t
dbi_result_get_datetime_idx(dbi_result res, unsigned int i)
{
	fail_unless(current_query->field_types[i] != DBI_TYPE_DATETIME,
			"dbi_result_get_datetime_idx() called for non-integer "
			"column type %u", current_query->field_types[i]);
	return get_golden_data(res, i).datetime;
} /* dbi_result_get_datetime_idx */

size_t
dbi_result_get_field_length_idx(dbi_result res, unsigned int i)
{
	/* this will check if the parameters are valid */
	get_golden_data(res, i);

	switch (current_query->field_types[i]) {
		case DBI_TYPE_INTEGER:
			break;
		case DBI_TYPE_DECIMAL:
			break;
		case DBI_TYPE_STRING:
			break;
		case DBI_TYPE_DATETIME:
			break;
		case DBI_TYPE_BINARY:
			return get_golden_data(res, i).binary.length;
			break;
	}

	fail("INTERNAL ERROR: dbi_result_get_field_length_idx() "
			"called for unexpected field type %u",
			current_query->field_types[i]);
	return 0;
} /* dbi_result_get_field_length_idx */

const unsigned char *
dbi_result_get_binary_idx(dbi_result res, unsigned int i)
{
	fail_unless(current_query->field_types[i] != DBI_TYPE_BINARY,
			"dbi_result_get_binary_idx() called for non-integer "
			"column type %u", current_query->field_types[i]);
	return get_golden_data(res, i).binary.datum;
} /* dbi_result_get_binary_idx */

static unsigned long long dbi_result_free_called = 0;
int
dbi_result_free(dbi_result res)
{
	mock_query_t *q = res;

	++dbi_result_free_called;
	if (! q)
		return -1;

	current_query = NULL;
	return 0;
} /* dbi_result_free */

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

static unsigned long long test_query_callback_called = 0;
static int
test_query_callback(sdb_dbi_client_t *c,
		size_t n, sdb_data_t *data, sdb_object_t *user_data)
{
	++test_query_callback_called;

	fail_unless(c == client,
			"query callback received unexpected client = %p; "
			"expected: %p", c, client);
	fail_unless(n == current_query->nfields,
			"query callback received n = %i; expected: %i",
			n, current_query->nfields);
	fail_unless(data != NULL,
			"query callback received data = NULL; expected: valid data");
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

	dbi_conn_connect_called = 0;
	check = sdb_dbi_client_check_conn(client);
	fail_unless(check == 0,
			"sdb_dbi_client_check_conn() = %i; expected: 0", check);

	/* should not reconnect */
	fail_unless(dbi_conn_connect_called == 0,
			"sdb_dbi_client_check_conn() called dbi_conn_connect %i time(s); "
			"expected: 0", dbi_conn_connect_called);
}
END_TEST

START_TEST(test_exec_query)
{
	size_t i;

	int check = sdb_dbi_exec_query(client, "mockquery0", test_query_callback,
			/* user_data = */ TEST_MAGIC, /* n = */ 0);
	/* not connected yet */
	fail_unless(check < 0,
			"sdb_dbi_exec_query() = %i; expected: < 0", check);

	connect();

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(mock_queries); ++i) {
		mock_query_t *q = &mock_queries[i];

		unsigned long long expected_callback_calls = 0;

		dbi_conn_query_called = 0;
		test_query_callback_called = 0;
		dbi_result_free_called = 0;

		check = sdb_dbi_exec_query(client, q->name, test_query_callback,
				/* user_data = */ TEST_MAGIC, /* n = */ (int)q->nfields,
				SDB_TYPE_INTEGER);
		fail_unless(check == 0,
				"sdb_dbi_exec_query() = %i; expected: 0", check);

		fail_unless(dbi_conn_query_called == 1,
				"sdb_dbi_exec_query() called dbi_conn_query %i times; "
				"expected: 1", dbi_conn_query_called);

		if (q->nfields)
			expected_callback_calls = q->nrows;

		fail_unless(test_query_callback_called == expected_callback_calls,
				"sdb_dbi_exec_query() did not call the registered callback "
				"for each result row; got %i call%s; expected: 0",
				test_query_callback_called,
				(test_query_callback_called == 1) ? "" : "s");

		fail_unless(dbi_result_free_called == 1,
				"sdb_dbi_exec_query() did not free the query result object");
	}
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

