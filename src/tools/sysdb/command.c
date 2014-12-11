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

#include "sysdb.h"

#include "tools/sysdb/command.h"
#include "tools/sysdb/input.h"

#include "frontend/proto.h"
#include "utils/error.h"
#include "utils/proto.h"
#include "utils/strbuf.h"

#include <errno.h>

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void
log_printer(sdb_strbuf_t *buf)
{
	uint32_t prio = sdb_proto_get_int(buf, 0);

	if (prio == UINT32_MAX) {
		printf("ERROR: Received a LOG message with invalid "
				"or missing priority\n");
		return;
	}
	sdb_strbuf_skip(buf, 0, sizeof(prio));

	printf("%s: %s\n", SDB_LOG_PRIO_TO_STRING((int)prio),
			sdb_strbuf_string(buf));
} /* log_printer */

static void
data_printer(sdb_strbuf_t *buf)
{
	size_t len = sdb_strbuf_len(buf);

	if ((! len) || (len == sizeof(uint32_t))) {
		/* empty command or empty reply */
		return;
	}
	else if (len < sizeof(uint32_t)) {
		printf("ERROR: Received a DATA message with invalid "
				"or missing data-type\n");
		return;
	}

	/* At the moment, we don't care about the result type. We simply print the
	 * result without further parsing it. */
	sdb_strbuf_skip(buf, 0, sizeof(uint32_t));
	printf("%s\n", sdb_strbuf_string(buf));
} /* data_printer */

static struct {
	int status;
	void (*printer)(sdb_strbuf_t *);
} response_printers[] = {
	{ SDB_CONNECTION_LOG,  log_printer },
	{ SDB_CONNECTION_DATA, data_printer },
};

/*
 * public API
 */

int
sdb_command_print_reply(sdb_client_t *client)
{
	sdb_strbuf_t *recv_buf;
	const char *result;
	uint32_t rcode = 0;

	int status = 0;
	size_t i;

	recv_buf = sdb_strbuf_create(1024);
	if (! recv_buf)
		return -1;

	if (sdb_client_recv(client, &rcode, recv_buf) < 0)
		rcode = UINT32_MAX;

	if (sdb_client_eof(client)) {
		sdb_strbuf_destroy(recv_buf);
		return -1;
	}

	if (rcode == UINT32_MAX) {
		printf("ERROR: ");
		status = -1;
	}
	else
		status = (int)rcode;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(response_printers); ++i) {
		if (status == response_printers[i].status) {
			response_printers[i].printer(recv_buf);
			sdb_strbuf_destroy(recv_buf);
			return status;
		}
	}

	result = sdb_strbuf_string(recv_buf);
	if (result && *result)
		printf("%s\n", result);
	else if (rcode == UINT32_MAX) {
		char errbuf[1024];
		printf("%s\n", sdb_strerror(errno, errbuf, sizeof(errbuf)));
	}

	sdb_strbuf_destroy(recv_buf);
	return status;
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

		sdb_client_send(input->client, SDB_CONNECTION_QUERY, query_len, query);

		/* The server will send back *something*, either error/log messages
		 * and/or the reply to the query. Here, we don't care about what it
		 * sends back. We'll wait for the first reply and then return to the
		 * main loop which will handle any subsequent replies, including
		 * eventually the reply to the query (if it's not the first reply). */
		/* TODO: wait for the actual reply instead */
		if (sdb_command_print_reply(input->client) < 0) {
			if (data)
				free(data);
			data = NULL;
		}
	}

	sdb_strbuf_skip(input->input, 0, input->query_len);
	input->tokenizer_pos -= input->query_len;
	input->query_len = 0;
	return data;
} /* sdb_command_exec */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

