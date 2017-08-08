#include <condition_variable>
#include <pthread.h>
#include <string>
#include <thread>

#include "io_buffer.h"
#include "source.h"

class source_http_mjpeg : public source
{
private:
	const std::string hostname, file;
	const int portnr;

	std::thread *th;

	bool retrieve_mjpeg_frame(io_buffer *cur_ib, unsigned char **data, int *data_len, unsigned char **needle, unsigned int *needle_len);

public:
	source_http_mjpeg(const std::string & hostname, const int portnr, const std::string & file, const int jpeg_quality, std::atomic_bool *const global_stopflag);
	virtual ~source_http_mjpeg();

	void operator()();
};
