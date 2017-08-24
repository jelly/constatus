#include "target.h"

class target_avi : public target
{
private:
	const int quality;

public:
	target_avi(source *const s, const std::string & store_path, const std::string & prefix, const int quality, const int max_time, const double interval, std::vector<frame_t> *const pre_record, const std::vector<filter *> *const filters, const char *const exec_start, const char *const exec_cycle, const char *const exec_end);
	virtual ~target_avi();

	void operator()();
};
