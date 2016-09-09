/*
 * SysDB - t/unit/utils/dbi_test.c
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
#endif /* HAVE_CONFIG_H */

#include "testutils.h"
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
static unsigned short field_types[] = {
	DBI_TYPE_INTEGER,
	DBI_TYPE_DECIMAL,
	DBI_TYPE_STRING,
	DBI_TYPE_DATETIME,
	DBI_TYPE_BINARY,
};
static char *field_names[] = {
	"field0",
	"field1",
	"field2",
	"field3",
	"field4",
};

#define DATUM(p) ((const unsigned char *)(p))
static mock_data_t golden_data[][5] = {
	{
		{ .integer  = 1234   },
		{ .decimal  = 1.234  },
		{ .string   = "abcd" },
		{ .datetime = 0      },
		{ .binary   = { 1, DATUM("a") } },
	},
	{
		{ .integer  = 2345   },
		{ .decimal  = 23.45  },
		{ .string   = "bcde" },
		{ .datetime = 1      },
		{ .binary   = { 4, DATUM("bcde") } },
	},
	{
		{ .integer  = 3456   },
		{ .decimal  = 345.6  },
		{ .string   = "cd"   },
		{ .datetime = 2      },
		{ .binary   = { 0, DATUM(NULL) } },
	},
	{
		{ .integer  = 4567   },
		{ .decimal  = 4567   },
		{ .string   = "d"    },
		{ .datetime = 3      },
		{ .binary   = { 13, DATUM("defghijklmnop") } },
	},
	{
		{ .integer  = 5678   },
		{ .decimal  = 56.78  },
		{ .string   = "efgh" },
		{ .datetime = 4      },
		{ .binary   = { 5, DATUM("efghi") } },
	},
};

/* query definitions */
static mock_query_t mock_queries[] = {
	{ "mockquery0", 5, 1, 0, NULL, NULL },
	{ "mockquery1", 0, 0, 1, field_types, field_names },
	{ "mockquery2", 1, 1, 1, field_types, field_names },
	{ "mockquery3", 2, 1, 1, field_types, field_names },
	{ "mockquery4", 5, 1, 1, field_types, field_names },
	{ "mockquery5", 5, 1, 2, field_types, field_names },
	{ "mockquery6", 5, 1, 3, field_types, field_names },
	{ "mockquery7", 5, 1, 4, field_types, field_names },
	{ "mockquery8", 5, 1, 5, field_types, field_names },
};

static mock_query_t *current_query = NULL;

/*
 * mocked functions
 */

/* dbi_driver, dbi_conn, dbi_result are void pointers */

#if LIBDBI_VERSION < 900
typedef void *dbi_inst;

int
dbi_initialize_r(const char *driverdir, dbi_inst *pInst);
void
dbi_shutdown_r(dbi_inst inst);
dbi_driver
dbi_driver_open_r(const char *name, dbi_inst inst);
dbi_driver
dbi_driver_list_r(dbi_driver curr, dbi_inst inst);
#endif

const dbi_inst INST = (void *)0x4711;

int
dbi_initialize_r(const char __attribute__((unused)) *driverdir, dbi_inst *pInst)
{
	*pInst = INST;
	return 0;
} /* dbi_initialize_r */

void
dbi_shutdown_r(dbi_inst inst)
{
	fail_unless(inst == INST,
			"dbi_shutdown_r() called with unexpected inst=%p; expected %p",
			inst, INST);
} /* dbi_shutdown_r */

dbi_driver
dbi_driver_open_r(const char *name, dbi_inst inst)
{
	fail_unless(inst == INST,
			"dbi_driver_open_r() called with unexpected inst=%p; expected %p",
			inst, INST);

	if (strcmp(name, "mockdriver"))
		return NULL;
	return (dbi_driver)"mockdriver";
} /* dbi_driver_open */

