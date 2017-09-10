// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <dlfcn.h>
#include <jansson.h>
#include <set>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <curl/curl.h>

#include "config.h"
#include "error.h"
#include "utils.h"
#include "source.h"
#include "source_v4l.h"
#include "source_http_jpeg.h"
#include "source_http_mjpeg.h"
#include "source_rtsp.h"
#include "source_plugin.h"
#include "http_server.h"
#include "motion_trigger.h"
#include "target.h"
#ifdef WITH_GWAVI
#include "target_avi.h"
#endif
#include "target_ffmpeg.h"
#include "target_jpeg.h"
#include "target_plugin.h"
#include "target_extpipe.h"
#include "filter.h"
#include "filter_mirror_v.h"
#include "filter_mirror_h.h"
#include "filter_noise_neighavg.h"
#include "filter_add_text.h"
#include "filter_add_scaled_text.h"
#include "filter_grayscale.h"
#include "filter_boost_contrast.h"
#include "filter_marker_simple.h"
#include "v4l2_loopback.h"
#include "filter_overlay.h"
#include "picio.h"
#include "filter_plugin.h"
#include "log.h"
#include "cfg.h"
#include "resize_cairo.h"
#include "filter_motion_only.h"

const char *json_str(const json_t *const in, const char *const key, const char *const descr)
{
	json_t *j_value = json_object_get(in, key);

	if (!j_value)
		error_exit(false, "\"%s\" missing (%s)", key, descr);

	char *value = NULL;
	if (json_unpack(j_value, "s", &value) == -1)
		error_exit(false, "\"%s\" does not have a string value", key);

	return value;
}

const char *json_str_optional(const json_t *const in, const char *const key)
{
	json_t *j_value = json_object_get(in, key);

	if (!j_value)
		return "";

	char *value = NULL;
	if (json_unpack(j_value, "s", &value) == -1)
		error_exit(false, "\"%s\" does not have a string value", key);

	return value;
}

bool json_bool(const json_t *const in, const char *const key, const char * const descr)
{
	json_t *j_value = json_object_get(in, key);

	if (!j_value)
		error_exit(false, "\"%s\" missing (%s)", key, descr);

	bool value = false;
	if (json_unpack(j_value, "b", &value) == -1)
		error_exit(false, "\"%s\" does not have a boolean value (use \"true\" or \"false\" without quotes)", key);

	return value;
}

int json_int(const json_t *const in, const char *const key, const char * const descr)
{
	json_t *j_value = json_object_get(in, key);

	if (!j_value)
		error_exit(false, "\"%s\" missing (%s)", key, descr);

	int value = 0;
	if (json_unpack(j_value, "i", &value) == -1)
		error_exit(false, "\"%s\" does not have an int value (do not put quotes around the value)", key);

	return value;
}

double json_float(const json_t *const in, const char *const key, const char * const descr)
{
	json_t *j_value = json_object_get(in, key);

	if (!j_value)
		error_exit(false, "\"%s\" missing (%s)", key, descr);

	double value = 0;
	if (json_unpack(j_value, "f", &value) == -1)
		error_exit(false, "\"%s\" does not have a float value (do not put quotes around the value, and make sure there's a dot in it)", key);

	return value;
}

void add_filters(std::vector<filter *> *af, const std::vector<filter *> *const in)
{
	for(filter *f : *in)
		af -> push_back(f);
}

uint8_t *load_selection_bitmap(const char *const selection_bitmap)
{
	uint8_t *sb = NULL;

	if (selection_bitmap[0]) {
		FILE *fh = fopen(selection_bitmap, "rb");
		if (!fh)
			error_exit(true, "Cannot open file \"%s\"", selection_bitmap);

		int w, h;
		load_PBM_file(fh, &w, &h, &sb);

		fclose(fh);
	}

	return sb;
}

