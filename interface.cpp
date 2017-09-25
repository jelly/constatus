#include "interface.h"
#include "error.h"

meta * interface::get_meta()
{
	static meta m;

	return &m;
}

interface::interface(const std::string & id) : id(id)
{
	th = NULL;
	local_stop_flag = false;
	ct = CT_NONE;
	paused = false;
}

interface::~interface()
{
}

std::string interface::get_id() const
{
	return id;
}

void interface::pauseCheck()
{
	pause_lock.lock();
	pause_lock.unlock();
}

bool interface::pause()
{
	if (paused)
		return false;


	if (!pause_lock.try_lock_for(std::chrono::milliseconds(1000))) // try 1 second; FIXME hardcoded?
		return false;

	paused = true;

	return true;
}

void interface::unpause()
{
	if (paused) {
		paused = false;
		pause_lock.unlock();
	}
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
