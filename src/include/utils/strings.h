/*
 * SysDB - src/include/utils/strings.h
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

#ifndef SDB_UTILS_STRINGS_H
#define SDB_UTILS_STRINGS_H 1

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * stringv_copy:
 * Copy a string vector from 'src' to 'dst'. If non-NULL, 'dst' will be
 * reallocated to fit the required size and old entries will be freed before
 * overriding them.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
stringv_copy(char ***dst, size_t *dst_len,
		const char * const *src, size_t src_len);

/*
 * stringv_append:
 * Append a string to a string vector.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
stringv_append(char ***s, size_t *s_len, const char *elem);

/*
 * stringv_free:
 * Free the memory used by 's' and all of it's elements.
 */
void
stringv_free(char ***s, size_t *s_len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_UTILS_STRINGS_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

