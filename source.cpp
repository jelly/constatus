// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "source.h"
#include "picio.h"

source::source(const int jpeg_quality, std::atomic_bool *const global_stopflagIn) : global_stopflag(global_stopflagIn)
{
	width = height = -1;
	ts = 0;

	frame_rgb = NULL;
	frame_jpeg = NULL;

	cond = PTHREAD_COND_INITIALIZER;

	lock = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutexattr_t ma;
	pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_ERRORCHECK);
	pthread_mutex_init(&lock, &ma);
}

source::~source()
{
	free(frame_rgb);
	free(frame_jpeg);
}

void source::set_frame(const encoding_t pe, const uint8_t *const data, const size_t size)
{
	if (data == NULL || size == 0)
		return;

	pthread_mutex_lock(&lock);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	ts = uint64_t(tv.tv_sec) * uint64_t(1000 * 1000) + uint64_t(tv.tv_usec);

	free(frame_rgb);
	frame_rgb = NULL;

	free(frame_jpeg);
	frame_jpeg = NULL;

	if (size) {
		if (pe == E_RGB) {
			frame_rgb = (uint8_t *)valloc(size);
			frame_rgb_len = size;
			memcpy(frame_rgb, data, size);
		}
		else {
			frame_jpeg = (uint8_t *)valloc(size);
			frame_jpeg_len = size;
			memcpy(frame_jpeg, data, size);
		}
	}

	pthread_mutex_unlock(&lock);

	pthread_cond_broadcast(&cond);
}

bool source::get_frame(const encoding_t pe, const int jpeg_quality, uint64_t *ts, int *width, int *height, uint8_t **frame, size_t *frame_len)
{
	pthread_mutex_lock(&lock);

	while(this -> ts <= *ts)
		pthread_cond_wait(&cond, &lock);

	*ts = this -> ts;

	bool rc = true;

	*width = this -> width;
	*height = this -> height;

	if (!frame_rgb && !frame_jpeg)
		rc = false;
	else if (pe == E_RGB) {
		if (!frame_rgb) {
			read_JPEG_memory(frame_jpeg, frame_jpeg_len, width, height, &frame_rgb);
			frame_rgb_len = *width * *height * 3;
		}

		*frame = (uint8_t *)valloc(frame_rgb_len);
		memcpy(*frame, frame_rgb, frame_rgb_len);
		*frame_len = frame_rgb_len;
	}
	else { // jpeg
		if (!frame_jpeg)
			write_JPEG_memory(*width, *height, jpeg_quality, frame_rgb, (char **)&frame_jpeg, &frame_jpeg_len);

		*frame = (uint8_t *)valloc(frame_jpeg_len);
		memcpy(*frame, frame_jpeg, frame_jpeg_len);
		*frame_len = frame_jpeg_len;
	}

	pthread_mutex_unlock(&lock);

	return rc;
}
