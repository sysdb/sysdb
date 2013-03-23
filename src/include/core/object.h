/*
 * SysDB - src/include/core/object.h
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

#ifndef SDB_CORE_OBJECT_H
#define SDB_CORE_OBJECT_H 1

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sdb_type;
typedef struct sdb_type sdb_type_t;

struct sdb_object;
typedef struct sdb_object sdb_object_t;

struct sdb_type {
	size_t size;

	int (*init)(sdb_object_t *, va_list);
	void (*destroy)(sdb_object_t *);
	sdb_object_t *(*clone)(const sdb_object_t *);
};
#define SDB_TYPE_INIT { 0, NULL, NULL, NULL }

struct sdb_object {
	sdb_type_t type;
	int ref_cnt;
};
#define SDB_OBJECT_INIT { SDB_TYPE_INIT, 1 }
#define SDB_OBJECT_TYPED_INIT(t) { (t), 1 }

typedef struct {
	sdb_object_t super;
	void *data;
	void (*destructor)(void *);
} sdb_object_wrapper_t;

#define SDB_OBJ(obj) ((sdb_object_t *)(obj))
#define SDB_CONST_OBJ(obj) ((const sdb_object_t *)(obj))
#define SDB_OBJ_WRAPPER(obj) ((sdb_object_wrapper_t *)(obj))
#define SDB_CONST_OBJ_WRAPPER(obj) ((const sdb_object_wrapper_t *)(obj))

/*
 * sdb_object_create:
 * Allocates a new sdb_object_t of the specified 'type'. The object will be
 * initialized to zero and then passed on to the 'init' function (if
 * specified). If specified, the 'destroy' callback will be called, when the
 * reference count drops to zero and before freeing the memory allocated by
 * the object itself.
 *
 * The init function will be called with the remaining arguments passed to
 * sdb_object_create. If the init function fails (returns a non-zero value),
 * the object will be destructed and destroyed.
 *
 * The reference count of the new object will be 1.
 *
 * Returns:
 *  - the newly allocated object
 *  - NULL on error
 */
sdb_object_t *
sdb_object_create(sdb_type_t type, ...);

/*
 * sdb_object_create_wrapper:
 * Create a new sdb_object_t wrapping some arbitrary other object.
 *
 * Creation and initialization of the wrapped object needs to happen outside
 * of the SysDB object system.
 */
sdb_object_t *
sdb_object_create_wrapper(void *data, void (*destructor)(void *));

#define SDB_OBJECT_WRAPPER_STATIC(obj, destructor) \
	{ SDB_OBJECT_INIT, (obj), (destructor) }

/*
 * sdb_object_deref:
 * Dereference the object and free the allocated memory in case the ref-count
 * drops to zero. In case a 'destructor' had been registered with the object,
 * it will be called before freeing the memory.
 */
void
sdb_object_deref(sdb_object_t *obj);

/*
 * sdb_object_ref:
 * Take ownership of the specified object, that is, increment the reference
 * count by one.
 */
void
sdb_object_ref(sdb_object_t *obj);

/*
 * sdb_object_clone:
 * Clone an existing object using its type's 'clone' callback. The callback is
 * responsible for correctly initializing a new object (which may be done
 * using the object create function or the object's type's init function).
 *
 * Returns:
 *  - the cloned object on success
 *  - NULL on error or if no clone callback is available
 */
sdb_object_t *
sdb_object_clone(const sdb_object_t *obj);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_CORE_OBJECT_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

