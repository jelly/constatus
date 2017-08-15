// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <curl/curl.h>
void setlogfile(const char *const other);
void log(const char *const what, ...);
int curl_log(CURL *handle, curl_infotype type, char *data, size_t size, void *userp);
