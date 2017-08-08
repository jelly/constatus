// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <atomic>
#include <string>
#include <thread>

#include "source.h"

class source_http_jpeg : public source
{
private:
	std::string url, auth;
	bool ignore_cert;
	std::thread *th;

public:
	source_http_jpeg(const std::string & url, const bool ignore_cert, const std::string & auth, const int jpeg_quality, std::atomic_bool *const global_stopflag);
	~source_http_jpeg();

	void operator()();
};
