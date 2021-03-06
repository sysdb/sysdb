/*
 * SysDB - t/unit/libsysdb_testutils.c
 * Copyright (C) 2014 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include "testutils.h"

#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>

int
sdb_regmatches(const char *regex, const char *string)
{
	regex_t reg;
	int status;

	status = regcomp(&reg, regex, REG_EXTENDED | REG_NOSUB);
	if (status)
		return status;

	status = regexec(&reg, string, /* matches = */ 0, NULL, /* flags = */ 0);
	regfree(&reg);
	return status;
} /* sdb_regmatches */

void
sdb_diff_strings(const char *desc, const char *got, const char *expected)
{
	size_t len1 = strlen(got);
	size_t len2 = strlen(expected);

	size_t i;
	int pos = -1;

	if (len1 != len2)
		pos = (int)SDB_MIN(len1, len2);

	for (i = 0; i < SDB_MIN(len1, len2); ++i) {
		if (got[i] != expected[i]) {
			pos = (int)i;
			break;
		}
	}

	fail_unless(pos == -1, "%s:\n"
			"         got: %s\n              %*s\n    expected: %s",
			desc, got, pos + 1, "^", expected);
} /* sdb_diff_strings */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