dbi_driver
dbi_driver_list_r(dbi_driver curr, dbi_inst inst)
{
	fail_unless(inst == INST,
			"dbi_driver_list_r() called with unexpected inst=%p; expected %p",
			inst, INST);

	if (!curr)
		return "mockdriver";
	return NULL;
} /* dbi_driver_list */

#if LIBDBI_VERSION < 900
int
dbi_initialize(const char *driverdir)
{
	return dbi_initialize_r(driverdir, NULL);
} /* dbi_initialize */

/* for some weird reason, gcc and clang complain about a missing prototype for
 * dbi_shutdown; however, the function is declared in dbi/dbi.h */
void
dbi_shutdown(void);
void
dbi_shutdown(void)
{
	dbi_shutdown_r(NULL);
} /* dbi_shutdown */

dbi_driver
dbi_driver_open(const char *name)
{
	return dbi_driver_open_r(name, NULL);
} /* dbi_driver_open */

dbi_driver
dbi_driver_list(dbi_driver curr)
{
	return dbi_driver_list_r(curr, NULL);
} /* dbi_driver_list */
#endif

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
	return golden_data[q->current_row - 1][i - 1];
} /* get_golden_data */

long long
dbi_result_get_longlong_idx(dbi_result res, unsigned int i)
{
	fail_unless(current_query->field_types[i - 1] == DBI_TYPE_INTEGER,
			"dbi_result_get_longlong_idx() called for non-integer "
			"column type %u", current_query->field_types[i - 1]);
	return get_golden_data(res, i).integer;
} /* dbi_result_get_longlong_idx */

double
dbi_result_get_double_idx(dbi_result res, unsigned int i)
{
	fail_unless(current_query->field_types[i - 1] == DBI_TYPE_DECIMAL,
			"dbi_result_get_double_idx() called for non-decimal "
			"column type %u", current_query->field_types[i - 1]);
	return get_golden_data(res, i).decimal;
} /* dbi_result_get_double_idx */

const char *
dbi_result_get_string_idx(dbi_result res, unsigned int i)
{
	fail_unless(current_query->field_types[i - 1] == DBI_TYPE_STRING,
			"dbi_result_get_string_idx() called for non-string "
			"column type %u", current_query->field_types[i - 1]);
	return get_golden_data(res, i).string;
} /* dbi_result_get_string_idx */

char *
dbi_result_get_string_copy_idx(dbi_result res, unsigned int i)
{
	fail_unless(current_query->field_types[i - 1] == DBI_TYPE_STRING,
			"dbi_result_get_string_copy_idx() called for non-string "
			"column type %u", current_query->field_types[i - 1]);
	if (! get_golden_data(res, i).string)
		return NULL;
	return strdup(get_golden_data(res, i).string);
} /* dbi_result_get_string_copy_idx */

time_t
dbi_result_get_datetime_idx(dbi_result res, unsigned int i)
{
	fail_unless(current_query->field_types[i - 1] == DBI_TYPE_DATETIME,
			"dbi_result_get_datetime_idx() called for non-datetime "
			"column type %u", current_query->field_types[i - 1]);
	return get_golden_data(res, i).datetime;
} /* dbi_result_get_datetime_idx */

size_t
dbi_result_get_field_length_idx(dbi_result res, unsigned int i)
{
	/* this will check if the parameters are valid */
	get_golden_data(res, i);

	switch (current_query->field_types[i - 1]) {
		case DBI_TYPE_INTEGER:
			return sizeof(long long);
			break;
		case DBI_TYPE_DECIMAL:
			return sizeof(double);
			break;
		case DBI_TYPE_STRING:
			return strlen(get_golden_data(res, i).string) + 1;
			break;
		case DBI_TYPE_DATETIME:
			return sizeof(time_t);
			break;
		case DBI_TYPE_BINARY:
			return get_golden_data(res, i).binary.length;
			break;
	}

	fail("INTERNAL ERROR: dbi_result_get_field_length_idx() "
			"called for unexpected field type %u",
			current_query->field_types[i - 1]);
	return 0;
} /* dbi_result_get_field_length_idx */

