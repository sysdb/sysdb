/*
 * SysDB - src/frontend/connection-private.h
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

/*
 * private data structures used by frontend modules
 */

#ifndef SDB_FRONTEND_CONNECTION_PRIVATE_H
#define SDB_FRONTEND_CONNECTION_PRIVATE_H 1

#include "frontend/connection.h"

#include "core/object.h"
#include "core/store.h"
#include "core/timeseries.h"
#include "utils/strbuf.h"

#include <inttypes.h>
#include <arpa/inet.h>

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sdb_conn {
	sdb_object_t super;

	/* file-descriptor of the open connection */
	int fd;

	/* connection and client information */
	struct sockaddr_storage client_addr;
	socklen_t client_addr_len;

	/* read buffer */
	sdb_strbuf_t *buf;

	/* connection / protocol state information */
	uint32_t cmd;
	uint32_t cmd_len;

	/* amount of data to skip, e.g., after receiving invalid commands; if this
	 * is non-zero, the 'skip_len' first bytes of 'buf' are invalid */
	size_t skip_len;

	sdb_strbuf_t *errbuf;

	/* user information */
	char *username; /* NULL if the user has not been authenticated */
	_Bool ready; /* indicates that startup finished successfully */
};
#define CONN(obj) ((sdb_conn_t *)(obj))

/*
 * node types
 */

typedef struct {
	sdb_conn_node_t super;
	sdb_store_matcher_t *matcher;
} conn_matcher_t;
#define CONN_MATCHER(obj) ((conn_matcher_t *)(obj))

typedef struct {
	sdb_conn_node_t super;
	int type;
	conn_matcher_t *filter;
} conn_list_t;
#define CONN_LIST(obj) ((conn_list_t *)(obj))

typedef struct {
	sdb_conn_node_t super;
	int type;
	char *name;
	conn_matcher_t *filter;
} conn_fetch_t;
#define CONN_FETCH(obj) ((conn_fetch_t *)(obj))

typedef struct {
	sdb_conn_node_t super;
	int type;
	conn_matcher_t *matcher;
	conn_matcher_t *filter;
} conn_lookup_t;
#define CONN_LOOKUP(obj) ((conn_lookup_t *)(obj))

typedef struct {
	sdb_conn_node_t super;
	char *hostname;
	char *metric;
	sdb_timeseries_opts_t opts;
} conn_ts_t;
#define CONN_TS(obj) ((conn_ts_t *)(obj))

/*
 * type helper functions
 */

static void __attribute__((unused))
conn_matcher_destroy(sdb_object_t *obj)
{
	sdb_object_deref(SDB_OBJ(CONN_MATCHER(obj)->matcher));
} /* conn_matcher_destroy */

static void __attribute__((unused))
conn_list_destroy(sdb_object_t *obj)
{
	sdb_object_deref(SDB_OBJ(CONN_LIST(obj)->filter));
} /* conn_list_destroy */

static void __attribute__((unused))
conn_fetch_destroy(sdb_object_t *obj)
{
	if (CONN_FETCH(obj)->name)
		free(CONN_FETCH(obj)->name);
	sdb_object_deref(SDB_OBJ(CONN_FETCH(obj)->filter));
} /* conn_fetch_destroy */

static void __attribute__((unused))
conn_lookup_destroy(sdb_object_t *obj)
{
	sdb_object_deref(SDB_OBJ(CONN_LOOKUP(obj)->matcher));
	sdb_object_deref(SDB_OBJ(CONN_LOOKUP(obj)->filter));
} /* conn_lookup_destroy */

static void __attribute__((unused))
conn_ts_destroy(sdb_object_t *obj)
{
	if (CONN_TS(obj)->hostname)
		free(CONN_TS(obj)->hostname);
	if (CONN_TS(obj)->metric)
		free(CONN_TS(obj)->metric);
} /* conn_ts_destroy */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_FRONTEND_CONNECTION_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

