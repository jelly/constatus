#include "target.h"

class target_jpeg : public target
{
private:
	const int quality;

public:
	target_jpeg(source *const s, const std::string & store_path, const std::string & prefix, const int quality, const int max_time, const double interval, const std::vector<filter *> *const filters, const char *const exec_start, const char *const exec_cycle, const char *const exec_end);
	virtual ~target_jpeg();

	void operator()();
};
