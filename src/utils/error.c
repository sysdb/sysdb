/*
 * SysDB - src/utils/error.c
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

#include "utils/error.h"

#include <pthread.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <stdlib.h>

/*
 * private data types
 */

typedef struct {
	int   prio;
	char  msg[SDB_MAX_ERROR];
	_Bool logged;
} sdb_error_ctx_t;
#define SDB_ERROR_INIT { -1, "", 1 }

/*
 * private variables
 */

static sdb_error_ctx_t default_error_ctx = SDB_ERROR_INIT;

static pthread_key_t error_ctx_key;
static _Bool         error_ctx_key_initialized = 0;

/*
 * private helper functions
 */

static void
sdb_error_ctx_destructor(void *ctx)
{
	if (! ctx)
		return;
	free(ctx);
} /* sdb_error_ctx_destructor */

static void
sdb_error_ctx_init(void)
{
	if (error_ctx_key_initialized)
		return;

	pthread_key_create(&error_ctx_key, sdb_error_ctx_destructor);
	error_ctx_key_initialized = 1;
} /* sdb_error_init */

static sdb_error_ctx_t *
sdb_error_ctx_create(void)
{
	sdb_error_ctx_t *ctx;

	ctx = malloc(sizeof(*ctx));
	if (! ctx)
		return NULL;

	*ctx = default_error_ctx;

	if (! error_ctx_key_initialized)
		sdb_error_ctx_init();
	pthread_setspecific(error_ctx_key, ctx);
	return ctx;
} /* sdb_error_ctx_create */

static sdb_error_ctx_t *
sdb_error_get_ctx(void)
{
	sdb_error_ctx_t *ctx;

	if (! error_ctx_key_initialized)
		sdb_error_ctx_init();
	ctx = pthread_getspecific(error_ctx_key);

	if (! ctx)
		ctx = sdb_error_ctx_create();
	if (! ctx)
		return NULL;
	return ctx;
} /* sdb_error_get_ctx */

static int
sdb_error_clear(void)
{
	sdb_error_ctx_t *ctx;

	ctx = sdb_error_get_ctx();
	if (! ctx)
		return -1;

	ctx->prio = -1;
	ctx->msg[0] = '\0';
	ctx->logged = 1;
	return 0;
} /* sdb_error_clear */

static int
sdb_error_vappend(const char *fmt, va_list ap)
{
	sdb_error_ctx_t *ctx;
	size_t len;

	ctx = sdb_error_get_ctx();
	if (! ctx)
		return -1;

	len = strlen(ctx->msg);
	if (len >= sizeof(ctx->msg))
		return 0; /* nothing written */

	ctx->logged = 0;
	return vsnprintf(ctx->msg + len, sizeof(ctx->msg) - len, fmt, ap);
} /* sdb_error_vappend */

static int
sdb_do_log(int prio)
{
	sdb_error_ctx_t *ctx;
	int ret;

	ctx = sdb_error_get_ctx();
	if (! ctx)
		return -1;

	if (prio >= 0)
		ctx->prio = prio;

	if (ctx->logged)
		return 0;

	ret = fprintf(stderr, "[%s] %s\n",
			SDB_LOG_PRIO_TO_STRING(prio), ctx->msg);
	ctx->logged = 1;
	return ret;
} /* sdb_do_log */

/*
 * public API
 */

int
sdb_log(int prio, const char *fmt, ...)
{
	va_list ap;
	int ret;

	if (sdb_error_clear())
		return -1;

	va_start(ap, fmt);
	ret = sdb_error_vappend(fmt, ap);
	va_end(ap);

	sdb_do_log(prio);
	return ret;
} /* sdb_log */

int
sdb_error_set(const char *fmt, ...)
{
	va_list ap;
	int ret;

	if (sdb_error_clear())
		return -1;

	va_start(ap, fmt);
	ret = sdb_error_vappend(fmt, ap);
	va_end(ap);

	return ret;
} /* sdb_error_set */

int
sdb_error_append(const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = sdb_error_vappend(fmt, ap);
	va_end(ap);

	return ret;
} /* sdb_error_append */

int
sdb_error_log(int prio)
{
	return sdb_do_log(prio);
} /* sdb_error_log */

const char *
sdb_error_get(void)
{
	sdb_error_ctx_t *ctx;

	ctx = sdb_error_get_ctx();
	if (! ctx)
		return "success";
	return ctx->msg;
} /* sdb_error_get */

int
sdb_error_get_prio(void)
{
	sdb_error_ctx_t *ctx;

	ctx = sdb_error_get_ctx();
	if (! ctx)
		return -1;
	return ctx->prio;
} /* sdb_error_get_prio */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

