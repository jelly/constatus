// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <syslog.h>
#include <curl/curl.h>

#include "error.h"
#include "log.h"

typedef struct
{
	uint8_t *p;
	size_t len;
} curl_data_t;

size_t curl_write_data(void *ptr, size_t size, size_t nmemb, void *ctx)
{
	curl_data_t *pctx = (curl_data_t *)ctx;

	size_t n = size * nmemb;

	size_t tl = pctx -> len + n;
	pctx -> p = (uint8_t *)realloc(pctx -> p, tl);
	if (!pctx -> p)
		error_exit(true, "Cannot allocate %zu bytes of memory", tl);

	memcpy(&pctx -> p[pctx -> len], ptr, n);
	pctx -> len += n;


	return n;
}

bool http_get(const std::string & url, const bool ignore_cert, const char *const auth, const bool verbose, uint8_t **const out, size_t *const out_n)
{
	CURL *ch = curl_easy_init();
	if (!ch)
		error_exit(false, "Failed to initialize CURL session");

	char error[CURL_ERROR_SIZE] = "?";
	if (curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, error))
		error_exit(false, "curl_easy_setopt(CURLOPT_ERRORBUFFER) failed: %s", error);

	curl_easy_setopt(ch, CURLOPT_DEBUGFUNCTION, curl_log);

	long timeout = 15;
	curl_data_t data = { NULL, 0 };

	std::string useragent = NAME " " VERSION;

	if (ignore_cert) {
		if (curl_easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 0))
			error_exit(false, "curl_easy_setopt(CURLOPT_SSL_VERIFYPEER) failed: %s", error);

		if (curl_easy_setopt(ch, CURLOPT_SSL_VERIFYHOST, 0))
			error_exit(false, "curl_easy_setopt(CURLOPT_SSL_VERIFYHOST) failed: %s", error);
	}

	if (auth) {
		curl_easy_setopt(ch, CURLOPT_HTTPAUTH, (long)CURLAUTH_ANY);
 
		curl_easy_setopt(ch, CURLOPT_USERPWD, auth);
	}

	if (curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1L))
		error_exit(false, "curl_easy_setopt(CURLOPT_FOLLOWLOCATION) failed: %s", error);

	if (curl_easy_setopt(ch, CURLOPT_USERAGENT, useragent.c_str()))
		error_exit(false, "curl_easy_setopt(CURLOPT_USERAGENT) failed: %s", error);

	if (curl_easy_setopt(ch, CURLOPT_URL, url.c_str()))
		error_exit(false, "curl_easy_setopt(CURLOPT_URL) failed: %s", error);

	if (curl_easy_setopt(ch, CURLOPT_CONNECTTIMEOUT, timeout))
		error_exit(false, "curl_easy_setopt(CURLOPT_CONNECTTIMEOUT) failed: %s", error);

	if (curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, curl_write_data))
		error_exit(false, "curl_easy_setopt(CURLOPT_WRITEFUNCTION) failed: %s", error);

	if (curl_easy_setopt(ch, CURLOPT_WRITEDATA, &data))
		error_exit(false, "curl_easy_setopt(CURLOPT_WRITEDATA) failed: %s", error);

	if (curl_easy_setopt(ch, CURLOPT_VERBOSE, verbose))
		error_exit(false, "curl_easy_setopt(CURLOPT_VERBOSE) failed: %s", error);

	bool ok = true;
	if (curl_easy_perform(ch)) {
		log(LL_ERR, "curl_easy_perform() failed: %s", error);
		ok = false;
	}

	curl_easy_cleanup(ch);

	*out = data.p;
	*out_n = data.len;

	return ok;
}
