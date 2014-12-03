/**
 * SysDB - src/liboconfig/utils.h
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

#ifndef SDB_OCONFIG_UTILS_H
#define SDB_OCONFIG_UTILS_H 1

#include "oconfig.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* oconfig_get_<type>:
 * Checks if the specified item has exactly one value of the respective type
 * and returns that value through the appropriate parameter.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
oconfig_get_string(oconfig_item_t *ci, char **value);
int
oconfig_get_number(oconfig_item_t *ci, double *value);
int
oconfig_get_boolean(oconfig_item_t *ci, bool *value);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SDB_OCONFIG_UTILS_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

