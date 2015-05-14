/*
 * SysDB - t/unit/frontend/query_test.c
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

#include "frontend/connection.h"
#include "frontend/parser.h"
#include "frontend/connection-private.h"
#include "testutils.h"

#include <check.h>

/*
 * private helpers
 */

static void
populate(void)
{
	sdb_data_t datum;

	sdb_store_host("h1", 1);
	sdb_store_host("h2", 3);

	datum.type = SDB_TYPE_STRING;
	datum.data.string = "v1";
	sdb_store_attribute("h1", "k1", &datum, 1);
	datum.data.string = "v2";
	sdb_store_attribute("h1", "k2", &datum, 2);
	datum.data.string = "v3";
	sdb_store_attribute("h1", "k3", &datum, 2);

	sdb_store_metric("h1", "m1", /* store */ NULL, 2);
	sdb_store_metric("h1", "m2", /* store */ NULL, 1);
	sdb_store_metric("h2", "m1", /* store */ NULL, 1);

	datum.type = SDB_TYPE_INTEGER;
	datum.data.integer = 42;
	sdb_store_metric_attr("h1", "m1", "k3", &datum, 2);

	sdb_store_service("h2", "s1", 1);
	sdb_store_service("h2", "s2", 2);

	datum.data.integer = 123;
	sdb_store_service_attr("h2", "s2", "k1", &datum, 2);
	datum.data.integer = 4711;
	sdb_store_service_attr("h2", "s2", "k2", &datum, 1);
} /* populate */

#define HOST_H1 \
	"{\"name\": \"h1\", \"last_update\": \"1970-01-01 00:00:00 +0000\", " \
			"\"update_interval\": \"0s\", \"backends\": [], " \
		"\"attributes\": [" \
			"{\"name\": \"k1\", \"value\": \"v1\", " \
				"\"last_update\": \"1970-01-01 00:00:00 +0000\", " \
				"\"update_interval\": \"0s\", \"backends\": []}," \
			"{\"name\": \"k2\", \"value\": \"v2\", " \
				"\"last_update\": \"1970-01-01 00:00:00 +0000\", " \
				"\"update_interval\": \"0s\", \"backends\": []}," \
			"{\"name\": \"k3\", \"value\": \"v3\", " \
				"\"last_update\": \"1970-01-01 00:00:00 +0000\", " \
				"\"update_interval\": \"0s\", \"backends\": []}], " \
		"\"metrics\": [" \
			"{\"name\": \"m1\", \"timeseries\": false, " \
				"\"last_update\": \"1970-01-01 00:00:00 +0000\", " \
				"\"update_interval\": \"0s\", \"backends\": [], " \
				"\"attributes\": [" \
					"{\"name\": \"hostname\", \"value\": \"h1\", " \
						"\"last_update\": \"1970-01-01 00:00:00 +0000\", " \
						"\"update_interval\": \"0s\", \"backends\": []}," \
					"{\"name\": \"k3\", \"value\": 42, " \
						"\"last_update\": \"1970-01-01 00:00:00 +0000\", " \
						"\"update_interval\": \"0s\", \"backends\": []}]}," \
			"{\"name\": \"m2\", \"timeseries\": false, " \
				"\"last_update\": \"1970-01-01 00:00:00 +0000\", " \
				"\"update_interval\": \"0s\", \"backends\": [], " \
				"\"attributes\": [" \
					"{\"name\": \"hostname\", \"value\": \"h1\", " \
						"\"last_update\": \"1970-01-01 00:00:00 +0000\", " \
						"\"update_interval\": \"0s\", \"backends\": []}]}]}"

