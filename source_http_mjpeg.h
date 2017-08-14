// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <condition_variable>
#include <pthread.h>
#include <string>
#include <thread>

#include "source.h"

class source_http_mjpeg : public source
{
private:
	const std::string url;
	const bool ignore_cert, verbose;
	std::thread *th;

public:
	source_http_mjpeg(const std::string & url, const bool ignore_cert, std::atomic_bool *const global_stopflag, const int resize_w, const int resize_h, const bool verbose);
	virtual ~source_http_mjpeg();

	void operator()();
};
