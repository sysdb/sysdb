/*
 * SysDB - src/utils/dbi.c
 * Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include "utils/dbi.h"
#include "utils/error.h"

#include <assert.h>

#include <dbi/dbi.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * private data types
 */

typedef struct {
	char *key;
	char *value;
} sdb_dbi_option_t;

struct sdb_dbi_options {
	sdb_dbi_option_t *options;
	size_t options_num;
};

struct sdb_dbi_client {
	char *driver;
	char *database;

	dbi_conn conn;

	sdb_dbi_options_t *options;
};

/*
 * private helper functions
 */

static const char *
sdb_dbi_strerror(dbi_conn conn)
{
	const char *errmsg = NULL;
	dbi_conn_error(conn, &errmsg);
	return errmsg;
} /* sdb_dbi_strerror */

static int
sdb_dbi_get_field(dbi_result res, unsigned int i,
		int type, sdb_data_t *data)
{
	switch (type) {
		case SDB_TYPE_INTEGER:
			data->data.integer = dbi_result_get_longlong_idx(res, i);
			break;
		case SDB_TYPE_DECIMAL:
			data->data.decimal = dbi_result_get_double_idx(res, i);
			break;
		case SDB_TYPE_STRING:
			data->data.string = dbi_result_get_string_idx(res, i);
			break;
		case SDB_TYPE_DATETIME:
			{
				/* libdbi does not provide any higher resolutions than that */
				time_t datetime = dbi_result_get_datetime_idx(res, i);
				data->data.datetime = SECS_TO_SDB_TIME(datetime);
			}
			break;
		case SDB_TYPE_BINARY:
			{
				size_t length = dbi_result_get_field_length_idx(res, i);
				const unsigned char *datum = dbi_result_get_binary_idx(res, i);
				data->data.binary.length = length;
				data->data.binary.datum = datum;
			}
			break;
		default:
			sdb_log(SDB_LOG_ERR, "dbi: Unexpected type %i while "
					"parsing query result.", type);
			return -1;
	}

	data->type = type;
	return 0;
} /* sdb_dbi_get_field */

static int
sdb_dbi_get_data(sdb_dbi_client_t *client, dbi_result res,
		unsigned int num_fields, sdb_dbi_data_cb callback,
		sdb_object_t *user_data)
{
	sdb_data_t data[num_fields];
	int types[num_fields];
	unsigned int i;

	unsigned long long num_rows;
	unsigned long long success = 0, n;

	assert(client && res && callback);
	assert(num_fields > 0);

	for (i = 0; i < num_fields; ++i) {
		types[i] = dbi_result_get_field_type_idx(res, i + 1);
		if (types[i] == DBI_TYPE_ERROR) {
			sdb_log(SDB_LOG_ERR, "dbi: failed to fetch data: %s",
					sdb_dbi_strerror(client->conn));
			return -1;
		}
		types[i] = DBI_TYPE_TO_SC(types[i]);
	}

	num_rows = dbi_result_get_numrows(res);
	if (num_rows < 1)
		return -1;

	for (n = 0; n < num_rows; ++n) {
		if (! dbi_result_seek_row(res, n + 1)) {
			sdb_log(SDB_LOG_ERR, "dbi: Failed to retrieve row %llu: %s",
					n, sdb_dbi_strerror(client->conn));
			continue;
		}

		for (i = 0; i < num_fields; ++i)
			if (sdb_dbi_get_field(res, (unsigned int)(i + 1),
						types[i], &data[i]))
				continue;

		if (callback(client, num_fields, data, user_data))
			continue;

		++success;
	}

	if (! success)
		return -1;
	return 0;
} /* sdb_dbi_get_data */

/*
 * public API
 */

sdb_dbi_options_t *
sdb_dbi_options_create(void)
{
	sdb_dbi_options_t *options;

	options = malloc(sizeof(options));
	if (! options)
		return NULL;

	options->options = NULL;
	options->options_num = 0;
	return options;
} /* sdb_dbi_options_create */

