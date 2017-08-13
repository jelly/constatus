// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <algorithm>
#include <atomic>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
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
#include <sys/types.h>
#include <unistd.h>

#include "error.h"
#include "http_server.h"
#include "picio.h"
#include "source.h"
#include "utils.h"
#include "log.h"

typedef struct {
	int fd;
	source *s;
	double fps;
	int quality;
	int time_limit;
	const std::vector<filter *> *filters;
	std::atomic_bool *global_stopflag;
	int resize_w, resize_h;
} http_thread_t;

void send_mjpeg_stream(int cfd, source *s, double fps, int quality, bool get, int time_limit, const std::vector<filter *> *const filters, std::atomic_bool *const global_stopflag, const int resize_w, const int resize_h)
{
        const char reply_headers[] =
                "HTTP/1.0 200 ok\r\n"
                "cache-control: no-cache\r\n"
                "pragma: no-cache\r\n"
		"server: " NAME " " VERSION "\r\n"
                "expires: thu, 01 dec 1994 16:00:00 gmt\r\n"
                "connection: close\r\n"
                "content-type: multipart/x-mixed-replace; boundary=--myboundary\r\n"
                "\r\n";

	if (WRITE(cfd, reply_headers, strlen(reply_headers)) <= 0)
	{
		log("short write on response header");
		close(cfd);
		return;
	}

	if (!get)
	{
		close(cfd);
		return;
	}

	bool first = true;
	uint8_t *prev_frame = NULL;

	uint64_t prev = 0;
	time_t end = time(NULL) + time_limit;
	for(;(time_limit <= 0 || time(NULL) < end) && !*global_stopflag;)
	{
		int w = -1, h = -1;
		uint8_t *work = NULL;
		size_t work_len = 0;
		s -> get_frame(filters -> empty() ? E_JPEG : E_RGB, quality, &prev, &w, &h, &work, &work_len);
		if (work == NULL || work_len == 0) {
			log("did not get a frame");
			continue;
		}

		// send header
                const char term[] = "\r\n";
                if (first)
                        first = false;
                else if (WRITE(cfd, term, strlen(term)) <= 0)
		{
			log("short write on terminating cr/lf");
                        break;
		}

		// decode, encode, etc frame
		if (filters -> empty() && !s -> need_scale()) {
			char img_h[4096] = { 0 };
			int len = snprintf(img_h, sizeof img_h, 
				"--myboundary\r\n"
				"Content-Type: image/jpeg\r\n"
				"Content-Size: %zu\r\n"
				"\r\n", work_len);
			if (WRITE(cfd, img_h, len) <= 0)
			{
				log("short write on boundary header");
				break;
			}

			if (WRITE(cfd, reinterpret_cast<char *>(work), work_len) <= 0)
			{
				log("short write on img data");
				break;
			}

			free(work);
		}
		else
		{
			apply_filters(filters, prev_frame, work, prev, w, h);

			char *data_out = NULL;
			size_t data_out_len = 0;
			FILE *fh = open_memstream(&data_out, &data_out_len);
			if (!fh)
				error_exit(true, "open_memstream() failed");

			if (resize_h != -1 || resize_w != -1) {
				int target_w = resize_w != -1 ? resize_w : w;
				int target_h = resize_h != -1 ? resize_h : h;

				uint8_t *temp = NULL;
				scale(work, w, h, &temp, target_w, target_h);

				write_JPEG_file(fh, target_w, target_h, quality, temp);
				free(temp);
			}
			else {
				write_JPEG_file(fh, w, h, quality, work);
			}

			fclose(fh);

			free(prev_frame);
			prev_frame = work;

			char img_h[4096] = { 0 };
			int len = snprintf(img_h, sizeof img_h, 
				"--myboundary\r\n"
				"Content-Type: image/jpeg\r\n"
				"Content-Size: %d\r\n"
				"\r\n", (int)data_out_len);
			if (WRITE(cfd, img_h, len) <= 0)
			{
				log("short write on boundary header");
				break;
			}

			if (WRITE(cfd, data_out, data_out_len) <= 0)
			{
				log("short write on img data");
				break;
			}

			free(data_out);
			data_out = NULL;
		}

		// FIXME adapt this by how long the wait_for_frame took
		if (fps > 0) {
			double us = 1000000.0 / fps;
			if (us)
				usleep((useconds_t)us);
		}
	}

	free(prev_frame);
}

