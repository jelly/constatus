// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <atomic>
#include <string>
#include <thread>

#include "source.h"

class source_rtsp : public source
{
private:
	std::string url;
	std::thread *th;

public:
	source_rtsp(const std::string & url, std::atomic_bool *const global_stopflag);
	~source_rtsp();

	void operator()();
};