bool find_interval_or_fps(const json_t *const in, double *const interval, const std::string & fps_name, double *const fps)
{
	json_t *j_interval = json_object_get(in, "interval");
	json_t *j_fps = json_object_get(in, fps_name.c_str());

	if (!j_interval && !j_fps)
		return false;

	if (j_interval) {
		*interval = json_float(in, "interval", "interval or fps");

		if (*interval == 0)
			return false;

		if (*interval < 0)
			*fps = -1.0;
		else
			*fps = 1.0 / *interval;
	}
	else {
		*fps = json_float(in, fps_name.c_str(), "interval or fps");

		if (*fps == 0)
			return false;

		if (*fps <= 0)
			*interval = -1.0;
		else
			*interval = 1.0 / *fps;
	}

	return true;
}

std::vector<filter *> *load_filters(const json_t *const in)
{
	std::vector<filter *> *const filters = new std::vector<filter *>();

	size_t n_std = json_array_size(in);

	for(size_t i=0; i<n_std; i++) {
		json_t *ae = json_array_get(in, i);

		const char *s_type = json_str(ae, "type", "filter-type (h-mirror, marker, etc)");
		if (strcasecmp(s_type, "null") == 0)
			filters -> push_back(new filter());
		else if (strcasecmp(s_type, "h-mirror") == 0)
			filters -> push_back(new filter_mirror_h());
		else if (strcasecmp(s_type, "v-mirror") == 0)
			filters -> push_back(new filter_mirror_v());
		else if (strcasecmp(s_type, "neighbour-noise-filter") == 0)
			filters -> push_back(new filter_noise_neighavg());
		else if (strcasecmp(s_type, "grayscale") == 0)
			filters -> push_back(new filter_grayscale());
		else if (strcasecmp(s_type, "filter-plugin") == 0) {
			const char *file = json_str(ae, "file", "filename of filter plugin");
			const char *par = json_str(ae, "par", "parameter for filter plugin");

			filters -> push_back(new filter_plugin(file, par));
		}
		else if (strcasecmp(s_type, "marker") == 0) {
			const char *s_position = json_str(ae, "mode", "marker mode, e.g. red, red-invert or invert");

			sm_mode_t sm = m_red;

			if (strcasecmp(s_position, "red") == 0)
				sm = m_red;
			else if (strcasecmp(s_position, "red-invert") == 0)
				sm = m_red_invert;
			else if (strcasecmp(s_position, "invert") == 0)
				sm = m_invert;

			const char *selection_bitmap = json_str(ae, "selection-bitmap", "bitmaps indicating which pixels to look at. must be same size as webcam image and must be a .pbm-file. leave empty to disable.");
			const uint8_t *sb = load_selection_bitmap(selection_bitmap);

			filters -> push_back(new filter_marker_simple(sm, sb));
		}
		else if (strcasecmp(s_type, "motion-only") == 0) {
			const char *selection_bitmap = json_str(ae, "selection-bitmap", "bitmaps indicating which pixels to look at. must be same size as webcam image and must be a .pbm-file. leave empty to disable.");
			const uint8_t *sb = load_selection_bitmap(selection_bitmap);

			filters -> push_back(new filter_motion_only(sb));
		}
		else if (strcasecmp(s_type, "boost-contrast") == 0)
			filters -> push_back(new filter_boost_contrast());
		else if (strcasecmp(s_type, "overlay") == 0) {
			const char *s_pic = json_str(ae, "picture", "PNG to overlay (with alpha channel!)");
			int x = json_int(ae, "x", "x-coordinate of overlay");
			int y = json_int(ae, "y", "y-coordinate of overlay");

			filters -> push_back(new filter_overlay(s_pic, x, y));
		}
		else if (strcasecmp(s_type, "text") == 0) {
			const char *s_text = json_str(ae, "text", "what text to show");
			const char *s_position = json_str(ae, "position", "where to put it, e.g. upper-left, lower-center, center-right");

			text_pos_t tp = center_center;

			if (strcasecmp(s_position, "upper-left") == 0)
				tp = upper_left;
			else if (strcasecmp(s_position, "upper-center") == 0)
				tp = upper_center;
			else if (strcasecmp(s_position, "upper-right") == 0)
				tp = upper_right;
			else if (strcasecmp(s_position, "center-left") == 0)
				tp = center_left;
			else if (strcasecmp(s_position, "center-center") == 0)
				tp = center_center;
			else if (strcasecmp(s_position, "center-right") == 0)
				tp = center_right;
			else if (strcasecmp(s_position, "lower-left") == 0)
				tp = lower_left;
			else if (strcasecmp(s_position, "lower-center") == 0)
				tp = lower_center;
			else if (strcasecmp(s_position, "lower-right") == 0)
				tp = lower_right;
			else
				error_exit(false, "(text-)position %s is not understood", s_position);

			filters -> push_back(new filter_add_text(s_text, tp));
		}
		else if (strcasecmp(s_type, "scaled-text") == 0) {
			const char *s_text = json_str(ae, "text", "what text to show");
			const char *font = json_str(ae, "font", "which font to use");
			int x = json_int(ae, "x", "x-coordinate of text");
			int y = json_int(ae, "y", "y-coordinate of text");
			int fs = json_int(ae, "font-size", "font size (in pixels)");
			int r = json_int(ae, "r", "red component of text color");
			int g = json_int(ae, "g", "green component of text color");
			int b = json_int(ae, "b", "blue component of text color");

			filters -> push_back(new filter_add_scaled_text(s_text, font, x, y, fs, r, g, b));
		}
		else {
			error_exit(false, "Filter %s is not known", s_type);
		}
	}

	return filters;
}

