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
#include "tools/sysdb/json.h"

#include "frontend/proto.h"
#include "utils/error.h"
#include "utils/proto.h"
#include "utils/strbuf.h"

#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>

#include <assert.h>
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static pid_t
exec_pager(int in)
{
	pid_t pid;
	long fd;

	sigset_t sigs;

	pid = fork();
	if (pid) /* parent or error */
		return pid;

	sigemptyset(&sigs);
	sigprocmask(SIG_SETMASK, &sigs, NULL);

	for (fd = 3; fd <= sysconf(_SC_OPEN_MAX); fd++)
		if (fd != in)
			close((int)fd);

	if (dup2(in, STDIN_FILENO) >= 0) {
		/* TODO: make configurable */
		char *argv[] = { "less", "-FRX" };

		close(in);
		in = STDIN_FILENO;

		if (execvp(argv[0], argv)) {
			char cmdbuf[1024], errbuf[1024];
			sdb_data_format(&(sdb_data_t){
						SDB_TYPE_STRING | SDB_TYPE_ARRAY,
						{ .array = { SDB_STATIC_ARRAY_LEN(argv), argv } },
					}, cmdbuf, sizeof(cmdbuf), SDB_UNQUOTED);
			sdb_log(SDB_LOG_WARNING, "Failed to execute pager %s: %s",
					cmdbuf, sdb_strerror(errno, errbuf, sizeof(errbuf)));
		}
	}

	/* else: something failed, simply print all input */
	while (42) {
		char buf[1024 + 1];
		ssize_t n;

		n = read(in, buf, sizeof(buf) - 1);
		if (!n) /* EOF */
			break;

		if (n > 0) {
			buf[n] = '\0';
			printf("%s", buf);
			continue;
		}

		if ((errno == EAGAIN) || (errno == EWOULDBLOCK)
				|| (errno == EINTR))
			continue;

		sdb_log(SDB_LOG_ERR, "Failed to read query result: %s",
				sdb_strerror(errno, buf, sizeof(buf)));
		exit(1);
	}
	exit(0);
} /* exec_pager */

static void
ok_printer(sdb_input_t __attribute__((unused)) *input, sdb_strbuf_t *buf)
{
	const char *msg = sdb_strbuf_string(buf);
	if (msg && *msg)
		printf("%s\n", msg);
	else
		printf("OK\n");
} /* ok_printer */

static void
log_printer(sdb_input_t __attribute__((unused)) *input, sdb_strbuf_t *buf)
{
	uint32_t prio = 0;

	if (sdb_proto_unmarshal_int32(SDB_STRBUF_STR(buf), &prio) < 0) {
		sdb_log(SDB_LOG_WARNING, "Received a LOG message with invalid "
				"or missing priority");
		prio = (uint32_t)SDB_LOG_ERR;
	}
	sdb_strbuf_skip(buf, 0, sizeof(prio));

	sdb_log((int)prio, "%s", sdb_strbuf_string(buf));
} /* log_printer */

static void
data_printer(sdb_input_t *input, sdb_strbuf_t *buf)
{
	size_t len = sdb_strbuf_len(buf);
	uint32_t type = 0;

	int pipefd[2] = { -1, -1 };
	FILE *out = stdout;
	pid_t pager = -1;

	if ((! len) || (len == sizeof(uint32_t))) {
		/* empty command or empty reply */
		return;
	}
	else if (len < sizeof(uint32_t)) {
		sdb_log(SDB_LOG_ERR, "Received a DATA message with invalid "
				"or missing data-type");
		return;
	}

	if (input->interactive) {
		if (pipe(pipefd)) {
			char errbuf[2014];
			sdb_log(SDB_LOG_WARNING, "Failed to open pipe: %s",
					sdb_strerror(errno, errbuf, sizeof(errbuf)));
		}
		else {
			out = fdopen(pipefd[1], "w");
			pager = exec_pager(pipefd[0]);
			if (pager < 0) {
				out = stdout;
				close(pipefd[0]);
				close(pipefd[1]);
			}
			else
				close(pipefd[0]);
		}
	}

	sdb_proto_unmarshal_int32(SDB_STRBUF_STR(buf), &type);
	sdb_strbuf_skip(buf, 0, sizeof(uint32_t));
	if (sdb_json_print(out, input, (int)type, buf))
		sdb_log(SDB_LOG_ERR, "Failed to print result");
	fprintf(out, "\n");

	if (out != stdout)
		fclose(out); /* will close pipefd[1] */
	if (pager > 0)
		waitpid(pager, NULL, 0);
} /* data_printer */

