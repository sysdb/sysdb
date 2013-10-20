/*
 * SysDB - src/include/utils/channel.h
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

#ifndef SDB_UTILS_CHANNEL_H
#define SDB_UTILS_CHANNEL_H 1

#include "core/object.h"

#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A channel is an asynchronous I/O multiplexer supporting multiple parallel
 * readers and writers. A channel may be buffered (depending on its 'size'
 * attribute). Writing fails unless buffer space is available and reading
 * fails if no data is available.
 */

struct sdb_channel;
typedef struct sdb_channel sdb_channel_t;

/*
 * sdb_channel_create:
 * Create a new channel for elements of size 'elem_size'. At most, 'size'
 * elements can be buffered in the channel (default: 1).
 *
 * Returns:
 *  - a channel object on success
 *  - a negative value else
 */
sdb_channel_t *
sdb_channel_create(size_t size, size_t elem_size);

/*
 * sdb_channel_destroy:
 * Removing all pending data and destroy the specified channel freeing its
 * memory.
 */
void
sdb_channel_destroy(sdb_channel_t *chan);

/*
 * sdb_channel_write:
 * Write an element to a channel. The memory pointed to by 'data' is copied to
 * the buffer based on the channel's element size.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_channel_write(sdb_channel_t *chan, const void *data);

/*
 * sdb_channel_read:
 * Read an element from a channel. The element is copied to the location
 * pointed to by 'data' which needs to be large enough to store a whole
 * element based on the channel's element size.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_channel_read(sdb_channel_t *chan, void *data);

/*
 * sdb_channel_select:
 * Wait for a channel to become "ready" for I/O. A channel is considered ready
 * if it is possible to perform the corresponding I/O operation successfully
 * *in some thread*. In case 'wantread' or 'read_data' is non-NULL, wait for
 * data to be available in the channel for reading. In case 'wantwrite' or
 * 'write_data' is non-NULL, wait for buffer space to be available for writing
 * to the channel. If non-NULL, the value pointed to by the 'want...'
 * arguments will be "true" iff the respective operation is ready. If the
 * '..._data' arguments are non-NULL, the respective operation is executed
 * atomically once the channel is ready for it. If 'abstime' is specified, the
 * operation will time out with an error if the specified absolute time has
 * passed.
 */
int
sdb_channel_select(sdb_channel_t *chan, int *wantread, void *read_data,
		int *wantwrite, void *write_data, const struct timespec *timeout);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_UTILS_CHANNEL_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