int
sdb_dbi_options_add(sdb_dbi_options_t *options,
		const char *key, const char *value)
{
	sdb_dbi_option_t *new;

	if ((! options) || (! key) || (! value))
		return -1;

	new = realloc(options->options,
			(options->options_num + 1) * sizeof(*options->options));
	if (! new)
		return -1;

	options->options = new;
	new = options->options + options->options_num;

	new->key = strdup(key);
	new->value = strdup(value);

	if ((! new->key) || (! new->value)) {
		if (new->key)
			free(new->key);
		if (new->value)
			free(new->value);
		return -1;
	}

	++options->options_num;
	return 0;
} /* sdb_dbi_options_add */

void
sdb_dbi_options_destroy(sdb_dbi_options_t *options)
{
	size_t i;

	if (! options)
		return;

	for (i = 0; i < options->options_num; ++i) {
		sdb_dbi_option_t *opt = options->options + i;

		if (opt->key)
			free(opt->key);
		if (opt->value)
			free(opt->value);
	}

	if (options->options)
		free(options->options);
	options->options = NULL;
	options->options_num = 0;
	free(options);
} /* sdb_dbi_options_destroy */

sdb_dbi_client_t *
sdb_dbi_client_create(const char *driver, const char *database)
{
	sdb_dbi_client_t *client;

	if ((! driver) || (! database))
		return NULL;

	client = malloc(sizeof(*client));
	if (! client)
		return NULL;
	memset(client, 0, sizeof(*client));

	client->conn = NULL;
	client->options = NULL;

	client->driver = strdup(driver);
	client->database = strdup(database);
	if ((! client->driver) || (! client->database)) {
		sdb_dbi_client_destroy(client);
		return NULL;
	}
	return client;
} /* sdb_dbi_client_create */

int
sdb_dbi_client_set_options(sdb_dbi_client_t *client,
		sdb_dbi_options_t *options)
{
	if (! client)
		return -1;

	if (client->options)
		sdb_dbi_options_destroy(client->options);
	client->options = options;
	return 0;
} /* sdb_dbi_client_set_options */

int
sdb_dbi_client_connect(sdb_dbi_client_t *client)
{
	dbi_driver driver;
	size_t i;

	if ((! client) || (! client->driver) || (! client->database))
		return -1;

	if (client->conn)
		dbi_conn_close(client->conn);

	driver = dbi_driver_open(client->driver);
	if (! driver) {
		sdb_error_set("dbi: failed to open DBI driver '%s'; "
				"possibly it's not installed.\n",
				client->driver);

		sdb_error_append("dbi: known drivers:\n");
		for (driver = dbi_driver_list(NULL); driver;
				driver = dbi_driver_list(driver)) {
			sdb_error_append("\t- %s\n", dbi_driver_get_name(driver));
		}
		sdb_error_chomp();
		sdb_error_log(SDB_LOG_ERR);
		return -1;
	}

	client->conn = dbi_conn_open(driver);
	if (! client->conn) {
		sdb_log(SDB_LOG_ERR, "dbi: failed to open connection "
				"object.");
		return -1;
	}

	if (client->options) {
		for (i = 0; i < client->options->options_num; ++i) {
			const char *opt;

			if (! dbi_conn_set_option(client->conn,
						client->options->options[i].key,
						client->options->options[i].value))
				continue;
			/* else: error */

			sdb_error_set("dbi: failed to set option '%s': %s\n",
					client->options->options[i].key,
					sdb_dbi_strerror(client->conn));

			sdb_error_append("dbi: known driver options:\n");
			for (opt = dbi_conn_get_option_list(client->conn, NULL); opt;
					opt = dbi_conn_get_option_list(client->conn, opt))
				sdb_error_append("\t- %s\n", opt);
			sdb_error_chomp();
			sdb_error_log(SDB_LOG_ERR);

			dbi_conn_close(client->conn);
			return -1;
		}
	}

	if (dbi_conn_set_option(client->conn, "dbname", client->database)) {
		sdb_log(SDB_LOG_ERR, "dbi: failed to set option 'dbname': %s",
				sdb_dbi_strerror(client->conn));
		dbi_conn_close(client->conn);
		return -1;
	}

	if (dbi_conn_connect(client->conn) < 0) {
		sdb_log(SDB_LOG_ERR, "dbi: failed to connect to database '%s': %s",
				client->database, sdb_dbi_strerror(client->conn));
		dbi_conn_close(client->conn);
		return -1;
	}
	return 0;
} /* sdb_dbi_client_connect */