void send_mpng_stream(int cfd, source *s, double fps, bool get, const int time_limit, const std::vector<filter *> *const filters, std::atomic_bool *const global_stopflag, const int resize_w, const int resize_h)
{
        const char reply_headers[] =
                "HTTP/1.0 200 ok\r\n"
                "cache-control: no-cache\r\n"
                "pragma: no-cache\r\n"
		"server: " NAME " " VERSION "\r\n"
                "expires: thu, 01 dec 1994 16:00:00 gmt\r\n"
                "connection: close\r\n"
                "content-type: multipart/x-mixed-replace; boundary=--myboundary\r\n"
                "\r\n";

	if (WRITE(cfd, reply_headers, strlen(reply_headers)) <= 0)
	{
		log("short write on response header");
		close(cfd);
		return;
	}

	if (!get)
	{
		close(cfd);
		return;
	}

	bool first = true;
	char *data_out = NULL;
	size_t data_out_len = 0;

	uint8_t *prev_frame = NULL;

	uint64_t prev = 0;
	time_t end = time(NULL) + time_limit;
	for(;(time_limit <= 0 || time(NULL) < end) && !*global_stopflag;)
	{
		int w = -1, h = -1;
		uint8_t *work = NULL;
		size_t work_len = 0;
		s -> get_frame(E_RGB, -1, &prev, &w, &h, &work, &work_len);

		if (work == NULL || work_len == 0) {
			log("did not get a frame");
			continue;
		}

		if (work_len == 0)
			continue;

		apply_filters(filters, prev_frame, work, prev, w, h);

		data_out = NULL;
		data_out_len = 0;
		FILE *fh = open_memstream(&data_out, &data_out_len);
		if (!fh)
			error_exit(true, "open_memstream() failed");

		if (s -> need_scale()) {
			int target_w = resize_w != -1 ? resize_w : w;
			int target_h = resize_h != -1 ? resize_h : h;

			uint8_t *temp = NULL;
			scale(work, w, h, &temp, target_w, target_h);

			write_PNG_file(fh, target_w, target_h, temp);

			free(temp);
		}
		else {
			write_PNG_file(fh, w, h, work);
		}

		fclose(fh);

		free(prev_frame);
		prev_frame = work;

		// send
                const char term[] = "\r\n";
                if (first)
                        first = false;
                else if (WRITE(cfd, term, strlen(term)) <= 0)
		{
			log("short write on terminating cr/lf");
                        break;
		}

                char img_h[4096] = { 0 };
		snprintf(img_h, sizeof img_h, 
                        "--myboundary\r\n"
                        "Content-Type: image/png\r\n"
			"Content-Size: %d\r\n"
                        "\r\n", (int)data_out_len);
		if (WRITE(cfd, img_h, strlen(img_h)) <= 0)
		{
			log("short write on boundary header");
			break;
		}

		if (WRITE(cfd, data_out, data_out_len) <= 0)
		{
			log("short write on img data");
			break;
		}

		free(data_out);
		data_out = NULL;

		// FIXME adapt this by how long the wait_for_frame took
		double us = 1000000.0 / fps;

		if (us)
			usleep((useconds_t)us);
	}

	free(data_out);

	free(prev_frame);
}

void send_png_frame(int cfd, source *s, bool get, const std::vector<filter *> *const filters, const int resize_w, const int resize_h)
{
        const char reply_headers[] =
                "HTTP/1.0 200 ok\r\n"
                "cache-control: no-cache\r\n"
                "pragma: no-cache\r\n"
		"server: " NAME " " VERSION "\r\n"
                "expires: thu, 01 dec 1994 16:00:00 gmt\r\n"
                "connection: close\r\n"
                "content-type: image/png\r\n"
                "\r\n";

	if (WRITE(cfd, reply_headers, strlen(reply_headers)) <= 0) {
		log("short write on response header");
		close(cfd);
		return;
	}

	if (!get) {
		close(cfd);
		return;
	}

	set_no_delay(cfd);

	uint64_t prev_ts = 0;
	int w = -1, h = -1;
	uint8_t *work = NULL;
	size_t work_len = 0;
	s -> get_frame(E_RGB, -1, &prev_ts, &w, &h, &work, &work_len);

	if (work == NULL || work_len == 0) {
		log("did not get a frame");
		return;
	}

	char *data_out = NULL;
	size_t data_out_len = 0;

	FILE *fh = open_memstream(&data_out, &data_out_len);
	if (!fh)
		error_exit(true, "open_memstream() failed");

	apply_filters(filters, NULL, work, prev_ts, w, h);

	if (s -> need_scale()) {
		int target_w = resize_w != -1 ? resize_w : w;
		int target_h = resize_h != -1 ? resize_h : h;

		uint8_t *temp = NULL;
		scale(work, w, h, &temp, target_w, target_h);

		write_PNG_file(fh, target_w, target_h, temp);

		free(temp);
	}
	else {
		write_PNG_file(fh, w, h, work);
	}

	free(work);

	fclose(fh);

	if (WRITE(cfd, data_out, data_out_len) <= 0)
		log("short write on img data");

	free(data_out);
}

