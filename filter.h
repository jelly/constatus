// (C) 2017 by folkert van heusden, released under AGPL v3.0
#ifndef __FILTER_H__
#define __FILTER_H__

#include <stdint.h>
#include <vector>

// NULL filter
class filter
{
public:
	filter();
	virtual ~filter();

	virtual void apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out);
};

void apply_filters(const std::vector<filter *> *const filters, const uint8_t *const prev, uint8_t *const work, const uint64_t ts, const int w, const int h);

#endif
