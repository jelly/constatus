#ifndef __TARGET_H__
#define __TARGET_H__

#include <string>
#include <thread>
#include <vector>

#include "interface.h"
#include "filter.h"
#include "source.h"

typedef struct
{
	uint64_t ts;
	uint8_t *data;
	size_t len;
	encoding_t e;
} frame_t;

class target : public interface
{
protected:
	source *s;
	const std::string store_path, prefix;
	const int max_time;
	const double interval;
	std::vector<frame_t> *pre_record;
	const std::vector<filter *> *const filters;
	const char *const exec_start, *const exec_cycle, *const exec_end;
	std::atomic_bool local_stop_flag;

	std::thread *th;

public:
	target(source *const s, const std::string & store_path, const std::string & prefix, const int max_time, const double interval, std::vector<frame_t> *const pre_record, const std::vector<filter *> *const filters, const char *const exec_start, const char *const exec_cycle, const char *const exec_end);
	virtual ~target();

	void start();
	void stop();

	virtual void operator()() = 0;
};

#endif
