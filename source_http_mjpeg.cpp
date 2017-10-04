// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <assert.h>
#include <string>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

#include "utils.h"
#include "source.h"
#include "source_http_mjpeg.h"
#include "picio.h"
#include "error.h"
#include "log.h"

typedef struct
{
	std::atomic_bool *stop_flag;
	source *s;
	bool first;
	bool header;
	uint8_t *data;
	size_t n;
	size_t req_len;
	uint64_t interval, next_frame_ts;
} work_data_t;

static int xfer_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	work_data_t *w = (work_data_t *)clientp;

	if (*w -> stop_flag)
		return 1;

	return 0;
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *mypt)
{
	work_data_t *w = (work_data_t *)mypt;
	const size_t full_size = size * nmemb;

	if (*w -> stop_flag)
		return 0;

	w -> data = (uint8_t *)realloc(w -> data, w -> n + full_size + 1);
	memcpy(&w -> data[w -> n], ptr, full_size);
	w -> n += full_size;
	w -> data[w -> n] = 0x00;

	// printf("h:%d  n:%zu req:%zu\n", w -> header, w -> n, w -> req_len);

	if (w -> header) {
		bool proper_header = true;

		char *header_end = strstr((char *)w -> data, "\r\n\r\n");
		if (!header_end) {
			header_end = strstr((char *)w -> data, "\n\n");
			if (!header_end)
				return full_size;

			proper_header = false;
		}

		*header_end = 0x0;

		char *cl = strstr((char *)w -> data, "Content-Length:");
		if (!cl) {
			log(LL_INFO, "Content-Length missing in header");
			return 0;
		}

		w -> req_len = atoi(&cl[15]);
		//printf("needed len: %zu\n", w -> req_len);

		w -> header = false;

		size_t left = w -> n - (strlen((char *)w -> data) + (proper_header?4:2));
		if (left) {
			//printf("LEFT %zu\n", left);
			memmove(w -> data, header_end + (proper_header?4:2), left);
		}
		w -> n = left;
	}
	else if (w -> n >= w -> req_len) {
		// printf("frame! (%p %zu/%zu)\n", w -> data, w -> n, w -> req_len);

		if (w -> first) {
			int width = -1, height = -1;
			unsigned char *temp = NULL;
			bool rc = read_JPEG_memory(w -> data, w -> req_len, &width, &height, &temp);
			free(temp);

			if (rc) {
				w -> first = false;
				log(LL_INFO, "first frame received, mjpeg size: %dx%d", width, height);

				w -> s -> set_size(width, height);
			}
			else {
				log(LL_INFO, "JPEG decode error");
			}
		}

		uint64_t now_ts = get_us();
		bool do_get = false;
		if (now_ts >= w -> next_frame_ts || w -> interval == 0) {
			do_get = true;
			w -> next_frame_ts += w -> interval;
		}

		if (w -> s -> work_required() && do_get && !w -> s -> is_paused()) {
			if (w -> s -> need_scale()) {
				int dw, dh;
				unsigned char *temp = NULL;
				if (read_JPEG_memory(w -> data, w -> req_len, &dw, &dh, &temp))
					w -> s -> set_scaled_frame(temp, dw, dh);
				free(temp);
			}
			else {
				w -> s -> set_frame(E_JPEG, w -> data, w -> req_len);
			}
		}

		size_t left = w -> n - w -> req_len;
		if (left) {
			memmove(w -> data, &w -> data[w -> req_len], left);
		//printf("LEFT %zu\n", left);
		}
		w -> n = left;

		w -> req_len = 0;

		w -> header = true;
	}

	if (w -> n > 16 * 1024 * 1024) { // sanity limit
		log(LL_INFO, "frame too big");
		return 0;
	}

	return full_size;
}


source_http_mjpeg::source_http_mjpeg(const std::string & id, const std::string & urlIn, const bool ic, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel) : source(id, max_fps, r, resize_w, resize_h, loglevel), url(urlIn), ignore_cert(ic)
{
	d = url;
}

source_http_mjpeg::~source_http_mjpeg()
{
	stop();
}

void source_http_mjpeg::operator()()
{
	log(LL_INFO, "source http mjpeg thread started");

	set_thread_name("src_h_mjpeg");

	for(;!local_stop_flag;)
	{
		log(LL_INFO, "(re-)connect to MJPEG source %s", url.c_str());

		CURL *ch = curl_easy_init();

		char error[CURL_ERROR_SIZE] = "?";
		if (curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, error))
			error_exit(false, "curl_easy_setopt(CURLOPT_ERRORBUFFER) failed: %s", error);

		curl_easy_setopt(ch, CURLOPT_DEBUGFUNCTION, curl_log);

		curl_easy_setopt(ch, CURLOPT_URL, url.c_str());

		if (curl_easy_setopt(ch, CURLOPT_TCP_KEEPALIVE, 1L))
			error_exit(false, "curl_easy_setopt(CURLOPT_TCP_KEEPALIVE) failed: %s", error);
	 
		if (curl_easy_setopt(ch, CURLOPT_TCP_KEEPIDLE, 120L))
			error_exit(false, "curl_easy_setopt(CURLOPT_TCP_KEEPIDLE) failed: %s", error);
	 
		if (curl_easy_setopt(ch, CURLOPT_TCP_KEEPINTVL, 60L))
			error_exit(false, "curl_easy_setopt(CURLOPT_TCP_KEEPINTVL) failed: %s", error);

		std::string useragent = NAME " " VERSION;

		if (curl_easy_setopt(ch, CURLOPT_USERAGENT, useragent.c_str()))
			error_exit(false, "curl_easy_setopt(CURLOPT_USERAGENT) failed: %s", error);

		if (ignore_cert) {
			if (curl_easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 0))
				error_exit(false, "curl_easy_setopt(CURLOPT_SSL_VERIFYPEER) failed: %s", error);

			if (curl_easy_setopt(ch, CURLOPT_SSL_VERIFYHOST, 0))
				error_exit(false, "curl_easy_setopt(CURLOPT_SSL_VERIFYHOST) failed: %s", error);
		}

		curl_easy_setopt(ch, CURLOPT_VERBOSE, loglevel >= LL_DEBUG);

		curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, write_data);

		work_data_t *w = new work_data_t;
		w -> stop_flag = &local_stop_flag;
		w -> s = this;
		w -> first = w -> header = true;
		w -> data = NULL;
		w -> n = 0;
		w -> interval = max_fps > 0.0 ? 1.0 / max_fps * 1000.0 * 1000.0 : 0;
		w -> next_frame_ts = get_us();
		curl_easy_setopt(ch, CURLOPT_WRITEDATA, w);

		curl_easy_setopt(ch, CURLOPT_XFERINFODATA, w);
		curl_easy_setopt(ch, CURLOPT_XFERINFOFUNCTION, xfer_callback);
		curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 0L);

		curl_easy_perform(ch);

		free(w -> data);
		delete w;

		curl_easy_cleanup(ch);

		usleep(101000);
	}

	log(LL_INFO, "source http mjpeg thread terminating");
}
