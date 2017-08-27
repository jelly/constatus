// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stddef.h>
#include <string.h>

#include "filter_grayscale.h"

filter_grayscale::filter_grayscale()
{
}

filter_grayscale::~filter_grayscale()
{
}

void filter_grayscale::apply_io(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	for(int i=0; i<w*h*3; i++) {
		uint8_t g = (in[i + 0] + in[i + 1] + in[i + 2]) / 3;

		out[i + 0] = out[i + 1] = out[i + 2] = g;
	}
}
