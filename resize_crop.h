#ifndef __RESIZE_CROP_H__
#define __RESIZE_CROP_H__

#include <stdint.h>
#include <string>

#include "resize.h"

class resize_crop : public resize
{
public:
	resize_crop();
	virtual ~resize_crop();

	void do_resize_crop(const int win, const int hin, const uint8_t *const in, const int wout, const int hout, uint8_t **out);
};

#endif