const unsigned char *
dbi_result_get_binary_idx(dbi_result res, unsigned int i)
{
	fail_unless(current_query->field_types[i - 1] == DBI_TYPE_BINARY,
			"dbi_result_get_binary_idx() called for non-binary "
			"column type %u", current_query->field_types[i - 1]);
	return get_golden_data(res, i).binary.datum;
} /* dbi_result_get_binary_idx */

unsigned char *
dbi_result_get_binary_copy_idx(dbi_result res, unsigned int i)
{
	const char *data;
	fail_unless(current_query->field_types[i - 1] == DBI_TYPE_BINARY,
			"dbi_result_get_binary_copy_idx() called for non-binary "
			"column type %u", current_query->field_types[i - 1]);
	data = (const char *)get_golden_data(res, i).binary.datum;
	if (! data)
		return NULL;
	return (unsigned char *)strdup(data);
} /* dbi_result_get_binary_copy_idx */

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

	dbi_conn_connect_called = 0;
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

static unsigned long long query_callback_called = 0;
static int
query_callback(sdb_dbi_client_t *c,
		size_t n, sdb_data_t *data, sdb_object_t *user_data)
{
	size_t i;

	++query_callback_called;

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

	for (i = 0; i < n; ++i) {
		int expected_type = DBI_TYPE_TO_SDB(current_query->field_types[i]);
		mock_data_t expected_data;

		fail_unless((int)data[i].type == expected_type,
				"query callback received unexpected type %i for "
				"column %zu; expected: %i", data[i].type, i,
				expected_type);

		expected_data = golden_data[current_query->current_row - 1][i];
		switch (expected_type) {
			case SDB_TYPE_INTEGER:
				fail_unless(data[i].data.integer == expected_data.integer,
						"query callback received unexpected data %lli "
						"for column %zu; expected: %lli",
						data[i].data.integer, i, expected_data.integer);
				break;
			case SDB_TYPE_DECIMAL:
				fail_unless(data[i].data.decimal == expected_data.decimal,
						"query callback received unexpected data %g "
						"for column %zu; expected: %g",
						data[i].data.decimal, i, expected_data.decimal);
				break;
			case SDB_TYPE_STRING:
				fail_unless(!strcmp(data[i].data.string, expected_data.string),
						"query callback received unexpected data %s "
						"for column %zu; expected: %s",
						data[i].data.string, i, expected_data.string);
				break;
			case SDB_TYPE_DATETIME:
				fail_unless(data[i].data.datetime
							== SECS_TO_SDB_TIME(expected_data.datetime),
						"query callback received unexpected data "PRIsdbTIME
						" for column %zu; expected: "PRIsdbTIME,
						data[i].data.integer, i,
						SECS_TO_SDB_TIME(expected_data.integer));
				break;
			case SDB_TYPE_BINARY:
				fail_unless(data[i].data.binary.length ==
							expected_data.binary.length,
						"query callback received unexpected "
						"binary data length %zu for column %zu; "
						"expected: %lli", data[i].data.binary.length, i,
						expected_data.binary.length);
				fail_unless(!memcmp(data[i].data.binary.datum,
							expected_data.binary.datum,
							expected_data.binary.length),
						"query callback received unexpected binary data %p "
						"for column %zu; expected: %p",
						data[i].data.binary.datum, i,
						expected_data.binary.datum);
				break;
			default:
				fail("INTERNAL ERROR: query callback received "
						"unknown type %i for column %zu",
						expected_type, i);
		}
	}
	return 0;
} /* query_callback */

/*
 * tests
 */

