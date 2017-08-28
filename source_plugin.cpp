// (C) 2017 by folkert van heusden, released under AGPL v3.0
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

source_plugin::source_plugin(const std::string & plugin_filename, const std::string & plugin_arg, const int resize_w, const int resize_h, const int loglevel) : source(resize_w, resize_h, loglevel)
{
	arg = init_plugin(plugin_arg.c_str());
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

	bool first = true, resize = resize_h != -1 || resize_w != -1;
	uint64_t ts = 0;

	for(;!local_stop_flag;)
	{
		if (work_required()) {
			uint8_t *work = NULL;
			size_t work_len = 0;

			get_frame(arg, &ts, &width, &height, &work);
			work_len = width * height * 3;

			unsigned char *temp = NULL;
			int dw = -1, dh = -1;
			if (first || resize) {
				if (!read_JPEG_memory(work, work_len, &dw, &dh, &temp)) {
					log(LL_INFO, "JPEG decode error");
					continue;
				}

				if (resize) {
					width = resize_w;
					height = resize_h;
				}
				else {
					width = dw;
					height = dh;
				}

				first = false;
			}

			if (resize)
				set_scaled_frame(temp, dw, dh);
			else
				set_frame(E_JPEG, work, work_len);

			free(temp);

			free(work);
		}
	}

	log(LL_INFO, "source plugins thread terminating");
}
