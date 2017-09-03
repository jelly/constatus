// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <atomic>
#include <string>
#include <vector>

#include "source.h"

void set_no_delay(int fd);
int start_listen(const char *adapter, int portnr, int listen_queue_size);
std::string get_endpoint_name(int fd);
ssize_t READ(int fd, char *whereto, size_t len);
ssize_t WRITE(int fd, const char *whereto, size_t len);
double get_ts();
uint64_t get_us();
std::string myformat(const char *const fmt, ...);
unsigned char *memstr(unsigned char *haystack, unsigned int haystack_len, unsigned char *needle, unsigned int needle_len);
void set_thread_name(const std::string & name);
std::string get_thread_name();
void mysleep(double slp, std::atomic_bool *const stop_flag, source *const s);
void *find_symbol(void *library, const char *const symbol, const char *const what, const char *const library_name);
char * un_url_escape(const char *const in);
std::vector<std::string> * split(std::string in, std::string splitter);
std::vector<std::pair<std::string, time_t> > * load_filelist(const std::string & dir, const std::string & prefix);
std::string myctime(const time_t t);
