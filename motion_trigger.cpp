// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <atomic>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <sys/time.h>

#include "error.h"
#include "source.h"
#include "utils.h"
#include "picio.h"
#include "target.h"
#include "filter.h"
#include "log.h"
#include "motion_trigger.h"

motion_trigger::motion_trigger(const std::string & id, source *const s, const int noise_level, const double percentage_pixels_changed, const int keep_recording_n_frames, const int ignore_n_frames_after_recording, const int camera_warm_up, const int pre_record_count, const std::vector<filter *> *const filters, std::vector<target *> *const targets, const uint8_t *pixel_select_bitmap, ext_trigger_t *const et, const double max_fps) : interface(id), s(s), noise_level(noise_level), percentage_pixels_changed(percentage_pixels_changed), keep_recording_n_frames(keep_recording_n_frames), ignore_n_frames_after_recording(ignore_n_frames_after_recording), camera_warm_up(camera_warm_up), pre_record_count(pre_record_count), filters(filters), targets(targets), pixel_select_bitmap(pixel_select_bitmap), et(et), max_fps(max_fps)
{
	if (et)
		et -> arg = et -> init_motion_trigger(et -> par.c_str());

	local_stop_flag = false;
	th = NULL;
	ct = CT_MOTIONTRIGGER;
}

motion_trigger::~motion_trigger()
{
	stop();

	if (et)
		et -> uninit_motion_trigger(et -> arg);

	for(target *t : *targets)
		delete t;
	delete targets;

	free((void *)pixel_select_bitmap);

	free_filters(filters);
	delete filters;
}

void motion_trigger::operator()()
{
	set_thread_name("motion_trigger");

	int w = -1, h = -1;
	uint64_t prev_ts = 0;

	uint8_t *prev_frame = NULL;
	bool motion = false;
	int stopping = 0, mute = 0;

	std::vector<frame_t> prerecord;

	s -> register_user();

	if (camera_warm_up)
		log(LL_INFO, "Warming up...");

	for(int i=0; i<camera_warm_up && !local_stop_flag; i++) {
		log(LL_DEBUG, "Warm-up... %d", i);

		uint8_t *frame = NULL;
		size_t frame_len = 0;
		// FIXME E_NONE ofzo of kijken wat de preferred is
		s -> get_frame(E_RGB, -1, &prev_ts, &w, &h, &frame, &frame_len);
		free(frame);
	}

	log(LL_INFO, "Go!");

	const int nl3 = noise_level * 3;

	for(;!local_stop_flag;) {
		pauseCheck();

		uint8_t *work = NULL;
		size_t work_len = 0;
		if (!s -> get_frame(E_RGB, -1, &prev_ts, &w, &h, &work, &work_len)) {
			free(work);
			continue;
		}

		apply_filters(filters, prev_frame, work, prev_ts, w, h);

		if (prev_frame) {
			int cnt = 0;
			bool triggered = false;

			if (et) {
				triggered = et -> detect_motion(et -> arg, prev_ts, w, h, prev_frame, work, pixel_select_bitmap);
			}
			else if (pixel_select_bitmap) {
				const uint8_t *pw = work, *pp = prev_frame;
				for(int i=0; i<w*h; i++) {
					if (!pixel_select_bitmap[i])
						continue;

					int lc = *pw++;
					lc += *pw++;
					lc += *pw++;

					int lp = *pp++;
					lp += *pp++;
					lp += *pp++;

					cnt += abs(lc - lp) >= nl3;
				}

				triggered = cnt > (percentage_pixels_changed / 100) * w * h;
			}
			else {
				const uint8_t *pw = work, *pp = prev_frame;
				for(int i=0; i<w*h; i++) {
					int lc = *pw++;
					lc += *pw++;
					lc += *pw++;

					int lp = *pp++;
					lp += *pp++;
					lp += *pp++;

					cnt += abs(lc - lp) >= nl3;
				}

				triggered = cnt > (percentage_pixels_changed / 100) * w * h;
			}

			get_meta() -> setDouble("$pixels-changed$", std::pair<uint64_t, double>(prev_ts, cnt * 100.0 / (w * h)));

			if (mute) {
				log(LL_DEBUG, "mute");
				mute--;
			}
			else if (triggered) {
				log(LL_INFO, "motion detected (%f%% of the pixels changed)", cnt * 100.0 / (w * h));

				if (!motion) {
					log(LL_DEBUG, " starting store");

					std::vector<frame_t> *pr = new std::vector<frame_t>(prerecord);
					prerecord.clear();

					for(size_t i=0; i<targets -> size(); i++)
						targets -> at(i) -> start(i == 0 ? pr : NULL); // FIXME for all targets

					if (targets -> empty())
						delete pr;

					motion = true;
				}

				stopping = 0;
			}
			else if (motion) {
				log(LL_DEBUG, "stop motion");
				stopping++;

				if (stopping > keep_recording_n_frames) {
					log(LL_INFO, " stopping");

					for(target *t : *targets)
						t -> stop();

					motion = false;
					mute = ignore_n_frames_after_recording;
				}
			}
		}

		if (pre_record_count > 0) {
			if (prerecord.size() >= pre_record_count) {
				free(prerecord.at(0).data);
				prerecord.erase(prerecord.begin() + 0);
			}

			prerecord.push_back({ prev_ts, w, h, work, work_len, E_RGB });
		}
		else {
			free(prev_frame);
		}

		prev_frame = work;

		if (max_fps > 0.0)
			mysleep(1.0 / max_fps, &local_stop_flag, s);
	}

	while(!prerecord.empty()) {
		free(prerecord.at(0).data);
		prerecord.erase(prerecord.begin() + 0);
	}

	if (pre_record_count <= 0)
		free(prev_frame);

	s -> unregister_user();
}
