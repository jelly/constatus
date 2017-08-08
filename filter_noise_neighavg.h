// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include "filter.h"

class filter_noise_neighavg : public filter
{
public:
	filter_noise_neighavg();
	virtual ~filter_noise_neighavg();

	void apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out);
};
