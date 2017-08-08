// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stdlib.h>

#include "filter_noise_neighavg.h"

filter_noise_neighavg::filter_noise_neighavg()
{
}

filter_noise_neighavg::~filter_noise_neighavg()
{
}

void filter_noise_neighavg::apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	for(int y = 1; y<h-1; y++) {
		for(int x=1; x<w-1; x++) {
			int out1 = 0, out2 = 0, out3 = 0;

			for(int yy=-1; yy<=1; yy++) {
				for(int xx=-1; xx<=1; xx++) {
					if (xx == 0 && yy == 0)
						continue;

					out1 += in[(y + yy) * w * 3 + (x + xx) * 3 + 0];
					out2 += in[(y + yy) * w * 3 + (x + xx) * 3 + 1];
					out3 += in[(y + yy) * w * 3 + (x + xx) * 3 + 2];
				}
			}

			out[y * w * 3 + x * 3 + 0] = out1 / 8;
			out[y * w * 3 + x * 3 + 1] = out2 / 8;
			out[y * w * 3 + x * 3 + 2] = out3 / 8;
		}
	}
}
