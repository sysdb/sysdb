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

#include <stdlib.h>
#include <string.h>

/*
 * public API
 */

int
stringv_copy(char ***dst, size_t *dst_len,
		const char * const *src, size_t src_len)
{
	char **tmp;
	size_t i;

	if (*dst) {
		tmp = realloc(*dst, src_len * sizeof(*tmp));
		if (tmp)
			memset(tmp, 0, src_len * sizeof(*tmp));
	}
	else
		tmp = calloc(src_len, sizeof(*tmp));

	if (! tmp)
		return -1;

	*dst = tmp;
	*dst_len = src_len;
	for (i = 0; i < src_len; ++i) {
		(*dst)[i] = strdup(src[i]);
		if (! (*dst)[i])
			return -1;
	}
	return 0;
} /* stringv_copy */

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

