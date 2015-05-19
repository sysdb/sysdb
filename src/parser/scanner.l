/*
 * SysDB - src/parser/scanner.l
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

%{

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "core/data.h"

#include "parser/parser.h"
#include "parser/grammar.h"

#include "utils/error.h"

#include <assert.h>
#include <errno.h>

#include <string.h>
#include <stdlib.h>

#include <time.h>

#define YY_EXTRA_TYPE sdb_parser_yyextra_t *

static struct {
	const char *name;
	int id;
} reserved_words[] = {
	{ "ALL",         ALL },
	{ "AND",         AND },
	{ "ANY",         ANY },
	{ "END",         END },
	{ "FALSE",       FALSE },
	{ "FETCH",       FETCH },
	{ "FILTER",      FILTER },
	{ "IN",          IN },
	{ "IS",          IS },
	{ "LAST",        LAST },
	{ "LIST",        LIST },
	{ "LOOKUP",      LOOKUP },
	{ "MATCHING",    MATCHING },
	{ "NOT",         NOT },
	{ "NULL",        NULL_T },
	{ "OR",          OR },
	{ "START",       START },
	{ "STORE",       STORE },
	{ "TIMESERIES",  TIMESERIES },
	{ "TRUE",        TRUE },
	{ "UPDATE",      UPDATE },

	/* object types */
	{ "host",        HOST_T },
	{ "hosts",       HOSTS_T },
	{ "service",     SERVICE_T },
	{ "services",    SERVICES_T },
	{ "metric",      METRIC_T },
	{ "metrics",     METRICS_T },
	{ "attribute",   ATTRIBUTE_T },
	{ "attributes",  ATTRIBUTES_T },
	/* queryable fields */
	{ "name",        NAME_T },
	{ "last_update", LAST_UPDATE_T },
	{ "age",         AGE_T },
	{ "interval",    INTERVAL_T },
	{ "backend",     BACKEND_T },
	{ "value",       VALUE_T },
};

void
sdb_parser_yyerror(YYLTYPE *lval, sdb_parser_yyscan_t scanner, const char *msg);

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
%option prefix="sdb_parser_yy" outfile="lex.yy.c"

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
string		('([^']|'')*')

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

/*
 * Time constants.
 */
date		([0-9]{4}-[0-9]{2}-[0-9]{2})
time		([0-9]{1,2}:[0-9]{1,2}(:[0-9]{1,2}(\.[0-9]{1,9})?)?)

%%

{whitespace} |
{simple_comment}	{ /* ignore */ }

{csc_start}			{ BEGIN(CSC); }
<CSC>{csc_inside}	{ /* ignore */ }
<CSC>{csc_end}		{ BEGIN(INITIAL); }
<CSC><<EOF>> {
		sdb_parser_yyerror(yylloc, yyscanner, "unterminated C-style comment");
		return SCANNER_ERROR;
	}

{identifier} {
		size_t i;
		for (i = 0; i < SDB_STATIC_ARRAY_LEN(reserved_words); ++i)
			if (! strcasecmp(reserved_words[i].name, yytext))
				return reserved_words[i].id;

		yylval->str = strdup(yytext);
		return IDENTIFIER;
	}
{string} {
		char *quot;
		size_t len;

		/* remove the leading and trailing quote */
		yytext[yyleng - 1] = '\0';
		yylval->str = strdup(yytext + 1);

		quot = yylval->str;
		len = yyleng - 2;
		while ((quot = strstr(quot, "''")) != NULL) {
			memmove(quot, quot + 1, len - (quot - yylval->str) - 1);
			yylval->str[len - 1] = '\0';
			--len;
			++quot;
		}
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

{date} {
		struct tm tm;
		memset(&tm, 0, sizeof(tm));
		if (! strptime(yytext, "%Y-%m-%d", &tm)) {
			char errmsg[1024];
			snprintf(errmsg, sizeof(errmsg),
				"Failed to parse '%s' as date", yytext);
			sdb_parser_yyerror(yylloc, yyscanner, errmsg);
			return SCANNER_ERROR;
		}
		yylval->datetime = SECS_TO_SDB_TIME(mktime(&tm));
		return DATE;
	}
{time} {
		struct tm tm;
		char time[9], ns[10];
		char *tmp;
		int t;

		memset(&tm, 0, sizeof(tm));
		memset(time, '\0', sizeof(time));
		memset(ns, '0', sizeof(ns));
		ns[sizeof(ns) - 1] = '\0';

		tmp = strchr(yytext, '.');
		if (tmp) {
			size_t i;
			*tmp = '\0';
			++tmp;
			strncpy(ns, tmp, sizeof(ns));
			for (i = strlen(ns); i < 9; ++i)
				ns[i] = '0';
		}
		strncpy(time, yytext, sizeof(time));
		if (tmp) {
			/* reset for better error messages */
			--tmp;
			*tmp = '.';
		}

		tmp = strchr(time, ':');
		assert(tmp);
		tmp = strchr(tmp + 1, ':');
		if (! tmp)
			strncat(time, ":00", sizeof(time) - strlen(time) - 1);

		if (! strptime(time, "%H:%M:%S", &tm)) {
			char errmsg[1024];
			snprintf(errmsg, sizeof(errmsg),
				"Failed to parse '%s' as time", yytext);
			sdb_parser_yyerror(yylloc, yyscanner, errmsg);
			return SCANNER_ERROR;
		}

		t = tm.tm_sec + 60 * tm.tm_min + 3600 * tm.tm_hour;
		yylval->datetime = SECS_TO_SDB_TIME(t);
		yylval->datetime += (sdb_time_t)strtoll(ns, NULL, 10);
		return TIME;
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

sdb_parser_yyscan_t
sdb_parser_scanner_init(const char *str, int len, sdb_parser_yyextra_t *yyext)
{
	yyscan_t scanner;

	if (! str)
		return NULL;

	if (sdb_parser_yylex_init(&scanner)) {
		char errbuf[1024];
		sdb_strbuf_sprintf(yyext->errbuf, "yylex_init_failed: %s",
			sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return NULL;
	}

	sdb_parser_yyset_extra(yyext, scanner);

	if (len < 0)
		len = strlen(str);

	/* the newly allocated buffer state (YY_BUFFER_STATE) is stored inside the
	 * scanner and, thus, will be freed by yylex_destroy */
	sdb_parser_yy_scan_bytes(str, len, scanner);
	return scanner;
} /* sdb_parser_scanner_init */

void
sdb_parser_scanner_destroy(sdb_parser_yyscan_t scanner)
{
	sdb_parser_yylex_destroy(scanner);
} /* sdb_parser_scanner_destroy */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

