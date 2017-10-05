// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <sys/time.h>

#include "error.h"
#include "utils.h"
#include "log.h"

static const char *logfile = NULL;
static int loglevel = LL_INFO;

void setlogfile(const char *const file, const int ll)
{
	logfile = strdup(file);
	loglevel = ll;
}

void _log(const int ll, const char *const what, va_list args)
{
	if (!logfile)
		return;

	if (ll > loglevel)
		return;

	struct timeval tv;
	gettimeofday(&tv, NULL);

	time_t tv_temp = tv.tv_sec;
	struct tm tm;
	localtime_r(&tv_temp, &tm);

	char *msg = NULL;
	(void)vasprintf(&msg, what, args);

	const char *lls = "???";
	switch(ll) {
		case LL_FATAL:
			lls = "FATAL";
			break;
		case LL_ERR:
			lls = "ERROR";
			break;
		case LL_WARNING:
			lls = "WARN";
			break;
		case LL_INFO:
			lls = "INFO";
			break;
		case LL_DEBUG:
			lls = "DEBUG";
			break;
		case LL_DEBUG_VERBOSE:
			lls = "DEBVR";
			break;
	}

	char *temp = NULL;
	asprintf(&temp, "%04d-%02d-%02d %02d:%02d:%02d.%06ld %5s %9s %s", 
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec,
			lls,
			get_thread_name().c_str(),
			msg);
	free(msg);

	FILE *fh = fopen(logfile, "a+");
	if (!fh)
		error_exit(true, "Cannot access logfile '%s'", logfile);

	fprintf(fh, "%s\n", temp);
	fclose(fh);

	printf("%s\n", temp);
	free(temp);
}

void log(const int ll, const std::string & what, ...)
{
	va_list ap;
	va_start(ap, what);

	_log(ll, what.c_str(), ap);

	va_end(ap);
}

void log(const int ll, const char *const what, ...)
{
	va_list ap;
	va_start(ap, what);

	_log(ll, what, ap);

	va_end(ap);
}

int curl_log(CURL *handle, curl_infotype type, char *data, size_t size, void *userp)
{
	switch(type) {
		case CURLINFO_TEXT:
			log(LL_DEBUG, "CURL: %s", data);
			return 0;
		case CURLINFO_HEADER_OUT:
			log(LL_DEBUG, "CURL: Send header");
			break;
		case CURLINFO_DATA_OUT:
			log(LL_DEBUG_VERBOSE, "CURL: Send data");
			break;
		case CURLINFO_SSL_DATA_OUT:
			log(LL_DEBUG_VERBOSE, "CURL: Send SSL data");
			break;
		case CURLINFO_HEADER_IN:
			log(LL_DEBUG, "CURL: Recv header");
			break;
		case CURLINFO_DATA_IN:
			log(LL_DEBUG_VERBOSE, "CURL: Recv data");
			break;
		case CURLINFO_SSL_DATA_IN:
			log(LL_DEBUG_VERBOSE, "CURL: Recv SSL data");
			break;
		default:
			return 0;
	}

	std::string buffer;

	for(size_t i=0; i<size; i++) {
		if (data[i] >= 32 && data[i] < 127)
			buffer += data[i];
		else if (data[i] == 10) {
			log(LL_DEBUG_VERBOSE, "CURL: %s", buffer.c_str());
			buffer.clear();
		}
		else if (data[i] == 13) {
		}
		else {
			buffer += '.';
		}
	}

	if (!buffer.empty())
		log(LL_DEBUG_VERBOSE, "CURL: %s", buffer.c_str());

	return 0;
}
