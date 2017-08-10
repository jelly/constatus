#include <assert.h>
#include <string>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

#include "utils.h"
#include "source.h"
#include "source_http_mjpeg.h"
#include "picio.h"
#include "frame.h"

typedef struct
{
	std::atomic_bool *global_stopflag;
	source *s;
	bool first;
	bool header;
	uint8_t *data;
	size_t n;
	size_t req_len;
} work_data_t;

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *mypt)
{
	work_data_t *w = (work_data_t *)mypt;
	const size_t full_size = size * nmemb;

	if (*w -> global_stopflag)
		return 0;

	w -> data = (uint8_t *)realloc(w -> data, w -> n + full_size + 1);
	memcpy(&w -> data[w -> n], ptr, full_size);
	w -> n += full_size;
	w -> data[w -> n] = 0x00;

	if (w -> header) {
		char *header_end = strstr((char *)w -> data, "\r\n\r\n");
		if (!header_end)
			return full_size;

		*header_end = 0x0;

		char *cl = strstr((char *)w -> data, "Content-Length:");
		if (!cl)
			printf("Content-Length missing from header\n");

		w -> header = false;

		size_t left = w -> n - (strlen((char *)w -> data) + 4);
		if (left) {
			printf("LEFT %zu\n", left);
			memmove(w -> data, header_end + 4, left);
		}
		w -> n = left;

		w -> req_len = atoi(&cl[16]);
		//printf("needed len: %zu\n", w -> req_len);
	}
	else if (w -> n == w -> req_len) {
		//printf("frame! (%p %zu/%zu)\n", w -> data, w -> n, w -> req_len);

		if (w -> first) {
			int width = -1, height = -1;
			unsigned char *temp = NULL;
			read_JPEG_memory(w -> data, w -> req_len, &width, &height, &temp);
			free(temp);

			w -> first = false;
			printf("%dx%d\n", width, height);

			w -> s -> set_size(width, height);
		}

		w -> s -> set_frame(E_JPEG, w -> data, w -> req_len);

		size_t left = w -> n - w -> req_len;
		if (left) {
			printf("LEFT %zu\n", left);
			memmove(w -> data, &w -> data[w -> req_len], left);
		}
		w -> n = left;

		w -> header = true;
	}

	return full_size;
}


source_http_mjpeg::source_http_mjpeg(const std::string & urlIn, const int jpeg_quality, std::atomic_bool *const global_stopflag) : source(jpeg_quality, global_stopflag), url(urlIn)
{
	th = new std::thread(std::ref(*this));
}

source_http_mjpeg::~source_http_mjpeg()
{
	th -> join();
	delete th;
}

void source_http_mjpeg::operator()()
{
	for(;!*global_stopflag;)
	{
		/* init the curl session */ 
		CURL *curl_handle = curl_easy_init();

		/* set URL to get here */ 
		curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());

		curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);

		curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);

		/* Switch on full protocol/debug output while testing */ 
		curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);

		/* disable progress meter, set to 0L to enable and disable debug output */ 
		curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);

		/* send all data to this function  */ 
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);

		/* write the page body to this file handle */ 
		work_data_t *w = new work_data_t;
		w -> global_stopflag = global_stopflag;
		w -> s = this;
		w -> first = w -> header = true;
		w -> data = NULL;
		w -> n = 0;
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, w);

		/* get it! */ 
		curl_easy_perform(curl_handle);

		free(w -> data);
		delete w;

		/* cleanup curl stuff */ 
		curl_easy_cleanup(curl_handle);
	}
}