stream_plugin_t * load_stream_plugin(const json_t *const in)
{
	const char *file = json_str(in, "stream-writer-plugin-file", "filename of stream writer plugin");
	if (file[0] == 0x00)
		return NULL;

	stream_plugin_t *sp = new stream_plugin_t;

	sp -> par = json_str(in, "stream-writer-plugin-parameter", "parameter for stream writer plugin");

	void *library = dlopen(file, RTLD_NOW);
	if (!library)
		error_exit(true, "Failed opening motion detection plugin library %s", file);

	sp -> init_plugin = (init_plugin_t)find_symbol(library, "init_plugin", "stream writer plugin", file);
	sp -> open_file = (open_file_t)find_symbol(library, "open_file", "stream writer plugin", file);
	sp -> write_frame = (write_frame_t)find_symbol(library, "write_frame", "stream writer plugin", file);
	sp -> close_file = (close_file_t)find_symbol(library, "close_file", "stream writer plugin", file);
	sp -> uninit_plugin = (uninit_plugin_t)find_symbol(library, "uninit_plugin", "stream writer plugin", file);

	return sp;
}

void interval_fps_error(const char *const name, const char *what, const char *id)
{
	error_exit(false, "Interval/%s %s not set or invalid (e.g. 0) for target (%s). Make sure that you use a float-value for the fps/interval, e.g. 13.0 instead of 13", name, what, id);
}

