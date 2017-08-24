#include "target.h"

typedef void *(* init_plugin_t)(const char *const argument);
typedef void (* open_file_t)(void *arg, const char *const fname_prefix, const double fps, const int quality);
typedef void (* write_frame_t)(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame);
typedef void (* close_file_t)(void *arg);
typedef void (* uninit_plugin_t)(void *arg);

typedef struct {
	std::string par;

	init_plugin_t init_plugin;
	open_file_t   open_file;
	write_frame_t write_frame;
	close_file_t  close_file;
	uninit_plugin_t uninit_plugin;

	void *arg;
} stream_plugin_t;

class target_plugin : public target
{
private:
	const int quality;
	stream_plugin_t *sp;

public:
	target_plugin(source *const s, const std::string & store_path, const std::string & prefix, const int quality, const int max_time, const double interval, std::vector<frame_t> *const pre_record, const std::vector<filter *> *const filters, const char *const exec_start, const char *const exec_cycle, const char *const exec_end, stream_plugin_t *const sp);
	virtual ~target_plugin();

	void operator()();
};
