// (C) 2017 by folkert van heusden, released under AGPL v3.0
#ifndef __FRAME_H__
#define __FRAME_H__

#include <pthread.h>
#include <stdint.h>

class frame
{
private:
	pthread_mutex_t lock;
	pthread_cond_t cond;
	uint64_t ts;
	uint8_t *data;
	size_t s;

public:
	frame();
	~frame();

	void set_frame(const uint8_t *const data, const size_t size);

	void wait_for_update(const uint64_t newer_than);

	uint64_t get_ts();
	void get_frame_data(uint8_t **const data, size_t *const size);
};

#endif
