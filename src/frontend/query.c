/*
 * SysDB - src/frontend/query.c
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

#include "sysdb.h"

#include "core/store.h"
#include "frontend/connection-private.h"
#include "utils/error.h"
#include "utils/strbuf.h"

#include <errno.h>

/*
 * public API
 */

int
sdb_fe_list(sdb_conn_t *conn)
{
	sdb_strbuf_t *buf;

	buf = sdb_strbuf_create(1024);
	if (! buf) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: Failed to create "
				"buffer to handle LIST command: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));

		sdb_strbuf_sprintf(conn->errbuf, "Out of memory");
		sdb_strbuf_destroy(buf);
		return -1;
	}

	if (sdb_store_tojson(buf)) {
		sdb_log(SDB_LOG_ERR, "frontend: Failed to serialize "
				"store to JSON");
		sdb_strbuf_sprintf(conn->errbuf, "Out of memory");
		sdb_strbuf_destroy(buf);
		return -1;
	}

	sdb_connection_send(conn, CONNECTION_OK,
			(uint32_t)sdb_strbuf_len(buf), sdb_strbuf_string(buf));
	sdb_strbuf_destroy(buf);
	return 0;
} /* sdb_fe_list */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

