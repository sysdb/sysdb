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

#include <assert.h>

#include <stdio.h>
#include <string.h>

/*
 * private helper functions
 */

static sdb_store_matcher_t *
name_iter_matcher(int m_type, int type, const char *cmp,
		sdb_store_expr_t *expr);

/*
 * public API
 */

int
sdb_fe_yylex(YYSTYPE *yylval, YYLTYPE *yylloc, sdb_fe_yyscan_t yyscanner);

sdb_fe_yyextra_t *
sdb_fe_yyget_extra(sdb_fe_yyscan_t scanner);

void
sdb_fe_yyerror(YYLTYPE *lval, sdb_fe_yyscan_t scanner, const char *msg);
void
sdb_fe_yyerrorf(YYLTYPE *lval, sdb_fe_yyscan_t scanner, const char *fmt, ...);

/* quick access to the current parse tree */
#define pt sdb_fe_yyget_extra(scanner)->parsetree

/* quick access to the parser mode */
#define parser_mode sdb_fe_yyget_extra(scanner)->mode

/* quick access to the parser's error buffer */
#define errbuf sdb_fe_yyget_extra(scanner)->errbuf

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
	int integer;

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
%token CMP_LT CMP_LE CMP_GE CMP_GT ALL ANY IN
%token CONCAT

%token HOST_T HOSTS_T SERVICE_T SERVICES_T METRIC_T METRICS_T
%token ATTRIBUTE_T ATTRIBUTES_T
%token NAME_T LAST_UPDATE_T AGE_T INTERVAL_T BACKEND_T

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
%nonassoc IN
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

%type <expr> expression object_expression

%type <integer> object_type object_type_plural
%type <integer> iterable
%type <integer> field

%type <sstr> cmp

