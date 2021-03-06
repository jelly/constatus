// (C) 2017 by folkert van heusden, released under AGPL v3.0
#ifndef __MOTION_ONLY_H__
#define __MOTION_ONLY_H__

#include <stdint.h>

#include "filter.h"

class filter_motion_only : public filter
{
private:
	const uint8_t *const psb;
	uint8_t *prev1, *prev2;
	const int noise_level;

public:
	filter_motion_only(const uint8_t *pixel_select_bitmap, const int noise_level);
	virtual ~filter_motion_only();

	bool uses_in_out() const { return false; }
	void apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out);
};

#endif
