/*
 * SysDB - src/utils/strings.c
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
#endif

#include "utils/strings.h"

#include <assert.h>

#include <stdlib.h>
#include <string.h>

/*
 * private helper functions
 */

static int
ensure_len(char ***s, size_t *s_len, size_t len)
{
	char **tmp;

	if ((! s) || (! s_len))
		return -1;

	if (! len) {
		if (*s)
			free(*s);
		*s = NULL;
		*s_len = 0;
		return 0;
	}

	if (*s) {
		tmp = realloc(*s, len * sizeof(*tmp));
		if (tmp && (len > *s_len))
			memset(tmp + *s_len, 0, (len - *s_len) * sizeof(*tmp));
	}
	else
		tmp = calloc(len, sizeof(*tmp));

	if (! tmp)
		return -1;

	*s = tmp;
	*s_len = len;
	return 0;
} /* ensure_len */

/*
 * public API
 */

int
stringv_copy(char ***dst, size_t *dst_len,
		const char * const *src, size_t src_len)
{
	size_t i;

	if (src_len && (! src))
		return -1;
	if (ensure_len(dst, dst_len, src_len))
		return -1;
	assert(dst);

	for (i = 0; i < src_len; ++i) {
		if ((*dst)[i])
			free((*dst)[i]);
		(*dst)[i] = strdup(src[i]);
		if (! (*dst)[i])
			return -1;
	}
	return 0;
} /* stringv_copy */

int
stringv_append(char ***s, size_t *s_len, const char *elem)
{
	size_t i;

	if ((! s_len) || ensure_len(s, s_len, *s_len + 1))
		return -1;
	assert(s);

	i = *s_len - 1;
	(*s)[i] = strdup(elem);
	if (! (*s)[i])
		return -1;
	return 0;
} /* stringv_append */

void
stringv_free(char ***s, size_t *s_len)
{
	size_t i;

	if (*s) {
		for (i = 0; i < *s_len; ++i) {
			if ((*s)[i])
				free((*s)[i]);
			(*s)[i] = NULL;
		}
		free(*s);
	}

	*s = NULL;
	*s_len = 0;
} /* stringv_free */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

