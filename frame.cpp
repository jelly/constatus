// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "frame.h"

frame::frame()
{
	lock = PTHREAD_MUTEX_INITIALIZER;
	cond = PTHREAD_COND_INITIALIZER;;
	data = NULL;
	s = 0;
	ts = 0;
}

frame::~frame()
{
	delete [] data;
}

void frame::set_frame(const uint8_t *const data_in, const size_t size)
{
	pthread_mutex_lock(&lock);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	ts = uint64_t(tv.tv_sec) * uint64_t(1000 * 1000) + uint64_t(tv.tv_usec);

	free(data);

	if (size) {
		data = (uint8_t *)malloc(size);
		s = size;
		memcpy(data, data_in, size);
	}
	else {
		data = NULL;
		s = 0;
	}

	pthread_mutex_unlock(&lock);

	pthread_cond_broadcast(&cond);
}

void frame::wait_for_update(const uint64_t newer_than)
{
	pthread_mutex_lock(&lock);

	while(ts <= newer_than)
		pthread_cond_wait(&cond, &lock);

	pthread_mutex_unlock(&lock);
}

uint64_t frame::get_ts()
{
	pthread_mutex_lock(&lock);

	uint64_t cur_ts = ts;

	pthread_mutex_unlock(&lock);

	return cur_ts;
}

void frame::get_frame_data(uint8_t **const data_out, size_t *const size)
{
	pthread_mutex_lock(&lock);

	if (s) {
		*data_out = (uint8_t *)malloc(s);
		memcpy(*data_out, data, s);
		*size = s;
	}
	else {
		*data_out = NULL;
		*size = 0;
	}

	pthread_mutex_unlock(&lock);
}
