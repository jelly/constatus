#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include <mutex>

class interface
{
protected:
	std::mutex pause_lock;

	void pauseCheck();

public:
	interface();
	virtual ~interface();

	virtual void start() = 0;
	void pause();
	void unpause();
	virtual void stop() = 0;
};

#endif
