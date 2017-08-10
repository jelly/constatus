// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <atomic>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "error.h"
#include "source.h"
#include "utils.h"
#include "filter.h"

typedef struct
{
	source *s;
	double fps;
	std::string dev;
	const std::vector<filter *> *filters;
	std::atomic_bool *global_stopflag;
} p2vl_thread_pars_t;

void *p2vl_thread(void *pin)
{
	p2vl_thread_pars_t *p = (p2vl_thread_pars_t *)pin;

	set_thread_name("p2vl");

	int v4l2sink = open(p -> dev.c_str(), O_WRONLY);
	if (v4l2sink == -1)
        	error_exit(true, "Failed to open v4l2sink device (%s)", p -> dev.c_str());

	// setup video for proper format
	struct v4l2_format v;
	v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	if (ioctl(v4l2sink, VIDIOC_G_FMT, &v))
		error_exit(true, "VIDIOC_G_FMT failed");

	uint8_t *prev_frame = NULL;

	uint64_t prev_ts = 0;
	bool first = true;

	for(;!*p -> global_stopflag;) {
		int w = -1, h = -1;
		uint8_t *work = NULL;
		size_t work_len = 0;
		p -> s -> get_frame(E_RGB, -1, &prev_ts, &w, &h, &work, &work_len);

		if (work == NULL || work_len == 0) {
			printf("did not get a frame\n");
			continue;
		}

		if (first) {
			first = false;

			v.fmt.pix.width = w;
			v.fmt.pix.height = h;
			v.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
			v.fmt.pix.sizeimage = w * h * 3;
			if (ioctl(v4l2sink, VIDIOC_S_FMT, &v) == -1)
				error_exit(true, "VIDIOC_S_FMT failed");
		}

		apply_filters(p -> filters, prev_frame, work, prev_ts, w, h);

		if (write(v4l2sink, work, work_len) == -1)
			error_exit(true, "write to video loopback failed");

		free(prev_frame);
		prev_frame = work;

		if (p -> fps > 0) {
			double us = 1000000.0 / p -> fps;
			if (us)
				usleep((useconds_t)us);
		}
	}

	free(prev_frame);

	free_filters(p -> filters);
	delete p;

	return NULL;
}

void start_p2vl_thread(source *const s, const double fps, const std::string & dev, const std::vector<filter *> *const filters, std::atomic_bool *const global_stopflag, pthread_t *th)
{
	p2vl_thread_pars_t *p = new p2vl_thread_pars_t;

	p -> s = s;
	p -> fps = fps;
	p -> dev = dev;
	p -> filters = filters;
	p -> global_stopflag = global_stopflag;

	pthread_attr_t tattr;
	pthread_attr_init(&tattr);
	pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);

	int rc = -1;
	if ((rc = pthread_create(th, &tattr, p2vl_thread, p)) != 0)
	{
		errno = rc;
		error_exit(true, "pthread_create failed (vloopback thread)");
	}
}
