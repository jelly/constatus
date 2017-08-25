#include "interface.h"
#include "error.h"

void interface::pauseCheck()
{
	pause_lock.lock();
	pause_lock.unlock();
}

interface::interface()
{
	th = NULL;
	local_stop_flag = false;
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

void interface::start()
{
	if (th)
		error_exit(false, "thread already running");

	local_stop_flag = false;

	th = new std::thread(std::ref(*this));
}

void interface::stop()
{
	if (th) {
		local_stop_flag = true;

		th -> join();
		delete th;

		th = NULL;
	}
}
