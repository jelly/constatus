// (C) 2017 by folkert van heusden, released under AGPL v3.0
#ifndef __MARKER_SIMPLE_H__
#define __MARKER_SIMPLE_H__

#include <stdint.h>

#include "filter.h"
#include "meta.h"

typedef enum { m_red, m_red_invert, m_invert } sm_mode_t;

class filter_marker_simple : public filter
{
private:
	const sm_mode_t mode;
	const uint8_t *const psb;
	meta *const m;
	const int noise_level;

	void updatePixel(uint8_t *const out, const int x, const int y, const int w);

public:
	filter_marker_simple(const sm_mode_t modeIn, const uint8_t *const pixel_select_bitmap, meta *const m, const int noise_level);
	virtual ~filter_marker_simple();

	bool uses_in_out() const { return false; }
	void apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out);
};

#endif
