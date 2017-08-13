// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <string>

#include "filter.h"
#include "source.h"
#include "write_stream_to_file.h"

typedef void * (*init_motion_trigger_t)(const char *const par);
typedef bool (*detect_motion_t)(const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame);


typedef struct
{
	void *library;

	init_motion_trigger_t init_motion_trigger;
	detect_motion_t detect_motion;

	const char *par;
} ext_trigger_t;

void start_motion_trigger_thread(source *const s, const int quality, const int noise_factor, const double percentage_pixels_changed, const int keep_recording_n_frames, const int ignore_n_frames_after_recording, const std::string & store_path, const std::string & prefix, const int max_file_time, const int camera_warm_up, const int pre_record_count, const std::vector<filter *> *const before, const std::vector<filter *> *const after, const int fps, const char *const exec_start, const char *const exec_cycle, const char *const exec_end, const o_format_t of, std::atomic_bool *const global_stopflag, const bool *pixel_select_bitmap, const ext_trigger_t *const et, pthread_t *th);
