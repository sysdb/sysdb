/*
 * SysDB - src/parser/grammar.y
 * Copyright (C) 2013-2015 Sebastian 'tokkee' Harl <sh@tokkee.org>
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
 * Grammar for the SysDB Query Language (SysQL).
 */

%{

#include "core/store.h"
#include "core/time.h"

#include "parser/ast.h"
#include "parser/parser.h"
#include "parser/grammar.h"

#include "utils/error.h"
#include "utils/llist.h"

#include <assert.h>

#include <stdio.h>
#include <string.h>

/*
 * public API
 */

int
sdb_parser_yylex(YYSTYPE *yylval, YYLTYPE *yylloc, sdb_parser_yyscan_t yyscanner);

sdb_parser_yyextra_t *
sdb_parser_yyget_extra(sdb_parser_yyscan_t scanner);

void
sdb_parser_yyerror(YYLTYPE *lval, sdb_parser_yyscan_t scanner, const char *msg);
void
sdb_parser_yyerrorf(YYLTYPE *lval, sdb_parser_yyscan_t scanner, const char *fmt, ...);

/* quick access to the current parse tree */
#define pt sdb_parser_yyget_extra(scanner)->parsetree

/* quick access to the parser mode */
#define parser_mode sdb_parser_yyget_extra(scanner)->mode

/* quick access to the parser's error buffer */
#define errbuf sdb_parser_yyget_extra(scanner)->errbuf

#define CK_OOM(p) \
	do { \
		if (! (p)) { \
			sdb_parser_yyerror(&yylloc, scanner, YY_("out of memory")); \
			YYABORT; \
		} \
	} while (0)

#define MODE_TO_STRING(m) \
	(((m) == SDB_PARSE_DEFAULT) ? "statement" \
		: ((m) == SDB_PARSE_COND) ? "condition" \
		: ((m) == SDB_PARSE_ARITH) ? "arithmetic expression" \
		: "UNKNOWN")

%}

%pure-parser
%lex-param {sdb_parser_yyscan_t scanner}
%parse-param {sdb_parser_yyscan_t scanner}
%locations
%error-verbose
%expect 0
%name-prefix "sdb_parser_yy"

%union {
	char *str;
	int integer;

	sdb_data_t data;
	sdb_time_t datetime;

	sdb_llist_t    *list;
	sdb_ast_node_t *node;

	struct { char *type; char *id; } metric_store;
}

%start statements

%token SCANNER_ERROR

%token AND OR IS NOT MATCHING FILTER
%token CMP_EQUAL CMP_NEQUAL CMP_REGEX CMP_NREGEX
%token CMP_LT CMP_LE CMP_GE CMP_GT ALL ANY IN
%token CONCAT

%token HOST_T HOSTS_T SERVICE_T SERVICES_T METRIC_T METRICS_T
%token ATTRIBUTE_T ATTRIBUTES_T
%token NAME_T LAST_UPDATE_T AGE_T INTERVAL_T BACKEND_T VALUE_T

%token LAST UPDATE

%token START END

/* NULL token */
%token NULL_T

%token TRUE FALSE

%token FETCH LIST LOOKUP STORE TIMESERIES

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
	store_statement
	timeseries_statement
	matching_clause
	filter_clause
	condition comparison
	expression object_expression

%type <integer> object_type object_type_plural
%type <integer> field
%type <integer> cmp

%type <data> data
	interval interval_elem
	array array_elem_list

%type <datetime> datetime
	start_clause end_clause
	last_update_clause

%type <metric_store> metric_store_clause

%destructor { free($$); } <str>
%destructor { sdb_object_deref(SDB_OBJ($$)); } <node>
%destructor { sdb_data_free_datum(&$$); } <data>

%%

