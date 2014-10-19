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
#include "core/store-private.h"
#include "core/time.h"

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

#define MODE_TO_STRING(m) \
	(((m) == SDB_PARSE_DEFAULT) ? "statement" \
		: ((m) == SDB_PARSE_COND) ? "condition" \
		: ((m) == SDB_PARSE_EXPR) ? "expression" \
		: "UNKNOWN")

%}

%pure-parser
%lex-param {sdb_fe_yyscan_t scanner}
%parse-param {sdb_fe_yyscan_t scanner}
%locations
%error-verbose
%expect 0
%name-prefix "sdb_fe_yy"

%union {
	const char *sstr; /* static string */
	char *str;

	sdb_data_t data;
	sdb_time_t datetime;

	sdb_llist_t     *list;
	sdb_conn_node_t *node;

	sdb_store_matcher_t *m;
	sdb_store_expr_t *expr;
}

%start statements

%token SCANNER_ERROR

%token AND OR IS NOT MATCHING FILTER
%token CMP_EQUAL CMP_NEQUAL CMP_REGEX CMP_NREGEX
%token CMP_LT CMP_LE CMP_GE CMP_GT
%token CONCAT

%token START END

/* NULL token */
%token NULL_T

%token FETCH LIST LOOKUP TIMESERIES

%token <str> IDENTIFIER STRING

%token <data> INTEGER FLOAT

%token <datetime> DATE TIME

/* Precedence (lowest first): */
%left OR
%left AND
%right NOT
%left CMP_EQUAL CMP_NEQUAL
%left CMP_LT CMP_LE CMP_GE CMP_GT
%nonassoc CMP_REGEX CMP_NREGEX
%left CONCAT
%nonassoc IS
%left '+' '-'
%left '*' '/' '%'
%left '[' ']'
%left '(' ')'
%left '.'

%type <list> statements
%type <node> statement
	fetch_statement
	list_statement
	lookup_statement
	timeseries_statement
	matching_clause
	filter_clause
	condition

%type <m> matcher
	compare_matcher

%type <expr> expression

%type <sstr> cmp

%type <data> data
	interval interval_elem

%type <datetime> datetime
	start_clause end_clause

%destructor { free($$); } <str>
%destructor { sdb_object_deref(SDB_OBJ($$)); } <node> <m> <expr>
%destructor { sdb_data_free_datum(&$$); } <data>

%%

