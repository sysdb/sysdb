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

/* free memory if most of the buffer is unused */
#define CHECK_SHRINK(buf) \
	do { \
		if ((buf)->pos < (buf)->size / 3) \
			/* don't free all memory to avoid churn */ \
			strbuf_resize((buf), 2 * (buf)->pos); \
	} while (0)

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
strbuf_resize(sdb_strbuf_t *buf, size_t new_size)
{
	char *tmp;

	if (new_size <= buf->pos)
		return -1;

	tmp = realloc(buf->string, new_size);
	if (! tmp)
		return -1;

	if (new_size)
		buf->string = tmp;
	else
		buf->string = NULL;
	buf->size = new_size;
	return 0;
} /* strbuf_resize */

/*
 * public API
 */

sdb_strbuf_t *
sdb_strbuf_create(size_t size)
{
	sdb_strbuf_t *buf;

	buf = calloc(1, sizeof(*buf));
	if (! buf)
		return NULL;

	buf->string = NULL;
	if (size) {
		buf->string = malloc(size);
		if (! buf->string) {
			free(buf);
			return NULL;
		}

		buf->string[0] = '\0';
	}

	buf->size = size;
	buf->pos  = 0;

	return buf;
} /* sdb_strbuf_create */

void
sdb_strbuf_destroy(sdb_strbuf_t *buf)
{
	if (! buf)
		return;

	if (buf->string)
		free(buf->string);
	free(buf);
} /* sdb_strbuf_destroy */

ssize_t
sdb_strbuf_vappend(sdb_strbuf_t *buf, const char *fmt, va_list ap)
{
	va_list aq;
	int status;

	if ((! buf) || (! fmt))
		return -1;

	assert((buf->size == 0) || (buf->string[buf->pos] == '\0'));

	if (! buf->size) {
		/* use some arbitrary but somewhat reasonable default */
		if (strbuf_resize(buf, 64))
			return -1;
	}
	/* make sure to reserve space for the nul-byte */
	else if (buf->pos >= buf->size - 1)
		if (strbuf_resize(buf, 2 * buf->size))
			return -1;

	assert(buf->size && buf->string);
	assert(buf->pos < buf->size);

	/* 'ap' is invalid after calling vsnprintf; thus copy before using it */
	va_copy(aq, ap);
	status = vsnprintf(buf->string + buf->pos,
			buf->size - buf->pos, fmt, ap);

	if (status < 0) {
		va_end(aq);
		return status;
	}

	/* 'status' does not include nul-byte */
	if ((size_t)status >= buf->size - buf->pos) {
		if (strbuf_resize(buf, buf->pos + (size_t)status + 1)) {
			va_end(aq);
			return -1;
		}

		/* reset string and try again */
		buf->string[buf->pos] = '\0';
		status = (int)sdb_strbuf_vappend(buf, fmt, aq);
	}
	else
		buf->pos += (size_t)status;

	va_end(aq);

	/* even though this function always appends to the existing buffer, the
	 * size might have previously been reset */
	CHECK_SHRINK(buf);

	return (ssize_t)status;
} /* sdb_strbuf_vappend */

ssize_t
sdb_strbuf_append(sdb_strbuf_t *buf, const char *fmt, ...)
{
	va_list ap;
	ssize_t status;

	va_start(ap, fmt);
	status = sdb_strbuf_vappend(buf, fmt, ap);
	va_end(ap);

	return status;
} /* sdb_strbuf_append */

ssize_t
sdb_strbuf_vsprintf(sdb_strbuf_t *buf, const char *fmt, va_list ap)
{
	if (! buf)
		return -1;

	if (buf->size) {
		buf->string[0] = '\0';
		buf->pos = 0;
	}

	return sdb_strbuf_vappend(buf, fmt, ap);
} /* sdb_strbuf_vsprintf */

ssize_t
sdb_strbuf_sprintf(sdb_strbuf_t *buf, const char *fmt, ...)
{
	va_list ap;
	ssize_t status;

	va_start(ap, fmt);
	status = sdb_strbuf_vsprintf(buf, fmt, ap);
	va_end(ap);

	return status;
} /* sdb_strbuf_sprintf */

