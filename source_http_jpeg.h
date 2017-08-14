// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <atomic>
#include <string>
#include <thread>

#include "source.h"

class source_http_jpeg : public source
{
private:
	const std::string url, auth;
	const bool ignore_cert, verbose;
	std::thread *th;

public:
	source_http_jpeg(const std::string & url, const bool ignore_cert, const std::string & auth, std::atomic_bool *const global_stopflag, const int resize_w, const int resize_h, const bool verbose);
	~source_http_jpeg();

	void operator()();
};