%type <data> data
	interval interval_elem
	array array_elem_list

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
				sdb_fe_yyerrorf(&yylloc, scanner,
						YY_("syntax error, unexpected statement, "
							"expecting %s"), MODE_TO_STRING(parser_mode));
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
				sdb_fe_yyerrorf(&yylloc, scanner,
						YY_("syntax error, unexpected statement, "
							"expecting %s"), MODE_TO_STRING(parser_mode));
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
				sdb_fe_yyerrorf(&yylloc, scanner,
						YY_("syntax error, unexpected condition, "
							"expecting %s"), MODE_TO_STRING(parser_mode));
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
				sdb_fe_yyerrorf(&yylloc, scanner,
						YY_("syntax error, unexpected expression, "
							"expecting %s"), MODE_TO_STRING(parser_mode));
				sdb_object_deref(SDB_OBJ($1));
				YYABORT;
			}

			if ($1) {
				sdb_conn_node_t *n;
				n = SDB_CONN_NODE(sdb_object_create_dT(/* name = */ NULL,
							conn_expr_t, conn_expr_destroy));
				n->cmd = SDB_CONNECTION_EXPR;
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
	FETCH object_type STRING filter_clause
		{
			$$ = SDB_CONN_NODE(sdb_object_create_dT(/* name = */ NULL,
						conn_fetch_t, conn_fetch_destroy));
			CONN_FETCH($$)->type = $2;
			CONN_FETCH($$)->host = $3;
			CONN_FETCH($$)->name = NULL;
			CONN_FETCH($$)->filter = CONN_MATCHER($4);
			$$->cmd = SDB_CONNECTION_FETCH;
		}
	|
	FETCH object_type STRING '.' STRING filter_clause
		{
			$$ = SDB_CONN_NODE(sdb_object_create_dT(/* name = */ NULL,
						conn_fetch_t, conn_fetch_destroy));
			CONN_FETCH($$)->type = $2;
			CONN_FETCH($$)->host = $3;
			CONN_FETCH($$)->name = $5;
			CONN_FETCH($$)->filter = CONN_MATCHER($6);
			$$->cmd = SDB_CONNECTION_FETCH;
		}
	;

/*
 * LIST <type> [FILTER <condition>];
 *
 * Returns a list of all hosts in the store.
 */
list_statement:
	LIST object_type_plural filter_clause
		{
			$$ = SDB_CONN_NODE(sdb_object_create_dT(/* name = */ NULL,
						conn_list_t, conn_list_destroy));
			CONN_LIST($$)->type = $2;
			CONN_LIST($$)->filter = CONN_MATCHER($3);
			$$->cmd = SDB_CONNECTION_LIST;
		}
	;

/*
 * LOOKUP <type> MATCHING <condition> [FILTER <condition>];
 *
 * Returns detailed information about <type> matching condition.
 */
lookup_statement:
	LOOKUP object_type_plural matching_clause filter_clause
		{
			$$ = SDB_CONN_NODE(sdb_object_create_dT(/* name = */ NULL,
						conn_lookup_t, conn_lookup_destroy));
			CONN_LOOKUP($$)->type = $2;
			CONN_LOOKUP($$)->matcher = CONN_MATCHER($3);
			CONN_LOOKUP($$)->filter = CONN_MATCHER($4);
			$$->cmd = SDB_CONNECTION_LOOKUP;
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
			$$->cmd = SDB_CONNECTION_TIMESERIES;
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
			$$->cmd = SDB_CONNECTION_MATCHER;
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

compare_matcher:
	expression cmp expression
		{
			sdb_store_matcher_op_cb cb = sdb_store_parse_matcher_op($2);
			assert(cb); /* else, the grammar accepts invalid 'cmp' */
			$$ = cb($1, $3);
			sdb_object_deref(SDB_OBJ($1));
			sdb_object_deref(SDB_OBJ($3));
		}
	|
	ANY iterable cmp expression
		{
			$$ = name_iter_matcher(MATCHER_ANY, $2, $3, $4);
			sdb_object_deref(SDB_OBJ($4));
		}
	|
	ALL iterable cmp expression
		{
			$$ = name_iter_matcher(MATCHER_ALL, $2, $3, $4);
			sdb_object_deref(SDB_OBJ($4));
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
	|
	expression IN expression
		{
			$$ = sdb_store_in_matcher($1, $3);
			sdb_object_deref(SDB_OBJ($1));
			sdb_object_deref(SDB_OBJ($3));
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
	object_expression
		{
			$$ = $1;
		}
	|
	data
		{
			$$ = sdb_store_expr_constvalue(&$1);
			sdb_data_free_datum(&$1);
		}
	;

object_expression:
	object_type '.' object_expression
		{
			$$ = sdb_store_expr_typed($1, $3);
			sdb_object_deref(SDB_OBJ($3));
		}
	|
	field
		{
			$$ = sdb_store_expr_fieldvalue($1);
		}
	|
	ATTRIBUTE_T '[' STRING ']'
		{
			$$ = sdb_store_expr_attrvalue($3);
			free($3); $3 = NULL;
		}
	;

object_type:
	HOST_T { $$ = SDB_HOST; }
	|
	SERVICE_T { $$ = SDB_SERVICE; }
	|
	METRIC_T { $$ = SDB_METRIC; }
	;

object_type_plural:
	HOSTS_T { $$ = SDB_HOST; }
	|
	SERVICES_T { $$ = SDB_SERVICE; }
	|
	METRICS_T { $$ = SDB_METRIC; }
	;

iterable:
	SERVICE_T { $$ = SDB_SERVICE; }
	|
	METRIC_T { $$ = SDB_METRIC; }
	|
	ATTRIBUTE_T { $$ = SDB_ATTRIBUTE; }
	|
	BACKEND_T { $$ = SDB_FIELD_BACKEND; }
	;

field:
	NAME_T { $$ = SDB_FIELD_NAME; }
	|
	LAST_UPDATE_T { $$ = SDB_FIELD_LAST_UPDATE; }
	|
	AGE_T { $$ = SDB_FIELD_AGE; }
	|
	INTERVAL_T { $$ = SDB_FIELD_INTERVAL; }
	|
	BACKEND_T { $$ = SDB_FIELD_BACKEND; }
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
	|
	array { $$ = $1; }
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
				sdb_fe_yyerrorf(&yylloc, scanner,
						YY_("syntax error, invalid time unit %s"), $2);
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

array:
	'[' array_elem_list ']'
		{
			$$ = $2;
		}
	;

array_elem_list:
	array_elem_list ',' data
		{
			size_t elem_size;

			if (($3.type & SDB_TYPE_ARRAY) || (($1.type & 0xff) != $3.type)) {
				sdb_fe_yyerrorf(&yylloc, scanner, YY_("syntax error, "
						"cannot use element of type %s in array of type %s"),
						SDB_TYPE_TO_STRING($3.type),
						SDB_TYPE_TO_STRING($1.type));
				sdb_data_free_datum(&$1);
				sdb_data_free_datum(&$3);
				YYABORT;
			}

			elem_size = sdb_data_sizeof($3.type);
			$1.data.array.values = realloc($1.data.array.values,
					($1.data.array.length + 1) * elem_size);
			if (! $1.data.array.values) {
				sdb_fe_yyerror(&yylloc, scanner, YY_("out of memory"));
				YYABORT;
			}

			memcpy((char *)$1.data.array.values
						+ $1.data.array.length * elem_size,
					&$3.data, elem_size);
			++$1.data.array.length;

			$$ = $1;
		}
	|
	data
		{
			size_t elem_size;

			if ($1.type & SDB_TYPE_ARRAY) {
				sdb_fe_yyerrorf(&yylloc, scanner, YY_("syntax error, "
						"cannot construct array of type %s"),
						SDB_TYPE_TO_STRING($1.type));
				sdb_data_free_datum(&$1);
				YYABORT;
			}

			$$ = $1;
			$$.type = $1.type | SDB_TYPE_ARRAY;
			$$.data.array.length = 1;
			elem_size = sdb_data_sizeof($1.type);
			$$.data.array.values = malloc(elem_size);
			if (! $$.data.array.values ) {
				sdb_fe_yyerror(&yylloc, scanner, YY_("out of memory"));
				YYABORT;
			}
			memcpy($$.data.array.values, &$1.data, elem_size);
		}
	;

%%

void
sdb_fe_yyerror(YYLTYPE *lval, sdb_fe_yyscan_t scanner, const char *msg)
{
	sdb_log(SDB_LOG_ERR, "frontend: parse error: %s", msg);
	sdb_strbuf_sprintf(errbuf, "%s", msg);
} /* sdb_fe_yyerror */

void
sdb_fe_yyerrorf(YYLTYPE *lval, sdb_fe_yyscan_t scanner, const char *fmt, ...)
{
	va_list ap, aq;
	va_start(ap, fmt);
	va_copy(aq, ap);
	sdb_vlog(SDB_LOG_ERR, fmt, ap);
	sdb_strbuf_vsprintf(errbuf, "%s", aq);
	va_end(ap);
} /* sdb_fe_yyerrorf */

static sdb_store_matcher_t *
name_iter_matcher(int m_type, int type, const char *cmp,
		sdb_store_expr_t *expr)
{
	sdb_store_matcher_op_cb cb = sdb_store_parse_matcher_op(cmp);
	sdb_store_expr_t *e;
	sdb_store_matcher_t *m, *tmp = NULL;
	assert(cb);

	/* hosts are never iterable */
	if (type == SDB_HOST) {
		return NULL;
	}

	if (type == SDB_FIELD_BACKEND)
		e = sdb_store_expr_fieldvalue(type);
	else
		e = sdb_store_expr_fieldvalue(SDB_FIELD_NAME);
	m = cb(e, expr);
	if (m_type == MATCHER_ANY)
		tmp = sdb_store_any_matcher(type, m);
	else if (m_type == MATCHER_ALL)
		tmp = sdb_store_all_matcher(type, m);
	sdb_object_deref(SDB_OBJ(m));
	sdb_object_deref(SDB_OBJ(e));
	return tmp;
} /* name_iter_matcher */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

