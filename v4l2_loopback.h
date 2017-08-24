#include <string>
#include <thread>
#include <vector>

#include "interface.h"
#include "filter.h"
#include "source.h"

class v4l2_loopback : public interface
{
private:
	source *const s;
	const double fps;
	const std::string dev;
	const std::vector<filter *> *const filters;

	std::atomic_bool local_stop_flag;
	std::thread *th;

public:
	v4l2_loopback(source *const s, const double fps, const std::string & dev, const std::vector<filter *> *const filters);
	virtual ~v4l2_loopback();

	void start();
	void stop();

	void operator()();
};
