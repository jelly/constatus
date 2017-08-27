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

	virtual bool uses_in_out() const { return true; }
	virtual void apply_io(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out);
	virtual void apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out);
};

void apply_filters(const std::vector<filter *> *const filters, const uint8_t *const prev, uint8_t *const work, const uint64_t ts, const int w, const int h);

void free_filters(const std::vector<filter *> *filters);

void scale(const uint8_t *in, const int win, const int hin, uint8_t **out, const int wout, const int hout);

#endif
