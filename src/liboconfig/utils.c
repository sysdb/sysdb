/**
 * SysDB - src/liboconfig/utils.c
 * Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "oconfig.h"

int
oconfig_get_string(oconfig_item_t *ci, char **value)
{
	if (! ci)
		return -1;

	if ((ci->values_num != 1) || (ci->values[0].type != OCONFIG_TYPE_STRING))
		return -1;

	if (value)
		*value = ci->values[0].value.string;
	return 0;
} /* oconfig_get_string */

int
oconfig_get_number(oconfig_item_t *ci, double *value)
{
	if (! ci)
		return -1;

	if ((ci->values_num != 1) || (ci->values[0].type != OCONFIG_TYPE_NUMBER))
		return -1;

	if (value)
		*value = ci->values[0].value.number;
	return 0;
} /* oconfig_get_number */

int
oconfig_get_boolean(oconfig_item_t *ci, _Bool *value)
{
	if (! ci)
		return -1;

	if ((ci->values_num != 1) || (ci->values[0].type != OCONFIG_TYPE_BOOLEAN))
		return -1;

	if (value)
		*value = ci->values[0].value.boolean != 0;
	return 0;
} /* oconfig_get_boolean */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

