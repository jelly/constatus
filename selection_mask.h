#ifndef __selection_mask_h__
#define __selection_mask_h__

#include <stdint.h>
#include <string>

#include "resize.h"

class selection_mask
{
private:
	resize *const r;

	uint8_t *pixels;
	int w, h;

	uint8_t *cache;
	int cw, ch;

public:
	selection_mask(resize *const r, const std::string & file);
	virtual ~selection_mask();

	uint8_t *get_mask(const int rw, const int rh);
};

#endif
