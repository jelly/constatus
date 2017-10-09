// (C) 2017 by folkert van heusden, released under AGPL v3.0
#ifndef __SOURCE_V4L__
#define __SOURCE_V4L__

#include <atomic>
#include <string>
#include <thread>

#include "source.h"

void image_yuv420_to_rgb(unsigned char *yuv420, unsigned char *rgb, int width, int height);
void image_yuyv2_to_rgb(const unsigned char* src, int width, int height, unsigned char* dst);

class source_v4l : public source
{
protected:
	int fd, vw, vh;
	unsigned int pixelformat;
	uint8_t *io_buffer;
	size_t unmap_size;
	bool prefer_jpeg, rpi_workaround;

	virtual bool try_v4l_configuration(int fd, int *width, int *height, unsigned int *format);

public:
	source_v4l(const std::string & id, const std::string & dev, const bool prefer_jpeg, const bool rpi_workaround, const int jpeg_quality, const double max_fps, const int w_override, const int h_override, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout);
	virtual ~source_v4l();

	virtual void operator()();
};

#endif