#define SERVICE_H2_S1 \
	"{\"name\": \"h2\", \"last_update\": \"1970-01-01 00:00:00 +0000\", " \
			"\"update_interval\": \"0s\", \"backends\": [], " \
		"\"services\": [" \
			"{\"name\": \"s1\", \"last_update\": \"1970-01-01 00:00:00 +0000\", " \
				"\"update_interval\": \"0s\", \"backends\": [], " \
				"\"attributes\": [" \
					"{\"name\": \"hostname\", \"value\": \"h2\", " \
						"\"last_update\": \"1970-01-01 00:00:00 +0000\", " \
						"\"update_interval\": \"0s\", \"backends\": []}]}]}"

#define METRIC_H1_M1 \
	"{\"name\": \"h1\", \"last_update\": \"1970-01-01 00:00:00 +0000\", " \
			"\"update_interval\": \"0s\", \"backends\": [], " \
		"\"metrics\": [" \
			"{\"name\": \"m1\", \"timeseries\": false, " \
				"\"last_update\": \"1970-01-01 00:00:00 +0000\", " \
				"\"update_interval\": \"0s\", \"backends\": [], " \
				"\"attributes\": [" \
					"{\"name\": \"hostname\", \"value\": \"h1\", " \
						"\"last_update\": \"1970-01-01 00:00:00 +0000\", " \
						"\"update_interval\": \"0s\", \"backends\": []}," \
					"{\"name\": \"k3\", \"value\": 42, " \
						"\"last_update\": \"1970-01-01 00:00:00 +0000\", " \
						"\"update_interval\": \"0s\", \"backends\": []}]}]}"

typedef struct {
	sdb_conn_t conn;
	sdb_strbuf_t *write_buf;
} mock_conn_t;
#define MOCK_CONN(obj) ((mock_conn_t *)(obj))
#define CONN(obj) ((sdb_conn_t *)(obj))

static void
mock_conn_destroy(sdb_conn_t *conn)
{
	sdb_strbuf_destroy(conn->buf);
	sdb_strbuf_destroy(conn->errbuf);
	sdb_strbuf_destroy(MOCK_CONN(conn)->write_buf);
	free(conn);
} /* mock_conn_destroy */

static ssize_t
mock_conn_read(sdb_conn_t *conn, size_t len)
{
	if (! conn)
		return -1;
	/* unused so far */
	return len;
} /* conn_read */

static ssize_t
mock_conn_write(sdb_conn_t *conn, const void *buf, size_t len)
{
	if (! conn)
		return -1;
	return sdb_strbuf_memappend(MOCK_CONN(conn)->write_buf, buf, len);
} /* conn_write */

static sdb_conn_t *
mock_conn_create(void)
{
	mock_conn_t *conn;

	conn = calloc(1, sizeof(*conn));
	if (! conn) {
		fail("INTERNAL ERROR: failed to allocate connection object");
		return NULL;
	}

	SDB_OBJ(conn)->name = "mock_connection";
	SDB_OBJ(conn)->ref_cnt = 1;

	conn->conn.buf = sdb_strbuf_create(0);
	conn->conn.errbuf = sdb_strbuf_create(0);
	conn->write_buf = sdb_strbuf_create(64);
	if ((! conn->conn.buf) || (! conn->conn.errbuf) || (! conn->write_buf)) {
		mock_conn_destroy(CONN(conn));
		fail("INTERNAL ERROR: failed to allocate connection object");
		return NULL;
	}

	conn->conn.read = mock_conn_read;
	conn->conn.write = mock_conn_write;

	conn->conn.username = "mock_user";
	conn->conn.cmd = SDB_CONNECTION_IDLE;
	conn->conn.cmd_len = 0;
	return CONN(conn);
} /* mock_conn_create */

