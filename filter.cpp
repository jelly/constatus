// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stdlib.h>
#include <string.h>

#include "filter.h"

void apply_filters(const std::vector<filter *> *const filters, const uint8_t *const prev, uint8_t *const work, const uint64_t ts, const int w, const int h)
{
	const size_t bytes = w * h * 3;
	uint8_t *const temp = (uint8_t *)valloc(bytes);

	for(filter *f : *filters) {
		f -> apply(ts, w, h, prev, work, temp);

		memcpy(work, temp, bytes);
	}

	free(temp);
}

void free_filters(const std::vector<filter *> *filters)
{
	for(filter *f : *filters)
		delete f;
}

void scale(const uint8_t *in, const int win, const int hin, uint8_t **out, const int wout, const int hout)
{
	*out = (uint8_t *)malloc(wout * hout * 3);

	const double maxw = std::max(win, wout);
	const double maxh = std::max(hin, hout);

	const double hins = hin / maxh;
	const double wins = win / maxw;
	const double houts = hout / maxh;
	const double wouts = wout / maxw;

	const double wins3 = wins * 3;
	const double wouts3 = wouts * 3;

	for(int y=0; y<maxh; y++) {
		const int in_scaled_y = y * hins;
		const int in_scaled_o = in_scaled_y * win * 3;
		const int out_scaled_y = y * houts;
		const int out_scaled_o = out_scaled_y * wout * 3;

		for(int x=0; x<maxw; x++) {
			int ino = in_scaled_o + x * wins3;
			int outo = out_scaled_o + x * wouts3;

			(*out)[outo + 0] = in[ino + 0];
			(*out)[outo + 1] = in[ino + 1];
			(*out)[outo + 2] = in[ino + 2];
		}
	}
}

filter::filter()
{
}

filter::~filter()
{
}

void filter::apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	memcpy(out, in, w * h * 3);
}
