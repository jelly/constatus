// (C) 2017 by folkert van heusden, released under AGPL v3.0
#ifndef __SOURCE_H__
#define __SOURCE_H__

#include <atomic>
#include <pthread.h>
#include <stdint.h>

typedef enum { E_RGB, E_JPEG } encoding_t;

class source
{
protected:
	int width, height, jpeg_quality;

	pthread_mutex_t lock;
	pthread_cond_t cond;
	uint64_t ts;
	uint8_t *frame_jpeg, *frame_rgb;
	size_t frame_jpeg_len, frame_rgb_len;
	std::atomic_bool *const global_stopflag;

public:
	source(const int jpeg_quality, std::atomic_bool *const global_stopflag);
	virtual ~source();

	bool get_frame(const encoding_t pe, const int jpeg_quality, uint64_t *ts, int *width, int *height, uint8_t **frame, size_t *frame_len);
	void set_frame(const encoding_t pe, const uint8_t *const data, const size_t size);
	void set_size(const int w, const int h) { width = w; height = h; }
};

#endif
