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
	int width, height;
	const int resize_w, resize_h;

	pthread_mutex_t lock;
	pthread_cond_t cond;
	uint64_t ts;
	uint8_t *frame_jpeg, *frame_rgb;
	size_t frame_jpeg_len, frame_rgb_len;
	std::atomic_bool *const global_stopflag;

public:
	source(std::atomic_bool *const global_stopflag, const int resize_w, const int resize_h);
	virtual ~source();

	bool get_frame(const encoding_t pe, const int jpeg_quality, uint64_t *ts, int *width, int *height, uint8_t **frame, size_t *frame_len);
	void set_frame(const encoding_t pe, const uint8_t *const data, const size_t size);
	void set_scaled_frame(const uint8_t *const in, const int sourcew, const int sourceh);
	void set_size(const int w, const int h) { width = w; height = h; }
	bool need_scale() const { return resize_h != -1 || resize_w != -1; }
};

#endif
