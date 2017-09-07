#ifndef __RESIZE_H__
#define __RESIZE_H__

#include <stdint.h>
#include <string>

class resize
{
public:
	resize();
	virtual ~resize();

	void do_resize(const int win, const int hin, const uint8_t *const in, const int wout, const int hout, uint8_t **out);
};

#endif
