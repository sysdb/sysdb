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
#include "core/data.h"
#include "core/time.h"
#include "utils/llist.h"
#include "utils/strbuf.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sdb_store_base_t represents the super-class of any object stored in the
 * database. It inherits from sdb_object_t and may safely be cast to a generic
 * object to access its name.
 */
struct sdb_store_base;
typedef struct sdb_store_base sdb_store_base_t;

/*
 * sdb_store_clear:
 * Clear the entire store and remove all stored objects.
 */
void
sdb_store_clear(void);

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

/*
 * sdb_store_has_host:
 * sdb_store_get_host:
 * Query the store for a host by its (canonicalized) name.
 *
 * sdb_store_get_host increments the ref count of the host object. The caller
 * needs to deref it when no longer using it.
 */
_Bool
sdb_store_has_host(const char *name);

sdb_store_base_t *
sdb_store_get_host(const char *name);

/*
 * sdb_store_attribute:
 * Add/update a host's attribute in the store. If the attribute, identified by
 * its key, already exists for the specified host, it will be updated to the
 * specified values. If the referenced host does not exist, an error will be
 * reported. Else, a new entry will be created in the store. Any memory
 * required for storing the entry will be allocated and managed by the store
 * itself.
 *
 * Returns:
 *  - 0 on success
 *  - a positive value if the new entry is older than the currently stored
 *    entry (in this case, no update will happen)
 *  - a negative value on error
 */
int
sdb_store_attribute(const char *hostname,
		const char *key, const sdb_data_t *value,
		sdb_time_t last_update);

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
sdb_store_service(const char *hostname, const char *name,
		sdb_time_t last_update);

/*
 * Store matchers may be used to lookup objects from the host based on their
 * various attributes. Each type of matcher evaluates attributes of the
 * respective object type.
 *
 * For each matcher object, *all* specified attributes have to match.
 *
 * A store matcher object inherits from sdb_object_t and, thus, may safely be
 * cast to a generic object.
 */
struct sdb_store_matcher;
typedef struct sdb_store_matcher sdb_store_matcher_t;
#define SDB_STORE_MATCHER(obj) ((sdb_store_matcher_t *)(obj))

/*
 * sdb_store_attr_matcher:
 * Creates a matcher matching attributes based on their name or value. Either
 * a complete name (which will have to match completely but case-independent)
 * or an extended POSIX regular expression may be specified.
 */
sdb_store_matcher_t *
sdb_store_attr_matcher(const char *attr_name, const char *attr_name_re,
		const char *attr_value, const char *attr_value_re);

/*
 * sdb_store_service_matcher:
 * Creates a matcher matching services based on their name or attributes.
 */
sdb_store_matcher_t *
sdb_store_service_matcher(const char *service_name, const char *service_name_re,
		sdb_store_matcher_t *attr_matcher);

/*
 * sdb_store_host_matcher:
 * Creates a matcher matching hosts based on their name, services assigned to
 * the host, or its attributes.
 */
sdb_store_matcher_t *
sdb_store_host_matcher(const char *host_name, const char *host_name_re,
		sdb_store_matcher_t *service_matcher,
		sdb_store_matcher_t *attr_matcher);

/*
 * sdb_store_dis_matcher:
 * Creates a matcher matching the disjunction (logical OR) of two matchers.
 */
sdb_store_matcher_t *
sdb_store_dis_matcher(sdb_store_matcher_t *left, sdb_store_matcher_t *right);

/*
 * sdb_store_con_matcher:
 * Creates a matcher matching the conjunction (logical AND) of two matchers.
 */
sdb_store_matcher_t *
sdb_store_con_matcher(sdb_store_matcher_t *left, sdb_store_matcher_t *right);

/*
 * sdb_store_matcher_matches:
 * Check whether the specified matcher matches the specified store object.
 *
 * Returns:
 *  - 1 if the object matches
 *  - 0 else
 */
int
sdb_store_matcher_matches(sdb_store_matcher_t *m, sdb_store_base_t *obj);

/*
 * sdb_store_lookup_cb:
 * Lookup callback. It is called for each matching object when looking up data
 * in the store. The lookup aborts if the callback returns non-zero.
 */
typedef int (*sdb_store_lookup_cb)(sdb_store_base_t *obj, void *user_data);

/*
 * sdb_store_lookup:
 * Look up objects in the store. The specified callback function is called for
 * each object in the store matching 'm'.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_store_lookup(sdb_store_matcher_t *m, sdb_store_lookup_cb cb,
		void *user_data);

/*
 * Flags for serialization functions.
 *
 * By default, the full object will be included in the serialized output. When
 * specifying any of the flags, the respective information will be left out.
 */
enum {
	SDB_SKIP_ATTRIBUTES         = 1 << 0,
	SDB_SKIP_SERVICES           = 1 << 1,
	SDB_SKIP_SERVICE_ATTRIBUTES = 1 << 2,
};

/*
 * sdb_store_tojson:
 * Serialize the entire store to JSON and append the result to the specified
 * buffer.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value on error
 */
int
sdb_store_tojson(sdb_strbuf_t *buf, int flags);

/*
 * sdb_store_host_tojson:
 * Serialize a host object to JSON and append the result to the specified
 * buffer.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value on error
 */
int
sdb_store_host_tojson(sdb_store_base_t *host, sdb_strbuf_t *buf, int flags);

/*
 * sdb_store_iter_cb:
 * Store iterator callback. Iteration stops if the callback returns non-zero.
 */
typedef int (*sdb_store_iter_cb)(sdb_store_base_t *obj, void *user_data);

/*
 * sdb_store_iterate:
 * Iterate the entire store, calling the specified callback for each object.
 * The user_data pointer is passed on to each call of the callback.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_store_iterate(sdb_store_iter_cb cb, void *user_data);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_CORE_STORE_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

