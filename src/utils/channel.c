/*
 * SysDB - src/utils/channel.c
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

#include "utils/channel.h"

#include <assert.h>

#include <stdlib.h>
#include <string.h>

#include <pthread.h>

/*
 * private data types
 */

struct sdb_channel {
	pthread_rwlock_t lock;

	/* maybe TODO: add support for 'nil' values using a boolean area */

	void  *data;
	size_t data_len;
	size_t elem_size;

	size_t head;
	size_t tail;
	_Bool full;
};

/*
 * private helper functions
 */

#define NEXT_WRITE(chan) (((chan)->tail + 1) % (chan)->data_len)
#define NEXT_READ(chan) (((chan)->head + 1) % (chan)->data_len)

#define ELEM(chan, i) \
	(void *)((char *)(chan)->data + (i) * (chan)->elem_size)
#define TAIL(chan) ELEM(chan, (chan)->tail)
#define HEAD(chan) ELEM(chan, (chan)->head)

/* Insert a new element at the end. */
static int
channel_write(sdb_channel_t *chan, const void *data)
{
	assert(chan && data);

	if (chan->full)
		return -1;

	memcpy(TAIL(chan), data, chan->elem_size);
	chan->tail = NEXT_WRITE(chan);

	chan->full = chan->tail == chan->head;
	return 0;
} /* channel_write */

/* retrieve the first element */
static int
channel_read(sdb_channel_t *chan, void *data)
{
	assert(chan && data);

	if ((chan->head == chan->tail) && (! chan->full))
		return -1;

	memcpy(data, HEAD(chan), chan->elem_size);
	chan->head = NEXT_READ(chan);

	chan->full = 0;
	return 0;
} /* channel_read */

/*
 * public API
 */

sdb_channel_t *
sdb_channel_create(size_t size, size_t elem_size)
{
	sdb_channel_t *chan;

	if (! elem_size)
		return NULL;
	if (! size)
		size = 1;

	chan = calloc(1, sizeof(*chan));
	if (! chan)
		return NULL;

	chan->data = calloc(size, elem_size);
	if (! chan->data) {
		sdb_channel_destroy(chan);
		return NULL;
	}

	chan->data_len = size;
	chan->elem_size = elem_size;

	pthread_rwlock_init(&chan->lock, /* attr = */ NULL);

	chan->head = chan->tail = 0;
	return chan;
} /* sdb_channel_create */

void
sdb_channel_destroy(sdb_channel_t *chan)
{
	if (! chan)
		return;

	pthread_rwlock_wrlock(&chan->lock);
	free(chan->data);
	chan->data = NULL;
	chan->data_len = 0;

	pthread_rwlock_unlock(&chan->lock);
	pthread_rwlock_destroy(&chan->lock);
	free(chan);
} /* sdb_channel_destroy */

int
sdb_channel_write(sdb_channel_t *chan, const void *data)
{
	int status;

	if ((! chan) || (! data))
		return -1;

	pthread_rwlock_wrlock(&chan->lock);
	status = channel_write(chan, data);
	pthread_rwlock_unlock(&chan->lock);
	return status;
} /* sdb_channel_write */

int
sdb_channel_read(sdb_channel_t *chan, void *data)
{
	int status;

	if ((! chan) || (! data))
		return -1;

	pthread_rwlock_wrlock(&chan->lock);
	status = channel_read(chan, data);
	pthread_rwlock_unlock(&chan->lock);
	return status;
} /* sdb_channel_read */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

