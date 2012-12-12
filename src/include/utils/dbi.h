/*
 * syscollector - src/include/utils/dbi.h
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

#ifndef SC_UTILS_DBI_H
#define SC_UTILS_DBI_H 1

#include "core/object.h"
#include "utils/data.h"

#include <stddef.h>

/* translate libdbi types to syscollector types */
#define DBI_TYPE_TO_SC(dt) \
	(((dt) == DBI_TYPE_INTEGER) \
		? SC_TYPE_INTEGER \
		: ((dt) == DBI_TYPE_DECIMAL) \
			? SC_TYPE_DECIMAL \
			: ((dt) == DBI_TYPE_STRING) \
				? SC_TYPE_STRING \
				: ((dt) == DBI_TYPE_DATETIME) \
					? SC_TYPE_DATETIME \
					: ((dt) == DBI_TYPE_BINARY) \
						? SC_TYPE_BINARY : 0)

#ifdef __cplusplus
extern "C" {
#endif

struct sc_dbi_options;
typedef struct sc_dbi_options sc_dbi_options_t;

struct sc_dbi_client;
typedef struct sc_dbi_client sc_dbi_client_t;

typedef int (*sc_dbi_data_cb)(sc_dbi_client_t *,
		size_t, sc_data_t *, sc_object_t *);

/*
 * sc_dbi_options_t:
 * This object stores DBI connection options (key/value) (e.g. host, dbname,
 * etc.). It may be used to dynamically create the list of options before
 * applying it to some client object.
 */
sc_dbi_options_t *
sc_dbi_options_create(void);

int
sc_dbi_options_add(sc_dbi_options_t *options,
		const char *key, const char *value);

void
sc_dbi_options_destroy(sc_dbi_options_t *options);

/*
 * sc_dbi_client_create:
 * Creates a new DBI client object using the specified DBI / DBD 'driver' and
 * connecting to the specified 'database'.
 *
 * Returns:
 *  - the client object on success
 *  - NULL else
 */
sc_dbi_client_t *
sc_dbi_client_create(const char *driver, const char *database);

/*
 * sc_dbi_client_set_options:
 * Apply connection options to an existing client object. This has to be done
 * before actually connecting to the database using sc_dbi_client_connect().
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sc_dbi_client_set_options(sc_dbi_client_t *client,
		sc_dbi_options_t *options);

/*
 * sc_dbi_client_connect:
 * Connect to the database using the options registered beforehand.
 *
 * This function may also be used to reconnect to the database.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sc_dbi_client_connect(sc_dbi_client_t *client);

/*
 * sc_dbi_exec_query:
 * Execute an SQL query on the database. The specified 'callback' will be
 * called for each row returned from the query. If 'n' is a value equal to or
 * greater than zero, it specifies the number of columns that are expected in
 * the query result. For each column, the caller then needs to also specify
 * the requested type (see the DBI_TYPE_* constants). If the number or types
 * do not match, an error will be reported and the query will fail. That is,
 * this allows to let sc_dbi_exec_query() do basic verification of the
 * returned values.
 *
 * The callback will receive the client object and an array containing the
 * field values of the current row. Any string / binary objects are managed by
 * libdbi, thus, it must not be freed or modified. If you need to keep the
 * object, make sure to make a copy of it.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sc_dbi_exec_query(sc_dbi_client_t *client, const char *query,
		sc_dbi_data_cb callback, sc_object_t *user_data, int n, ...);

/*
 * sc_dbi_client_destroy:
 * Disconnect from the database and destroy the client object.
 */
void
sc_dbi_client_destroy(sc_dbi_client_t *client);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SC_UTILS_DBI_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

