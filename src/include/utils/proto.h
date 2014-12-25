/*
 * SysDB - src/include/utils/proto.h
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

#ifndef SDB_UTILS_PROTO_H
#define SDB_UTILS_PROTO_H 1

#include "core/data.h"

#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sdb_proto_host, sdb_proto_service, sdb_proto_metric:
 * Protocol-specific representations of the basic information of stored
 * objects.
 */
typedef struct {
	sdb_time_t last_update;
	const char *name;
} sdb_proto_host_t;

typedef struct {
	sdb_time_t last_update;
	const char *hostname;
	const char *name;
} sdb_proto_service_t;

typedef struct {
	sdb_time_t last_update;
	const char *hostname;
	const char *name;
	const char *store_type; /* optional */
	const char *store_id;   /* optional */
} sdb_proto_metric_t;

typedef struct {
	sdb_time_t last_update;
	int parent_type;
	const char *hostname; /* optional */
	const char *parent;
	const char *key;
	const sdb_data_t *value;
} sdb_proto_attribute_t;

/*
 * sdb_proto_marshal:
 * Encode the message into the wire format by adding an appropriate header.
 * The encoded message is written to buf which has to be large enough to store
 * the header (64 bits) and the entire message.
 *
 * Returns:
 *  - The number of bytes of the full encoded message on success. The function
 *    does not write more than 'buf_len' bytes. If the output was truncated
 *    then the return value is the number of bytes which would have been
 *    written if enough space had been available.
 *  - a negative value on error
 */
ssize_t
sdb_proto_marshal(char *buf, size_t buf_len, uint32_t code,
		uint32_t msg_len, const char *msg);

/*
 * sdb_proto_marshal_data:
 * Encode a datum into the wire format and write it to buf.
 *
 * Returns:
 *  - The number of bytes of the full encoded datum on success. The function
 *    does not write more than 'buf_len' bytes. If the output was truncated
 *    then the return value is the number of bytes which would have been
 *    written if enough space had been available.
 *  - a negative value else
 */
ssize_t
sdb_proto_marshal_data(char *buf, size_t buf_len, const sdb_data_t *datum);

/*
 * sdb_proto_marshal_host, sdb_proto_marshal_service,
 * sdb_proto_marshal_metric, sdb_proto_marshal_attribute:
 * Encode the basic information of a stored object into the wire format and
 * write it to buf. These functions are similar to the sdb_store_<type>
 * functions. See their documentation for details about the arguments.
 *
 * Returns:
 *  - The number of bytes of the full encoded datum on success. The function
 *    does not write more than 'buf_len' bytes. If the output was truncated
 *    then the return value is the number of bytes which would have been
 *    written if enough space had been available.
 *  - a negative value else
 */
ssize_t
sdb_proto_marshal_host(char *buf, size_t buf_len,
		const sdb_proto_host_t *host);
ssize_t
sdb_proto_marshal_service(char *buf, size_t buf_len,
		const sdb_proto_service_t *svc);
ssize_t
sdb_proto_marshal_metric(char *buf, size_t buf_len,
		const sdb_proto_metric_t *metric);
ssize_t
sdb_proto_marshal_attribute(char *buf, size_t buf_len,
		const sdb_proto_attribute_t *attr);

/*
 * sdb_proto_unmarshal_header:
 * Read and decode a message header from the specified string.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_proto_unmarshal_header(const char *buf, size_t buf_len,
		uint32_t *code, uint32_t *msg_len);

/*
 * sdb_proto_unmarshal_int32:
 * Read and decode a 32-bit integer from the specified string.
 */
uint32_t
sdb_proto_unmarshal_int32(const char *buf, size_t buf_len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_UTILS_PROTO_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

