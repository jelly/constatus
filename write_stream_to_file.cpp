// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <atomic>
#include <stdlib.h>
extern "C" {
#include <gwavi.h>
}
#include <string>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <sys/time.h>

#include "error.h"
#include "source.h"
#include "picio.h"
#include "utils.h"
#include "filter.h"
#include "exec.h"
#include "write_stream_to_file.h"

typedef struct
{
	std::string path, prefix;
	source *s;
	std::atomic_bool stop_flag;
	int quality, max_time;
	double interval;
	std::vector<frame_t> *pre_record;
	const std::vector<filter *> *filters;
	const char *exec_start, *exec_cycle, *exec_end;
	std::atomic_bool *global_stopflag;
} store_thread_pars_t;

void *store_thread(void *pin)
{
	store_thread_pars_t *p = (store_thread_pars_t *)pin;

	set_thread_name("store_" + p -> prefix);

	time_t cut_ts = time(NULL) + p -> max_time;

	uint64_t prev_ts = 0;
	bool is_start = true;
	std::string name;

	uint8_t *prev_frame = NULL;

	struct gwavi_t *gwavi = NULL;

	for(;!p -> stop_flag && !*p -> global_stopflag;) {
		int w = -1, h = -1;
		uint8_t *work = NULL;
		size_t work_len = 0;
		p -> s -> get_frame(p -> filters -> empty() ? E_JPEG : E_RGB, p -> quality, &prev_ts, &w, &h, &work, &work_len);

		if (work == NULL || work_len == 0) {
			printf("did not get a frame\n");
			continue;
		}

		if (p -> max_time > 0 && time(NULL) >= cut_ts) {
			printf("new file\n");

			if (gwavi)
				gwavi_close(gwavi);
			gwavi = NULL;

			cut_ts = time(NULL) + p -> max_time;
		}

		if (!gwavi) {
			struct timeval tv;
			gettimeofday(&tv, NULL);
			struct tm tm;
			localtime_r(&tv.tv_sec, &tm);

			name = myformat("%s/%s%04d-%02d-%02d_%02d:%02d:%02d.%03d.avi",
					p -> path.c_str(), p -> prefix.c_str(),
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
					tm.tm_hour, tm.tm_min, tm.tm_sec,
					tv.tv_usec / 1000);

			if (p -> exec_start && is_start) {
				exec(p -> exec_start, name);
				is_start = false;
			}
			else if (p -> exec_cycle) {
				exec(p -> exec_cycle, name);
			}

			int fps = p -> interval <= 0 ? 25 : std::max(1, int(1 / p -> interval));

			gwavi = gwavi_open((char *)name.c_str(), w, h, (char *)"MJPG", fps, NULL);
			if (!gwavi)
				error_exit(true, "Cannot create %s", name.c_str());

			if (p -> pre_record) {
				for(frame_t pair : *p -> pre_record) {
					if (pair.e == E_JPEG)
						gwavi_add_frame(gwavi, pair.data, pair.len);
					else {
						char *data = NULL;
						size_t data_size = 0;
						write_JPEG_memory(w, h, p -> quality, pair.data, &data, &data_size);

						gwavi_add_frame(gwavi, (unsigned char *)data, data_size);

						free(data);
					}

					free(pair.data);
				}

				delete p -> pre_record;
				p -> pre_record = NULL;
			}
		}

		if (p -> filters -> empty())
			gwavi_add_frame(gwavi, work, work_len);
		else {
			apply_filters(p -> filters, prev_frame, work, prev_ts, w, h);

			char *data = NULL;
			size_t data_size = 0;
			write_JPEG_memory(w, h, p -> quality, work, &data, &data_size);
			gwavi_add_frame(gwavi, (unsigned char *)data, data_size);
			free(data);
		}

		free(prev_frame);
		prev_frame = work;

		double slp = p -> interval;
		while(slp > 0 && !*p -> global_stopflag) {
			double cur = std::min(slp, 0.1);
			slp -= cur;

			int s = cur;

			uint64_t us = (cur - s) * 1000 * 1000;

			// printf("%d, %lu\n", s, us);

			if (s)
				sleep(s); // FIXME handle signals

			if (us)
				usleep(us); // FIXME handle signals
		}
	}

	if (gwavi)
		gwavi_close(gwavi);

	free(prev_frame);

	if (p -> exec_end)
		exec(p -> exec_end, name);

	delete p;

	return NULL;
}

void start_store_thread(source *const s, const std::string & store_path, const std::string & prefix, const int quality, const int max_time, const double interval, std::vector<frame_t> *const pre_record, const std::vector<filter *> *const filters, const char *const exec_start, const char *const exec_cycle, const char *const exec_end, std::atomic_bool *const global_stopflag, std::atomic_bool **local_stop_flag, pthread_t *th)
{
	store_thread_pars_t *p = new store_thread_pars_t;

	p -> path = store_path;
	p -> prefix = prefix;
	p -> s = s;
	p -> quality = quality;
	p -> max_time = max_time;
	p -> stop_flag = false;
	p -> interval = interval;
	p -> pre_record = pre_record;
	p -> filters = filters;
	p -> exec_start = exec_start;
	p -> exec_cycle = exec_cycle;
	p -> exec_end = exec_end;
	p -> global_stopflag = global_stopflag;

	int rc = -1;
	if ((rc = pthread_create(th, NULL, store_thread, p)) != 0)
	{
		errno = rc;
		error_exit(true, "pthread_create failed (store thread)");
	}

	*local_stop_flag = &p -> stop_flag;
}
