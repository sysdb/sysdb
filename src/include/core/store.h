/*
 * syscollector - src/include/core/store.h
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

#ifndef SC_CORE_STORE_H
#define SC_CORE_STORE_H 1

#include "syscollector.h"
#include "core/object.h"
#include "utils/time.h"
#include "utils/llist.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	sc_object_t parent;

	sc_time_t last_update;
	char *name;
} sc_store_obj_t;
#define SC_STORE_OBJ_INIT { SC_OBJECT_INIT, 0, NULL }
#define SC_STORE_OBJ(obj) ((sc_store_obj_t *)(obj))

typedef struct {
	sc_store_obj_t parent;
#define svc_last_update parent.last_update
#define svc_name parent.name

	char *hostname;
} sc_service_t;
#define SC_SVC_INIT { SC_STORE_OBJ_INIT, NULL }
#define SC_SVC(obj) ((sc_service_t *)(obj))

typedef struct {
	sc_store_obj_t parent;
#define host_last_update parent.last_update
#define host_name parent.name

	sc_llist_t *services;
} sc_host_t;
#define SC_HOST_INIT { SC_STORE_OBJ_INIT, NULL }
#define SC_HOST(obj) ((sc_host_t *)(obj))

sc_host_t *
sc_host_create(char *name);

sc_host_t *
sc_host_clone(const sc_host_t *host);

int
sc_store_host(const sc_host_t *host);

const sc_host_t *
sc_store_get_host(char *name);

sc_service_t *
sc_service_create(char *hostname, char *name);

sc_service_t *
sc_service_clone(const sc_service_t *svc);

int
sc_store_service(const sc_service_t *svc);

const sc_service_t *
sc_store_get_service(const sc_host_t *host, char *name);

int
sc_store_dump(FILE *fh);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SC_CORE_STORE_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

