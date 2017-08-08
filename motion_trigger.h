// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <string>

#include "filter.h"
#include "source.h"

void start_motion_trigger_thread(source *const s, const int quality, const int noise_factor, const double percentage_pixels_changed, const int keep_recording_n_frames, const int ignore_n_frames_after_recording, const std::string & store_path, const std::string & prefix, const int max_file_time, const int camera_warmup, const int pre_record_count, const std::vector<filter *> *before, const std::vector<filter *> *after, const int fps, const char *const exec_start, const char *const exec_cycle, const char *const exec_end, std::atomic_bool *const global_stopflag);
