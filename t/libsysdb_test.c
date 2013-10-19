/*
 * SysDB - t/libsysdb_test.c
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

#include "libsysdb_test.h"

#include <check.h>
#include <stdio.h>

int
main(void)
{
	int failed = 0;
	size_t i;

	suite_creator_t creators[] = {
		{ util_channel_suite, NULL },
		{ util_dbi_suite, NULL },
		{ util_llist_suite, NULL },
		{ util_strbuf_suite, NULL },
	};

	for (i = 0; i < SDB_STATIC_ARRAY_LEN(creators); ++i) {
		SRunner *sr;
		Suite *s;

		if (creators[i].msg)
			printf("%s\n", creators[i].msg);

		if (!creators[i].creator)
			continue;

		s = creators[i].creator();
		sr = srunner_create(s);
		srunner_run_all(sr, CK_NORMAL);
		failed += srunner_ntests_failed(sr);
		srunner_free(sr);
	}

	return failed;
} /* main */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

