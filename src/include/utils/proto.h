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

#include "utils/strbuf.h"

#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sdb_proto_marshal:
 * Encode the message into the wire format by adding an appropriate header.
 * The encoded message is written to buf which has to be large enough to store
 * the header (64 bits) and the entire message.
 *
 * Returns:
 *  - the number of bytes of the full encoded message on success (even if less
 *    than that fit into and was written to the buffer)
 *  - a negative value on error
 */
ssize_t
sdb_proto_marshal(char *buf, size_t buf_len, uint32_t code,
		uint32_t msg_len, const char *msg);

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
 * sdb_proto_unmarshal_int:
 * Read and decode an integer from the specified string.
 */
uint32_t
sdb_proto_unmarshal_int(const char *buf, size_t buf_len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_UTILS_PROTO_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