statements:
	statements ';' statement
		{
			/* only accepted in default parse mode */
			if (parser_mode != SDB_PARSE_DEFAULT) {
				char errmsg[1024];
				snprintf(errmsg, sizeof(errmsg),
						YY_("syntax error, unexpected statement, "
							"expecting %s"), MODE_TO_STRING(parser_mode));
				sdb_fe_yyerror(&yylloc, scanner, errmsg);
				sdb_object_deref(SDB_OBJ($3));
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
			/* only accepted in default parse mode */
			if (parser_mode != SDB_PARSE_DEFAULT) {
				char errmsg[1024];
				snprintf(errmsg, sizeof(errmsg),
						YY_("syntax error, unexpected statement, "
							"expecting %s"), MODE_TO_STRING(parser_mode));
				sdb_fe_yyerror(&yylloc, scanner, errmsg);
				sdb_object_deref(SDB_OBJ($1));
				YYABORT;
			}

			if ($1) {
				sdb_llist_append(pt, SDB_OBJ($1));
				sdb_object_deref(SDB_OBJ($1));
			}
		}
	|
	condition
		{
			/* only accepted in condition parse mode */
			if (! (parser_mode & SDB_PARSE_COND)) {
				char errmsg[1024];
				snprintf(errmsg, sizeof(errmsg),
						YY_("syntax error, unexpected condition, "
							"expecting %s"), MODE_TO_STRING(parser_mode));
				sdb_fe_yyerror(&yylloc, scanner, errmsg);
				sdb_object_deref(SDB_OBJ($1));
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
			/* only accepted in expression parse mode */
			if (! (parser_mode & SDB_PARSE_EXPR)) {
				char errmsg[1024];
				snprintf(errmsg, sizeof(errmsg),
						YY_("syntax error, unexpected expression, "
							"expecting %s"), MODE_TO_STRING(parser_mode));
				sdb_fe_yyerror(&yylloc, scanner, errmsg);
				sdb_object_deref(SDB_OBJ($1));
				YYABORT;
			}

			if ($1) {
				sdb_conn_node_t *n;
				n = SDB_CONN_NODE(sdb_object_create_dT(/* name = */ NULL,
							conn_expr_t, conn_expr_destroy));
				n->cmd = CONNECTION_EXPR;
				CONN_EXPR(n)->expr = $1;

				sdb_llist_append(pt, SDB_OBJ(n));
				sdb_object_deref(SDB_OBJ(n));
			}
		}
	;

statement:
	fetch_statement
	|
	list_statement
	|
	lookup_statement
	|
	timeseries_statement
	|
	/* empty */
		{
			$$ = NULL;
		}
	;

/*
 * FETCH <type> <hostname> [FILTER <condition>];
 *
 * Retrieve detailed information about a single host.
 */
fetch_statement:
	FETCH IDENTIFIER STRING filter_clause
		{
			/* TODO: support other types as well */
			if (strcasecmp($2, "host")) {
				char errmsg[strlen($2) + 32];
				snprintf(errmsg, sizeof(errmsg),
						YY_("unknown data-source %s"), $2);
				sdb_fe_yyerror(&yylloc, scanner, errmsg);
				free($2); $2 = NULL;
				free($3); $3 = NULL;
				sdb_object_deref(SDB_OBJ($4));
				YYABORT;
			}

			$$ = SDB_CONN_NODE(sdb_object_create_dT(/* name = */ NULL,
						conn_fetch_t, conn_fetch_destroy));
			CONN_FETCH($$)->type = SDB_HOST;
			CONN_FETCH($$)->name = $3;
			CONN_FETCH($$)->filter = CONN_MATCHER($4);
			$$->cmd = CONNECTION_FETCH;
			free($2); $2 = NULL;
		}
	;

/*
 * LIST <type> [FILTER <condition>];
 *
 * Returns a list of all hosts in the store.
 */
list_statement:
	LIST IDENTIFIER filter_clause
		{
			int type = sdb_store_parse_object_type_plural($2);
			if (type < 0) {
				char errmsg[strlen($2) + 32];
				snprintf(errmsg, sizeof(errmsg),
						YY_("unknown data-source %s"), $2);
				sdb_fe_yyerror(&yylloc, scanner, errmsg);
				free($2); $2 = NULL;
				sdb_object_deref(SDB_OBJ($3));
				YYABORT;
			}

			$$ = SDB_CONN_NODE(sdb_object_create_dT(/* name = */ NULL,
						conn_list_t, conn_list_destroy));
			CONN_LIST($$)->type = type;
			CONN_LIST($$)->filter = CONN_MATCHER($3);
			$$->cmd = CONNECTION_LIST;
			free($2); $2 = NULL;
		}
	;

/*
 * LOOKUP <type> MATCHING <condition> [FILTER <condition>];
 *
 * Returns detailed information about <type> matching condition.
 */
lookup_statement:
	LOOKUP IDENTIFIER matching_clause filter_clause
		{
			/* TODO: support other types as well */
			if (strcasecmp($2, "hosts")) {
				char errmsg[strlen($2) + 32];
				snprintf(errmsg, sizeof(errmsg),
						YY_("unknown data-source %s"), $2);
				sdb_fe_yyerror(&yylloc, scanner, errmsg);
				free($2); $2 = NULL;
				sdb_object_deref(SDB_OBJ($3));
				sdb_object_deref(SDB_OBJ($4));
				YYABORT;
			}

			$$ = SDB_CONN_NODE(sdb_object_create_dT(/* name = */ NULL,
						conn_lookup_t, conn_lookup_destroy));
			CONN_LOOKUP($$)->type = SDB_HOST;
			CONN_LOOKUP($$)->matcher = CONN_MATCHER($3);
			CONN_LOOKUP($$)->filter = CONN_MATCHER($4);
			$$->cmd = CONNECTION_LOOKUP;
			free($2); $2 = NULL;
		}
	;

matching_clause:
	MATCHING condition { $$ = $2; }
	|
	/* empty */ { $$ = NULL; }

filter_clause:
	FILTER condition { $$ = $2; }
	|
	/* empty */ { $$ = NULL; }

/*
 * TIMESERIES <host>.<metric> [START <datetime>] [END <datetime>];
 *
 * Returns a time-series for the specified host's metric.
 */
timeseries_statement:
	TIMESERIES STRING '.' STRING start_clause end_clause
		{
			$$ = SDB_CONN_NODE(sdb_object_create_dT(/* name = */ NULL,
						conn_ts_t, conn_ts_destroy));
			CONN_TS($$)->hostname = $2;
			CONN_TS($$)->metric = $4;
			CONN_TS($$)->opts.start = $5;
			CONN_TS($$)->opts.end = $6;
			$$->cmd = CONNECTION_TIMESERIES;
		}
	;

start_clause:
	START datetime { $$ = $2; }
	|
	/* empty */ { $$ = sdb_gettime() - SDB_INTERVAL_HOUR; }

end_clause:
	END datetime { $$ = $2; }
	|
	/* empty */ { $$ = sdb_gettime(); }

/*
 * Basic expressions.
 */

condition:
	matcher
		{
			if (! $1) {
				/* TODO: improve error reporting */
				sdb_fe_yyerror(&yylloc, scanner,
						YY_("syntax error, invalid condition"));
				YYABORT;
			}

			$$ = SDB_CONN_NODE(sdb_object_create_dT(/* name = */ NULL,
						conn_matcher_t, conn_matcher_destroy));
			$$->cmd = CONNECTION_MATCHER;
			CONN_MATCHER($$)->matcher = $1;
		}
	;

matcher:
	'(' matcher ')'
		{
			$$ = $2;
		}
	|
	matcher AND matcher
		{
			$$ = sdb_store_con_matcher($1, $3);
			sdb_object_deref(SDB_OBJ($1));
			sdb_object_deref(SDB_OBJ($3));
		}
	|
	matcher OR matcher
		{
			$$ = sdb_store_dis_matcher($1, $3);
			sdb_object_deref(SDB_OBJ($1));
			sdb_object_deref(SDB_OBJ($3));
		}
	|
	NOT matcher
		{
			$$ = sdb_store_inv_matcher($2);
			sdb_object_deref(SDB_OBJ($2));
		}
	|
	compare_matcher
		{
			$$ = $1;
		}
	;

/*
 * <object_type>.<object_attr> <cmp> <value>
 *
 * Parse matchers comparing object attributes with a value.
 */
compare_matcher:
	'.' IDENTIFIER cmp expression
		{
			$$ = sdb_store_matcher_parse_field_cmp($2, $3, $4);
			free($2); $2 = NULL;
			sdb_object_deref(SDB_OBJ($4));
		}
	|
	IDENTIFIER cmp expression
		{
			$$ = sdb_store_matcher_parse_cmp($1, NULL, $2, $3);
			free($1); $1 = NULL;
			sdb_object_deref(SDB_OBJ($3));
		}
	|
	IDENTIFIER '[' STRING ']' cmp expression
		{
			$$ = sdb_store_matcher_parse_cmp($1, $3, $5, $6);
			free($1); $1 = NULL;
			free($3); $3 = NULL;
			sdb_object_deref(SDB_OBJ($6));
		}
	|
	expression IS NULL_T
		{
			$$ = sdb_store_isnull_matcher($1);
			sdb_object_deref(SDB_OBJ($1));
		}
	|
	expression IS NOT NULL_T
		{
			$$ = sdb_store_isnnull_matcher($1);
			sdb_object_deref(SDB_OBJ($1));
		}
	;

expression:
	'(' expression ')'
		{
			$$ = $2;
		}
	|
	expression '+' expression
		{
			$$ = sdb_store_expr_create(SDB_DATA_ADD, $1, $3);
			sdb_object_deref(SDB_OBJ($1)); $1 = NULL;
			sdb_object_deref(SDB_OBJ($3)); $3 = NULL;
		}
	|
	expression '-' expression
		{
			$$ = sdb_store_expr_create(SDB_DATA_SUB, $1, $3);
			sdb_object_deref(SDB_OBJ($1)); $1 = NULL;
			sdb_object_deref(SDB_OBJ($3)); $3 = NULL;
		}
	|
	expression '*' expression
		{
			$$ = sdb_store_expr_create(SDB_DATA_MUL, $1, $3);
			sdb_object_deref(SDB_OBJ($1)); $1 = NULL;
			sdb_object_deref(SDB_OBJ($3)); $3 = NULL;
		}
	|
	expression '/' expression
		{
			$$ = sdb_store_expr_create(SDB_DATA_DIV, $1, $3);
			sdb_object_deref(SDB_OBJ($1)); $1 = NULL;
			sdb_object_deref(SDB_OBJ($3)); $3 = NULL;
		}
	|
	expression '%' expression
		{
			$$ = sdb_store_expr_create(SDB_DATA_MOD, $1, $3);
			sdb_object_deref(SDB_OBJ($1)); $1 = NULL;
			sdb_object_deref(SDB_OBJ($3)); $3 = NULL;
		}
	|
	expression CONCAT expression
		{
			$$ = sdb_store_expr_create(SDB_DATA_CONCAT, $1, $3);
			sdb_object_deref(SDB_OBJ($1)); $1 = NULL;
			sdb_object_deref(SDB_OBJ($3)); $3 = NULL;
		}
	|
	'.' IDENTIFIER
		{
			int field = sdb_store_parse_field_name($2);
			free($2); $2 = NULL;
			$$ = sdb_store_expr_fieldvalue(field);
		}
	|
	IDENTIFIER '[' STRING ']'
		{
			$$ = sdb_store_expr_attrvalue($3);
			free($1); $1 = NULL;
			free($3); $3 = NULL;
		}
	|
	data
		{
			$$ = sdb_store_expr_constvalue(&$1);
			sdb_data_free_datum(&$1);
		}
	;

cmp:
	CMP_EQUAL { $$ = "="; }
	|
	CMP_NEQUAL { $$ = "!="; }
	|
	CMP_REGEX { $$ = "=~"; }
	|
	CMP_NREGEX { $$ = "!~"; }
	|
	CMP_LT { $$ = "<"; }
	|
	CMP_LE { $$ = "<="; }
	|
	CMP_GE { $$ = ">="; }
	|
	CMP_GT { $$ = ">"; }
	;

data:
	STRING { $$.type = SDB_TYPE_STRING; $$.data.string = $1; }
	|
	INTEGER { $$ = $1; }
	|
	FLOAT { $$ = $1; }
	|
	datetime { $$.type = SDB_TYPE_DATETIME; $$.data.datetime = $1; }
	|
	interval { $$ = $1; }
	;

datetime:
	DATE TIME { $$ = $1 + $2; }
	|
	DATE { $$ = $1; }
	|
	TIME { $$ = $1; }
	;

interval:
	interval interval_elem
		{
			$$.data.datetime = $1.data.datetime + $2.data.datetime;
		}
	|
	interval_elem { $$ = $1; }
	;

interval_elem:
	INTEGER IDENTIFIER
		{
			sdb_time_t unit = 1;

			unit = sdb_strpunit($2);
			if (! unit) {
				char errmsg[strlen($2) + 32];
				snprintf(errmsg, sizeof(errmsg),
						YY_("invalid time unit %s"), $2);
				sdb_fe_yyerror(&yylloc, scanner, errmsg);
				free($2); $2 = NULL;
				YYABORT;
			}
			free($2); $2 = NULL;

			$$.type = SDB_TYPE_DATETIME;
			$$.data.datetime = (sdb_time_t)$1.data.integer * unit;

			if ($1.data.integer < 0) {
				sdb_fe_yyerror(&yylloc, scanner,
						YY_("syntax error, negative intervals not supported"));
				YYABORT;
			}
		}
	;

%%

void
sdb_fe_yyerror(YYLTYPE *lval, sdb_fe_yyscan_t scanner, const char *msg)
{
	sdb_log(SDB_LOG_ERR, "frontend: parse error: %s", msg);
} /* sdb_fe_yyerror */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

