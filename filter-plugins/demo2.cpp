// (C) 2017 by folkert van heusden, this file is released in the public domain
#include <algorithm>
#include <stdint.h>
#include <string.h>

extern "C" {

void init_filter(const char *const parameter)
{
	// you can use the parameter for anything you want
	// e.g. the filename of a configuration file or
	// maybe a variable or whatever
}

void apply_filter(const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame, uint8_t *const result)
{
	const size_t bytes = w * h * 3;

	memcpy(result, current_frame, bytes);

	for(size_t i=0; i<bytes; i += 3)
		std::swap(result[i + 0], result[i + 2]);
}

}
