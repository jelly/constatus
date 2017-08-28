// (C) 2017 by folkert van heusden, this file is released in the public domain
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

void *init_plugin(const char *const argument)
{
	return NULL;
}

void get_frame(void *arg, uint64_t *const ts, int *const w, int *const h, uint8_t **frame_data)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	*ts = (uint64_t)tv.tv_sec * (uint64_t)(1000 * 1000) + (uint64_t)tv.tv_usec;

	*w = 352;
	*h = 288;

	size_t n = *w * *h * 3;
	*frame_data = (uint8_t *)malloc(n);
	memset(*frame_data, 0x10, n / 2);
	memset(&(*frame_data)[n / 2], 0xe0, n / 2);
}

void uninit_plugin(void *arg)
{
}
