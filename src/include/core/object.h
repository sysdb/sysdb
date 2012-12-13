/*
 * syscollector - src/include/core/object.h
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

#ifndef SC_CORE_OBJECT_H
#define SC_CORE_OBJECT_H 1

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sc_object;
typedef struct sc_object sc_object_t;

struct sc_object {
	int    ref_cnt;
	void (*destructor)(sc_object_t *);
	size_t size;
};
#define SC_OBJECT_INIT { 1, NULL, 0 }

typedef struct {
	sc_object_t super;
	void *data;
	void (*destructor)(void *);
} sc_object_wrapper_t;

#define SC_OBJ(obj) ((sc_object_t *)(obj))
#define SC_OBJ_WRAPPER(obj) ((sc_object_wrapper_t *)(obj))

/*
 * sc_object_create:
 * Allocates a new sc_object_t of the specified 'size'. The object will be
 * initialized to zero and then passed on to the 'init' function (if
 * specified). If specified, the 'destructor' will be called, when the
 * reference count drops to zero and before freeing the memory allocated by
 * the object itself.
 *
 * If the init function fails (returns a non-zero value), the object will be
 * destructed and destroyed.
 *
 * The reference count of the new object will be 1.
 *
 * Returns:
 *  - the newly allocated object
 *  - NULL on error
 */
sc_object_t *
sc_object_create(size_t size, int (*init)(sc_object_t *, va_list),
		void (*destructor)(sc_object_t *), ...);

/*
 * sc_object_create_wrapper:
 * Create a new sc_object_t wrapping some arbitrary other object.
 */
sc_object_t *
sc_object_create_wrapper(void *data, void (*destructor)(void *));

#define SC_OBJECT_WRAPPER_STATIC(obj, destructor) \
	{ SC_OBJECT_INIT, (obj), (destructor) }

/*
 * sc_object_deref:
 * Dereference the object and free the allocated memory in case the ref-count
 * drops to zero. In case a 'destructor' had been registered with the object,
 * it will be called before freeing the memory.
 */
void
sc_object_deref(sc_object_t *obj);

/*
 * sc_object_ref:
 * Take ownership of the specified object, that is, increment the reference
 * count by one.
 */
void
sc_object_ref(sc_object_t *obj);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SC_CORE_OBJECT_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