target * load_target(const json_t *const j_in, source *const s)
{
	target *t = NULL;

	std::string id = json_str_optional(j_in, "id");
	const char *path = json_str(j_in, "path", "directory to write to");
	const char *prefix = json_str(j_in, "prefix", "string to begin filename with");
	const char *exec_start = json_str(j_in, "exec-start", "script to start when recording starts");
	const char *exec_cycle = json_str(j_in, "exec-cycle", "script to start when the record file is restarted");
	const char *exec_end = json_str(j_in, "exec-end", "script to start when the recording stops");
	int restart_interval = json_int(j_in, "restart-interval", "after how many seconds should the stream-file be restarted");
#ifdef WITH_GWAVI
	const char *format = json_str(j_in, "format", "AVI, EXTPIPE, FFMPEG (for mp4, flv, etc), JPEG or PLUGIN");
#else
	const char *format = json_str(j_in, "format", "EXTPIPE, FFMPEG (for mp4, flv, etc), JPEG or PLUGIN");
#endif
	int jpeg_quality = json_int(j_in, "quality", "JPEG quality, this influences the size");

	double fps = 0, interval = 0;
	if (!find_interval_or_fps(j_in, &interval, "fps", &fps))
		interval_fps_error("fps", "for writing frames to disk (FPS as the video frames are retrieved)", id.c_str());

	double override_fps = json_float(j_in, "override-fps", "What FPS to use in the output file. That way you can let the resulting file be played faster (or slower) than how the stream is obtained from the camera. -1.0 to disable");

	std::vector<filter *> *filters = load_filters(json_object_get(j_in, "filters"));

	if (strcasecmp(format, "AVI") == 0)
#ifdef WITH_GWAVI
		t = new target_avi(id, s, path, prefix, jpeg_quality, restart_interval, interval, filters, exec_start, exec_cycle, exec_end, override_fps);
#else
		error_exit(false, "libgwavi not compiled in");
#endif
	else if (strcasecmp(format, "EXTPIPE") == 0) {
		std::string cmd = json_str(j_in, "cmd", "Command to send the frames to");

		t = new target_extpipe(id, s, path, prefix, jpeg_quality, restart_interval, interval, filters, exec_start, exec_cycle, exec_end, cmd);
	}
	else if (strcasecmp(format, "FFMPEG") == 0) {
		int bitrate = json_int(j_in, "bitrate", "How many bits per second to emit. For 352x288 200000 is a sane value. This value affects the quality.");
		std::string type = json_str(j_in, "ffmpeg-type", "E.g. flv, mp4");
		const char *const parameters = json_str_optional(j_in, "ffmpeg-parameters");

		t = new target_ffmpeg(id, parameters, s, path, prefix, restart_interval, interval, type, bitrate, filters, exec_start, exec_cycle, exec_end, override_fps);
	}
	else if (strcasecmp(format, "JPEG") == 0)
		t = new target_jpeg(id, s, path, prefix, jpeg_quality, restart_interval, interval, filters, exec_start, exec_cycle, exec_end);
	else if (strcasecmp(format, "PLUGIN") == 0) {
		stream_plugin_t *sp = load_stream_plugin(j_in);

		t = new target_plugin(id, s, path, prefix, jpeg_quality, restart_interval, interval, filters, exec_start, exec_cycle, exec_end, sp, override_fps);
	}
	else {
		error_exit(false, "Format %s is unknown (stream to disk backends)", format);
	}

	return t;
}

void version()
{
	printf(NAME " " VERSION "\n");
	printf("(C) 2017 by Folkert van Heusden\n\n");
}

void help()
{
	printf("-c x   select configuration file\n");
	printf("-f     fork into the background\n");
	printf("-p x   file to write PID to\n");
	printf("-v     enable verbose mode\n");
	printf("-V     show version & exit\n");
	printf("-h     this help\n");
}

