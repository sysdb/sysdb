/*
 * SysDB - src/utils/strbuf.c
 * Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include "utils/strbuf.h"

#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/*
 * private data structures
 */

struct sdb_strbuf {
	char  *string;
	size_t size;
	size_t pos;
};

/*
 * private helper functions
 */

static int
strbuf_resize(sdb_strbuf_t *strbuf)
{
	char *tmp;

	assert(strbuf->size);

	tmp = realloc(strbuf->string, 2 * strbuf->size);
	if (! tmp)
		return -1;

	strbuf->string = tmp;
	strbuf->size *= 2;
	return 0;
} /* strbuf_resize */

/*
 * public API
 */

sdb_strbuf_t *
sdb_strbuf_create(size_t size)
{
	sdb_strbuf_t *strbuf;

	if (! size)
		return NULL;

	strbuf = calloc(1, sizeof(*strbuf));
	if (! strbuf)
		return NULL;

	strbuf->string = malloc(size);
	if (! strbuf->string) {
		free(strbuf);
		return NULL;
	}

	strbuf->string[0] = '\0';
	strbuf->size = size;
	strbuf->pos  = 0;

	return strbuf;
} /* sdb_strbuf_create */

void
sdb_strbuf_destroy(sdb_strbuf_t *strbuf)
{
	if (! strbuf)
		return;

	free(strbuf->string);
	free(strbuf);
} /* sdb_strbuf_destroy */

ssize_t
sdb_strbuf_vappend(sdb_strbuf_t *strbuf, const char *fmt, va_list ap)
{
	int status;

	if ((! strbuf) || (! fmt))
		return -1;

	assert(strbuf->string[strbuf->pos] == '\0');

	if (strbuf->pos >= strbuf->size)
		if (strbuf_resize(strbuf))
			return -1;

	status = vsnprintf(strbuf->string + strbuf->pos,
			strbuf->size - strbuf->pos, fmt, ap);

	if (status < 0)
		return status;

	if ((size_t)status >= strbuf->size - strbuf->pos) {
		strbuf_resize(strbuf);

		/* reset string and try again */
		strbuf->string[strbuf->pos] = '\0';
		return sdb_strbuf_vappend(strbuf, fmt, ap);
	}

	strbuf->pos += (size_t)status;
	return (ssize_t)status;
} /* sdb_strbuf_vappend */

ssize_t
sdb_strbuf_append(sdb_strbuf_t *strbuf, const char *fmt, ...)
{
	va_list ap;
	ssize_t status;

	va_start(ap, fmt);
	status = sdb_strbuf_vappend(strbuf, fmt, ap);
	va_end(ap);

	return status;
} /* sdb_strbuf_append */

ssize_t
sdb_strbuf_vsprintf(sdb_strbuf_t *strbuf, const char *fmt, va_list ap)
{
	if (! strbuf)
		return -1;

	strbuf->string[0] = '\0';
	strbuf->pos = 0;

	return sdb_strbuf_vappend(strbuf, fmt, ap);
} /* sdb_strbuf_vsprintf */

ssize_t
sdb_strbuf_sprintf(sdb_strbuf_t *strbuf, const char *fmt, ...)
{
	va_list ap;
	ssize_t status;

	va_start(ap, fmt);
	status = sdb_strbuf_vsprintf(strbuf, fmt, ap);
	va_end(ap);

	return status;
} /* sdb_strbuf_sprintf */

const char *
sdb_strbuf_string(sdb_strbuf_t *strbuf)
{
	if (! strbuf)
		return NULL;
	return strbuf->string;
} /* sdb_strbuf_string */

size_t
sdb_strbuf_len(sdb_strbuf_t *strbuf)
{
	if (! strbuf)
		return 0;
	return strbuf->pos;
} /* sdb_strbuf_string */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