void send_jpg_frame(int cfd, source *s, bool get, int quality, const std::vector<filter *> *const filters, const int resize_w, const int resize_h)
{
        const char reply_headers[] =
                "HTTP/1.0 200 ok\r\n"
                "cache-control: no-cache\r\n"
                "pragma: no-cache\r\n"
		"server: " NAME " " VERSION "\r\n"
                "expires: thu, 01 dec 1994 16:00:00 gmt\r\n"
                "connection: close\r\n"
                "content-type: image/jpeg\r\n"
                "\r\n";

	if (WRITE(cfd, reply_headers, strlen(reply_headers)) <= 0) {
		log("short write on response header");
		close(cfd);
		return;
	}

	if (!get)
	{
		close(cfd);
		return;
	}

	set_no_delay(cfd);

	uint64_t prev_ts = 0;
	int w = -1, h = -1;
	uint8_t *work = NULL;
	size_t work_len = 0;
	s -> get_frame(filters -> empty() ? E_JPEG : E_RGB, quality, &prev_ts, &w, &h, &work, &work_len);

	if (work == NULL || work_len == 0) {
		log("did not get a frame");
		return;
	}

	char *data_out = NULL;
	size_t data_out_len = 0;
	FILE *fh = open_memstream(&data_out, &data_out_len);
	if (!fh)
		error_exit(true, "open_memstream() failed");

	if (filters -> empty() && !s -> need_scale())
		fwrite(work, work_len, 1, fh);
	else {
		apply_filters(filters, NULL, work, prev_ts, w, h);

		if (resize_h != -1 || resize_w != -1) {
			int target_w = resize_w != -1 ? resize_w : w;
			int target_h = resize_h != -1 ? resize_h : h;

			uint8_t *temp = NULL;
			scale(work, w, h, &temp, target_w, target_h);

			write_JPEG_file(fh, target_w, target_h, quality, temp);
			free(temp);
		}
		else {
			write_JPEG_file(fh, w, h, quality, work);
		}
	}

	fclose(fh);

	if (WRITE(cfd, data_out, data_out_len) <= 0)
		log("short write on img data");

	free(data_out);

	free(work);
}

void handle_http_client(int cfd, source *s, double fps, int quality, int time_limit, const std::vector<filter *> *const filters, std::atomic_bool *const global_stopflag, const int resize_w, const int resize_h)
{
	sigset_t all_sigs;
	sigfillset(&all_sigs);
	pthread_sigmask(SIG_BLOCK, &all_sigs, NULL);

	log("connected with: %s", get_endpoint_name(cfd).c_str());

	char request_headers[65536] = { 0 };
	int h_n = 0;

	for(;!*global_stopflag;)
	{
		int rc = read(cfd, &request_headers[h_n], sizeof request_headers - h_n - 1);
		if (rc == -1)
		{
			if (errno == EINTR || errno == EAGAIN)
				continue;

			log("error receiving request headers");
			close(cfd);
			return;
		}
		else if (rc == 0)
		{
			log("error receiving request headers");
			close(cfd);
			return;
		}

		h_n += rc;

		request_headers[h_n] = 0x00;

		if (strstr(request_headers, "\r\n\r\n") || strstr(request_headers, "\n\n"))
			break;
	}

	char *path = NULL;

	bool get = true;
	if (strncmp(request_headers, "GET ", 4) == 0)
	{
		get = true;
		path = strdup(&request_headers[4]);
	}
	else if (strncmp(request_headers, "HEAD ", 5) == 0)
	{
		get = false;
		path = strdup(&request_headers[5]);
	}
	else
	{
		close(cfd);
		return;
	}

	char *dummy = strchr(path, '\r');
	if (!dummy)
		dummy = strchr(path, '\n');
	if (dummy)
		*dummy = 0x00;

	dummy = strchr(path, ' ');
	if (dummy)
		*dummy = 0x00;

	log("URL: %s", path);

	if (strcmp(path, "/stream.mjpeg") == 0)
		send_mjpeg_stream(cfd, s, fps, quality, get, time_limit, filters, global_stopflag, resize_w, resize_h);
	else if (strcmp(path, "/stream.mpng") == 0)
		send_mpng_stream(cfd, s, fps, get, time_limit, filters, global_stopflag, resize_w, resize_h);
	else if (strcmp(path, "/image.png") == 0)
		send_png_frame(cfd, s, get, filters, resize_w, resize_h);
	else if (strcmp(path, "/image.jpg") == 0)
		send_jpg_frame(cfd, s, get, quality, filters, resize_w, resize_h);
	else if (strcmp(path, "/stream.html") == 0)
	{
		const char reply[] =
			"HTTP/1.0 200 OK\r\n"
			"cache-control: no-cache\r\n"
			"pragma: no-cache\r\n"
			"server: " NAME " " VERSION "\r\n"
			"expires: thu, 01 dec 1994 16:00:00 gmt\r\n"
			"content-type: text/html\r\n"
			"connection: close\r\n"
			"\r\n"
			"<html><body><img src=\"stream.mjpeg\"></body></html>";

		if (WRITE(cfd, reply, sizeof(reply)) <= 0)
			log("short write on response header");
	}
	else
	{
		const char reply[] =
			"HTTP/1.0 404 url not found\r\n"
			"cache-control: no-cache\r\n"
			"pragma: no-cache\r\n"
			"server: " NAME " " VERSION "\r\n"
			"expires: thu, 01 dec 1994 16:00:00 gmt\r\n"
			"content-type: text/html\r\n"
			"connection: close\r\n"
			"\r\n"
			"URL not found. Use either \"<A HREF=\"stream.mjpeg\">stream.mjpeg</A>\" or \"<A HREF=\"stream.html\">stream.html</A>\".\r\n";

		if (WRITE(cfd, reply, sizeof(reply)) <= 0)
			log("short write on response header");
	}

	free(path);

	close(cfd);
}

