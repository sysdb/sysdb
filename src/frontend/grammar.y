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

#include "frontend/connection-private.h"
#include "frontend/parser.h"
#include "frontend/grammar.h"

#include "core/store.h"

#include "utils/error.h"
#include "utils/llist.h"

#include <stdio.h>
#include <string.h>

int
sdb_fe_yylex(YYSTYPE *yylval, YYLTYPE *yylloc, sdb_fe_yyscan_t yyscanner);

sdb_fe_yyextra_t *
sdb_fe_yyget_extra(sdb_fe_yyscan_t scanner);

void
sdb_fe_yyerror(YYLTYPE *lval, sdb_fe_yyscan_t scanner, const char *msg);

/* quick access to the current parse tree */
#define pt sdb_fe_yyget_extra(scanner)->parsetree

/* quick access to the parser mode */
#define parser_mode sdb_fe_yyget_extra(scanner)->mode

%}

%pure-parser
%lex-param {sdb_fe_yyscan_t scanner}
%parse-param {sdb_fe_yyscan_t scanner}
%locations
%error-verbose
%expect 0
%name-prefix="sdb_fe_yy"

%union {
	char *str;

	sdb_llist_t     *list;
	sdb_conn_node_t *node;
}

%start statements

%token SCANNER_ERROR

%token <str> IDENTIFIER STRING
%token <node> FETCH LIST

%type <list> statements
%type <node> statement
	fetch_statement
	list_statement
	expression

%%

statements:
	statements ';' statement
		{
			/* only accept this in default parse mode */
			if (parser_mode != SDB_PARSE_DEFAULT) {
				sdb_fe_yyerror(&yylloc, scanner,
						YY_("syntax error, unexpected statement, "
							"expecting expression"));
				YYABORT;
			}

			if ($3) {
				sdb_llist_append(pt, SDB_OBJ($3));
				sdb_object_deref(SDB_OBJ($3));
			}
		}
	|
	statement
		{
			/* only accept this in default parse mode */
			if (parser_mode != SDB_PARSE_DEFAULT) {
				sdb_fe_yyerror(&yylloc, scanner,
						YY_("syntax error, unexpected statement, "
							"expecting expression"));
				YYABORT;
			}

			if ($1) {
				sdb_llist_append(pt, SDB_OBJ($1));
				sdb_object_deref(SDB_OBJ($1));
			}
		}
	|
	expression
		{
			/* only accept this in expression parse mode */
			if (! (parser_mode & SDB_PARSE_EXPR)) {
				sdb_fe_yyerror(&yylloc, scanner,
						YY_("syntax error, unexpected expression, "
							"expecting statement"));
				YYABORT;
			}

			if ($1) {
				sdb_llist_append(pt, SDB_OBJ($1));
				sdb_object_deref(SDB_OBJ($1));
			}
		}
	;

statement:
	fetch_statement
	|
	list_statement
	|
	/* empty */
		{
			$$ = NULL;
		}
	;

/*
 * FETCH <hostname>;
 *
 * Retrieve detailed information about a single host.
 */
fetch_statement:
	FETCH STRING
		{
			$$ = SDB_CONN_NODE(sdb_object_create_dT(/* name = */ NULL,
						conn_fetch_t, conn_fetch_destroy));
			CONN_FETCH($$)->name = strdup($2);
			$$->cmd = CONNECTION_FETCH;
			free($2);
			$2 = NULL;
		}
	;

/*
 * LIST;
 *
 * Returns a list of all hosts in the store.
 */
list_statement:
	LIST
		{
			$$ = SDB_CONN_NODE(sdb_object_create_T(/* name = */ NULL,
						sdb_conn_node_t));
			$$->cmd = CONNECTION_LIST;
		}
	;

expression:
	STRING
		{
			$$ = SDB_CONN_NODE(sdb_object_create_T(/* name = */ NULL,
						conn_node_matcher_t));
			$$->cmd = CONNECTION_EXPR;
			/* XXX: this is just a placeholder for now */
			CONN_MATCHER($$)->matcher = sdb_store_host_matcher($1,
					/* name_re = */ NULL, /* service = */ NULL,
					/* attr = */ NULL);
			free($1);
			$1 = NULL;
		}
	;

%%

void
sdb_fe_yyerror(YYLTYPE *lval, sdb_fe_yyscan_t scanner, const char *msg)
{
	sdb_log(SDB_LOG_ERR, "frontend: parse error: %s", msg);
} /* sdb_fe_yyerror */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