/* TODO: move this into a test helper module */
static void
fail_if_strneq(const char *got, const char *expected, size_t n, const char *fmt, ...)
{
	sdb_strbuf_t *buf;
	va_list ap;

	size_t len1 = strlen(got);
	size_t len2 = strlen(expected);

	size_t i;
	int pos = -1;

	if (n) {
		len1 = SDB_MIN(len1, n);
		len2 = SDB_MIN(len2, n);
	}

	if (len1 != len2)
		pos = (int)SDB_MIN(len1, len2);

	for (i = 0; i < SDB_MIN(len1, len2); ++i) {
		if (got[i] != expected[i]) {
			pos = (int)i;
			break;
		}
	}

	if (pos == -1)
		return;

	buf = sdb_strbuf_create(64);
	va_start(ap, fmt);
	sdb_strbuf_vsprintf(buf, fmt, ap);

	fail("%s\n         got: %s\n              %*s\n    expected: %s",
			sdb_strbuf_string(buf), got, pos + 1, "^", expected);
} /* fail_if_strneq */

/*
 * tests
 */

static struct {
	int type;
	const char *hostname;
	const char *name;
	const char *filter;

	int expected;
	uint32_t code;
	size_t len;
	const char *data;
} exec_fetch_data[] = {
	/* hosts */
	{
		SDB_HOST, "h1", NULL, NULL,
		0, SDB_CONNECTION_DATA, 1110, HOST_H1,
	},
	{
		SDB_HOST, "h1", NULL, "age >= 0s", /* always matches */
		0, SDB_CONNECTION_DATA, 1110, HOST_H1,
	},
	{
		SDB_HOST, "h1", NULL, "age < 0s", /* never matches */
		-1, UINT32_MAX, 0, NULL, /* FETCH fails if the object doesn't exist */
	},
	{
		SDB_HOST, "x1", NULL, NULL, /* does not exist */
		-1, UINT32_MAX, 0, NULL,
	},
	{
		SDB_HOST, "h1", "s1", NULL, /* invalid args */
		-1, UINT32_MAX, 0, NULL,
	},
	/* services */
	{
		SDB_SERVICE, "h2", "s1", NULL,
		0, SDB_CONNECTION_DATA, 356, SERVICE_H2_S1,
	},
	{
		SDB_SERVICE, "h2", "s1", "age >= 0s", /* always matches */
		0, SDB_CONNECTION_DATA, 356, SERVICE_H2_S1,
	},
	{
		SDB_SERVICE, "h2", "s1", "age < 0s", /* never matches */
		-1, UINT32_MAX, 0, NULL,
	},
	{
		SDB_SERVICE, "h2", "s1", "name = 'h2'", /* only matches host */
		-1, UINT32_MAX, 0, NULL,
	},
	{
		SDB_SERVICE, "h2", "x1", NULL, /* does not exist */
		-1, UINT32_MAX, 0, NULL,
	},
	{
		SDB_SERVICE, "x2", "s1", NULL, /* does not exist */
		-1, UINT32_MAX, 0, NULL,
	},
	{
		SDB_SERVICE, "h2", NULL, NULL, /* invalid args */
		-1, UINT32_MAX, 0, NULL,
	},
	/* metrics */
	{
		SDB_METRIC, "h1", "m1", NULL,
		0, SDB_CONNECTION_DATA, 489, METRIC_H1_M1,
	},
	{
		SDB_METRIC, "h1", "m1", "age >= 0s", /* always matches */
		0, SDB_CONNECTION_DATA, 489, METRIC_H1_M1,
	},
	{
		SDB_METRIC, "h1", "m1", "age < 0s", /* never matches */
		-1, UINT32_MAX, 0, NULL,
	},
	{
		SDB_METRIC, "h1", "m1", "name = 'h1'", /* only matches host */
		-1, UINT32_MAX, 0, NULL,
	},
	{
		SDB_METRIC, "h1", "x1", NULL, /* does not exist */
		-1, UINT32_MAX, 0, NULL,
	},
	{
		SDB_METRIC, "x1", "m1", NULL, /* does not exist */
		-1, UINT32_MAX, 0, NULL,
	},
	{
		SDB_METRIC, "x1", NULL, NULL, /* invalid args */
		-1, UINT32_MAX, 0, NULL,
	},
};