ssize_t
sdb_strbuf_memappend(sdb_strbuf_t *buf, const void *data, size_t n)
{
	if ((! buf) || (! data))
		return -1;

	assert((buf->size == 0) || (buf->string[buf->pos] == '\0'));

	if (buf->pos + n + 1 >= buf->size) {
		size_t newsize = buf->size * 2;

		if (! newsize)
			newsize = 64;
		while (buf->pos + n + 1 >= newsize)
			newsize *= 2;

		if (strbuf_resize(buf, newsize))
			return -1;
	}

	assert(buf->size && buf->string);
	assert(buf->pos < buf->size);

	memcpy((void *)(buf->string + buf->pos), data, n);
	buf->pos += n;
	buf->string[buf->pos] = '\0';

	/* even though this function always appends to the existing buffer, the
	 * size might have previously been reset */
	CHECK_SHRINK(buf);

	return (ssize_t)n;
} /* sdb_strbuf_memappend */

ssize_t
sdb_strbuf_memcpy(sdb_strbuf_t *buf, const void *data, size_t n)
{
	if ((! buf) || (! data))
		return -1;

	if (buf->size) {
		buf->string[0] = '\0';
		buf->pos = 0;
	}

	return sdb_strbuf_memappend(buf, data, n);
} /* sdb_strbuf_memcpy */

ssize_t
sdb_strbuf_read(sdb_strbuf_t *buf, int fd, size_t n)
{
	ssize_t ret;

	if (! buf)
		return -1;

	if (buf->pos + n + 1 >= buf->size)
		if (strbuf_resize(buf, buf->pos + n + 1))
			return -1;

	ret = read(fd, buf->string + buf->pos, n);
	if (ret > 0)
		buf->pos += (size_t)ret;
	return ret;
} /* sdb_strbuf_read */

ssize_t
sdb_strbuf_chomp(sdb_strbuf_t *buf)
{
	ssize_t ret = 0;

	if (! buf)
		return -1;

	assert((!buf->size) || (buf->pos < buf->size));
	assert(buf->pos <= buf->size);

	while ((buf->pos > 0)
			&& (buf->string[buf->pos - 1] == '\n')) {
		--buf->pos;
		buf->string[buf->pos] = '\0';
		++ret;
	}

	return ret;
} /* sdb_strbuf_chomp */

void
sdb_strbuf_skip(sdb_strbuf_t *buf, size_t offset, size_t n)
{
	char *start;
	size_t len;

	if ((! buf) || (! n))
		return;

	if (offset >= buf->pos)
		return;

	len = buf->pos - offset;

	if (n >= len) {
		buf->string[offset] = '\0';
		buf->pos = offset;
		return;
	}

	assert(offset + n < buf->pos);
	assert(offset < buf->pos);

	start = buf->string + offset;
	memmove(start, start + n, len - n);
	buf->pos -= n;
	buf->string[buf->pos] = '\0';

	/* don't resize now but wait for the next write to avoid churn */
} /* sdb_strbuf_skip */

void
sdb_strbuf_clear(sdb_strbuf_t *buf)
{
	if ((! buf) || (! buf->size))
		return;

	buf->string[0] = '\0';
	buf->pos = 0;

	/* don't resize now but wait for the next write to avoid churn */
} /* sdb_strbuf_clear */

const char *
sdb_strbuf_string(sdb_strbuf_t *buf)
{
	if (! buf)
		return NULL;
	if (! buf->size)
		return "";
	return buf->string;
} /* sdb_strbuf_string */

size_t
sdb_strbuf_len(sdb_strbuf_t *buf)
{
	if (! buf)
		return 0;
	return buf->pos;
} /* sdb_strbuf_string */

size_t
sdb_strbuf_cap(sdb_strbuf_t *buf)
{
	if (! buf)
		return 0;
	return buf->size;
} /* sdb_strbuf_cap */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

