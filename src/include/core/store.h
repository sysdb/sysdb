/*
 * SysDB - src/include/core/store.h
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

#ifndef SDB_CORE_STORE_H
#define SDB_CORE_STORE_H 1

#include "sysdb.h"
#include "core/object.h"
#include "core/time.h"
#include "utils/llist.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const sdb_type_t sdb_host_type;
extern const sdb_type_t sdb_attribute_type;
extern const sdb_type_t sdb_service_type;

typedef struct {
	sdb_object_t super;
	sdb_time_t last_update;
} sdb_store_obj_t;
#define SDB_STORE_OBJ_INIT(t) { SDB_OBJECT_TYPED_INIT(t), 0 }
#define SDB_STORE_OBJ(obj) ((sdb_store_obj_t *)(obj))
#define SDB_CONST_STORE_OBJ(obj) ((const sdb_store_obj_t *)(obj))

typedef struct {
	sdb_store_obj_t super;

	char *hostname;
} sdb_service_t;
#define SDB_SVC_INIT { SDB_STORE_OBJ_INIT(sdb_service_type), NULL }
#define SDB_SVC(obj) ((sdb_service_t *)(obj))
#define SDB_CONST_SVC(obj) ((const sdb_service_t *)(obj))

typedef struct {
	sdb_store_obj_t super;

	char *attr_value;
	char *hostname;
} sdb_attribute_t;
#define SDB_ATTR_INIT { SDB_STORE_OBJ_INIT(sdb_attribute_type), NULL, NULL }
#define SDB_ATTR(obj) ((sdb_attribute_t *)(obj))
#define SDB_CONST_ATTR(obj) ((const sdb_attribute_t *)(obj))

typedef struct {
	sdb_store_obj_t super;

	sdb_llist_t *attributes;
	sdb_llist_t *services;
} sdb_host_t;
#define SDB_HOST_INIT { SDB_STORE_OBJ_INIT(sdb_host_type), NULL, NULL }
#define SDB_HOST(obj) ((sdb_host_t *)(obj))
#define SDB_CONST_HOST(obj) ((const sdb_host_t *)(obj))

/* shortcuts for accessing the sdb_store_obj_t attributes of inheriting
 * objects */
#define _last_update super.last_update

/*
 * sdb_store_host:
 * Add/update a host in the store. If the host, identified by its
 * canonicalized name, already exists, it will be updated according to the
 * specified name and timestamp. Else, a new entry will be created in the
 * store. Any memory required for storing the entry will be allocated an
 * managed by the store itself.
 *
 * Returns:
 *  - 0 on success
 *  - a positive value if the new entry is older than the currently stored
 *    entry (in this case, no update will happen)
 *  - a negative value on error
 */
int
sdb_store_host(const char *name, sdb_time_t last_update);

_Bool
sdb_store_has_host(const char *name);

/*
 * sdb_store_attribute:
 * Add/update a host's attribute in the store. If the attribute, identified by
 * its name, already exists for the specified host, it will be updated
 * according to the specified 'attr' object. If the referenced host does not
 * exist, an error will be reported. Else, a new entry will be created in the
 * store. Any memory required for storing the entry will be allocated and
 * managed by the store itself. The specified attribute-object will not be
 * referenced or further accessed.
 *
 * Returns:
 *  - 0 on success
 *  - a positive value if the new entry is older than the currently stored
 *    entry (in this case, no update will happen)
 *  - a negative value on error
 */
int
sdb_store_attribute(const sdb_attribute_t *attr);

/*
 * sdb_store_service:
 * Add/update a store in the store. If the service, identified by its name,
 * already exists for the specified host, it will be updated according to the
 * specified 'service' object. If the referenced host does not exist, an error
 * will be reported. Else, a new entry will be created in the store. Any
 * memory required for storing the entry will be allocated an managed by the
 * store itself. The specified service-object will not be referenced or
 * further accessed.
 *
 * Returns:
 *  - 0 on success
 *  - a positive value if the new entry is older than the currently stored
 *    entry (in this case, no update will happen)
 *  - a negative value on error
 */
int
sdb_store_service(const sdb_service_t *svc);

const sdb_service_t *
sdb_store_get_service(const sdb_host_t *host, const char *name);

int
sdb_store_dump(FILE *fh);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_CORE_STORE_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

