#include <unistd.h>

#include "target.h"
#include "error.h"

target::target(source *const s, const std::string & store_path, const std::string & prefix, const int max_time, const double interval, std::vector<frame_t> *const pre_record, const std::vector<filter *> *const filters, const char *const exec_start, const char *const exec_cycle, const char *const exec_end) : s(s), store_path(store_path), prefix(prefix), max_time(max_time), interval(interval), pre_record(pre_record), filters(filters), exec_start(exec_start), exec_cycle(exec_cycle), exec_end(exec_end)
{
	th = NULL;
	local_stop_flag = false;
}

target::~target()
{
	stop();
	free_filters(filters);
}

void target::start()
{
	if (th)
		error_exit(false, "Record thread already running");

	local_stop_flag = false;

	th = new std::thread(std::ref(*this));
}

void target::stop()
{
	if (th) {
		local_stop_flag = true;

		th -> join();
		delete th;

		th = NULL;
	}
}
