#include <assert.h>
#include <string>
#include <string.h>
#include <unistd.h>

#include "io_buffer.h"
#include "utils.h"
#include "source.h"
#include "source_http_mjpeg.h"
#include "picio.h"
#include "frame.h"

source_http_mjpeg::source_http_mjpeg(const std::string & hostnameIn, const int portnrIn, const std::string & fileIn, const int jpeg_quality, std::atomic_bool *const global_stopflag) : source(jpeg_quality, global_stopflag), hostname(hostnameIn), file(fileIn), portnr(portnrIn)
{
	th = new std::thread(std::ref(*this));
	th -> detach();
}

source_http_mjpeg::~source_http_mjpeg()
{
}

void source_http_mjpeg::operator()()
{
	int fd = -1;
	io_buffer *ib = NULL;
	unsigned char *needle = NULL;
	unsigned int needle_len = 0;
	bool first = true;

	for(;!*global_stopflag;) {
		if (fd == -1) {
			delete ib;
			free(needle);

			fd = connect_to(hostname, portnr);

			if (fd == -1)
			{
				sleep(1); // FIXME exponential back-off and o-o-o bitmap
				continue;
			}

			std::string get_request = std::string("GET ") + file + " HTTP/1.0\r\nHost: " + hostname + "\r\n\r\n";

			size_t len = get_request.size();
			if (WRITE(fd, get_request.c_str(), len) != ssize_t(len))
			{
				close(fd);
				fd = -1;
				continue;
			}

			ib = new io_buffer(fd);

			printf("connected\n");
		}

		unsigned char *frame_data = NULL;
		int data_len = 0;
		if (!retrieve_mjpeg_frame(ib, &frame_data, &data_len, &needle, &needle_len))
		{
			printf("no frame\n");

			close(fd);
			fd = -1;
		}
		else
		{
			printf("frame! (%p %d)\n", frame_data, data_len);

			if (first) {
				unsigned char *temp = NULL;
				read_JPEG_memory(frame_data, data_len, &width, &height, &temp);
				free(temp);

				first = false;
				printf("%dx%d\n", width, height);
			}

			set_frame(E_JPEG, frame_data, data_len);

			free(frame_data);
		}
	}
}

bool source_http_mjpeg::retrieve_mjpeg_frame(io_buffer *cur_ib, unsigned char **data, int *data_len, unsigned char **needle, unsigned int *needle_len)
{
	const int max_size = 10 * 1024 * 1024;
	*data = static_cast<unsigned char *>(malloc(max_size + 1)); // call me if frames get larger
	*data_len = 0;

	bool process_header = true;

	for(;!*global_stopflag;) {
		int data_read = 0;

		if (!cur_ib -> read_available(&(*data)[*data_len], &data_read, max_size - *data_len))
		{
			printf("no data\n");
			free(*data);
			*data = NULL;
			*data_len = 0;
			return false;
		}
		assert(data_read >= 0);
		if (data_read == 0)
			return true;

		*data_len += data_read;

		// search
		if (*needle) // then search for end of frame
		{
			if (process_header)
			{
				unsigned char *search_start = *data;
				int search_len = *data_len;
				bool lf = false;
				unsigned char *terminator = memstr(search_start, search_len, reinterpret_cast<unsigned char *>(const_cast<char *>("\r\n\r\n")), 4);
				if (!terminator)
				{
					terminator = memstr(search_start, search_len, reinterpret_cast<unsigned char *>(const_cast<char *>("\n\n")), 2);
					lf = true;
				}
				if (!terminator)
					continue;

				unsigned char *push_back_p = terminator + (lf?2:4);
				int push_back_len = int(*data_len - (push_back_p - *data));
				assert(push_back_len >= 0);
				cur_ib -> push_back(push_back_p, push_back_len);
				*data_len = 0;

				process_header = false;
			}
			else // data
			{
				unsigned char *search_start = *data;
				int search_len = *data_len;
				unsigned char *needle_found = memstr(search_start, search_len, *needle, *needle_len);
				if (!needle_found)
					continue;

				unsigned char *end_frame = NULL;
				if (needle_found[-2] == '\r')
					end_frame = needle_found - 2; // frames end with \r\n
				else
					end_frame = needle_found - 1; // frame ending with \n
				if (end_frame < search_start)
					end_frame = search_start;
				// push back
				unsigned char *push_back_p = needle_found;
				int push_back_len = int(search_len - (push_back_p - search_start));
				assert(push_back_len > 0);
				cur_ib -> push_back(push_back_p, push_back_len);

				*data_len = int(end_frame - *data);
				assert(*data_len >= 0);
				*data = static_cast<unsigned char *>(realloc(*data, *data_len));

				process_header = true;

				return true;
			}
		}
		else
		{
			// printf("main-header %d\n", *data_len - old_len);
			// search for end of header so that we can look for 'boundary='
			bool lf = false;
			unsigned char *terminator = memstr(*data, *data_len, reinterpret_cast<unsigned char *>(const_cast<char *>("\r\n\r\n")), 4);
			if (!terminator)
			{
				lf = true;
				terminator = memstr(*data, *data_len, reinterpret_cast<unsigned char *>(const_cast<char *>("\n\n")), 2);
			}
			if (!terminator)
			{
				if (*data_len > 32768) // response headers should not be more than 32KB
				{
					printf("Response headers overflow\n");

					free(*data);
					*data = NULL;
					*data_len = 0;
					return false;
				}
				continue;
			}

			terminator[lf?2:4] = 0x00;
			char *boundary = strstr(reinterpret_cast<char *>(*data), "boundary=");
			if (!boundary) // mallformed headers, start over
			{
				printf("No boundary in response headers\n");

				free(*data);
				*data = NULL;
				*data_len = 0;
				return false;
			}

			char *line_end = strchr(boundary + 9, '\r');
			if (line_end)
				*line_end = 0x00;
			line_end = strchr(boundary + 9, '\n');
			if (line_end)
				*line_end = 0x00;
			char *boundary_end = strchr(boundary + 9, ' ');
			if (!boundary_end)
				boundary_end = strchr(boundary + 9, ';');
			if (boundary_end)
				*boundary_end = 0x00;

			*needle_len = static_cast<unsigned int>(strlen(boundary + 9));
			*needle = static_cast<unsigned char *>(malloc(*needle_len));
			memcpy(*needle, boundary + 9, *needle_len);

			unsigned char *push_back_p = terminator + 4;
			int push_back_len = int(*data_len - (push_back_p - *data));
			assert(push_back_len >= 0);
			cur_ib -> push_back(push_back_p, push_back_len);
			*data_len = 0;

			printf("MJPEG boundary: %s\n", boundary + 9);
		}
	}
}
