// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <atomic>
#include <string>
#include <thread>

#include "source.h"

class source_rtsp : public source
{
private:
	const std::string url;

public:
	source_rtsp(const std::string & url, const double max_fps, const int resize_w, const int resize_h, const int loglevel);
	~source_rtsp();

	void operator()();
};
