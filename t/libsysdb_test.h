/*
 * SysDB - t/libsysdb_test.h
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

#ifndef T_LIBSYSDB_H
#define T_LIBSYSDB_H 1

#include "sysdb.h"
#include "core/object.h"

#include "libsysdb_testutils.h"

#include <check.h>
#include <string.h>

/*
 * private testing helpers
 */

/* static string object:
 * Any such object may is of type sdb_object_t but may never be destroyed. */
#define SSTRING_OBJ(name) { \
	/* type = */ { sizeof(sdb_object_t), NULL, NULL }, \
	/* ref_cnt = */ 1, /* name = */ (name) }

/*
 * test-related data-types
 */

typedef struct {
	Suite *(*creator)(void);
	const char *msg;
} suite_creator_t;

/*
 * test suites
 */

/* t/core/data_test */
Suite *
core_data_suite(void);

/* t/core/object_test */
Suite *
core_object_suite(void);

/* t/core/store_test */
Suite *
core_store_suite(void);

/* t/core/store_lookup_test */
Suite *
core_store_lookup_suite(void);

/* t/core/time_test */
Suite *
core_time_suite(void);

/* t/frontend/connection_test */
Suite *
fe_conn_suite(void);

/* t/frontend/parser_test */
Suite *
fe_parser_suite(void);

/* t/frontend/sock_test */
Suite *
fe_sock_suite(void);

/* t/utils/channel_test */
Suite *
util_channel_suite(void);

/* t/utils/dbi_test */
Suite *
util_dbi_suite(void);

/* t/utils/llist_test */
Suite *
util_llist_suite(void);

/* t/utils/strbuf_test */
Suite *
util_strbuf_suite(void);

/* t/utils/unixsock_test */
Suite *
util_unixsock_suite(void);

#endif /* T_LIBSYSDB_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

