/*
 * SysDB - src/tools/sysdb/scanner.l
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

%{

/*
 * This is a simplified version of frontend/scanner.l. The only purpose is to
 * find queries (terminated by semicolon).
 */

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "tools/sysdb/input.h"

#include <ctype.h>
#include <string.h>

#ifdef YY_INPUT
#	undef YY_INPUT
#endif
#define YY_INPUT(buf, result, max_size) \
	sdb_input_readline((buf), (size_t *)&(result), (max_size))

#define APPEND() \
	do { \
		if (! isspace((int)yytext[0])) \
			sysdb_input->have_input = 1; \
		sysdb_input->query_len += strlen(yytext); \
	} while (0)

%}

%option interactive
%option 8bit
%option yylineno
%option nodefault
%option noyywrap
%option warn

%x CSC

newline			(\n|\r\n)
simple_comment	("--"[^\n\r]*)

/*
 * C style comments
 */
csc_start	\/\*
csc_inside	([^*/]+|[^*]\/|\*[^/])
csc_end		\*\/

/*
 * Strings.
 */
/* TODO: fully support SQL strings */
string		('([^']|'')*')

%%

	/*
	 * Here, we only care about syntax elements that may include semicolons
	 * and escape their meaning as a query terminator.
	 */

{simple_comment}	{ APPEND(); }

{csc_start}			{ APPEND(); BEGIN(CSC); }
<CSC>{csc_inside}	{ APPEND(); }
<CSC>{csc_end}		{ APPEND(); BEGIN(INITIAL); }
<CSC><<EOF>>		{ return -1; }

{string}			{ APPEND(); }

	/*
	 * The following rules are specific to the command line tool.
	 */

";\n"		{ APPEND(); sdb_input_exec_query(); }
";"			{ APPEND(); sdb_input_exec_query(); }

{newline}	{
				APPEND();
				if (! sysdb_input->have_input)
					/* give the input module a chance to do stuff on empty lines */
					sdb_input_exec_query();
			}

.			{ APPEND(); }

%%

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

