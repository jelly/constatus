// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <algorithm>
#include <atomic>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <jansson.h>
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
#include "target.h"
#include "target_avi.h"
#include "target_ffmpeg.h"

typedef struct {
	int fd;
	source *s;
	double fps;
	int quality;
	int time_limit;
	const std::vector<filter *> *filters;
	std::atomic_bool *global_stopflag;
	resize *r;
	int resize_w, resize_h;
	bool motion_compatible, allow_admin, archive_acces;
	configuration_t *cfg;
	std::string snapshot_dir;
} http_thread_t;

void send_mjpeg_stream(int cfd, source *s, double fps, int quality, bool get, int time_limit, const std::vector<filter *> *const filters, std::atomic_bool *const global_stopflag, resize *const r, const int resize_w, const int resize_h)
{
        const char reply_headers[] =
                "HTTP/1.0 200 OK\r\n"
                "Cache-Control: no-cache\r\n"
                "Pragma: no-cache\r\n"
		"Server: " NAME " " VERSION "\r\n"
                "Expires: thu, 01 dec 1994 16:00:00 gmt\r\n"
                "Connection: close\r\n"
                "Content-Type: multipart/x-mixed-replace; boundary=--myboundary\r\n"
                "\r\n";

	if (WRITE(cfd, reply_headers, strlen(reply_headers)) <= 0)
	{
		log(LL_DEBUG, "short write on response header");
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
	bool sc = resize_h != -1 || resize_w != -1;

	uint64_t prev = 0;
	time_t end = time(NULL) + time_limit;
	for(;(time_limit <= 0 || time(NULL) < end) && !*global_stopflag;)
	{
		int w = -1, h = -1;
		uint8_t *work = NULL;
		size_t work_len = 0;
		s -> get_frame(filters -> empty() && !sc ? E_JPEG : E_RGB, quality, &prev, &w, &h, &work, &work_len);
		if (work == NULL || work_len == 0) {
			log(LL_ERR, "did not get a frame");
			continue;
		}

		// send header
                const char term[] = "\r\n";
                if (first)
                        first = false;
                else if (WRITE(cfd, term, strlen(term)) <= 0)
		{
			log(LL_DEBUG, "short write on terminating cr/lf");
			free(work);
                        break;
		}

		// decode, encode, etc frame
		if (filters -> empty() && !sc) {
			char img_h[4096] = { 0 };
			int len = snprintf(img_h, sizeof img_h, 
				"--myboundary\r\n"
				"Content-Type: image/jpeg\r\n"
				"Content-Length: %zu\r\n"
				"\r\n", work_len);
			if (WRITE(cfd, img_h, len) <= 0)
			{
				log(LL_DEBUG, "short write on boundary header");
				free(work);
				break;
			}

			if (WRITE(cfd, reinterpret_cast<char *>(work), work_len) <= 0)
			{
				log(LL_DEBUG, "short write on img data");
				free(work);
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
				r -> do_resize(w, h, work, target_w, target_h, &temp);

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
				"Content-Length: %zu\r\n"
				"\r\n", data_out_len);
			if (WRITE(cfd, img_h, len) <= 0)
			{
				log(LL_DEBUG, "short write on boundary header");
				break;
			}

			if (WRITE(cfd, data_out, data_out_len) <= 0)
			{
				log(LL_DEBUG, "short write on img data");
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

void send_mpng_stream(int cfd, source *s, double fps, bool get, const int time_limit, const std::vector<filter *> *const filters, std::atomic_bool *const global_stopflag, resize *const r, const int resize_w, const int resize_h)
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
		log(LL_DEBUG, "short write on response header");
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
		if (!s -> get_frame(E_RGB, -1, &prev, &w, &h, &work, &work_len))
			continue;

		if (work == NULL || work_len == 0) {
			log(LL_ERR, "did not get a frame");
			continue;
		}

		apply_filters(filters, prev_frame, work, prev, w, h);

		data_out = NULL;
		data_out_len = 0;
		FILE *fh = open_memstream(&data_out, &data_out_len);
		if (!fh)
			error_exit(true, "open_memstream() failed");

		if (resize_w != -1 || resize_h != -1) {
			int target_w = resize_w != -1 ? resize_w : w;
			int target_h = resize_h != -1 ? resize_h : h;

			uint8_t *temp = NULL;
			r -> do_resize(w, h, work, target_w, target_h, &temp);

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
			log(LL_DEBUG, "short write on terminating cr/lf");
                        break;
		}

                char img_h[4096] = { 0 };
		snprintf(img_h, sizeof img_h, 
                        "--myboundary\r\n"
                        "Content-Type: image/png\r\n"
			"Content-Length: %d\r\n"
                        "\r\n", (int)data_out_len);
		if (WRITE(cfd, img_h, strlen(img_h)) <= 0)
		{
			log(LL_DEBUG, "short write on boundary header");
			break;
		}

		if (WRITE(cfd, data_out, data_out_len) <= 0)
		{
			log(LL_DEBUG, "short write on img data");
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

void send_png_frame(int cfd, source *s, bool get, const std::vector<filter *> *const filters, resize *const r, const int resize_w, const int resize_h)
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
		log(LL_DEBUG, "short write on response header");
		close(cfd);
		return;
	}

	if (!get) {
		close(cfd);
		return;
	}

	set_no_delay(cfd, true);

	uint64_t prev_ts = 0;
	int w = -1, h = -1;
	uint8_t *work = NULL;
	size_t work_len = 0;
	s -> get_frame(E_RGB, -1, &prev_ts, &w, &h, &work, &work_len);

	if (work == NULL || work_len == 0) {
		log(LL_ERR, "did not get a frame");
		return;
	}

	char *data_out = NULL;
	size_t data_out_len = 0;

	FILE *fh = open_memstream(&data_out, &data_out_len);
	if (!fh)
		error_exit(true, "open_memstream() failed");

	apply_filters(filters, NULL, work, prev_ts, w, h);

	if (resize_h != -1 || resize_w != -1) {
		int target_w = resize_w != -1 ? resize_w : w;
		int target_h = resize_h != -1 ? resize_h : h;

		uint8_t *temp = NULL;
		r -> do_resize(w, h, work, target_w, target_h, &temp);

		write_PNG_file(fh, target_w, target_h, temp);

		free(temp);
	}
	else {
		write_PNG_file(fh, w, h, work);
	}

	free(work);

	fclose(fh);

	if (WRITE(cfd, data_out, data_out_len) <= 0)
		log(LL_DEBUG, "short write on img data");

	free(data_out);
}

void send_jpg_frame(int cfd, source *s, bool get, int quality, const std::vector<filter *> *const filters, resize *const r, const int resize_w, const int resize_h)
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
		log(LL_DEBUG, "short write on response header");
		close(cfd);
		return;
	}

	if (!get)
	{
		close(cfd);
		return;
	}

	set_no_delay(cfd, true);

	bool sc = resize_h != -1 || resize_w != -1;

	uint64_t prev_ts = 0;
	int w = -1, h = -1;
	uint8_t *work = NULL;
	size_t work_len = 0;
	s -> get_frame(filters -> empty() && !sc ? E_JPEG : E_RGB, quality, &prev_ts, &w, &h, &work, &work_len);

	if (work == NULL || work_len == 0) {
		log(LL_DEBUG, "did not get a frame");
		return;
	}

	char *data_out = NULL;
	size_t data_out_len = 0;
	FILE *fh = open_memstream(&data_out, &data_out_len);
	if (!fh)
		error_exit(true, "open_memstream() failed");

	if (filters -> empty() && !sc)
		fwrite(work, work_len, 1, fh);
	else {
		apply_filters(filters, NULL, work, prev_ts, w, h);

		if (resize_h != -1 || resize_w != -1) {
			int target_w = resize_w != -1 ? resize_w : w;
			int target_h = resize_h != -1 ? resize_h : h;

			uint8_t *temp = NULL;
			r -> do_resize(w, h, work, target_w, target_h, &temp);

			write_JPEG_file(fh, target_w, target_h, quality, temp);
			free(temp);
		}
		else {
			write_JPEG_file(fh, w, h, quality, work);
		}
	}

	fclose(fh);

	if (WRITE(cfd, data_out, data_out_len) <= 0)
		log(LL_DEBUG, "short write on img data");

	free(data_out);

	free(work);
}

std::string describe_interface(const interface *const i)
{
	std::string id_int = myformat("%p", i);
	std::string id_ext = i -> get_id();

	std::string type = "!!!", div = "some_section";
	switch(i -> get_class_type()) {
		case CT_HTTPSERVER:
			type = "HTTP server";
			div = "http-server";
			break;
		case CT_MOTIONTRIGGER:
			type = "motion trigger";
			div = "motion-trigger";
			break;
		case CT_TARGET:
			type = "stream writer";
			div = "stream-writer";
			break;
		case CT_SOURCE:
			type = "source";
			div = "source";
			break;
		case CT_LOOPBACK:
			type = "video4linux loopback";
			div = "v4l-loopback";
			break;
		case CT_NONE:
			type = "???";
			break;
	}

	if (id_ext.empty())
		id_ext = type;

	std::string out = std::string("<div id=\"") + div + "\"><h2>" + id_ext + "</h2><p>description: " + i -> get_description() + "<br>type: " + type + "</p><ul>";

	if (i -> is_paused()) {
		out += myformat("<li><a href=\"unpause?%s\">unpause</a>", id_int.c_str());
	}
	else {
		out += myformat("<li><a href=\"pause?%s\">pause</a>", id_int.c_str());
	}

	if (i -> is_running()) {
		out += myformat("<li><a href=\"stop?%s\">stop</a>", id_int.c_str());
	}
	else {
		out += myformat("<li><a href=\"start?%s\">start</a>", id_int.c_str());
	}
	out += myformat("<li><a href=\"restart?%s\">restart</a>", id_int.c_str());

	out += "</ul></div>";

	return out;
}

interface *find_by_id(configuration_t *const cfg, const std::string & id)
{
	interface *ret = NULL;

	for(interface *i : cfg -> interfaces) {
		std::string id_int = myformat("%p", i);

		if (id == id_int) {
			ret = i;
			break;
		}
	}

	return ret;
}

bool pause(configuration_t *const cfg, const std::string & which, const bool p)
{
	bool rc = false;

	interface *i = find_by_id(cfg, which);
	if (i != NULL) {
		if (p)
			rc = i -> pause();
		else {
			i -> unpause();
			rc = true;
		}
	}

	return rc;
}

bool start_stop(configuration_t *const cfg, const std::string & which, const bool strt)
{
	bool rc = false;

	interface *i = find_by_id(cfg, which);

	if (i != NULL) {
		if (strt)
			i -> start();
		else
			i -> stop(); // FIXME time-out?
		rc = true;
	}

	return rc;
}

bool take_a_picture(source *const s, const std::string & snapshot_dir, const int quality)
{
	uint64_t prev_ts = 0;
	int w = -1, h = -1;
	uint8_t *work = NULL;
	size_t work_len = 0;
	s -> get_frame(E_JPEG, quality, &prev_ts, &w, &h, &work, &work_len);

	if (work == NULL || work_len == 0) {
		log(LL_DEBUG, "did not get a frame");
		return false;
	}

	std::string name = gen_filename(snapshot_dir, "snapshot-", "jpg", get_us(), 0);

	log(LL_INFO, "Snapshot will be written to %s", name.c_str());

	FILE *fh = fopen(name.c_str(), "w");
	if (!fh) {
		log(LL_ERR, "open_memstream() failed");
		free(work);
		return false;
	}

	bool rc = fwrite(work, 1, work_len, fh) == work_len;

	fclose(fh);

	free(work);

	return rc;
}

interface * start_a_video(source *const s, const std::string & snapshot_dir, const int quality)
{
	const std::string id = "snapshot started by HTTP server";

	std::vector<filter *> *const filters = new std::vector<filter *>();

#ifdef WITH_GWAVI
	interface *i = new target_avi(id, s, snapshot_dir, "snapshot-", quality, -1, -1, filters, "", "", "", -1);
#else
	interface *i = new target_ffmpeg(id, "", s, snapshot_dir, "snapshot-", -1, -1, "mp4", 201000, filters, "", "", "", -1);
#endif
	i -> start();

	return i;
}

bool validate_file(const std::string & snapshot_dir, const std::string & filename)
{
	bool rc = false;

	auto *files = load_filelist(snapshot_dir, "");

	for(auto cur : *files) {
		if (cur.name == filename) {
			rc = true;
			break;
		}
	}

	delete files;

	return rc;
}

std::string run_rest(configuration_t *const cfg, const std::string & path, const std::string & pars, const int quality, const std::string & snapshot_dir, source *const s)
{
	int code = 200;
	std::vector<std::string> *parts = split(path, "/");

	json_t *json = json_object();

	bool cmd = parts -> at(0) == "cmd";

	cfg -> lock.lock();
	interface *i = (parts -> size() >= 1 && !cmd) ? find_by_id(cfg, parts -> at(0)) : NULL;

	if (parts -> size() < 2) {
		code = 500;
		json_object_set(json, "msg", json_string("ID/cmd missing"));
		json_object_set(json, "result", json_false());
	}
	else if (cmd && parts -> at(1) == "list") {
		json_object_set(json, "msg", json_string("OK"));
		json_object_set(json, "result", json_true());

		json_t *out_arr = json_array();
		json_object_set(json, "data", out_arr);

		for(interface *cur : cfg -> interfaces) {
			json_t *record = json_object();

			json_object_set(record, "id", json_string(myformat("%p", cur).c_str()));
			json_object_set(record, "id-str", json_string(cur -> get_id().c_str()));
			json_object_set(record, "id-descr", json_string(cur -> get_description().c_str()));

			switch(cur -> get_class_type()) {
				case CT_HTTPSERVER:
					json_object_set(record, "type", json_string("http-server"));
					break;
				case CT_MOTIONTRIGGER:
					json_object_set(record, "type", json_string("motion-trigger"));
					break;
				case CT_TARGET:
					json_object_set(record, "type", json_string("stream-writer"));
					break;
				case CT_SOURCE:
					json_object_set(record, "type", json_string("source"));
					break;
				case CT_LOOPBACK:
					json_object_set(record, "type", json_string("video4linux-loopback"));
					break;
				default:
					json_object_set(record, "type", json_string("INTERNAL ERROR"));
					break;
			}

			json_object_set(record, "running", json_string(cur -> is_paused() ? "false" : "true"));

			json_object_set(record, "enabled", json_string(cur -> is_running() ? "true" : "false"));

			json_array_append_new(out_arr, record);
		}
	}
	else if (cmd && parts -> at(1) == "list-files") {
		json_object_set(json, "msg", json_string("OK"));
		json_object_set(json, "result", json_true());

		json_t *out_arr = json_array();
		json_object_set(json, "data", out_arr);

		auto *files = load_filelist(snapshot_dir, "");

		for(auto file : *files) {
			json_t *record = json_object();

			json_object_set(record, "file", json_string(file.name.c_str()));
			json_object_set(record, "mtime", json_integer(file.last_change));
			json_object_set(record, "size", json_integer(file.size));

			json_array_append_new(out_arr, record);
		}

		delete files;
	}
	else if (cmd && parts -> at(1) == "take-a-snapshot") {
		if (take_a_picture(s, snapshot_dir, quality)) {
			json_object_set(json, "msg", json_string("OK"));
			json_object_set(json, "result", json_true());
		}
		else {
			json_object_set(json, "msg", json_string("failed"));
			json_object_set(json, "result", json_false());
		}
	}
	else if (cmd && parts -> at(1) == "start-a-recording") {
		interface *i = start_a_video(s, snapshot_dir, quality);

		if (i) {
			json_object_set(json, "msg", json_string("OK"));
			json_object_set(json, "result", json_true());

			cfg -> interfaces.push_back(i);
		}
		else {
			json_object_set(json, "msg", json_string("failed"));
			json_object_set(json, "result", json_false());
		}

	}
	// commands that require an ID
	else if (i == NULL) {
		code = 404;
		json_object_set(json, "msg", json_string("ID not found"));
		json_object_set(json, "result", json_false());
	}
	else if (parts -> at(1) == "pause") {
		if (i -> pause()) {
			json_object_set(json, "msg", json_string("OK"));
			json_object_set(json, "result", json_true());
		}
		else {
			json_object_set(json, "msg", json_string("failed"));
			json_object_set(json, "result", json_false());
		}
	}
	else if (parts -> at(1) == "unpause") {
		i -> unpause();
		json_object_set(json, "msg", json_string("OK"));
		json_object_set(json, "result", json_true());
	}
	else if (parts -> at(1) == "stop") {
		if (start_stop(cfg, pars, false)) {
			json_object_set(json, "msg", json_string("OK"));
			json_object_set(json, "result", json_true());
		}
		else {
			json_object_set(json, "msg", json_string("failed"));
			json_object_set(json, "result", json_false());
		}
	}
	else if (parts -> at(1) == "start") {
		if (start_stop(cfg, pars, true)) {
			json_object_set(json, "msg", json_string("OK"));
			json_object_set(json, "result", json_true());
		}
		else {
			json_object_set(json, "msg", json_string("failed"));
			json_object_set(json, "result", json_false());
		}
	}
	else {
		code = 404;
	}

	cfg -> lock.unlock();

	delete parts;

	char *js = json_dumps(json, JSON_COMPACT);
	std::string reply = myformat("HTTP/1.0 %d\r\nServer: " NAME " " VERSION "\r\nContent-Type: application/json\r\n\r\n%s", code, js);
	free(js);

	json_decref(json);

	return reply;
}

void send_file(const int cfd, const std::string & path, const char *const name)
{
	std::string complete_path = path + "/" + name;

	std::string type = "text/html";
	bool dl = false;
	if (complete_path.size() >= 3) {
		std::string ext = complete_path.substr(complete_path.size() -3);

		if (ext == "avi")
			type = "video/x-msvideo";
		else if (ext == "mp4")
			type = "video/mp4";
		else if (ext == "png")
			type = "image/png";
		else if (ext == "jpg")
			type = "image/jpeg";
		else if (ext == "css")
			type = "text/css";
		else if (ext == "flv")
			type = "video/x-flv";

		dl = ext == "avi" || ext == "flv";
	}

	log(LL_WARNING, "Sending file %s of type %s", complete_path.c_str(), type.c_str());

	FILE *fh = fopen(complete_path.c_str(), "r");
	if (!fh) {
		log(LL_WARNING, "Cannot access %s: %s", complete_path.c_str(), strerror(errno));

		std::string headers = "HTTP/1.0 404 na\r\nServer: " NAME " " VERSION "\r\n\r\n";

		(void)WRITE(cfd, headers.c_str(), headers.size());

		return;
	}

	std::string name_header = dl ? myformat("Content-Disposition: attachment; filename=\"%s\"\r\n", name) : "";
	std::string headers = "HTTP/1.0 200 OK\r\nServer: " NAME " " VERSION "\r\n" + name_header + "Content-Type: " + type + "\r\n\r\n";
	(void)WRITE(cfd, headers.c_str(), headers.size());

	// FIXME
	while(!feof(fh)) {
		char buffer[4096];

		int rc = fread(buffer, 1, sizeof(buffer), fh);
		if (rc <= 0)
			break;

		if (WRITE(cfd, buffer, rc) <= 0) {
			log(LL_INFO, "Short write");
			break;
		}
	}

	fclose(fh);
}

bool sort_files_last_change(const file_t &left, const file_t & right)
{
	return left.last_change < right.last_change;
}

bool sort_files_size(const file_t &left, const file_t & right)
{
	return left.size < right.size;
}

bool sort_files_name(const file_t &left, const file_t & right)
{
	return left.name.compare(right.name) < 0;
}

void handle_http_client(int cfd, source *s, double fps, int quality, int time_limit, const std::vector<filter *> *const filters, std::atomic_bool *const global_stopflag, resize *const r, const int resize_w, const int resize_h, const bool motion_compatible, configuration_t *const cfg, const std::string & snapshot_dir, const bool allow_admin, const bool archive_acces)
{
	const std::string http_200_header =
			"HTTP/1.0 200\r\n"
			"Cache-Control: no-cache\r\n"
			"Pragma: no-cache\r\n"
			"Server: " NAME " " VERSION "\r\n"
			"Expires: thu, 01 dec 1994 16:00:00 gmt\r\n"
			"Content-Type: text/html\r\n"
			"Connection: close\r\n"
			"\r\n";

	const std::string html_header =
			"<!DOCTYPE html>"
			"<html>"
			"<head>"
			"<link href=\"/stylesheet.css\" rel=\"stylesheet\" media=\"screen\">"
			"<link rel=\"shortcut icon\" href=\"/favicon.ico\">"
			"<title>" NAME " " VERSION "</title>"
			"</head>"
			"<body>";

	const std::string html_tail =
			"<p><br><br><br></p><hr><div id=\"tail\"><p>" NAME " was written by folkert@vanheusden.com</p></div>"
			"</body>"
			"</html>";

	const std::string action_failed = "<h1>%s</h1><p>Action failed</p>";
	const std::string action_succeeded = "<h1>%s</h1><p>Action succeeded</p>";

	sigset_t all_sigs;
	sigfillset(&all_sigs);
	pthread_sigmask(SIG_BLOCK, &all_sigs, NULL);

	char request_headers[65536] = { 0 };
	int h_n = 0;

	struct pollfd fds[1] = { { cfd, POLLIN, 0 } };

	for(;!*global_stopflag && !motion_compatible;) {
		if (poll(fds, 1, 250) == 0)
			continue;

		int rc = read(cfd, &request_headers[h_n], sizeof request_headers - h_n - 1);
		if (rc == -1)
		{
			if (errno == EINTR || errno == EAGAIN)
				continue;

			log(LL_DEBUG, "error receiving request headers");
			close(cfd);
			return;
		}
		else if (rc == 0)
		{
			log(LL_DEBUG, "error receiving request headers");
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
	if (motion_compatible) {
	}
	else if (strncmp(request_headers, "GET ", 4) == 0)
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

	char *pars = NULL;

	if (!motion_compatible) {
		char *dummy = strchr(path, '\r');
		if (!dummy)
			dummy = strchr(path, '\n');
		if (dummy)
			*dummy = 0x00;

		dummy = strchr(path, ' ');
		if (dummy)
			*dummy = 0x00;

		log(LL_DEBUG, "URL: %s", path);

		char *q = strchr(path, '?');
		if (q) {
			*q = 0x00;

			pars = un_url_escape(q + 1);
		}
	}

	if (strcmp(path, "/stream.mjpeg") == 0 || motion_compatible)
		send_mjpeg_stream(cfd, s, fps, quality, get, time_limit, filters, global_stopflag, r, resize_w, resize_h);
	else if (strcmp(path, "/stream.mpng") == 0)
		send_mpng_stream(cfd, s, fps, get, time_limit, filters, global_stopflag, r, resize_w, resize_h);
	else if (strcmp(path, "/image.png") == 0)
		send_png_frame(cfd, s, get, filters, r, resize_w, resize_h);
	else if (strcmp(path, "/image.jpg") == 0)
		send_jpg_frame(cfd, s, get, quality, filters, r, resize_w, resize_h);
	else if (strcmp(path, "/stylesheet.css") == 0) {
		struct stat st;
		if (stat("stylesheet.css", &st) == 0)
			send_file(cfd, "./", "stylesheet.css");
		else {
			std::string reply = http_200_header + "\r\n";

			if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
				log(LL_DEBUG, "short write on response header");
		}
	}
	else if (strcmp(path, "/stream.html") == 0)
	{
		std::string reply = http_200_header + html_header + "<img src=\"stream.mjpeg\">" + html_tail;

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response header");
	}
	else if (strcmp(path, "/index.html") == 0 || strcmp(path, "/") == 0)
	{
		std::string reply = http_200_header + html_header + "<div id=\"main\"><h1>" NAME " " VERSION "</h1>"
			"<ul>"
			"<li><a href=\"/stream.mjpeg\">MJPEG stream</a>"
			"<li><a href=\"/stream.html\">Same MJPEG stream but in a HTML wrapper</a>"
			"<li><a href=\"/stream.mpng\">MPNG stream</a>"
			"<li><a href=\"/image.jpg\">Show snapshot in JPG format</a>"
			"<li><a href=\"/image.png\">Show snapshot in PNG format</a>";

			if (allow_admin) {
				reply += "<li><a href=\"/snapshot-img/\">Take a snapshot and store it on disk</a>"
					"<li><a href=\"/snapshot-video/\">Start a video recording</a>";
			}

			if (archive_acces) {
				reply += "<li><a href=\"/view-snapshots/\">View recordings</a>";
			}

			reply += "</ul></div>"
			;

		if (allow_admin) {
			cfg -> lock.lock();
			for(interface *i : cfg -> interfaces)
				reply += describe_interface(i);
			cfg -> lock.unlock();
		}

		reply += html_tail; 

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response header");
	}
	else if (strncmp(path, "/send-file", 10) == 0 && (archive_acces || allow_admin)) {
		char *file = un_url_escape(pars ? pars : "FAIL");

		if (pars && validate_file(snapshot_dir, file)) {
			send_file(cfd, snapshot_dir, file);
		}
		else {
			log(LL_WARNING, "%s is not a valid file", file);

			std::string reply = "HTTP/1.0 404\r\n\r\nfailed";

			if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
				log(LL_DEBUG, "short write on response header");
		}

		free(file);
	}
	else if (strcmp(path, "/view-snapshots/") == 0 && (archive_acces || allow_admin)) {
		std::string reply = http_200_header + html_header + "<h1>list of files in " + snapshot_dir + "</h1><table border=1>";

		auto *files = load_filelist(snapshot_dir, "");

		if (!pars || strcmp(pars, "last-change") == 0)
			std::sort(files -> begin(), files -> end(), sort_files_last_change);
		else if (strcmp(pars, "name") == 0)
			std::sort(files -> begin(), files -> end(), sort_files_name);
		else if (strcmp(pars, "size") == 0)
			std::sort(files -> begin(), files -> end(), sort_files_size);
		else
			reply += myformat("Ignoring unknown sort method %s", pars);

		reply += "<tr><th><a href=\"?name\">name</a></th><th><a href=\"?last-change\">last change</a></th><th><a href=\"?size\">size</a></th></tr>";

		for(auto file : *files)
			reply += std::string("<tr><td><a href=\"/send-file?") + file.name + "\">" + file.name + "</a></td><td>" + myctime(file.last_change) + "</td><td>" + myformat("%zu", file.size) + "</td>";

		delete files;

		reply += "</table>" + html_tail;

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response header");
	}
	else if (!allow_admin) {
		goto do404;
	}
	else if (strcmp(path, "/pause") == 0 || strcmp(path, "/unpause") == 0) {
		std::string reply = http_200_header + "???";

		cfg -> lock.lock();
		if (pause(cfg, pars ? pars : "", strcmp(path, "/pause") == 0))
			reply = http_200_header + html_header + myformat(action_succeeded.c_str(), "pause/unpause") + html_tail;
		else
			reply = http_200_header + html_header + myformat(action_failed.c_str(), "pause/unpause") + html_tail;
		cfg -> lock.unlock();

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response header");
	}
	else if (strcmp(path, "/start") == 0 || strcmp(path, "/stop") == 0) {
		std::string reply = http_200_header + "???";

		cfg -> lock.lock();
		if (start_stop(cfg, pars ? pars : "", strcmp(path, "/start") == 0))
			reply = http_200_header + html_header + myformat(action_succeeded.c_str(), "start/stop") + html_tail;
		else
			reply = http_200_header + html_header + myformat(action_failed.c_str(), "start/stop") + html_tail;
		cfg -> lock.unlock();

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response header");
	}
	else if (strcmp(path, "/restart") == 0) {
		std::string reply = http_200_header + "???";

		cfg -> lock.lock();
		if (start_stop(cfg, pars ? pars : "", false)) {
			if (start_stop(cfg, pars ? pars : "", true))
				reply = http_200_header + html_header + myformat(action_succeeded.c_str(), "restart") + html_tail;
			else
				reply = http_200_header + html_header + myformat(action_failed.c_str(), "restart") + html_tail;
		}
		cfg -> lock.unlock();

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response header");
	}
	else if (strcmp(path, "/snapshot-img/") == 0) {
		std::string reply = http_200_header + "???";

		if (take_a_picture(s, snapshot_dir, quality))
			reply = http_200_header + html_header + myformat(action_succeeded.c_str(), "snapshot image") + html_tail;
		else
			reply = http_200_header + html_header + myformat(action_failed.c_str(), "snapshot image") + html_tail;

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response header");
	}
	else if (strcmp(path, "/snapshot-video/") == 0) {
		interface *i = start_a_video(s, snapshot_dir, quality);

		std::string reply = http_200_header + "???";
		if (i) {
			reply = http_200_header + html_header + myformat(action_succeeded.c_str(), "snapshot image") + html_tail;

			cfg -> lock.lock();
			cfg -> interfaces.push_back(i);
			cfg -> lock.unlock();
		}
		else {
			reply = http_200_header + html_header + myformat(action_failed.c_str(), "snapshot image") + html_tail;
		}

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response header");
	}
	else if (strncmp(path, "/favicon.ico", 6) == 0) {
		struct stat st;
		if (stat("favicon.ico", &st) == 0)
			send_file(cfd, "./", "favicon.ico");
		else {
			std::string reply = http_200_header + "\r\n";

			if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
				log(LL_DEBUG, "short write on response header");
		}
	}
	else if (strncmp(path, "/rest/", 6) == 0) {
		std::string reply = run_rest(cfg, &path[6], pars ? pars : "", quality, snapshot_dir, s);

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response");
	}
	else {
	do404:
		log(LL_INFO, "Path %s not found", path);

		if (WRITE(cfd, "HTTP/1.0 404\r\n\r\nwhat?", 4) <= 0)
			log(LL_DEBUG, "short write on response header");
	}

	free(path);
	free(pars);

	close(cfd);
}

void * handle_http_client_thread(void *ct_in)
{
	http_thread_t *ct = (http_thread_t *)ct_in;

	set_thread_name("http_client");

	ct -> s -> register_user();

	handle_http_client(ct -> fd, ct -> s, ct -> fps, ct -> quality, ct -> time_limit, ct -> filters, ct -> global_stopflag, ct -> r, ct -> resize_w, ct -> resize_h, ct -> motion_compatible, ct -> cfg, ct -> snapshot_dir, ct -> allow_admin, ct -> archive_acces);

	ct -> s -> unregister_user();

	delete ct;

	return NULL;
}

http_server::http_server(configuration_t *const cfg, const std::string & id, const std::string & http_adapter, const int http_port, source *const src, const double fps, const int quality, const int time_limit, const std::vector<filter *> *const f, resize *const r, const int resize_w, const int resize_h, const bool motion_compatible, const bool allow_admin, const bool archive_acces, const std::string & snapshot_dir) : cfg(cfg), interface(id), src(src), fps(fps), quality(quality), time_limit(time_limit), f(f), r(r), resize_w(resize_w), resize_h(resize_h), motion_compatible(motion_compatible), allow_admin(allow_admin), archive_acces(archive_acces), snapshot_dir(snapshot_dir)
{
	fd = start_listen(http_adapter.c_str(), http_port, 5);
	ct = CT_HTTPSERVER;

	d = myformat("[%s]:%d", http_adapter.c_str(), http_port);
}

http_server::~http_server()
{
	close(fd);

	free_filters(f);

	delete f;
}

void http_server::operator()()
{
	set_thread_name("http_server");

	struct pollfd fds[] = { { fd, POLLIN, 0 } };

	std::vector<pthread_t> handles;

	for(;!local_stop_flag;) {
		for(size_t i=0; i<handles.size();) {
			if (pthread_tryjoin_np(handles.at(i), NULL) == 0)
				handles.erase(handles.begin() + i);
			else
				i++;
		}

		getMeta() -> setInt("http-viewers", std::pair<uint64_t, int>(get_us(), handles.size()));

		pauseCheck();

		if (poll(fds, 1, 500) == 0)
			continue;

		int cfd = accept(fd, NULL, 0);
		if (cfd == -1)
			continue;

		log(LL_INFO, "HTTP connected with: %s", get_endpoint_name(cfd).c_str());

		http_thread_t *ct = new http_thread_t;

		ct -> fd = cfd;
		ct -> s = src;
		ct -> fps = fps;
		ct -> quality = quality;
		ct -> time_limit = time_limit;
		ct -> filters = f;
		ct -> global_stopflag = &local_stop_flag;
		ct -> r = r;
		ct -> resize_w = resize_w;
		ct -> resize_h = resize_h;
		ct -> motion_compatible = motion_compatible;
		ct -> cfg = cfg;
		ct -> snapshot_dir = snapshot_dir;
		ct -> allow_admin = allow_admin;
		ct -> archive_acces = archive_acces;

		pthread_t th;
		int rc = -1;
		if ((rc = pthread_create(&th, NULL, handle_http_client_thread, ct)) != 0)
		{
			errno = rc;
			error_exit(true, "pthread_create failed (http server)");
		}

		handles.push_back(th);
	}

	for(size_t i=0; i<handles.size(); i++)
		pthread_join(handles.at(i), NULL);

	log(LL_INFO, "HTTP server thread terminating");
}
