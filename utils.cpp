// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <algorithm>
#include <assert.h>
#include <atomic>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "error.h"
#include "source.h"

void set_no_delay(int fd)
{
        int flag = 1;

        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) < 0)
                error_exit(true, "could not set TCP_NODELAY on socket");
}

int start_listen(const char *adapter, int portnr, int listen_queue_size)
{
	struct sockaddr_in6 server_addr;
	int server_addr_len = 0;
	int fd = socket(AF_INET6, SOCK_STREAM, 0);
	if (fd == -1)
		error_exit(true, "failed creating socket");

	int reuse_addr = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_addr, sizeof(reuse_addr)) == -1)
		error_exit(true, "setsockopt(SO_REUSEADDR) failed");

#ifdef TCP_FASTOPEN
	int qlen = 128;
	setsockopt(fd, SOL_TCP, TCP_FASTOPEN, &qlen, sizeof qlen);
#endif

	server_addr_len = sizeof(server_addr);
	memset((char *)&server_addr, 0x00, server_addr_len);
	server_addr.sin6_family = AF_INET6;
	server_addr.sin6_port = htons(portnr);
	if (!adapter || strcmp(adapter, "0.0.0.0") == 0)
	{
		server_addr.sin6_addr = in6addr_any;
	}
	else
	{
		if (inet_pton(AF_INET6, adapter, &server_addr.sin6_addr) == 0)
		{
			fprintf(stderr, "\n");
			fprintf(stderr, " * inet_pton(%s) failed: %s\n", adapter, strerror(errno));
			fprintf(stderr, " * If you're trying to use an IPv4 address (e.g. 192.168.0.1 or so)\n");
			fprintf(stderr, " * then do not forget to place ::FFFF: in front of the address,\n");
			fprintf(stderr, " * e.g.: ::FFFF:192.168.0.1\n\n");
			error_exit(false, "listen socket initialisation failure");
		}
	}

	if (bind(fd, (struct sockaddr *)&server_addr, server_addr_len) == -1)
		error_exit(true, "bind() failed");

	if (listen(fd, listen_queue_size) == -1)
		error_exit(true, "listen() failed");

	return fd;
}

std::string get_endpoint_name(int fd)
{
	char buffer[4096] = { "?" };
	struct sockaddr_in6 addr;
	socklen_t addr_len = sizeof addr;

	if (getpeername(fd, (struct sockaddr *)&addr, &addr_len) == -1)
		snprintf(buffer, sizeof buffer, "[FAILED TO FIND NAME OF %d: %s (1)]", fd, strerror(errno));
	else
	{
		char buffer2[4096];

		if (inet_ntop(AF_INET6, &addr.sin6_addr, buffer2, sizeof buffer2))
			snprintf(buffer, sizeof buffer, "[%s]:%d", buffer2, ntohs(addr.sin6_port));
		else
			snprintf(buffer, sizeof buffer, "[FAILED TO FIND NAME OF %d: %s (1)]", fd, strerror(errno));
	}

	return std::string(buffer);
}

ssize_t READ(int fd, char *whereto, size_t len)
{
	ssize_t cnt=0;

	while(len > 0)
	{
		ssize_t rc = read(fd, whereto, len);

		if (rc == -1)
		{
			if (errno == EINTR || errno == EAGAIN)
				continue;

			return -1;
		}
		else if (rc == 0)
			break;
		else
		{
			whereto += rc;
			len -= rc;
			cnt += rc;
		}
	}

	return cnt;
}

ssize_t WRITE(int fd, const char *whereto, size_t len)
{
	ssize_t cnt=0;

	while(len > 0)
	{
		ssize_t rc = write(fd, whereto, len);

		if (rc == -1)
		{
			if (errno == EINTR || errno == EAGAIN)
				continue;

			return -1;
		}
		else if (rc == 0)
			return -1;
		else
		{
			whereto += rc;
			len -= rc;
			cnt += rc;
		}
	}

	return cnt;
}

double get_ts()
{
        struct timeval ts;

        if (gettimeofday(&ts, NULL) == -1)
                error_exit(true, "gettimeofday failed");

        return double(ts.tv_sec) + double(ts.tv_usec) / 1000000.0;
}

std::string myformat(const char *const fmt, ...)
{
	char *buffer = NULL;
        va_list ap;

        va_start(ap, fmt);
        (void)vasprintf(&buffer, fmt, ap);
        va_end(ap);

	std::string result = buffer;
	free(buffer);

	return result;
}

unsigned char *memstr(unsigned char *haystack, unsigned int haystack_len, unsigned char *needle, unsigned int needle_len)
{
	if (haystack_len < needle_len)
		return NULL;

	unsigned int pos = 0;
	for(;(haystack_len - pos) >= needle_len;)
	{
		unsigned char *p = (unsigned char *)memchr(&haystack[pos], needle[0], haystack_len - pos);
		if (!p)
			return NULL;

		pos = (unsigned int)(p - haystack);
		if ((haystack_len - pos) < needle_len)
			break;

		if (memcmp(&haystack[pos], needle, needle_len) == 0)
			return &haystack[pos];

		pos++;
	}

	return NULL;
}

void set_thread_name(const std::string & name)
{
	std::string full_name = "cs:" + name;

	if (full_name.length() > 15)
		full_name = full_name.substr(0, 15);

	pthread_setname_np(pthread_self(), full_name.c_str());
}

std::string get_thread_name()
{
	char buffer[16];

	pthread_getname_np(pthread_self(), buffer, sizeof buffer);

	return buffer;
}

void mysleep(double slp, std::atomic_bool *const stop_flag, source *const s)
{
	bool unreg = slp >= 1.0;

	if (unreg)
		s -> unregister_user();

	while(slp > 0 && !*stop_flag) {
		double cur = std::min(slp, 0.1);
		slp -= cur;

		int s = cur;

		uint64_t us = (cur - s) * 1000 * 1000;

		// printf("%d, %lu\n", s, us);

		if (s)
			::sleep(s); // FIXME handle signals

		if (us)
			usleep(us); // FIXME handle signals
	}

	if (unreg)
		s -> register_user();
}
