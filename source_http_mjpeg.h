#include <condition_variable>
#include <pthread.h>
#include <string>
#include <thread>

#include "source.h"

class source_http_mjpeg : public source
{
private:
	std::string url;
	std::thread *th;

public:
	source_http_mjpeg(const std::string & url, const int jpeg_quality, std::atomic_bool *const global_stopflag);
	virtual ~source_http_mjpeg();

	void operator()();
};