START_TEST(test_exec_fetch)
{
	sdb_conn_t *conn = mock_conn_create();
	sdb_store_matcher_t *filter = NULL;

	uint32_t code = UINT32_MAX, msg_len = UINT32_MAX;
	const char *data;
	ssize_t tmp;
	size_t len;
	int check;

	if (exec_fetch_data[_i].filter) {
		filter = sdb_fe_parse_matcher(exec_fetch_data[_i].filter, -1, NULL);
		ck_assert_msg(filter != NULL);
	}

	check = sdb_fe_exec_fetch(conn, exec_fetch_data[_i].type,
			exec_fetch_data[_i].hostname, exec_fetch_data[_i].name, filter);
	fail_unless(check == exec_fetch_data[_i].expected,
			"sdb_fe_exec_fetch(%s, %s, %s, %s) = %d; expected: %d",
			SDB_STORE_TYPE_TO_NAME(exec_fetch_data[_i].type),
			exec_fetch_data[_i].hostname, exec_fetch_data[_i].name,
			exec_fetch_data[_i].filter, check, exec_fetch_data[_i].expected);

	len = sdb_strbuf_len(MOCK_CONN(conn)->write_buf);

	if (exec_fetch_data[_i].code == UINT32_MAX) {
		fail_unless(len == 0,
				"sdb_fe_exec_fetch(%s, %s, %s, %s) returned data on error",
			SDB_STORE_TYPE_TO_NAME(exec_fetch_data[_i].type),
			exec_fetch_data[_i].hostname, exec_fetch_data[_i].name,
			exec_fetch_data[_i].filter);
		sdb_object_deref(SDB_OBJ(filter));
		mock_conn_destroy(conn);
		return;
	}

	data = sdb_strbuf_string(MOCK_CONN(conn)->write_buf);
	tmp = sdb_proto_unmarshal_header(data, len, &code, &msg_len);
	ck_assert_msg(tmp == (ssize_t)(2 * sizeof(uint32_t)));
	data += tmp;
	len -= tmp;

	fail_unless((code == exec_fetch_data[_i].code)
				&& ((size_t)msg_len == exec_fetch_data[_i].len),
			"sdb_fe_exec_fetch(%s, %s, %s, %s) returned %u, %u; expected: %u, %zu",
			SDB_STORE_TYPE_TO_NAME(exec_fetch_data[_i].type),
			exec_fetch_data[_i].hostname, exec_fetch_data[_i].name,
			exec_fetch_data[_i].filter, code, msg_len,
			exec_fetch_data[_i].code, exec_fetch_data[_i].len);

	tmp = sdb_proto_unmarshal_int32(data, len, &code);
	fail_unless(code == SDB_CONNECTION_FETCH,
			"sdb_fe_exec_fetch(%s, %s, %s, %s) returned %s object; expected: FETCH",
			SDB_STORE_TYPE_TO_NAME(exec_fetch_data[_i].type),
			exec_fetch_data[_i].hostname, exec_fetch_data[_i].name,
			exec_fetch_data[_i].filter, SDB_CONN_MSGTYPE_TO_STRING((int)code));
	data += tmp;
	len -= tmp;

	fail_if_strneq(data, exec_fetch_data[_i].data, (size_t)msg_len,
			"sdb_fe_exec_fetch(%s, %s, %s, %s) returned '%s'; expected: '%s'",
			SDB_STORE_TYPE_TO_NAME(exec_fetch_data[_i].type),
			exec_fetch_data[_i].hostname, exec_fetch_data[_i].name,
			exec_fetch_data[_i].filter, data, exec_fetch_data[_i].data);

	sdb_object_deref(SDB_OBJ(filter));
	mock_conn_destroy(conn);
}
END_TEST

TEST_MAIN("frontend::query")
{
	TCase *tc = tcase_create("core");
	tcase_add_checked_fixture(tc, populate, sdb_store_clear);
	tcase_add_loop_test(tc, test_exec_fetch, 0, SDB_STATIC_ARRAY_LEN(exec_fetch_data));
	ADD_TCASE(tc);
}
TEST_MAIN_END

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