static struct {
	int status;
	void (*printer)(sdb_input_t *, sdb_strbuf_t *);
} response_printers[] = {
	{ SDB_CONNECTION_OK,   ok_printer },
	{ SDB_CONNECTION_LOG,  log_printer },
	{ SDB_CONNECTION_DATA, data_printer },
};

static void
clear_query(sdb_input_t *input)
{
	sdb_strbuf_skip(input->input, 0, input->query_len);
	input->tokenizer_pos -= input->query_len;
	input->query_len = 0;
	input->have_input = 0;
} /* clear_query */

/*
 * public API
 */

int
sdb_command_print_reply(sdb_input_t *input)
{
	sdb_strbuf_t *recv_buf;
	const char *result;
	uint32_t rcode = 0;

	int status = -1;
	size_t i;

	recv_buf = sdb_strbuf_create(1024);
	if (! recv_buf)
		return -1;

	if (sdb_client_recv(input->client, &rcode, recv_buf) < 0)
		rcode = UINT32_MAX;

	if (sdb_client_eof(input->client)) {
		sdb_strbuf_destroy(recv_buf);
		return -1;
	}

	if (rcode != UINT32_MAX)
		status = (int)rcode;

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(response_printers); ++i) {
		if (status == response_printers[i].status) {
			response_printers[i].printer(input, recv_buf);
			sdb_strbuf_destroy(recv_buf);
			return status;
		}
	}

	result = sdb_strbuf_string(recv_buf);
	if (result && *result)
		sdb_log(SDB_LOG_ERR, "%s", result);
	else if (rcode == UINT32_MAX) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "%s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
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
	}

	if (sdb_client_eof(input->client)) {
		if (sdb_input_reconnect()) {
			clear_query(input);
			return data;
		}
	}
	else if (! query_len)
		return NULL;

	sdb_client_send(input->client, SDB_CONNECTION_QUERY, query_len, query);

	/* The server may send back log messages but will eventually reply to the
	 * query, which is either DATA or ERROR. */
	while (42) {
		int status = sdb_command_print_reply(input);
		if (status < 0) {
			sdb_log(SDB_LOG_ERR, "Failed to read reply from server");
			break;
		}

		if ((status == SDB_CONNECTION_OK)
				|| (status == SDB_CONNECTION_DATA)
				|| (status == SDB_CONNECTION_ERROR))
			break;
	}
	clear_query(input);
	return data;
} /* sdb_command_exec */

void
sdb_command_print_server_version(sdb_input_t *input)
{
	sdb_strbuf_t *buf = sdb_strbuf_create(32);
	uint32_t code = 0, version = 0;
	const char *extra;

	if ((sdb_client_rpc(input->client, SDB_CONNECTION_SERVER_VERSION,
					0, NULL, &code, buf) < 0) || (code != SDB_CONNECTION_OK))
		return;
	if (sdb_strbuf_len(buf) < sizeof(version))
		return;

	sdb_proto_unmarshal_int32(SDB_STRBUF_STR(buf), &version);
	extra = sdb_strbuf_string(buf) + sizeof(version);
	sdb_log(SDB_LOG_INFO, "SysDB server %d.%d.%d%s",
			SDB_VERSION_DECODE((int)version), extra);
	sdb_strbuf_destroy(buf);
} /* sdb_command_print_server_version */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

