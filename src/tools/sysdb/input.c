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

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "tools/sysdb/input.h"

#include "utils/strbuf.h"

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

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

/*
 * private helper functions
 */

static size_t
input_readline(sdb_input_t *input)
{
	const char *prompt = "sysdb=> ";
	char *line;

	size_t len;

	if (input->query_len)
		prompt = "sysdb-> ";

	line = readline(prompt);

	if (! line)
		return 0;

	len = strlen(line) + 1;

	sdb_strbuf_append(input->input, line);
	sdb_strbuf_append(input->input, "\n");
	free(line);
	return len;
} /* input_readline */

/*
 * API
 */

ssize_t
sdb_input_readline(sdb_input_t *input, char *buf,
		int *n_chars, size_t max_chars)
{
	size_t len;

	len = sdb_strbuf_len(input->input) - input->tokenizer_pos;

	if (! len) {
		size_t n = input_readline(input);
		if (! n) {
			*n_chars = 0; /* YY_NULL */
			return 0;
		}
		len += n;
	}

	len = (len < max_chars) ? len : max_chars;
	strncpy(buf, sdb_strbuf_string(input->input) + input->tokenizer_pos, len);
	input->tokenizer_pos += len;
	*n_chars = (int)len;

	return (ssize_t)len;
} /* sdb_input_readline */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

