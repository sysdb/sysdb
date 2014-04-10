/*
 * SysDB - src/tools/sysdb/command.c
 * Copyright (C) 2014 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include "tools/sysdb/command.h"
#include "tools/sysdb/input.h"

#include "frontend/proto.h"
#include "utils/error.h"
#include "utils/strbuf.h"

#include <errno.h>

#include <assert.h>
#include <ctype.h>
#include <string.h>

/*
 * public API
 */

int
sdb_command_print_reply(sdb_client_t *client)
{
	sdb_strbuf_t *recv_buf;
	const char *result;
	uint32_t rcode = 0;

	recv_buf = sdb_strbuf_create(1024);
	if (! recv_buf)
		return -1;

	if (sdb_client_recv(client, &rcode, recv_buf) < 0)
		rcode = UINT32_MAX;

	if (sdb_client_eof(client))
		return -1;

	if (rcode == UINT32_MAX)
		printf("ERROR: ");
	result = sdb_strbuf_string(recv_buf);
	if (result && *result)
		printf("%s\n", result);
	else if (rcode == UINT32_MAX) {
		char errbuf[1024];
		printf("%s\n", sdb_strerror(errno, errbuf, sizeof(errbuf)));
	}

	sdb_strbuf_destroy(recv_buf);
	return 0;
} /* sdb_command_print_reply */

char *
sdb_command_exec(sdb_input_t *input)
{
	const char *query;
	uint32_t query_len;

	char *data = NULL;

	query = sdb_strbuf_string(input->input);
	query_len = (uint32_t)input->query_len;

	assert(input->query_len <= input->tokenizer_pos);

	/* removing leading and trailing newlines */
	while (query_len && (*query == '\n')) {
		++query;
		--query_len;
	}
	while (query_len && (query[query_len - 1]) == '\n')
		--query_len;

	if (query_len) {
		data = strndup(query, query_len);
		/* ignore errors; we'll only hide the command from the caller */

		sdb_client_send(input->client, CONNECTION_QUERY, query_len, query);
		if (sdb_command_print_reply(input->client))
			return NULL;
	}

	sdb_strbuf_skip(input->input, 0, input->query_len);
	input->tokenizer_pos -= input->query_len;
	input->query_len = 0;
	return data;
} /* sdb_command_exec */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

