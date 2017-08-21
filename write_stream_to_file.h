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
	uint64_t ts;
	uint8_t *data;
	size_t len;
	encoding_t e;
} frame_t;

typedef enum { OF_AVI, OF_JPEG, OF_PLUGIN } o_format_t;

typedef void *(* init_plugin_t)(const char *const argument);
typedef void (* open_file_t)(void *arg, const char *const fname_prefix, const double fps, const int quality);
typedef void (* write_frame_t)(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame);
typedef void (* close_file_t)(void *arg);
typedef void (* uninit_plugin_t)(void *arg);

typedef struct {
	std::string par;

	init_plugin_t init_plugin;
	open_file_t   open_file;
	write_frame_t write_frame;
	close_file_t  close_file;
	uninit_plugin_t uninit_plugin;

	void *arg;
} stream_plugin_t;

void start_store_thread(source *const s, const std::string & store_path, const std::string & prefix, const int quality, const int max_time, const double interval, std::vector<frame_t> *const pre_record, const std::vector<filter *> *const filters, const char *const exec_start, const char *const exec_cycle, const char *const exec_end, std::atomic_bool *const global_stopflag, const o_format_t of, stream_plugin_t *const sp, std::atomic_bool **local_stop_flag, pthread_t *th);

#endif
