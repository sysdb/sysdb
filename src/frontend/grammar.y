/*
 * SysDB - src/frontend/grammar.y
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

#include "frontend/parser.h"
#include "frontend/grammar.h"
#include "utils/error.h"

#include <stdio.h>

void
sdb_fe_yyerror(YYLTYPE *lval, sdb_fe_yyscan_t scanner, const char *msg);

%}

%pure-parser
%lex-param {sdb_fe_yyscan_t scanner}
%parse-param {sdb_fe_yyscan_t scanner}
%locations
%error-verbose
%expect 0
%name-prefix="sdb_fe_yy"

%start statements

%token SCANNER_ERROR

%token IDENTIFIER
%token LIST

%%

statements:
	statements ';' statement
		{
		}
	|
	statement
		{
		}
	;

statement:
	list_statement
		{
		}
	|
	/* empty */
	;

list_statement:
	LIST
	;

%%

void
sdb_fe_yyerror(YYLTYPE *lval, sdb_fe_yyscan_t scanner, const char *msg)
{
	sdb_log(SDB_LOG_ERR, "frontend: parse error: %s", msg);
} /* sdb_fe_yyerror */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

