// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <vector>

#include "source.h"
#include "filter.h"

void start_http_server(const char *const http_adapter, const int http_port, source *const src, const double fps, const int quality, const int time_limit, const std::vector<filter *> *const f, std::atomic_bool *const global_stopflag, const int resize_w, const int resize_h, pthread_t *th);
