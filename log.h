// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <string>
#include <curl/curl.h>

#define LL_FATAL	0
#define LL_ERR		1
#define LL_WARNING	2
#define LL_INFO		3
#define LL_DEBUG	4
#define LL_DEBUG_VERBOSE	5

void setlogfile(const char *const other, const int loglevel);
void log(const int loglevel, const char *const what, ...);
void log(const int loglevel, const std::string & what, ...);
int curl_log(CURL *handle, curl_infotype type, char *data, size_t size, void *userp);
