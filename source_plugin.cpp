// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

#include "error.h"
#include "http_client.h"
#include "source.h"
#include "source_plugin.h"
#include "picio.h"
#include "filter.h"
#include "log.h"
#include "utils.h"

source_plugin::source_plugin(const std::string & id, const std::string & plugin_filename, const std::string & plugin_arg, const double max_fps, const int resize_w, const int resize_h, const int loglevel) : source(id, max_fps, resize_w, resize_h, loglevel)
{
	void *library = dlopen(plugin_filename.c_str(), RTLD_NOW);
	if (!library)
		error_exit(true, "Failed opening motion detection plugin library %s", plugin_filename.c_str());

	init_plugin = (init_plugin_t)find_symbol(library, "init_plugin", "video source plugin", plugin_filename.c_str());
	get_frame = (get_frame_t)find_symbol(library, "get_frame", "video source plugin", plugin_filename.c_str());
	uninit_plugin = (uninit_plugin_t)find_symbol(library, "uninit_plugin", "video source plugin", plugin_filename.c_str());

	arg = init_plugin(plugin_arg.c_str());

	d = plugin_filename + " / " + plugin_arg;
}

source_plugin::~source_plugin()
{
	stop();

	uninit_plugin(arg);
}

void source_plugin::operator()()
{
	log(LL_INFO, "source plugins thread started");

	set_thread_name("src_plugins");

	bool resize = resize_h != -1 || resize_w != -1;
	uint64_t ts = 0;

	const uint64_t interval = max_fps > 0.0 ? 1.0 / max_fps * 1000.0 * 1000.0 : 0;

	for(;!local_stop_flag;)
	{
		time_t start_ts = get_us();

		if (work_required()) {
			uint8_t *work = NULL;
			size_t work_len = 0;

			get_frame(arg, &ts, &width, &height, &work);
			work_len = width * height * 3;

			if (resize)
				set_scaled_frame(work, resize_w, resize_h);
			else
				set_frame(E_RGB, work, work_len);

			free(work);
		}

		uint64_t end_ts = get_us();
		int64_t left = interval - (end_ts - start_ts);

		if (interval > 0 && left > 0)
			usleep(left);
	}

	log(LL_INFO, "source plugins thread terminating");
}
