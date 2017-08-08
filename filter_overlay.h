// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include "filter.h"

class filter_overlay : public filter
{
	int w, h;
	uint8_t *pixels;

public:
	filter_overlay(const std::string & file);
	~filter_overlay();

	void apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out);
};
