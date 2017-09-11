// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stddef.h>
#include <string.h>

#include "filter_apply_mask.h"

filter_apply_mask::filter_apply_mask(const uint8_t *const pixel_select_bitmap) : pixel_select_bitmap(pixel_select_bitmap)
{
}

filter_apply_mask::~filter_apply_mask()
{
}

void filter_apply_mask::apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	for(int i=0; i<w*h; i++) {
		if (pixel_select_bitmap[i])
			continue;

		const int i3 = i * 3;
		in_out[i3 + 0] = in_out[i3 + 1] = in_out[i3 + 2] = 0;
	}
}
