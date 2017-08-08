// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <string.h>

#include "error.h"
#include "filter_overlay.h"
#include "picio.h"

filter_overlay::filter_overlay(const std::string & file)
{
	FILE *fh = fopen(file.c_str(), "rb");
	if (!fh)
		error_exit(true, "%s failed to read", file.c_str());

	read_PNG_file_rgba(fh, &w, &h, &pixels);

	fclose(fh);
}

filter_overlay::~filter_overlay()
{
}

void filter_overlay::apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	memcpy(out, in, w * h * 3);

	int cw = std::min(this -> w, w);
	int ch = std::min(this -> h, h);

	for(int y=0; y<ch; y++) {
		for(int x=0; x<cw; x++) {
			int out_offset = y * w * 3 + x * 3;
			int pic_offset = y * this -> w * 4 + x * 4;

			uint8_t alpha = pixels[pic_offset + 3], ialpha = 255 - alpha;
			out[out_offset + 0] = (int(pixels[pic_offset + 0]) * alpha + (out[out_offset + 0] * ialpha)) / 256;
			out[out_offset + 1] = (int(pixels[pic_offset + 1]) * alpha + (out[out_offset + 1] * ialpha)) / 256;
			out[out_offset + 2] = (int(pixels[pic_offset + 2]) * alpha + (out[out_offset + 2] * ialpha)) / 256;
		}
	}
}
