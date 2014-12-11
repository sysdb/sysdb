/*
 * SysDB - src/tools/sysdb/input.c
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
 * This module implements the core of the command line tool. It handles all
 * input from the user and the remote server, interacting with the scanner and
 * command handling as needed.
 *
 * The main loop is managed by the flex scanner which parses the user input.
 * It will call into this module (using sdb_input_readline()) whenever it
 * needs further input to continue parsing. Whenever it finds a full query
 * (terminated by a semicolon), it will hand the query back to this module
 * (using sdb_input_exec_query()) which will then execute it.
 *
 * Most of the process life-time will be spend waiting for input. User input
 * and (asynchronous) server replies are handled at the same time.
 */

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "tools/sysdb/input.h"
#include "tools/sysdb/command.h"

#include "utils/error.h"
#include "utils/strbuf.h"

#include <errno.h>

#include <sys/select.h>

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <termios.h>
#include <unistd.h>

#if HAVE_EDITLINE_READLINE_H
#	include <editline/readline.h>
#	if HAVE_EDITLINE_HISTORY_H
#		include <editline/history.h>
#	endif
#elif HAVE_READLINE_READLINE_H
#	include <readline/readline.h>
#	if HAVE_READLINE_HISTORY_H
#		include <readline/history.h>
#	endif
#elif HAVE_READLINE_H
#	include <readline.h>
#	if HAVE_HISTORY_H
#		include <history.h>
#	endif
#endif /* READLINEs */

extern int yylex(void);

/*
 * public variables
 */

sdb_input_t *sysdb_input = NULL;

/*
 * private variables
 */

static struct termios orig_term_attrs;
static bool have_orig_term_attrs;

/*
 * private helper functions
 */

static void
reset_term_attrs(void)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &orig_term_attrs);
} /* reset_term_attrs */

static void
term_rawmode(void)
{
	struct termios attrs;

	if (! have_orig_term_attrs) {
		memset(&orig_term_attrs, 0, sizeof(orig_term_attrs));
		tcgetattr(STDIN_FILENO, &orig_term_attrs);
		atexit(reset_term_attrs);
		have_orig_term_attrs = 1;
	}

	/* setup terminal to operate in non-canonical mode
	 * and single character input */
	memset(&attrs, 0, sizeof(attrs));
	tcgetattr(STDIN_FILENO, &attrs);
	attrs.c_lflag &= (tcflag_t)(~ICANON);
	attrs.c_cc[VMIN] = 1;
	tcsetattr(STDIN_FILENO, TCSANOW, &attrs);
} /* term_rawmode */

static void
handle_input(char *line)
{
	if (! line) {
		sysdb_input->eof = 1;
		return;
	}

	if (sdb_client_eof(sysdb_input->client))
		sdb_input_reconnect();

	sdb_strbuf_append(sysdb_input->input, "%s\n", line);
	free(line);

	if (sysdb_input->interactive)
		rl_callback_handler_remove();
} /* handle_input */

/* wait for a new line of data to be available */
static ssize_t
input_readline(void)
{
	size_t len;

	fd_set fds;
	int client_fd;

	const char *prompt = "sysdb=> ";

	len = sdb_strbuf_len(sysdb_input->input);

	if (! sysdb_input->interactive) {
		char *line = readline("");
		handle_input(line);
		return (ssize_t)(sdb_strbuf_len(sysdb_input->input) - len);
	}

	if (sysdb_input->query_len)
		prompt = "sysdb-> ";
	if (sdb_client_eof(sysdb_input->client))
		prompt = "!-> ";

	rl_callback_handler_install(prompt, handle_input);
	client_fd = sdb_client_sockfd(sysdb_input->client);
	while ((sdb_strbuf_len(sysdb_input->input) == len)
			&& (! sysdb_input->eof)) {
		bool connected = !sdb_client_eof(sysdb_input->client);
		int max_fd, n;

		/* XXX: some versions of libedit don't properly reset the terminal in
		 * rl_callback_read_char(); detect those versions */
		term_rawmode();

		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		max_fd = STDIN_FILENO;

		if (connected) {
			FD_SET(client_fd, &fds);
			if (client_fd > max_fd)
				max_fd = client_fd;
		}

		n = select(max_fd + 1, &fds, NULL, NULL, /* timeout = */ NULL);
		if (n < 0)
			return (ssize_t)n;
		else if (! n)
			continue;

		/* handle user input with highest priority */
		if (FD_ISSET(STDIN_FILENO, &fds)) {
			rl_callback_read_char();
			continue;
		}

		if ((! connected) || (! FD_ISSET(client_fd, &fds)))
			continue;

		/* some response / error message from the server pending */
		/* XXX: clear current line */
		printf("\n");
		sdb_command_print_reply(sysdb_input->client);

		if (sdb_client_eof(sysdb_input->client)) {
			rl_callback_handler_remove();
			/* XXX */
			printf("Remote side closed the connection.\n");
			/* return EOF -> restart scanner */
			return 0;
		}
		else
			rl_forced_update_display();
	}

	/* new data available */
	return (ssize_t)(sdb_strbuf_len(sysdb_input->input) - len);
} /* input_readline */

/*
 * public API
 */

int
sdb_input_init(sdb_input_t *input)
{
	/* register input handler */
	sysdb_input = input;

	input->interactive = isatty(STDIN_FILENO) != 0;
	errno = 0;
	if (input->interactive)
		term_rawmode();
	return 0;
} /* sdb_input_init */

int
sdb_input_mainloop(void)
{
	while (! sysdb_input->eof)
		yylex();
	return 0;
} /* sdb_input_mainloop */

ssize_t
sdb_input_readline(char *buf, size_t *n_chars, size_t max_chars)
{
	const char *data;
	size_t len;

	len = sdb_strbuf_len(sysdb_input->input) - sysdb_input->tokenizer_pos;

	if (! len) {
		ssize_t n = input_readline();
		if (n <= 0) {
			*n_chars = 0; /* YY_NULL */
			return n;
		}
		len += (size_t)n;
	}

	len = (len < max_chars) ? len : max_chars;
	data = sdb_strbuf_string(sysdb_input->input);
	data += sysdb_input->tokenizer_pos;
	strncpy(buf, data, len);

	sysdb_input->tokenizer_pos += len;
	*n_chars = (int)len;
	return (ssize_t)len;
} /* sdb_input_readline */

int
sdb_input_exec_query(void)
{
	char *query = sdb_command_exec(sysdb_input);

	HIST_ENTRY *hist;
	const char *prev = NULL;

	if (! query)
		return -1;

	hist = history_get(history_length);
	if (hist)
		prev = hist->line;

	if (*query != ' ')
		if ((! prev) || strcmp(prev, query))
			add_history(query);
	free(query);
	return 0;
} /* sdb_input_exec_query */

int
sdb_input_reconnect(void)
{
	sdb_client_close(sysdb_input->client);
	if (sdb_client_connect(sysdb_input->client, sysdb_input->user)) {
		printf("Failed to reconnect to SysDBd.\n");
		return -1;
	}
	printf("Successfully reconnected to SysDBd.\n");
	return 0;
} /* sdb_input_reconnect */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

