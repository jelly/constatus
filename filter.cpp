// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stdlib.h>
#include <string.h>

#include "filter.h"
#include "log.h"

void apply_filters(const std::vector<filter *> *const filters, const uint8_t *const prev, uint8_t *const work, const uint64_t ts, const int w, const int h)
{
	const size_t bytes = w * h * 3;
	uint8_t *const temp = (uint8_t *)valloc(bytes);

	bool flag = false;
	for(filter *f : *filters) {
		if (f -> uses_in_out()) {
			if (flag == false)
				f -> apply_io(ts, w, h, prev, work, temp);
			else
				f -> apply_io(ts, w, h, prev, temp, work);

			flag = !flag;
		}
		else {
			if (flag == false)
				f -> apply(ts, w, h, prev, work);
			else
				f -> apply(ts, w, h, prev, temp);
		}
	}

	if (flag == true)
		memcpy(work, temp, bytes);

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

	const int max_offset = wout * hout * 3 - 3;

	const double maxw = std::max(win, wout);
	const double maxh = std::max(hin, hout);

	const double hins = hin / maxh;
	const double wins = win / maxw;
	const double houts = hout / maxh;
	const double wouts = wout / maxw;

	for(int y=0; y<maxh; y++) {
		const int in_scaled_y = y * hins;
		const int in_scaled_o = in_scaled_y * win * 3;
		const int out_scaled_y = y * houts;
		const int out_scaled_o = out_scaled_y * wout * 3;

		for(int x=0; x<maxw; x++) {
			int ino = in_scaled_o + int(x * wins) * 3;
			int outo = out_scaled_o + int(x * wouts) * 3;

			outo = std::min(max_offset, outo);

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

void filter::apply_io(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	log(LL_FATAL, "filter::apply_io should not be called");
}

void filter::apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	log(LL_FATAL, "filter::apply should not be called");
}
