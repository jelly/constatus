// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <atomic>
#include <string>

#include "filter.h"
#include "source.h"
#include "target.h"
#include "selection_mask.h"

typedef void * (*init_motion_trigger_t)(const char *const par);
typedef bool (*detect_motion_t)(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame, const uint8_t *const pixel_selection_bitmap);
typedef void (*uninit_motion_trigger_t)(void *arg);


typedef struct
{
	void *library;

	init_motion_trigger_t init_motion_trigger;
	detect_motion_t detect_motion;
	uninit_motion_trigger_t uninit_motion_trigger;

	std::string par;

	void *arg;
} ext_trigger_t;

class motion_trigger : public interface
{
private:
	source *s;
	int noise_level;
	double percentage_pixels_changed;
	int keep_recording_n_frames;
	int ignore_n_frames_after_recording;
	int camera_warm_up, pre_record_count;
	const std::vector<filter *> *const filters;
	double max_fps;
	selection_mask *const pixel_select_bitmap;
	ext_trigger_t *const et;
	std::vector<target *> *const targets;

public:
	motion_trigger(const std::string & id, source *const s, const int noise_level, const double percentage_pixels_changed, const int keep_recording_n_frames, const int ignore_n_frames_after_recording, const int camera_warm_up, const int pre_record_count, const std::vector<filter *> *const before, std::vector<target *> *const targets, selection_mask *const pixel_select_bitmap, ext_trigger_t *const et, const double max_fps);
	virtual ~motion_trigger();

	void operator()();
};
