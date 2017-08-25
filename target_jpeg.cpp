#include <unistd.h>

#include "target_jpeg.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"

target_jpeg::target_jpeg(source *const s, const std::string & store_path, const std::string & prefix, const int quality, const int max_time, const double interval, const std::vector<filter *> *const filters, const char *const exec_start, const char *const exec_cycle, const char *const exec_end) : target(s, store_path, prefix, max_time, interval, filters, exec_start, exec_cycle, exec_end), quality(quality)
{
}

target_jpeg::~target_jpeg()
{
	stop();
}

void target_jpeg::operator()()
{
	set_thread_name("storej_" + prefix);

	uint64_t prev_ts = 0;
	bool is_start = true;
	std::string name;

	uint8_t *prev_frame = NULL;

	int f_nr = 0;

	// FIXME pre-recorded frames

	for(;!local_stop_flag;) {
		pauseCheck();

		int w = -1, h = -1;
		uint8_t *work = NULL;
		size_t work_len = 0;
		if (!s -> get_frame(filters -> empty() ? E_JPEG : E_RGB, quality, &prev_ts, &w, &h, &work, &work_len))
			continue;

		if (work == NULL || work_len == 0) {
			log(LL_INFO, "did not get a frame");
			continue;
		}

		struct timeval tv;
		gettimeofday(&tv, NULL);
		struct tm tm;
		localtime_r(&tv.tv_sec, &tm);

		name = myformat("%s/%s%04d-%02d-%02d_%02d:%02d:%02d.%03d-%d.jpg",
				store_path.c_str(), prefix.c_str(),
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec,
				tv.tv_usec / 1000,
				f_nr++);

		if (exec_start && is_start) {
			exec(exec_start, name);
			is_start = false;
		}

		log(LL_DEBUG_VERBOSE, "Write frame to %s", name.c_str());
		FILE *fh = fopen(name.c_str(), "wb");
		if (!fh)
			error_exit(true, "Cannot create file %s", name.c_str());

		if (filters -> empty())
			fwrite(work, work_len, 1, fh);
		else {
			apply_filters(filters, prev_frame, work, prev_ts, w, h);

			char *data = NULL;
			size_t data_size = 0;
			write_JPEG_memory(w, h, quality, work, &data, &data_size);
			fwrite(data, data_size, 1, fh);
			free(data);
		}

		fclose(fh);

		free(prev_frame);
		prev_frame = work;

		mysleep(interval, &local_stop_flag);
	}

	free(prev_frame);

	if (exec_end)
		exec(exec_end, name);
}
