// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <string>

#include "filter.h"
#include "source.h"
#include "write_stream_to_file.h"

void start_motion_trigger_thread(source *const s, const int quality, const int noise_factor, const double percentage_pixels_changed, const int keep_recording_n_frames, const int ignore_n_frames_after_recording, const std::string & store_path, const std::string & prefix, const int max_file_time, const int camera_warm_up, const int pre_record_count, const std::vector<filter *> *const before, const std::vector<filter *> *const after, const int fps, const char *const exec_start, const char *const exec_cycle, const char *const exec_end, const o_format_t of, std::atomic_bool *const global_stopflag, const bool *pixel_select_bitmap, pthread_t *th);