START_TEST(test_sdb_dbi_client_connect)
{
	sdb_dbi_options_t *opts;
	int check;

	check = sdb_dbi_client_connect(client);
	fail_unless(check == 0,
			"sdb_dbi_client_connect() = %i; expected: 0", check);
	fail_unless(dbi_conn_connect_called == 1,
			"sdb_dbi_client_create() called dbi_conn_connect %i times; "
			"expected: 1", dbi_conn_connect_called);

	/* calling it again shall reconnect */
	check = sdb_dbi_client_connect(client);
	fail_unless(check == 0,
			"repeated sdb_dbi_client_connect() = %i; expected: 0", check);
	fail_unless(dbi_conn_connect_called == 2,
			"repeated sdb_dbi_client_create() called dbi_conn_connect %i times; "
			"expected: 2", dbi_conn_connect_called);

	opts = sdb_dbi_options_create();
	fail_unless(opts != NULL,
			"sdb_dbi_options_create() returned NULL; expected <opts>");
	check = sdb_dbi_options_add(opts, "a", "1");
	fail_unless(check == 0,
			"sdb_dbi_options_add('a', '1') = %d; expected: 0", check);
	check = sdb_dbi_options_add(opts, "b", "2");
	fail_unless(check == 0,
			"sdb_dbi_options_add('b', '2') = %d; expected: 0", check);

	check = sdb_dbi_client_set_options(client, opts);
	fail_unless(check == 0,
			"sdb_dbi_client_set_options() = %d; expected: 0", check);
	/* reconnect with options */
	check = sdb_dbi_client_connect(client);
	fail_unless(check == 0,
			"repeated, with options sdb_dbi_client_connect() = %i; expected: 0",
			check);
	fail_unless(dbi_conn_connect_called == 3,
			"repeated, with options sdb_dbi_client_create() called "
			"dbi_conn_connect %i times; expected: 3", dbi_conn_connect_called);

	/* The client takes ownership of the options, so no need to destroy it. */
}
END_TEST

START_TEST(test_sdb_dbi_client_check_conn)
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

START_TEST(test_sdb_dbi_exec_query)
{
	size_t i;

	int check = sdb_dbi_exec_query(client, "mockquery0", query_callback,
			/* user_data = */ TEST_MAGIC, /* n = */ 0);
	/* not connected yet */
	fail_unless(check < 0,
			"sdb_dbi_exec_query() = %i; expected: < 0", check);

	connect();

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(mock_queries); ++i) {
		mock_query_t *q = &mock_queries[i];

		unsigned long long expected_callback_calls = 0;

		dbi_conn_query_called = 0;
		query_callback_called = 0;
		dbi_result_free_called = 0;

		/* sdb_dbi_exec_query will only use as many type arguments are needed,
		 * so we can safely pass in the maximum number of arguments required
		 * on each call */
		check = sdb_dbi_exec_query(client, q->name, query_callback,
				/* user_data = */ TEST_MAGIC, /* n = */ (int)q->nfields,
				SDB_TYPE_INTEGER, SDB_TYPE_DECIMAL, SDB_TYPE_STRING,
				SDB_TYPE_DATETIME, SDB_TYPE_BINARY);
		fail_unless(check == 0,
				"sdb_dbi_exec_query() = %i; expected: 0", check);

		fail_unless(dbi_conn_query_called == 1,
				"sdb_dbi_exec_query() called dbi_conn_query %i times; "
				"expected: 1", dbi_conn_query_called);

		if (q->nfields)
			expected_callback_calls = q->nrows;

		fail_unless(query_callback_called == expected_callback_calls,
				"sdb_dbi_exec_query() did not call the registered callback "
				"for each result row; got %i call%s; expected: 0",
				query_callback_called,
				(query_callback_called == 1) ? "" : "s");

		fail_unless(dbi_result_free_called == 1,
				"sdb_dbi_exec_query() did not free the query result object");
	}
}
END_TEST

TEST_MAIN("utils::dbi")
{
	TCase *tc = tcase_create("core");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_sdb_dbi_client_connect);
	tcase_add_test(tc, test_sdb_dbi_client_check_conn);
	tcase_add_test(tc, test_sdb_dbi_exec_query);
	ADD_TCASE(tc);
}
TEST_MAIN_END

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

