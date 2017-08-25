#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include <atomic>
#include <mutex>
#include <thread>

class interface
{
protected:
	std::mutex pause_lock;
	std::thread *th;
	std::atomic_bool local_stop_flag;

	void pauseCheck();

public:
	interface();
	virtual ~interface();

	void start();
	void pause();
	void unpause();
	void stop();

	virtual void operator()() = 0;
};

#endif
