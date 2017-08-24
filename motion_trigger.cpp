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

typedef struct
{
	source *s;
	int noise_level;
	double percentage_changed;
	int stop_after_frame_count;
	int mute_after_record_frame_count;
	int quality;
	int camera_warm_up, pre_record_count;
	const std::vector<filter *> *const before;
	int fps;
	const uint8_t *pixel_select_bitmap;
	ext_trigger_t *const et;
	std::atomic_bool *const global_stopflag;
	target *t;
} mt_pars_t;

void * motion_trigger_thread(void *pin)
{
	mt_pars_t *p = (mt_pars_t *)pin;

	set_thread_name("motion_trigger");

	if (p -> et)
		p -> et -> arg = p -> et -> init_motion_trigger(p -> et -> par);

	int w = -1, h = -1;
	uint64_t prev_ts = 0;

	uint8_t *prev_frame = NULL;
	bool motion = false;
	std::atomic_bool *stop_flag = NULL;
	int stopping = 0, mute = 0;

	std::vector<frame_t> prerecord;

	if (p -> camera_warm_up)
		log(LL_INFO, "Warming up...");

	for(int i=0; i<p -> camera_warm_up && !*p -> global_stopflag; i++) {
		log(LL_DEBUG, "Warm-up... %d", i);

		uint8_t *frame = NULL;
		size_t frame_len = 0;
		// FIXME E_NONE ofzo of kijken wat de preferred is
		p -> s -> get_frame(E_RGB, p -> quality, &prev_ts, &w, &h, &frame, &frame_len);
		free(frame);
	}

	log(LL_INFO, "Go!");

	for(;!*p -> global_stopflag;) {
		uint8_t *work = NULL;
		size_t work_len = 0;
		p -> s -> get_frame(E_RGB, p -> quality, &prev_ts, &w, &h, &work, &work_len);
		if (work == NULL || work_len == 0)
			continue;

		apply_filters(p -> before, prev_frame, work, prev_ts, w, h);

		if (prev_frame) {
			int cnt = 0;
			bool triggered = false;

			if (p -> et) {
				triggered = p -> et -> detect_motion(p -> et -> arg, prev_ts, w, h, prev_frame, work, p -> pixel_select_bitmap);
			}
			else if (p -> pixel_select_bitmap) {
				const uint8_t *pw = work, *pp = prev_frame;
				for(int i=0; i<w*h; i++) {
					if (!p -> pixel_select_bitmap[i])
						continue;

					int lc = *pw++;
					lc += *pw++;
					lc += *pw++;
					lc /= 3;

					int lp = *pp++;
					lp += *pp++;
					lp += *pp++;
					lp /= 3;

					cnt += abs(lc - lp) >= p -> noise_level;
				}

				triggered = cnt > (p -> percentage_changed / 100) * w * h;
			}
			else {
				const uint8_t *pw = work, *pp = prev_frame;
				for(int i=0; i<w*h; i++) {
					int lc = *pw++;
					lc += *pw++;
					lc += *pw++;
					lc /= 3;

					int lp = *pp++;
					lp += *pp++;
					lp += *pp++;
					lp /= 3;

					cnt += abs(lc - lp) >= p -> noise_level;
				}

				triggered = cnt > (p -> percentage_changed / 100) * w * h;
			}

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

					p -> t -> start();
					motion = true;
				}

				stopping = 0;
			}
			else if (stop_flag) {
				if (motion) {
					log(LL_DEBUG, "stop motion");
					stopping++;

					if (stopping > p -> stop_after_frame_count) {
						log(LL_INFO, " stopping");

						p -> t -> stop();

						motion = false;
						mute = p -> mute_after_record_frame_count;
					}
				}
			}
		}

		if (prerecord.size() >= p -> pre_record_count) {
			free(prerecord.at(0).data);
			prerecord.erase(prerecord.begin() + 0);
		}

		prerecord.push_back({ prev_ts, work, work_len, E_RGB });

		prev_frame = work;
	}

	while(!prerecord.empty()) {
		free(prerecord.at(0).data);
		prerecord.erase(prerecord.begin() + 0);
	}

	if (p -> et)
		p -> et -> uninit_motion_trigger(p -> et -> arg);

	free((void *)p -> pixel_select_bitmap);

	return NULL;
}

void start_motion_trigger_thread(source *const s, const int quality, const int noise_factor, const double percentage_pixels_changed, const int keep_recording_n_frames, const int ignore_n_frames_after_recording, const int camera_warm_up, const int pre_record_count, const std::vector<filter *> *const before, const int fps, target *const t, std::atomic_bool *const global_stopflag, const uint8_t *pixel_select_bitmap, ext_trigger_t *const et, pthread_t *th)
{
	// FIXME static
	static mt_pars_t p = { s, noise_factor, percentage_pixels_changed, keep_recording_n_frames, ignore_n_frames_after_recording, quality, camera_warm_up, pre_record_count, before, fps, pixel_select_bitmap, et, global_stopflag, t };

	int rc = -1;
	if ((rc = pthread_create(th, NULL, motion_trigger_thread, &p)) != 0)
	{
		errno = rc;
		error_exit(true, "pthread_create failed (motion trigger)");
	}
}
