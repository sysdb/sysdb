/*
 * SysDB - src/include/parser/parser.h
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

#ifndef SDB_PARSER_PARSER_H
#define SDB_PARSER_PARSER_H 1

#include "core/store.h"
#include "parser/ast.h"
#include "utils/llist.h"
#include "utils/strbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* parser modes */
enum {
	/* parser accepts any command statement */
	SDB_PARSE_DEFAULT = 0,

	/* parser accepts any conditional statement */
	SDB_PARSE_COND    = 1 << 1,

	/* parser accepts any arithmetic expression */
	SDB_PARSE_ARITH   = 1 << 2,
};

/*
 * sdb_parser_parse:
 * Parse the specified query of the specified length. If len is a negative
 * value, use the entire string.
 *
 * Returns:
 *  - a list of AST nodes (sdb_ast_node_t) on success; each node describes one
 *    statement of the query
 *  - NULL else; an error message will be written to the specified error
 *    buffer
 */
sdb_llist_t *
sdb_parser_parse(const char *query, int len, sdb_strbuf_t *errbuf);

/*
 * sdb_parser_parse_conditional:
 * Parse a single conditional expression which can be evaluated in the
 * specified context (any valid store object type). This function is similar
 * to sdb_parse_parse but will only accept a single conditional expression.
 * The return value is guaranteed to satisfy SDB_AST_IS_LOGICAL().
 */
sdb_ast_node_t *
sdb_parser_parse_conditional(int context,
		const char *cond, int len, sdb_strbuf_t *errbuf);

/*
 * sdb_parser_parse_arith:
 * Parse a single arithmetic expression which can be evaluated in the
 * specified context (any valid store object type). This function is similar
 * to sdb_parse_parse but will only accept a single arithmetic expression. The
 * return value is guaranteed to satisfy SDB_AST_IS_ARITHMETIC().
 */
sdb_ast_node_t *
sdb_parser_parse_arith(int context,
		const char *expr, int len, sdb_strbuf_t *errbuf);

/*
 * sdb_parser_analyze:
 * Semantical analysis of a parse-tree.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else; an error message will be written to the provided
 *    error buffer
 */
int
sdb_parser_analyze(sdb_ast_node_t *node, sdb_strbuf_t *errbuf);

/*
 * sdb_parser_analyze_conditional:
 * Semantical analysis of a conditional node in the specified context (any
 * valid store object type).
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else; an error message will be written to the provided
 *    error buffer
 */
int
sdb_parser_analyze_conditional(int context,
		sdb_ast_node_t *node, sdb_strbuf_t *errbuf);

/*
 * sdb_parser_analyze_arith:
 * Semantical analysis of an arithmetic node in the specified context (any
 * valid store object type).
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else; an error message will be written to the provided
 *    error buffer
 */
int
sdb_parser_analyze_arith(int context,
		sdb_ast_node_t *node, sdb_strbuf_t *errbuf);

/*
 * Low-level interface.
 */

/* scanner/parser's YY_EXTRA data */
typedef struct {
	/* list of sdb_ast_node_t objects */
	sdb_llist_t *parsetree;

	/* parser mode */
	int mode;

	/* buffer for parser error messages */
	sdb_strbuf_t *errbuf;
} sdb_parser_yyextra_t;

/* see yyscan_t */
typedef void *sdb_parser_yyscan_t;

/*
 * sdb_parser_scanner_init:
 * Allocate and initialize a scanner object. It will operate on the specified
 * string of the specified length. If len is less than zero, use the entire
 * string. The scanner/parser extra data stores shared state information
 * between the scanner and the parser.
 */
sdb_parser_yyscan_t
sdb_parser_scanner_init(const char *str, int len, sdb_parser_yyextra_t *yyext);

/*
 * sdb_parser_scanner_destroy:
 * Destroy a scanner object freeing all of its memory.
 */
void
sdb_parser_scanner_destroy(sdb_parser_yyscan_t scanner);

/*
 * sdb_parser_yyparse:
 * Invoke the low-level parser using the specified scanner. The result will be
 * returned through the scanner/parser's extra data.
 *
 * Returns:
 *  - 0 on success
 *  - a non-zero value else; the error buffer stored in the scanner/parser's
 *    extra data provides an error message in this case
 */
int
sdb_parser_yyparse(sdb_parser_yyscan_t scanner);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_PARSER_PARSER_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

