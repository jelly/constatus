// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <atomic>
#include <string>
#include <thread>

#include "source.h"

typedef void *(* init_plugin_t)(const char *const argument);
typedef void (* get_frame_t)(void *arg, uint64_t *const ts, int *const w, int *const h, uint8_t **frame_data);
typedef void (* uninit_plugin_t)(void *arg);

class source_plugin : public source
{
private:
	init_plugin_t init_plugin;
	get_frame_t get_frame;
	uninit_plugin_t uninit_plugin;
	void *arg;

public:
	source_plugin(const std::string & plugin_filename, const std::string & plugin_arg, const double max_fps, const int resize_w, const int resize_h, const int loglevel);
	~source_plugin();

	void operator()();
};
