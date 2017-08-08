// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stdio.h>
#include <unistd.h>

#include "error.h"
#include "http_client.h"
#include "source.h"
#include "source_http_jpeg.h"
#include "picio.h"

source_http_jpeg::source_http_jpeg(const std::string & urlIn, const bool ignoreCertIn, const std::string & authIn, const int jpeg_quality, std::atomic_bool *const global_stopflag) : source(jpeg_quality, global_stopflag), url(urlIn), auth(authIn), ignore_cert(ignoreCertIn)
{
	th = new std::thread(std::ref(*this));
}

source_http_jpeg::~source_http_jpeg()
{
}

void source_http_jpeg::operator()()
{
	bool first = true;

	for(;!*global_stopflag;)
	{
		uint8_t *work = NULL;
		size_t work_len = 0;

		if (!http_get(url, ignore_cert, auth.empty() ? NULL : auth.c_str(), &work, &work_len))
		{
			printf("did not get a frame\n");
			usleep(101000);
			continue;
		}

		if (first) {
                        unsigned char *temp = NULL;
                        read_JPEG_memory(work, work_len, &width, &height, &temp);
			free(temp);

			first = false;
		}

		set_frame(E_JPEG, work, work_len);

		free(work);
	}
}