void * handle_http_client_thread(void *ct_in)
{
	http_thread_t *ct = (http_thread_t *)ct_in;

	set_thread_name("http_client");

	handle_http_client(ct -> fd, ct -> s, ct -> fps, ct -> quality, ct -> time_limit, ct -> filters, ct -> global_stopflag, ct -> resize_w, ct -> resize_h);

	delete ct;

	return NULL;
}

void * http_server_thread(void *p)
{
	http_thread_t *st = (http_thread_t *)p;

	set_thread_name("http_server");

	struct pollfd fds[] = { { st -> fd, POLLIN, 0 } };

	for(;!*st -> global_stopflag;) {
		if (poll(fds, 1, 100) == 0)
			continue;

		int cfd = accept(st -> fd, NULL, 0);
		if (cfd == -1)
			continue;

		log("HTTP connected with: %s", get_endpoint_name(cfd).c_str());

		http_thread_t *ct = new http_thread_t;

		ct -> fd = cfd;
		ct -> s = st -> s;
		ct -> fps = st -> fps;
		ct -> quality = st -> quality;
		ct -> time_limit = st -> time_limit;
		ct -> filters = st -> filters;
		ct -> global_stopflag = st -> global_stopflag;
		ct -> resize_w = st -> resize_w;
		ct -> resize_h = st -> resize_h;

		pthread_attr_t tattr;
		pthread_attr_init(&tattr);
		pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);

		pthread_t th;
		int rc = -1;
		if ((rc = pthread_create(&th, &tattr, handle_http_client_thread, ct)) != 0)
		{
			errno = rc;
			error_exit(true, "pthread_create failed (http server)");
		}
	}

	delete st;

	log("HTTP server thread terminating");

	return NULL;
}

void start_http_server(const char *const http_adapter, const int http_port, source *const src, const double fps, const int quality, const int time_limit, const std::vector<filter *> *const filters, std::atomic_bool *const global_stopflag, const int resize_w, const int resize_h, pthread_t *th)
{
	if (http_port != -1)
	{
		int http_sfd = start_listen(http_adapter, http_port, 5);

		http_thread_t *st = new http_thread_t;

		st -> fd = http_sfd;
		st -> s = src;
		st -> fps = fps;
		st -> quality = quality;
		st -> time_limit = time_limit;
		st -> filters = filters;
		st -> global_stopflag = global_stopflag;
		st -> resize_w = resize_w;
		st -> resize_h = resize_h;

		int rc = -1;
		if ((rc = pthread_create(th, NULL, http_server_thread, st)) != 0)
		{
			errno = rc;
			error_exit(true, "pthread_create failed (http server main)");
		}
	}
}
