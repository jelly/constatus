// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <assert.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <errno.h>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>

#include "error.h"
#include "io_buffer.h"

io_buffer::io_buffer(int fd_in) : pb(NULL), pb_len(0), fd(fd_in)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
		error_exit(true, "Failed to set NONBLOCKING on fd %d", fd);
}

io_buffer::~io_buffer()
{
	free(pb);
}

void io_buffer::push_back(unsigned char *what, int what_len)
{
	assert(what_len >= 0);
	assert(pb_len >= 0);

	if (what_len > 0)
	{
		pb = static_cast<unsigned char *>(realloc(pb, pb_len + what_len));

		memcpy(&pb[pb_len], what, what_len);
		pb_len += what_len;
	}
}

std::string * io_buffer::read_line(unsigned int max_len, bool strip_cr)
{
	char *buffer = static_cast<char *>(malloc(max_len + 1));
	unsigned int bytes_in = 0;

	while(bytes_in < max_len)
	{
		int n_read = -1;
		if (!read_available(reinterpret_cast<unsigned char *>(&buffer[bytes_in]), &n_read, max_len - bytes_in) || n_read == 0)
		{
			free(buffer);
			return NULL;
		}

		bytes_in += n_read;
		buffer[bytes_in] = 0x00;

		char *lf = strchr(buffer, '\n');
		if (lf != NULL)
		{
			int n_pb = int(&buffer[bytes_in] - lf) - 1;
			push_back((unsigned char *)(lf + 1), n_pb);

			if (lf > buffer && strip_cr)
			{
				char *cr = lf - 1;
				if (*cr == '\r')
					*cr = 0x00;
			}
			*lf = 0x00;

			std::string *out = new std::string(buffer);
			free(buffer);
			return out;
		}
	}
	free(buffer);

	return NULL;
}

// returns false in case of read error
bool io_buffer::read_available(unsigned char *where_to, int *where_to_len, int max_len)
{
	assert(pb_len >= 0);
	if (pb_len)
	{
		if (max_len >= pb_len)
		{
			memcpy(where_to, pb, pb_len);
			*where_to_len = pb_len;

			free(pb);
			pb = NULL;
			pb_len = 0;

			if (max_len > *where_to_len)
			{
				ssize_t rc = read(fd, &where_to[*where_to_len], max_len - *where_to_len);
				if (rc > 0)
					*where_to_len += int(rc);
				// printf("rc: %d\n", rc);
			}
		}
		else
		{
			*where_to_len = max_len;
			memcpy(where_to, pb, max_len);
			memmove(&pb[0], &pb[max_len], pb_len - max_len);
			pb_len -= max_len;
			// could realloc pb to smaller
		}

		assert(*where_to_len >= 0);
		assert(pb_len >= 0);

		return true;
	}

	fd_set rfds;
	struct timeval tv;
	for(;;)
	{
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);

		tv.tv_sec = 0;
		tv.tv_usec = 500000;

		int rc = select(fd + 1, &rfds, NULL, NULL, &tv);
		if (rc == -1)
		{
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return false;
		}

		if (rc > 0 && FD_ISSET(fd, &rfds))
		{
			ssize_t rc_read = read(fd, where_to, max_len);
			if (rc_read == -1)
			{
				if (errno == EINTR || errno == EAGAIN)
					continue;
				return false;
			}
			else if (rc_read == 0)
			{
				*where_to_len = 0;
				return true;
			}

			*where_to_len = int(rc_read);
			assert(*where_to_len >= 0);
			return true;
		}
	}
}
