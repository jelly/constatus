// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stdlib.h>
#include <string.h>

#include "filter.h"

void apply_filters(const std::vector<filter *> *const filters, const uint8_t *const prev, uint8_t *const work, const uint64_t ts, const int w, const int h)
{
	const size_t bytes = w * h * 3;
	uint8_t *const temp = (uint8_t *)malloc(bytes);

	for(filter *f : *filters) {
		f -> apply(ts, w, h, prev, work, temp);

		memcpy(work, temp, bytes);
	}

	free(temp);
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