int main(int argc, char *argv[])
{
	const char *pid_file = NULL;
	const char *cfg_file = "constatus.conf";
	bool do_fork = false, verbose = false;
	int ll = LL_INFO;

	int c = -1;
	while((c = getopt(argc, argv, "c:p:fhvV")) != -1) {
		switch(c) {
			case 'c':
				cfg_file = optarg;
				break;

			case 'p':
				pid_file = optarg;
				break;

			case 'f':
				do_fork = true;
				break;

			case 'h':
				help();
				return 0;

			case 'v':
				verbose = true;
				ll++;
				break;

			case 'V':
				version();
				return 0;

			default:
				help();
				return 1;
		}
	}

	signal(SIGCHLD, SIG_IGN);

	log(LL_INFO, "Loading %s...", cfg_file);

	curl_global_init(CURL_GLOBAL_ALL);

	FILE *fh = fopen(cfg_file, "r");
	if (!fh)
		error_exit(true, "Cannot access configuration file '%s'", cfg_file);

	json_error_t error;
	json_t *json_cfg = json_loadf(fh, 0, &error);
	if (!json_cfg)
		error_exit(false, "At column %d in line %d the following JSON problem was found in the configuration file: %s (%s", error.column, error.line, error.text, error.source);

	fclose(fh);

	//***

	const char *const general_name = "general";
	json_t *j_gen = json_object_get(json_cfg, general_name);
	const char *logfile = json_str(j_gen, "logfile", "file where to store logging");

	int loglevel = -1;

	if (verbose) {
		loglevel = ll;
		setlogfile(logfile[0] ? logfile : NULL, loglevel);
	}
	else {
		const char *ll = json_str(j_gen, "log-level", "log level (fatal, error, warning, info, debug, debug-verbose)");
		if (strcasecmp(ll, "fatal") == 0)
			loglevel = LL_FATAL;
		else if (strcasecmp(ll, "error") == 0)
			loglevel = LL_ERR;
		else if (strcasecmp(ll, "warning") == 0)
			loglevel = LL_WARNING;
		else if (strcasecmp(ll, "info") == 0)
			loglevel = LL_INFO;
		else if (strcasecmp(ll, "debug") == 0)
			loglevel = LL_DEBUG;
		else if (strcasecmp(ll, "debug-verbose") == 0)
			loglevel = LL_DEBUG_VERBOSE;
		else
			error_exit(false, "Log level '%s' not recognized", ll);

		setlogfile(logfile[0] ? logfile : NULL, loglevel);
	}

	std::string resize_type = json_str(j_gen, "resize-type", "can be regular or cairo. selects what method will be used for resizing the video stream (if requested).");

	resize *r = NULL;
	if (resize_type == "cairo") {
		r = new resize_cairo();
	}
	else if (resize_type == "regular") {
		r = new resize();
	}
	else {
		error_exit(false, "Scaler/resizer of type \"%s\" is not known", resize_type.c_str());
	}


	log(LL_INFO, " *** " NAME " v" VERSION " starting ***");

	//***

	configuration_t cfg;
	cfg.lock.lock();

	const char *const source_name = "source";
	json_t *j_source = json_object_get(json_cfg, source_name);

	source *s = NULL;

	{
		log(LL_INFO, "Configuring the video-source...");
		const char *s_type = json_str(j_source, "type", "source-type");

		std::string id = json_str_optional(j_source, "id");
		double max_fps = json_float(j_source, "max-fps", "limit the number of frames per second acquired to this value or -1.0 to disable");
		if (max_fps == 0)
			error_exit(false, "Video-source: max-fps must be either > 0 or -1.0. Use -1.0 for no FPS limit.");

		int resize_w = json_int(j_source, "resize-width", "resize picture width to this (-1 to disable)");
		int resize_h = json_int(j_source, "resize-height", "resize picture height to this (-1 to disable)");

		if (strcasecmp(s_type, "v4l") == 0) {
			bool pref_jpeg = json_bool(j_source, "prefer-jpeg", "if the camera is capable of JPEG, should that be used");
			bool rpi_wa = json_bool(j_source, "rpi-workaround", "the raspberry pi camera has a bug, this enables a workaround");
			int w = json_int(j_source, "width", "width of picture");
			int h = json_int(j_source, "height", "height of picture");
			int jpeg_quality = json_int(j_source, "quality", "JPEG quality, this influences the size");

			s = new source_v4l(id, json_str(j_source, "device", "linux v4l2 device"), pref_jpeg, rpi_wa, jpeg_quality, max_fps, w, h, r, resize_w, resize_h, loglevel);
		}
		else if (strcasecmp(s_type, "jpeg") == 0) {
			bool ign_cert = json_bool(j_source, "ignore-cert", "ignore SSL errors");
			const char *auth = json_str(j_source, "http-auth", "HTTP authentication string");
			const char *url = json_str(j_source, "url", "address of JPEG stream");

			s = new source_http_jpeg(id, url, ign_cert, auth, max_fps, r, resize_w, resize_h, loglevel);
		}
		else if (strcasecmp(s_type, "mjpeg") == 0) {
			const char *url = json_str(j_source, "url", "address of MJPEG stream");
			bool ign_cert = json_bool(j_source, "ignore-cert", "ignore SSL errors");

			s = new source_http_mjpeg(id, url, ign_cert, max_fps, r, resize_w, resize_h, loglevel);
		}
		else if (strcasecmp(s_type, "rtsp") == 0) {
			const char *url = json_str(j_source, "url", "address of JPEG stream");

			s = new source_rtsp(id, url, max_fps, r, resize_w, resize_h, loglevel);
		}
		else if (strcasecmp(s_type, "plugin") == 0) {
			std::string plugin_bin = json_str(j_source, "source-plugin-file", "filename of video data source plugin");
			std::string plugin_arg = json_str(j_source, "source-plugin-parameter", "parameter for video data source plugin");

			s = new source_plugin(id, plugin_bin, plugin_arg, max_fps, r, resize_w, resize_h, loglevel);
		}
		else {
			log(LL_FATAL, " no source defined!");
		}

		if (s)
			s -> start();
	}

	//***

	// listen adapter, listen port, source, fps, jpeg quality, time limit (in seconds)
	log(LL_INFO, "Configuring the HTTP server(s)...");
	const char *const http_server_name = "http-server";
	json_t *j_hls = json_object_get(json_cfg, http_server_name);
	if (j_hls) {
		size_t n_hl = json_array_size(j_hls);
		log(LL_DEBUG, " %zu http server(s)", n_hl);

		if (n_hl == 0)
			log(LL_WARNING, " 0 servers, is that correct?");

		for(size_t i=0; i<n_hl; i++) {
			json_t *hle = json_array_get(j_hls, i);

			std::string id = json_str_optional(hle, "id");
			const char *listen_adapter = json_str(hle, "listen-adapter", "network interface to listen on or 0.0.0.0 for all");
			int listen_port = json_int(hle, "listen-port", "port to listen on");
			printf(" HTTP server listening on %s:%d\n", listen_adapter, listen_port);
			int jpeg_quality = json_int(hle, "quality", "JPEG quality, this influences the size");
			int time_limit = json_int(hle, "time-limit", "how long (in seconds) to stream before the connection is closed");
			int resize_w = json_int(hle, "resize-width", "resize picture width to this (-1 to disable)");
			int resize_h = json_int(hle, "resize-height", "resize picture height to this (-1 to disable)");
			bool motion_compatible = json_bool(hle, "motion-compatible", "only stream MJPEG and do not wait for HTTP request");
			bool allow_admin = json_bool(hle, "allow-admin", "when enabled, you can partially configure services");
			bool archive_access = json_bool(hle, "archive-access", "when enabled, you can retrieve recorded video/images");
			std::string snapshot_dir = json_str(hle, "snapshot-dir", "where to store snapshots (triggered by HTTP server). see \"allow-admin\".");

			std::vector<filter *> *http_filters = load_filters(json_object_get(hle, "filters"));

			double fps = 0, interval = 0;
			if (!find_interval_or_fps(hle, &interval, "fps", &fps))
				interval_fps_error("fps", "for showing video frames", id.c_str());

			interface *h = new http_server(&cfg, id, listen_adapter, listen_port, s, fps, jpeg_quality, time_limit, http_filters, r, resize_w, resize_h, motion_compatible, allow_admin, archive_access, snapshot_dir);
			h -> start();
			cfg.interfaces.push_back(h);
		}
	}
	else {
		log(LL_INFO, " no HTTP server");
	}

	//***

	log(LL_INFO, "Configuring the video-loopback...");
	const char *const video_loopback_name = "video-loopback";
	json_t *j_vl = json_object_get(json_cfg, video_loopback_name);
	if (j_vl) {
		std::string id = json_str_optional(j_vl, "id");
		const char *dev = json_str(j_vl, "device", "Linux v4l2 device to connect to");

		double fps = 0, interval = 0;
		if (!find_interval_or_fps(j_vl, &interval, "fps", &fps))
			interval_fps_error("fps", "for sending frames to loopback", id.c_str());

		std::vector<filter *> *loopback_filters = load_filters(json_object_get(j_vl, "filters"));

		interface *f = new v4l2_loopback(id, s, fps, dev, loopback_filters);
		f -> start();
		cfg.interfaces.push_back(f);
	}
	else {
		log(LL_INFO, " no video loopback");
	}

	//***

	// source, jpeg quality, noise factor, pixels changed % trigger, record min n frames, ignore n frames after record, path to store mjpeg files in, filename prefix, max file duration, camera-warm-up(ignore first x frames), pre-record-count(how many frames to store of just-before-the-motion-started)
	//start_motion_trigger_thread(s, 75, 32, 0.6, 15, 5, "./", "motion-", 3600, 10, 15, &filters_before, &filters_after);
	log(LL_INFO, "Configuring the motion trigger(s)...");
	ext_trigger_t *et = NULL;
	const char *const motion_trigger_name = "motion-trigger";
	json_t *j_mts = json_object_get(json_cfg, motion_trigger_name);
	if (j_mts) {
		size_t n_mt = json_array_size(j_mts);
		log(LL_DEBUG, " %zu motion trigger(s)", n_mt);

		if (n_mt == 0)
			log(LL_WARNING, " 0 triggers, is that correct?");

		for(size_t i=0; i<n_mt; i++) {
			json_t *mte = json_array_get(j_mts, i);

			std::string id = json_str_optional(j_vl, "id");
			int noise_level = json_int(mte, "noise-factor", "at what difference levell is the pixel considered to be changed");
			double pixels_changed_perctange = json_float(mte, "pixels-changed-percentage", "what %% of pixels need to be changed before the motion trigger is triggered");
			int min_duration = json_int(mte, "min-duration", "minimum number of frames to record");
			int mute_duration = json_int(mte, "mute-duration", "how long not to record (in frames) after motion has stopped");
			int warmup_duration = json_int(mte, "warmup-duration", "how many frames to ignore so that the camera can warm-up");
			int pre_motion_record_duration = json_int(mte, "pre-motion-record-duration", "how many frames to record that happened before the motion started");
			double max_fps = json_float(mte, "max-fps", "maximum number of frames per second to analyze (or -1.0 for no limit)");
			if (max_fps == 0)
				error_exit(false, "Motion triggers: max-fps must be either > 0 or -1.0. Use -1.0 for no FPS limit.");

			const char *selection_bitmap = json_str(mte, "selection-bitmap", "bitmaps indicating which pixels to look at. must be same size as webcam image and must be a .pbm-file. leave empty to disable.");

			std::vector<filter *> *filters_detection = load_filters(json_object_get(mte, "filters-detection"));

/////////////
			json_t *j_targets = json_object_get(mte, "targets");
			size_t n_targets = json_array_size(j_targets);
			log(LL_DEBUG, " %zu motion target(s)", n_targets);

			if (n_targets == 0)
				log(LL_WARNING, " 0 motion stream-targets, is that correct?");

			std::vector<target *> *motion_targets = new std::vector<target *>();
			for(size_t i=0; i<n_targets; i++) {
				json_t *j_target = json_array_get(j_targets, i);

				motion_targets -> push_back(load_target(j_target, s));
			}
//////////

			const char *file = json_str(mte, "trigger-plugin-file", "filename of motion detection plugin");
			if (file[0]) {
				et = new ext_trigger_t;
				et -> par = json_str(mte, "trigger-plugin-parameter", "parameter for motion detection plugin");
				et -> library = dlopen(file, RTLD_NOW);
				if (!et -> library)
					error_exit(true, "Failed opening motion detection plugin library %s", file);

				et -> init_motion_trigger = (init_motion_trigger_t)find_symbol(et -> library, "init_motion_trigger", "motion detection plugin", file);
				et -> detect_motion = (detect_motion_t)find_symbol(et -> library, "detect_motion", "motion detection plugin", file);
				et -> uninit_motion_trigger = (uninit_motion_trigger_t)find_symbol(et -> library, "uninit_motion_trigger", "motion detection plugin", file);
			}

			const uint8_t *sb = load_selection_bitmap(selection_bitmap);

			interface *m = new motion_trigger(id, s, noise_level, pixels_changed_perctange, min_duration, mute_duration, warmup_duration, pre_motion_record_duration, filters_detection, motion_targets, sb, et, max_fps);
			m -> start();
			cfg.interfaces.push_back(m);
		}
	}
	else {
		log(LL_INFO, " no motion trigger defined");
	}

	//***

	// store all there's to see in a file
	// source, path, filename prefix, jpeg quality, max duration per file, ignore timelapse/fps
	// timelaps/fps value: 0.1 means 10fps, 10 means 1 frame every 10 seconds
	//start_store_thread(s, "./", "all-", 25, 3600, -1, NULL, &filters_after);

	// store all there's to see in a file, timelapse
	// source, path, filename prefix, jpeg quality, max duration per file, time lapse interval/fps
	//start_store_thread(s, "./", "tl-", 100, 86400, 60, NULL, &filters_after);
	log(LL_INFO, "Configuring the stream-to-disk backend(s)...");
	const char *const stream_to_disk_name = "stream-to-disk";
	json_t *j_std = json_object_get(json_cfg, stream_to_disk_name);
	if (j_std) {
		size_t n_std = json_array_size(j_std);
		log(LL_DEBUG, " %zu disk streams", n_std);

		if (n_std == 0)
			log(LL_WARNING, " 0 streams, is that correct?");

		for(size_t i=0; i<n_std; i++) {
			json_t *ae = json_array_get(j_std, i);

			interface *t = load_target(ae, s);
			t -> start();
			cfg.interfaces.push_back(t);
		}
	}
	else {
		log(LL_INFO, " no stream-to-disk backends defined");
	}

	if (!s)
		error_exit(false, "No video-source selected");

	std::set<std::string> valid_keys;
	valid_keys.insert(general_name);
	valid_keys.insert(source_name);
	valid_keys.insert(http_server_name);
	valid_keys.insert(video_loopback_name);
	valid_keys.insert(motion_trigger_name);
	valid_keys.insert(stream_to_disk_name);

	const char *key = NULL;
	json_t *value = NULL;
	json_object_foreach(json_cfg, key, value) {
		if (valid_keys.find(key) == valid_keys.end())
			error_exit(false, "Entry \"%s\" in %s is not understood", key, cfg_file);
	}

	if (pid_file && !do_fork)
		log(LL_WARNING, "Will not write a PID file when not forking");

	if (do_fork) {
		if (daemon(0, 0) == -1)
			error_exit(true, "daemon() failed");

		if (pid_file) {
			FILE *fh = fopen(pid_file, "w");
			if (!fh)
				error_exit(true, "Cannot create %s", pid_file);


			fprintf(fh, "%d", getpid());
			fclose(fh);
		}

		for(;;)
			sleep(86400);
	}

	if (s)
		cfg.interfaces.push_back(s);

	cfg.lock.unlock();

	log(LL_INFO, "System started");

	getchar();

	log(LL_INFO, "Terminating");

	cfg.lock.lock();

	for(interface *t : cfg.interfaces)
		t -> stop();

	log(LL_DEBUG, "Waiting for threads to terminate");

	for(interface *t : cfg.interfaces)
		delete t;

	json_decref(json_cfg);

	delete r;

	if (et) {
		dlclose(et -> library);
		delete et;
	}

	curl_global_cleanup();

	log(LL_INFO, "Bye bye");

	return 0;
}
