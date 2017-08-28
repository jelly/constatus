// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <vector>

#include "source.h"
#include "filter.h"

class http_server : public interface
{
private:
	source *const src;
	const double fps;
	const int quality, time_limit;
	const std::vector<filter *> *const f;
	const int resize_w, resize_h;
	const bool motion_compatible;

	int fd;

public:
	http_server(const char *const http_adapter, const int http_port, source *const src, const double fps, const int quality, const int time_limit, const std::vector<filter *> *const f, const int resize_w, const int resize_h, const bool motion_compatible);
	virtual ~http_server();

	void operator()();
};