statements:
	statements ';' statement
		{
			/* only accepted in default parse mode */
			if (parser_mode != SDB_PARSE_DEFAULT) {
				sdb_parser_yyerrorf(&yylloc, scanner,
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
				sdb_parser_yyerrorf(&yylloc, scanner,
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
				sdb_parser_yyerrorf(&yylloc, scanner,
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
			if (! (parser_mode & SDB_PARSE_ARITH)) {
				sdb_parser_yyerrorf(&yylloc, scanner,
						YY_("syntax error, unexpected expression, "
							"expecting %s"), MODE_TO_STRING(parser_mode));
				sdb_object_deref(SDB_OBJ($1));
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
	lookup_statement
	|
	store_statement
	|
	timeseries_statement
	|
	/* empty */
		{
			$$ = NULL;
		}
	;

/*
 * FETCH host <hostname> [FILTER <condition>];
 * FETCH <type> <hostname>.<name> [FILTER <condition>];
 *
 * Retrieve detailed information about a single object.
 */
fetch_statement:
	FETCH object_type STRING filter_clause
		{
			$$ = sdb_ast_fetch_create($2, NULL, -1, NULL, $3, 1, $4);
			CK_OOM($$);
		}
	|
	FETCH object_type STRING '.' STRING filter_clause
		{
			$$ = sdb_ast_fetch_create($2, $3, -1, NULL, $5, 1, $6);
			CK_OOM($$);
		}
	;

/*
 * LIST <type> [FILTER <condition>];
 *
 * Returns a list of all objects in the store.
 */
list_statement:
	LIST object_type_plural filter_clause
		{
			$$ = sdb_ast_list_create($2, $3);
			CK_OOM($$);
		}
	;

/*
 * LOOKUP <type> [MATCHING <condition>] [FILTER <condition>];
 *
 * Returns detailed information about objects matching a condition.
 */
lookup_statement:
	LOOKUP object_type_plural matching_clause filter_clause
		{
			$$ = sdb_ast_lookup_create($2, $3, $4);
			CK_OOM($$);
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
 * STORE <type> <name>|<host>.<name> [LAST UPDATE <datetime>];
 * STORE METRIC <host>.<name> STORE <type> <id> [LAST UPDATE <datetime>];
 * STORE <type> ATTRIBUTE <parent>.<key> <datum> [LAST UPDATE <datetime>];
 *
 * Store or update an object in the database.
 */
store_statement:
	STORE HOST_T STRING last_update_clause
		{
			$$ = sdb_ast_store_create(SDB_HOST, NULL, 0, NULL,
					$3, $4, NULL, NULL, SDB_DATA_NULL);
			CK_OOM($$);
		}
	|
	STORE SERVICE_T STRING '.' STRING last_update_clause
		{
			$$ = sdb_ast_store_create(SDB_SERVICE, $3, 0, NULL,
					$5, $6, NULL, NULL, SDB_DATA_NULL);
			CK_OOM($$);
		}
	|
	STORE METRIC_T STRING '.' STRING metric_store_clause last_update_clause
		{
			$$ = sdb_ast_store_create(SDB_METRIC, $3, 0, NULL,
					$5, $7, $6.type, $6.id, SDB_DATA_NULL);
			CK_OOM($$);
		}
	|
	STORE HOST_T ATTRIBUTE_T STRING '.' STRING data last_update_clause
		{
			$$ = sdb_ast_store_create(SDB_ATTRIBUTE, $4, 0, NULL,
					$6, $8, NULL, NULL, $7);
			CK_OOM($$);
		}
	|
	STORE SERVICE_T ATTRIBUTE_T STRING '.' STRING '.' STRING data last_update_clause
		{
			$$ = sdb_ast_store_create(SDB_ATTRIBUTE, $4, SDB_SERVICE, $6,
					$8, $10, NULL, NULL, $9);
			CK_OOM($$);
		}
	|
	STORE METRIC_T ATTRIBUTE_T STRING '.' STRING '.' STRING data last_update_clause
		{
			$$ = sdb_ast_store_create(SDB_ATTRIBUTE, $4, SDB_METRIC, $6,
					$8, $10, NULL, NULL, $9);
			CK_OOM($$);
		}
	;

last_update_clause:
	LAST UPDATE datetime { $$ = $3; }
	|
	/* empty */ { $$ = sdb_gettime(); }

metric_store_clause:
	STORE STRING STRING { $$.type = $2; $$.id = $3; }
	|
	/* empty */ { $$.type = $$.id = NULL; }

/*
 * TIMESERIES <host>.<metric> [START <datetime>] [END <datetime>];
 *
 * Returns a time-series for the specified host's metric.
 */
timeseries_statement:
	TIMESERIES STRING '.' STRING start_clause end_clause
		{
			$$ = sdb_ast_timeseries_create($2, $4, $5, $6);
			CK_OOM($$);
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
	'(' condition ')'
		{
			$$ = $2;
		}
	|
	condition AND condition
		{
			$$ = sdb_ast_op_create(SDB_AST_AND, $1, $3);
			CK_OOM($$);
		}
	|
	condition OR condition
		{
			$$ = sdb_ast_op_create(SDB_AST_OR, $1, $3);
			CK_OOM($$);
		}
	|
	NOT condition
		{
			$$ = sdb_ast_op_create(SDB_AST_NOT, NULL, $2);
			CK_OOM($$);
		}
	|
	comparison
		{
			$$ = $1;
		}
	;

comparison:
	expression cmp expression
		{
			$$ = sdb_ast_op_create($2, $1, $3);
			CK_OOM($$);
		}
	|
	ANY expression cmp expression
		{
			sdb_ast_node_t *n = sdb_ast_op_create($3, NULL, $4);
			CK_OOM(n);
			$$ = sdb_ast_iter_create(SDB_AST_ANY, $2, n);
			CK_OOM($$);
		}
	|
	ALL expression cmp expression
		{
			sdb_ast_node_t *n = sdb_ast_op_create($3, NULL, $4);
			CK_OOM(n);
			$$ = sdb_ast_iter_create(SDB_AST_ALL, $2, n);
			CK_OOM($$);
		}
	|
	expression IS NULL_T
		{
			$$ = sdb_ast_op_create(SDB_AST_ISNULL, NULL, $1);
			CK_OOM($$);
		}
	|
	expression IS NOT NULL_T
		{
			$$ = sdb_ast_op_create(SDB_AST_ISNULL, NULL, $1);
			CK_OOM($$);
			$$ = sdb_ast_op_create(SDB_AST_NOT, NULL, $$);
			CK_OOM($$);
		}
	|
	expression IS TRUE
		{
			$$ = sdb_ast_op_create(SDB_AST_ISTRUE, NULL, $1);
			CK_OOM($$);
		}
	|
	expression IS NOT TRUE
		{
			$$ = sdb_ast_op_create(SDB_AST_ISTRUE, NULL, $1);
			CK_OOM($$);
			$$ = sdb_ast_op_create(SDB_AST_NOT, NULL, $$);
			CK_OOM($$);
		}
	|
	expression IS FALSE
		{
			$$ = sdb_ast_op_create(SDB_AST_ISFALSE, NULL, $1);
			CK_OOM($$);
		}
	|
	expression IS NOT FALSE
		{
			$$ = sdb_ast_op_create(SDB_AST_ISFALSE, NULL, $1);
			CK_OOM($$);
			$$ = sdb_ast_op_create(SDB_AST_NOT, NULL, $$);
			CK_OOM($$);
		}
	|
	expression IN expression
		{
			$$ = sdb_ast_op_create(SDB_AST_IN, $1, $3);
			CK_OOM($$);
		}
	|
	expression NOT IN expression
		{
			$$ = sdb_ast_op_create(SDB_AST_IN, $1, $4);
			CK_OOM($$);
			$$ = sdb_ast_op_create(SDB_AST_NOT, NULL, $$);
			CK_OOM($$);
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
			$$ = sdb_ast_op_create(SDB_AST_ADD, $1, $3);
			CK_OOM($$);
		}
	|
	expression '-' expression
		{
			$$ = sdb_ast_op_create(SDB_AST_SUB, $1, $3);
			CK_OOM($$);
		}
	|
	expression '*' expression
		{
			$$ = sdb_ast_op_create(SDB_AST_MUL, $1, $3);
			CK_OOM($$);
		}
	|
	expression '/' expression
		{
			$$ = sdb_ast_op_create(SDB_AST_DIV, $1, $3);
			CK_OOM($$);
		}
	|
	expression '%' expression
		{
			$$ = sdb_ast_op_create(SDB_AST_MOD, $1, $3);
			CK_OOM($$);
		}
	|
	expression CONCAT expression
		{
			$$ = sdb_ast_op_create(SDB_AST_CONCAT, $1, $3);
			CK_OOM($$);
		}
	|
	object_expression
		{
			$$ = $1;
		}
	|
	data
		{
			$$ = sdb_ast_const_create($1);
			CK_OOM($$);
		}
	;

object_expression:
	object_type '.' object_expression
		{
			$$ = sdb_ast_typed_create($1, $3);
			CK_OOM($$);
		}
	|
	ATTRIBUTE_T '.' object_expression
		{
			$$ = sdb_ast_typed_create(SDB_ATTRIBUTE, $3);
			CK_OOM($$);
		}
	|
	field
		{
			$$ = sdb_ast_value_create($1, NULL);
			CK_OOM($$);
		}
	|
	ATTRIBUTE_T '[' STRING ']'
		{
			$$ = sdb_ast_value_create(SDB_ATTRIBUTE, $3);
			CK_OOM($$);
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
	|
	VALUE_T { $$ = SDB_FIELD_VALUE; }
	|
	TIMESERIES { $$ = SDB_FIELD_TIMESERIES; }
	;

cmp:
	CMP_EQUAL { $$ = SDB_AST_EQ; }
	|
	CMP_NEQUAL { $$ = SDB_AST_NE; }
	|
	CMP_REGEX { $$ = SDB_AST_REGEX; }
	|
	CMP_NREGEX { $$ = SDB_AST_NREGEX; }
	|
	CMP_LT { $$ = SDB_AST_LT; }
	|
	CMP_LE { $$ = SDB_AST_LE; }
	|
	CMP_GE { $$ = SDB_AST_GE; }
	|
	CMP_GT { $$ = SDB_AST_GT; }
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
			sdb_time_t unit = sdb_strpunit($2);
			if (! unit) {
				sdb_parser_yyerrorf(&yylloc, scanner,
						YY_("syntax error, invalid time unit %s"), $2);
				free($2); $2 = NULL;
				YYABORT;
			}
			free($2); $2 = NULL;

			if ($1.data.integer < 0) {
				sdb_parser_yyerror(&yylloc, scanner,
						YY_("syntax error, negative intervals not supported"));
				YYABORT;
			}

			$$.type = SDB_TYPE_DATETIME;
			$$.data.datetime = (sdb_time_t)$1.data.integer * unit;
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
			size_t elem_size = sdb_data_sizeof($3.type);

			if (($3.type & SDB_TYPE_ARRAY) || (($1.type & 0xff) != $3.type)) {
				sdb_parser_yyerrorf(&yylloc, scanner, YY_("syntax error, "
						"cannot use element of type %s in array of type %s"),
						SDB_TYPE_TO_STRING($3.type),
						SDB_TYPE_TO_STRING($1.type));
				sdb_data_free_datum(&$1);
				sdb_data_free_datum(&$3);
				YYABORT;
			}

			$$ = $1;
			$$.data.array.values = realloc($$.data.array.values,
					($$.data.array.length + 1) * elem_size);
			CK_OOM($$.data.array.values);

			memcpy((char *)$$.data.array.values + $$.data.array.length * elem_size,
					&$3.data, elem_size);
			++$$.data.array.length;
		}
	|
	data
		{
			size_t elem_size = sdb_data_sizeof($1.type);

			if ($1.type & SDB_TYPE_ARRAY) {
				sdb_parser_yyerrorf(&yylloc, scanner, YY_("syntax error, "
						"cannot construct array of type %s"),
						SDB_TYPE_TO_STRING($1.type));
				sdb_data_free_datum(&$1);
				YYABORT;
			}

			$$ = $1;
			$$.type |= SDB_TYPE_ARRAY;
			$$.data.array.values = malloc(elem_size);
			CK_OOM($$.data.array.values);

			memcpy($$.data.array.values, &$1.data, elem_size);
			$$.data.array.length = 1;
		}
	;

%%

void
sdb_parser_yyerror(YYLTYPE *lval, sdb_parser_yyscan_t scanner, const char *msg)
{
	sdb_log(SDB_LOG_ERR, "parser: parse error: %s", msg);
	sdb_strbuf_sprintf(errbuf, "%s", msg);
} /* sdb_parser_yyerror */

void
sdb_parser_yyerrorf(YYLTYPE *lval, sdb_parser_yyscan_t scanner, const char *fmt, ...)
{
	va_list ap, aq;
	va_start(ap, fmt);
	va_copy(aq, ap);
	sdb_vlog(SDB_LOG_ERR, fmt, ap);
	sdb_strbuf_vsprintf(errbuf, fmt, aq);
	va_end(ap);
} /* sdb_parser_yyerrorf */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

