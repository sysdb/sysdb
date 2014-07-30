/*
 * SysDB - src/frontend/scanner.l
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

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "core/data.h"
#include "frontend/connection.h"
#include "frontend/parser.h"
#include "frontend/grammar.h"
#include "utils/error.h"

#include <errno.h>

#include <string.h>
#include <stdlib.h>

#define YY_EXTRA_TYPE sdb_fe_yyextra_t *

void
sdb_fe_yyerror(YYLTYPE *lval, sdb_fe_yyscan_t scanner, const char *msg);

%}

%option never-interactive
%option reentrant
%option bison-bridge
%option bison-locations
%option 8bit
%option yylineno
%option nodefault
%option noinput
%option nounput
%option noyywrap
%option verbose
%option warn
%option prefix="sdb_fe_yy" outfile="lex.yy.c"

%x CSC

whitespace		([ \t\n\r\f]+)
simple_comment	("--"[^\n\r]*)

/*
 * C style comments
 */
csc_start	\/\*
csc_inside	([^*/]+|[^*]\/|\*[^/])
csc_end		\*\/

/*
 * Strings and identifiers.
 */
identifier	([A-Za-z_][A-Za-z_0-9$]*)
/* TODO: fully support SQL strings */
string		('[^']*')

/*
 * Numeric constants.
 */
dec			([\+\-]?[0-9]+)
exp			([\+\-]?[0-9]+[Ee]\+?[0-9]+)
integer		({dec}|{exp})
float1		([\+\-]?[0-9]+\.[0-9]*([Ee][\+\-]?[0-9]+)?)
float2		([\+\-]?[0-9]*\.[0-9]+([Ee][\+\-]?[0-9]+)?)
float3		([\+\-]?[0-9]+[Ee]\-[0-9]+)
float4		([\+\-]?[Ii][Nn][Ff]([Ii][Nn][Ii][Tt][Yy])?)
float5		([Nn][Aa][Nn])
float		({float1}|{float2}|{float3}|{float4}|{float5})

%%

{whitespace} |
{simple_comment}	{ /* ignore */ }

{csc_start}			{ BEGIN(CSC); }
<CSC>{csc_inside}	{ /* ignore */ }
<CSC>{csc_end}		{ BEGIN(INITIAL); }
<CSC><<EOF>> {
		sdb_fe_yyerror(yylloc, yyscanner, "unterminated C-style comment");
		return SCANNER_ERROR;
	}

{identifier} {
		/* XXX: simplify handling of reserved words */
		if (! strcasecmp(yytext, "AND"))
			return AND;
		else if (! strcasecmp(yytext, "FETCH"))
			return FETCH;
		else if (! strcasecmp(yytext, "IS"))
			return IS;
		else if (! strcasecmp(yytext, "LIST"))
			return LIST;
		else if (! strcasecmp(yytext, "LOOKUP"))
			return LOOKUP;
		else if (! strcasecmp(yytext, "MATCHING"))
			return MATCHING;
		else if (! strcasecmp(yytext, "NOT"))
			return NOT;
		else if (! strcasecmp(yytext, "NULL"))
			return NULL_T;
		else if (! strcasecmp(yytext, "OR"))
			return OR;

		yylval->str = strdup(yytext);
		return IDENTIFIER;
	}
{string} {
		yytext[yyleng - 1] = '\0';
		yylval->str = strdup(yytext + 1);
		return STRING;
	}
{integer} {
		yylval->data.data.integer = (int64_t)strtoll(yytext, NULL, 10);
		yylval->data.type = SDB_TYPE_INTEGER;
		return INTEGER;
	}
{float} {
		yylval->data.data.decimal = strtod(yytext, NULL);
		yylval->data.type = SDB_TYPE_DECIMAL;
		return FLOAT;
	}

=	{ return CMP_EQUAL; }
!=	{ return CMP_NEQUAL; }
=~	{ return CMP_REGEX; }
!~	{ return CMP_NREGEX; }
\<	{ return CMP_LT; }
\<=	{ return CMP_LE; }
\>=	{ return CMP_GE; }
\>	{ return CMP_GT; }
\|\|	{ return CONCAT; }

.	{ /* XXX: */ return yytext[0]; }

%%

sdb_fe_yyscan_t
sdb_fe_scanner_init(const char *str, int len, sdb_fe_yyextra_t *yyext)
{
	yyscan_t scanner;

	if (! str)
		return NULL;

	if (sdb_fe_yylex_init(&scanner)) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "frontend: yylex_init failed: %s",
			sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return NULL;
	}

	sdb_fe_yyset_extra(yyext, scanner);

	if (len < 0)
		len = strlen(str);

	/* the newly allocated buffer state (YY_BUFFER_STATE) is stored inside the
	 * scanner and, thus, will be freed by yylex_destroy */
	sdb_fe_yy_scan_bytes(str, len, scanner);
	return scanner;
} /* sdb_fe_scanner_init */

void
sdb_fe_scanner_destroy(sdb_fe_yyscan_t scanner)
{
	sdb_fe_yylex_destroy(scanner);
} /* sdb_fe_scanner_destroy */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

