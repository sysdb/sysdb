/*
 * syscollector - src/utils/string.c
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

#include "utils/string.h"

#include <stdio.h>
#include <string.h>

/*
 * public API
 */

char *
sc_strerror(int errnum, char *strerrbuf, size_t buflen)
{
#if STRERROR_R_CHAR_P
	{
		char *tmp = strerror_r(errnum, strerrbuf, buflen);
		if (*strerrbuf = '\0') {
			if (tmp && (tmp != strerrbuf) && (*tmp != '\0'))
				strncpy(strerrbuf, tmp, buflen);
			else
				snprintf(strerrbuf, buflen, "unknown error #%i "
						"(strerror_r(3) did not return an error message)",
						errnum);
		}
	}
#else
	if (strerror_r(errnum, strerrbuf, buflen))
		snprintf(strerrbuf, buflen, "unknown error #%i "
				"(strerror_r(3) failed)", errnum);
#endif

	strerrbuf[buflen - 1] = '\0';
	return strerrbuf;
} /* sc_strerror */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

