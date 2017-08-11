// (C) 2017 by folkert van heusden, released under AGPL v3.0
#ifndef __write_stream_to_file__
#define __write_stream_to_file__

#include <atomic>
#include <stdint.h>
#include <string>
#include <vector>

#include "source.h"
#include "filter.h"

typedef struct
{
	uint8_t *data;
	size_t len;
	encoding_t e;
} frame_t;

typedef enum { OF_AVI, OF_JPEG } o_format_t;

void start_store_thread(source *const s, const std::string & store_path, const std::string & prefix, const int quality, const int max_time, const double interval, std::vector<frame_t> *const pre_record, const std::vector<filter *> *const filters, const char *const exec_start, const char *const exec_cycle, const char *const exec_end, std::atomic_bool *const global_stopflag, const o_format_t of, std::atomic_bool **local_stop_flag, pthread_t *th);

#endif
