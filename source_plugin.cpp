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

source_plugin::source_plugin(const std::string & id, const std::string & plugin_filename, const std::string & plugin_arg, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel) : source(id, max_fps, r, resize_w, resize_h, loglevel)
{
	library = dlopen(plugin_filename.c_str(), RTLD_NOW);
	if (!library)
		error_exit(true, "Failed opening source plugin library %s: %s", plugin_filename.c_str(), dlerror());

	init_plugin = (sp_init_plugin_t)find_symbol(library, "init_plugin", "video source plugin", plugin_filename.c_str());
	uninit_plugin = (sp_uninit_plugin_t)find_symbol(library, "uninit_plugin", "video source plugin", plugin_filename.c_str());

	arg = init_plugin(this, plugin_arg.c_str());

	d = plugin_filename + " / " + plugin_arg;
}

source_plugin::~source_plugin()
{
	stop();

	uninit_plugin(arg);

	dlclose(library);
}

void source_plugin::operator()()
{
	log(LL_INFO, "source plugins thread started");

	set_thread_name("src_plugins");

	// all work is performed in the plugin itself
	for(;!local_stop_flag;)
		usleep(101000);

	log(LL_INFO, "source plugins thread terminating");
}
