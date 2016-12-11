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

#include "utils/error.h"
#include "utils/strbuf.h"
#include "tools/sysdb/json.h"

#ifdef HAVE_LIBYAJL
#	include <yajl/yajl_parse.h>
#	include <yajl/yajl_gen.h>
#endif

#include <unistd.h>
#include <stdio.h>

#ifdef HAVE_LIBYAJL

/*
 * YAJL callbacks
 */

#define GEN(obj) ((yajl_gen)(obj))
#define OK(cb) ((cb) == yajl_gen_status_ok)

static int
gen_null(void *ctx) { return OK(yajl_gen_null(GEN(ctx))); }

static int
gen_boolean(void *ctx, int v) { return OK(yajl_gen_bool(GEN(ctx), v)); }

static int
gen_number(void *ctx, const char *v, size_t l)
{
	return OK(yajl_gen_number(GEN(ctx), v, l));
}

static int
gen_string(void *ctx, const unsigned char *v, size_t l)
{
	return OK(yajl_gen_string(GEN(ctx), v, l));
}

static int
gen_start_map(void *ctx) { return OK(yajl_gen_map_open(GEN(ctx))); }

static int
gen_end_map(void *ctx) { return OK(yajl_gen_map_close(GEN(ctx))); }

static int
gen_start_array(void *ctx) { return OK(yajl_gen_array_open(GEN(ctx))); }

static int
gen_end_array(void *ctx) { return OK(yajl_gen_array_close(GEN(ctx))); }

static yajl_callbacks reformatters = {
	gen_null,
	gen_boolean,
	NULL, /* gen_integer; */
	NULL, /* gen_doube; both default to gen_number */
	gen_number,
	gen_string,
	gen_start_map,
	gen_string,
	gen_end_map,
	gen_start_array,
	gen_end_array,
};

static void
printer(void __attribute__((unused)) *ctx, const char *str, size_t len)
{
	write(1, str, len);
} /* printer */

#endif /* HAVE_LIBYAJL */

/*
 * public API
 */

int
sdb_json_print(sdb_input_t *input, sdb_strbuf_t *buf)
{
#ifdef HAVE_LIBYAJL
	const unsigned char *json;
	size_t json_len;

	yajl_handle h;
	yajl_gen gen;
	yajl_status status;

	int ret = 0;

	if (!input->interactive) {
		/* no formatting */
		printf("%s\n", sdb_strbuf_string(buf));
		return 0;
	}

	gen = yajl_gen_alloc(/* alloc_funcs */ NULL);
	if (! gen)
		return -1;

	yajl_gen_config(gen, yajl_gen_beautify, 1);
	yajl_gen_config(gen, yajl_gen_validate_utf8, 1);
	yajl_gen_config(gen, yajl_gen_print_callback, printer, NULL);

	h = yajl_alloc(&reformatters, /* alloc_funcs */ NULL, (void *)gen);
	if (! h) {
		yajl_gen_free(gen);
		return -1;
	}

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

	yajl_gen_free(gen);
	yajl_free(h);
	return ret;
#else /* HAVE_LIBYAJL */
	(void)input;
	printf("%s\n", sdb_strbuf_string(buf));
	return 0;
#endif /* HAVE_LIBYAJL */
} /* sdb_json_print */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

