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
#include <string.h>

#include <unistd.h>

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
strbuf_resize(sdb_strbuf_t *strbuf, size_t new_size)
{
	char *tmp;

	if (new_size <= strbuf->size)
		return 0;

	tmp = realloc(strbuf->string, new_size);
	if (! tmp)
		return -1;

	strbuf->string = tmp;
	strbuf->size = new_size;
	return 0;
} /* strbuf_resize */

/*
 * public API
 */

sdb_strbuf_t *
sdb_strbuf_create(size_t size)
{
	sdb_strbuf_t *strbuf;

	strbuf = calloc(1, sizeof(*strbuf));
	if (! strbuf)
		return NULL;

	strbuf->string = NULL;
	if (size) {
		strbuf->string = malloc(size);
		if (! strbuf->string) {
			free(strbuf);
			return NULL;
		}

		strbuf->string[0] = '\0';
	}

	strbuf->size = size;
	strbuf->pos  = 0;

	return strbuf;
} /* sdb_strbuf_create */

void
sdb_strbuf_destroy(sdb_strbuf_t *strbuf)
{
	if (! strbuf)
		return;

	if (strbuf->string)
		free(strbuf->string);
	free(strbuf);
} /* sdb_strbuf_destroy */

ssize_t
sdb_strbuf_vappend(sdb_strbuf_t *strbuf, const char *fmt, va_list ap)
{
	va_list aq;
	int status;

	if ((! strbuf) || (! fmt))
		return -1;

	assert((strbuf->size == 0) || (strbuf->string[strbuf->pos] == '\0'));

	if (strbuf->pos >= strbuf->size)
		/* use some arbitrary but somewhat reasonable default */
		if (strbuf_resize(strbuf, strbuf->size ? 2 * strbuf->size : 64))
			return -1;

	assert(strbuf->size && strbuf->string);
	assert(strbuf->pos < strbuf->size);

	/* 'ap' is invalid after calling vsnprintf; thus copy before using it */
	va_copy(aq, ap);
	status = vsnprintf(strbuf->string + strbuf->pos,
			strbuf->size - strbuf->pos, fmt, ap);

	if (status < 0) {
		va_end(aq);
		return status;
	}

	if ((size_t)status >= strbuf->size - strbuf->pos) {
		strbuf_resize(strbuf, (size_t)status + 1);

		/* reset string and try again */
		strbuf->string[strbuf->pos] = '\0';
		status = (int)sdb_strbuf_vappend(strbuf, fmt, aq);
	}
	else
		strbuf->pos += (size_t)status;

	va_end(aq);
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

	if (strbuf->size) {
		strbuf->string[0] = '\0';
		strbuf->pos = 0;
	}

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

ssize_t
sdb_strbuf_memappend(sdb_strbuf_t *strbuf, const void *data, size_t n)
{
	if ((! strbuf) || (! data))
		return -1;

	assert((strbuf->size == 0) || (strbuf->string[strbuf->pos] == '\0'));

	if (strbuf->pos + n + 1 >= strbuf->size) {
		size_t newsize = strbuf->size * 2;

		if (! newsize)
			newsize = 64;
		while (strbuf->pos + n + 1 >= newsize)
			newsize *= 2;

		if (strbuf_resize(strbuf, newsize))
			return -1;
	}

	assert(strbuf->size && strbuf->string);
	assert(strbuf->pos < strbuf->size);

	memcpy((void *)(strbuf->string + strbuf->pos), data, n);
	strbuf->pos += n;
	strbuf->string[strbuf->pos] = '\0';

	return (ssize_t)n;
} /* sdb_strbuf_memappend */

ssize_t
sdb_strbuf_memcpy(sdb_strbuf_t *strbuf, const void *data, size_t n)
{
	if ((! strbuf) || (! data))
		return -1;

	if (strbuf->size) {
		strbuf->string[0] = '\0';
		strbuf->pos = 0;
	}

	return sdb_strbuf_memappend(strbuf, data, n);
} /* sdb_strbuf_memcpy */

ssize_t
sdb_strbuf_read(sdb_strbuf_t *strbuf, int fd, size_t n)
{
	if (! strbuf)
		return -1;

	if (strbuf_resize(strbuf, strbuf->pos + n + 1))
		return -1;

	return read(fd, strbuf->string + strbuf->pos, n);
} /* sdb_strbuf_read */

ssize_t
sdb_strbuf_chomp(sdb_strbuf_t *strbuf)
{
	ssize_t ret = 0;

	if (! strbuf)
		return -1;

	assert((!strbuf->size) || (strbuf->pos < strbuf->size));
	assert(strbuf->pos <= strbuf->size);

	while ((strbuf->pos > 0)
			&& (strbuf->string[strbuf->pos - 1] == '\n')) {
		--strbuf->pos;
		strbuf->string[strbuf->pos] = '\0';
		++ret;
	}

	return ret;
} /* sdb_strbuf_chomp */

void
sdb_strbuf_skip(sdb_strbuf_t *strbuf, size_t n)
{
	if ((! strbuf) || (! n))
		return;

	if (n >= strbuf->pos) {
		strbuf->string[0] = '\0';
		strbuf->pos = 0;
		return;
	}

	assert(n < strbuf->pos);
	memmove(strbuf->string, strbuf->string + n, strbuf->pos - n);
	strbuf->pos -= n;
	strbuf->string[strbuf->pos] = '\0';
} /* sdb_strbuf_skip */

const char *
sdb_strbuf_string(sdb_strbuf_t *strbuf)
{
	if (! strbuf)
		return NULL;
	if (! strbuf->size)
		return "";
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

