/*
 * SysDB - src/tools/sysdb/json.c
 * Copyright (C) 2016 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include "sysdb.h"

#include "core/store.h"
#include "frontend/proto.h"
#include "utils/error.h"
#include "utils/strbuf.h"
#include "tools/sysdb/json.h"

#ifdef HAVE_LIBYAJL
#	include <yajl/yajl_parse.h>
#endif

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_LIBYAJL

/*
 * formatter
 */

typedef struct {
	/* The context describes the state of the formatter along with the
	 * respective parent contexts. */
	int context[8];
	ssize_t array_indices[8];
	size_t current;
	int next_context;

	bool have_output;
} formatter_t;
#define F(obj) ((formatter_t *)(obj))
#define F_INIT { { 0 }, { -1, -1, -1, -1, -1, -1, -1, -1 }, 0, 0, false }

static int
push(formatter_t *f, int type)
{
	f->current++;
	if (f->current >= SDB_STATIC_ARRAY_LEN(f->context)) {
		sdb_log(SDB_LOG_ERR, "Nesting level too deep");
		return false;
	}
	f->context[f->current] = type;
	return true;
} /* push */

static void
pop(formatter_t *f)
{
	if (f->current == 0)
		return;

	f->next_context = f->context[f->current];
	f->current--;
} /* pop */

static void
print(formatter_t *f, const char *s, ssize_t l)
{
	if (l >= 0) {
		/* 's' may point into a larger buffer and not be null-terminated */
		char buf[l + 1];
		strncpy(buf, s, l);
		buf[l] = '\0';
		printf("%s", buf);
	}
	else
		printf("%s", s);

	f->have_output = true;
} /* print */

static void
indent(formatter_t *f)
{
	size_t i;
	for (i = 0; i < f->current - 1; i++)
		print(f, "\t", -1);
}

static void
format(formatter_t *f, const char *s, ssize_t l)
{
	if (f->array_indices[f->current] >= 0) {
		if (f->array_indices[f->current] != 0)
			print(f, ", ", -1);
		f->array_indices[f->current]++;
	}

	print(f, s, l);
	return;
} /* format */

static int
format_key(formatter_t *f, const char *k, ssize_t l)
{
	int type = 0;

	if (! strncasecmp("services", k, l))
		type = SDB_SERVICE;
	else if (! strncasecmp("metrics", k, l))
		type = SDB_METRIC;
	else if (! strncasecmp("attributes", k, l))
		type = SDB_ATTRIBUTE;

	if (f->have_output)
		print(f, "\n", -1);
	indent(f);
	print(f, k, l);
	print(f, ": ", -1);

	f->next_context = type;
	return true;
} /* format_key */

/*
 * YAJL callbacks
 */

static int
fmt_null(void *ctx) {
	formatter_t *f = F(ctx);
	format(f, "NULL", -1);
	return true;
}

static int
fmt_boolean(void *ctx, int v) {
	formatter_t *f = F(ctx);
	if (v)
		format(f, "true", -1);
	else
		format(f, "false", -1);
	return true;
}

static int
fmt_number(void *ctx, const char *v, size_t l)
{
	formatter_t *f = F(ctx);
	format(f, v, l);
	return true;
}

static int
fmt_string(void *ctx, const unsigned char *v, size_t l)
{
	formatter_t *f = F(ctx);
	format(f, (const char *)v, l);
	return true;
}

static int
fmt_start_map(void *ctx) {
	formatter_t *f = F(ctx);
	const char *name;

	if (!push(f, f->next_context))
		return false;

	if (f->have_output)
		print(f, "\n", -1);

	name = SDB_STORE_TYPE_TO_NAME(f->context[f->current]);
	if (strcmp(name, "unknown")) {
		if (f->have_output)
			print(f, "\n", -1);
		indent(f);
		format(f, name, -1);
	}
	return true;
}

static int
fmt_map_key(void *ctx, const unsigned char *v, size_t l)
{
	formatter_t *f = F(ctx);
	return format_key(f, (const char *)v, l);
}

static int
fmt_end_map(void *ctx) {
	formatter_t *f = F(ctx);
	pop(f);
	return true;
}

static int
fmt_start_array(void *ctx) {
	formatter_t *f = F(ctx);
	f->array_indices[f->current] = 0;
	return true;
}

static int
fmt_end_array(void *ctx) {
	formatter_t *f = F(ctx);
	f->array_indices[f->current] = -1;
	return true;
}

static yajl_callbacks fmts = {
	fmt_null,
	fmt_boolean,
	NULL, /* fmt_integer; */
	NULL, /* fmt_double; both default to fmt_number */
	fmt_number,
	fmt_string,
	fmt_start_map,
	fmt_map_key,
	fmt_end_map,
	fmt_start_array,
	fmt_end_array,
};

#endif /* HAVE_LIBYAJL */

/*
 * public API
 */

int
sdb_json_print(sdb_input_t *input, int type, sdb_strbuf_t *buf)
{
#ifdef HAVE_LIBYAJL
	const unsigned char *json;
	size_t json_len;

	yajl_handle h;
	yajl_status status;
	formatter_t f = F_INIT;

	int ret = 0;

	if (!input->interactive) {
		/* no formatting */
		printf("%s\n", sdb_strbuf_string(buf));
		return 0;
	}

	/* Store lookups always return hosts at the top-level. */
	f.context[0] = SDB_HOST;
	switch (type) {
	case SDB_CONNECTION_LIST:
	case SDB_CONNECTION_LOOKUP:
		/* Array types */
		f.array_indices[0] = 0;
		break;
	case SDB_CONNECTION_TIMESERIES:
		f.context[0] = SDB_TIMESERIES;
		break;
	}
	f.next_context = f.context[0];

	h = yajl_alloc(&fmts, /* alloc_funcs */ NULL, &f);
	if (! h)
		return -1;

	json = (const unsigned char *)sdb_strbuf_string(buf);
	json_len = sdb_strbuf_len(buf);
	status = yajl_parse(h, json, json_len);
	if (status == yajl_status_ok)
		status = yajl_complete_parse(h);

	if (status != yajl_status_ok) {
		unsigned char *err = yajl_get_error(h, 1, json, json_len);
		sdb_log(SDB_LOG_ERR, "%s", err);
		yajl_free_error(h, err);
		ret = -1;
	}

	yajl_free(h);
	return ret;
#else /* HAVE_LIBYAJL */
	(void)input;
	(void)type;
	printf("%s\n", sdb_strbuf_string(buf));
	return 0;
#endif /* HAVE_LIBYAJL */
} /* sdb_json_print */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