int
sdb_dbi_client_check_conn(sdb_dbi_client_t *client)
{
	if (! client)
		return -1;

	if (! client->conn)
		return sdb_dbi_client_connect(client);

	if (dbi_conn_ping(client->conn))
		return 0;
	return sdb_dbi_client_connect(client);
} /* sdb_dbi_client_check_conn */

int
sdb_dbi_exec_query(sdb_dbi_client_t *client, const char *query,
		sdb_dbi_data_cb callback, sdb_object_t *user_data, int n, ...)
{
	dbi_result res;
	unsigned int num_fields;

	int status;

	if ((! client) || (! client->conn) || (! query))
		return -1;

	res = dbi_conn_query(client->conn, query);
	if (! res) {
		sdb_log(SDB_LOG_ERR, "dbi: failed to execute query '%s': %s",
				query, sdb_dbi_strerror(client->conn));
		return -1;
	}

	if (dbi_result_get_numrows(res) == DBI_ROW_ERROR) {
		sdb_log(SDB_LOG_ERR, "dbi: failed to fetch rows for query "
				"'%s': %s", query, sdb_dbi_strerror(client->conn));
		dbi_result_free(res);
		return -1;
	}

	if (dbi_result_get_numrows(res) < 1) { /* no data */
		dbi_result_free(res);
		return 0;
	}

	num_fields = dbi_result_get_numfields(res);

	if (n >= 0) {
		va_list types;
		int i;

		if (n != (int)num_fields) {
			sdb_log(SDB_LOG_ERR, "dbi: number of returned fields (%i) "
					"does not match the number of requested fields (%i) "
					"for query '%s'.", num_fields, n, query);
			dbi_result_free(res);
			return -1;
		}

		va_start(types, n);
		status = 0;

		for (i = 0; i < n; ++i) {
			unsigned short field_type = dbi_result_get_field_type_idx(res,
					(unsigned int)(i + 1));

			unsigned int type = va_arg(types, unsigned int);

			field_type = DBI_TYPE_TO_SC(field_type);

			/* column count starts at 1 */
			if ((unsigned int)field_type != type) {
				sdb_log(SDB_LOG_ERR, "dbi: type of column '%s' (%u) "
						"does not match requested type (%u).",
						dbi_result_get_field_name(res, (unsigned int)i + 1),
						field_type, type);
				status = -1;
			}
		}

		va_end(types);

		if (status) {
			dbi_result_free(res);
			return status;
		}
	}

	if (num_fields < 1) { /* no data */
		dbi_result_free(res);
		return 0;
	}

	status = sdb_dbi_get_data(client, res, num_fields, callback, user_data);

	dbi_result_free(res);
	return status;
} /* sdb_dbi_exec_query */

void
sdb_dbi_client_destroy(sdb_dbi_client_t *client)
{
	if (! client)
		return;

	if (client->driver)
		free(client->driver);
	client->driver = NULL;

	if (client->database)
		free(client->database);
	client->database = NULL;

	if (client->conn)
		dbi_conn_close(client->conn);

	if (client->options)
		sdb_dbi_options_destroy(client->options);
	client->options = NULL;

	free(client);
} /* sdb_dbi_client_destroy */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

