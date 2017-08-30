#ifndef __CFG_H__
#define __CFG_H__

#include <mutex>
#include <vector>

#include "interface.h"

typedef struct
{
	std::vector<interface *> interfaces;

	std::mutex lock;
} configuration_t;

#endif
