#include "interface.h"

void interface::pauseCheck()
{
	pause_lock.lock();
	pause_lock.unlock();
}

interface::interface()
{
}

interface::~interface()
{
}

void interface::pause()
{
	pause_lock.lock();
}

void interface::unpause()
{
	pause_lock.unlock();
}
