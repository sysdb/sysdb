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

#include <errno.h>

#include <stdlib.h>
#include <string.h>

#include <time.h>

#include <pthread.h>

/*
 * private data types
 */

struct sdb_channel {
	pthread_mutex_t lock;

	/* signaling for select() operation */
	pthread_cond_t cond;

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

/* Insert a new element at the end; chan->lock must be held.
 * Returns 0 if data has been written or if data may be written
 * if 'data' is NULL. */
static int
channel_write(sdb_channel_t *chan, const void *data)
{
	assert(chan);

	if (chan->full)
		return -1;
	else if (! data)
		return 0;

	memcpy(TAIL(chan), data, chan->elem_size);
	chan->tail = NEXT_WRITE(chan);

	chan->full = chan->tail == chan->head;
	pthread_cond_broadcast(&chan->cond);
	return 0;
} /* channel_write */

/* Retrieve the first element; chan->lock must be held.
 * Returns 0 if data has been read or if data is available
 * if 'data' is NULL. */
static int
channel_read(sdb_channel_t *chan, void *data)
{
	assert(chan);

	if ((chan->head == chan->tail) && (! chan->full))
		return -1;
	else if (! data)
		return 0;

	memcpy(data, HEAD(chan), chan->elem_size);
	chan->head = NEXT_READ(chan);

	chan->full = 0;
	pthread_cond_broadcast(&chan->cond);
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

	pthread_mutex_init(&chan->lock, /* attr = */ NULL);
	pthread_cond_init(&chan->cond, /* attr = */ NULL);

	chan->head = chan->tail = 0;
	return chan;
} /* sdb_channel_create */

void
sdb_channel_destroy(sdb_channel_t *chan)
{
	if (! chan)
		return;

	pthread_mutex_lock(&chan->lock);
	free(chan->data);
	chan->data = NULL;
	chan->data_len = 0;

	pthread_cond_destroy(&chan->cond);

	pthread_mutex_unlock(&chan->lock);
	pthread_mutex_destroy(&chan->lock);
	free(chan);
} /* sdb_channel_destroy */

int
sdb_channel_select(sdb_channel_t *chan, int *wantread, void *read_data,
		int *wantwrite, void *write_data, const struct timespec *timeout)
{
	struct timespec abstime;
	int status = 0;

	if (! chan) {
		errno = EINVAL;
		return -1;
	}

	if ((! wantread) && (! read_data) && (! wantwrite) && (! write_data)) {
		errno = EINVAL;
		return -1;
	}

	if (timeout) {
		if (clock_gettime(CLOCK_REALTIME, &abstime))
			return -1;

		abstime.tv_sec += timeout->tv_sec;
		abstime.tv_nsec += timeout->tv_nsec;
	}

	pthread_mutex_lock(&chan->lock);
	while (! status) {
		int read_status, write_status;

		read_status = channel_read(chan, read_data);
		write_status = channel_write(chan, write_data);

		if ((! read_status) || (! write_status)) {
			if (wantread)
				*wantread = read_status == 0;
			if (wantwrite)
				*wantwrite = write_status == 0;

			if (((wantread || read_data) && (! read_status))
					|| ((wantwrite || write_data) && (! write_status)))
				break;
		}

		if (timeout)
			status = pthread_cond_timedwait(&chan->cond, &chan->lock,
					&abstime);
		else
			status = pthread_cond_wait(&chan->cond, &chan->lock);
	}

	pthread_mutex_unlock(&chan->lock);
	if (status) {
		errno = status;
		return -1;
	}
	return 0;
} /* sdb_channel_select */

int
sdb_channel_write(sdb_channel_t *chan, const void *data)
{
	int status;

	if ((! chan) || (! data))
		return -1;

	pthread_mutex_lock(&chan->lock);
	status = channel_write(chan, data);
	pthread_mutex_unlock(&chan->lock);
	return status;
} /* sdb_channel_write */

int
sdb_channel_read(sdb_channel_t *chan, void *data)
{
	int status;

	if ((! chan) || (! data))
		return -1;

	pthread_mutex_lock(&chan->lock);
	status = channel_read(chan, data);
	pthread_mutex_unlock(&chan->lock);
	return status;
} /* sdb_channel_read */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

