// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <string>

#include "source.h"

void start_p2vl_thread(source *const s, const double fps, const std::string & dev, const std::vector<filter *> *const filters, std::atomic_bool *const global_stopflag, pthread_t *th);
