/**
 * oconfig - src/utils.h
 * Copyright (C) 2012 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef OCONFIG_UTILS_H
#define OCONFIG_UTILS_H 1

#include "oconfig.h"

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
oconfig_get_boolean(oconfig_item_t *ci, _Bool *value);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OCONFIG_UTILS_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

