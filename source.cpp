// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "source.h"
#include "picio.h"
#include "filter.h"

source::source(std::atomic_bool *const global_stopflagIn, const int resize_w, const int resize_h) : resize_w(resize_w), resize_h(resize_h), global_stopflag(global_stopflagIn)
{
	width = height = -1;
	ts = 0;

	frame_rgb = NULL;
	frame_jpeg = NULL;

	cond = PTHREAD_COND_INITIALIZER;

	lock = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutexattr_t ma;
	pthread_mutexattr_init(&ma);
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
	struct timeval tv;
	gettimeofday(&tv, NULL);

	struct timespec tc = { tv.tv_sec + 5, 0 }; // FIXME hardcoded timeout

	bool err = false;

	pthread_mutex_lock(&lock);

	while(this -> ts <= *ts) {
		if (pthread_cond_timedwait(&cond, &lock, &tc) == ETIMEDOUT) {
			err = true;
			break;
		}
	}

	if (err) {
		if (this -> width == -1) {
			*width = 352;
			*height = 288;
		}
		else {
			*width = this -> width;
			*height = this -> height;
		}

		size_t bytes = *width * *height * 3;
		uint8_t *fail = (uint8_t *)malloc(bytes);
		memset(fail, 0x80, bytes);

		if (pe == E_RGB) {
			*frame = fail;
			*frame_len = bytes;
			fail = NULL;
		}
		else {
			write_JPEG_memory(*width, *height, jpeg_quality, fail, (char **)frame, frame_len);
		}

		free(fail);

		return true;
	}

	*ts = this -> ts;

	bool rc = true;

	*width = this -> width;
	*height = this -> height;

	if (!frame_rgb && !frame_jpeg)
		rc = false;
	else if (pe == E_RGB) {
		if (!frame_rgb) {
			rc = read_JPEG_memory(frame_jpeg, frame_jpeg_len, width, height, &frame_rgb);
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

void source::set_scaled_frame(const uint8_t *const in, const int sourcew, const int sourceh)
{
	int target_w = resize_w != -1 ? resize_w : sourcew;
	int target_h = resize_h != -1 ? resize_h : sourceh;

	//printf("%dx%d => %dx%d\n", sourcew, sourceh, target_w, target_h);
	uint8_t *out = NULL;
	scale(in, sourcew, sourceh, &out, target_w, target_h);

	set_frame(E_RGB, out, target_w * target_h * 3);

	width = target_w;
	height = target_h;

	free(out);
}
