// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stdio.h>
#include <unistd.h>

#include "error.h"
#include "http_client.h"
#include "source.h"
#include "source_http_jpeg.h"
#include "picio.h"
#include "filter.h"
#include "log.h"
#include "utils.h"

source_http_jpeg::source_http_jpeg(const std::string & urlIn, const bool ignoreCertIn, const std::string & authIn, const int resize_w, const int resize_h, const int ll) : source(resize_w, resize_h, ll), url(urlIn), auth(authIn), ignore_cert(ignoreCertIn)
{
}

source_http_jpeg::~source_http_jpeg()
{
	stop();
}

void source_http_jpeg::operator()()
{
	log(LL_INFO, "source http jpeg thread started");

	set_thread_name("src_h_jpeg");

	bool first = true, resize = resize_h != -1 || resize_w != -1;

	for(;!local_stop_flag;)
	{
		uint8_t *work = NULL;
		size_t work_len = 0;

		if (!http_get(url, ignore_cert, auth.empty() ? NULL : auth.c_str(), loglevel == LL_DEBUG_VERBOSE, &work, &work_len))
		{
			log(LL_INFO, "did not get a frame");
			usleep(101000);
			continue;
		}

		unsigned char *temp = NULL;
		int dw = -1, dh = -1;
		if (first || resize) {
                        if (!read_JPEG_memory(work, work_len, &dw, &dh, &temp)) {
				log(LL_INFO, "JPEG decode error");
				continue;
			}

			if (resize) {
				width = resize_w;
				height = resize_h;
			}
			else {
				width = dw;
				height = dh;
			}

			first = false;
		}

		if (resize)
			set_scaled_frame(temp, dw, dh);
		else
			set_frame(E_JPEG, work, work_len);

		free(temp);

		free(work);
	}

	log(LL_INFO, "source http jpeg thread terminating");
}
