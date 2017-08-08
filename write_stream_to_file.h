// (C) 2017 by folkert van heusden, released under AGPL v3.0
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

std::atomic_bool * start_store_thread(source *const s, const std::string & store_path, const std::string & prefix, const int quality, const int max_time, const double interval, std::vector<frame_t> *const pre_record, const std::vector<filter *> *const filters, const char *const exec_start, const char *const exec_cycle, const char *const exec_end, std::atomic_bool *const global_stopflag);
