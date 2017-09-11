// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include "filter.h"

class filter_apply_mask : public filter
{
private:
	const uint8_t *pixel_select_bitmap;

public:
	filter_apply_mask(const uint8_t *const pixel_select_bitmap);
	~filter_apply_mask();

	bool uses_in_out() const { return false; }
	void apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out);
};
