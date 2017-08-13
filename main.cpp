// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <dlfcn.h>
#include <jansson.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <curl/curl.h>

#include "error.h"
#include "source.h"
#include "source_v4l.h"
#include "source_http_jpeg.h"
#include "source_http_mjpeg.h"
#include "source_rtsp.h"
#include "http_server.h"
#include "motion_trigger.h"
#include "write_stream_to_file.h"
#include "filter.h"
#include "filter_mirror_v.h"
#include "filter_mirror_h.h"
#include "filter_noise_neighavg.h"
#include "filter_add_text.h"
#include "filter_add_scaled_text.h"
#include "filter_grayscale.h"
#include "filter_boost_contrast.h"
#include "filter_marker_simple.h"
#include "push_to_vloopback.h"
#include "filter_overlay.h"
#include "picio.h"
#include "filter_ext.h"
#include "log.h"

const char *json_str(const json_t *const in, const char *const key, const char *const descr)
{
	json_t *j_value = json_object_get(in, key);

	if (!j_value)
		error_exit(false, "\"%s\" missing (%s)", key, descr);

	return json_string_value(j_value);
}

bool json_bool(const json_t *const in, const char *const key, const char * const descr)
{
	json_t *j_value = json_object_get(in, key);

	if (!j_value)
		error_exit(false, "\"%s\" missing (%s)", key, descr);

	return json_boolean_value(j_value);
}

int json_int(const json_t *const in, const char *const key, const char * const descr)
{
	json_t *j_value = json_object_get(in, key);

	if (!j_value)
		error_exit(false, "\"%s\" missing (%s)", key, descr);

	return json_integer_value(j_value);
}

double json_float(const json_t *const in, const char *const key, const char * const descr)
{
	json_t *j_value = json_object_get(in, key);

	if (!j_value)
		error_exit(false, "\"%s\" missing (%s)", key, descr);

	return json_real_value(j_value);
}

void add_filters(std::vector<filter *> *af, const std::vector<filter *> *const in)
{
	for(filter *f : *in)
		af -> push_back(f);
}

bool *load_selection_bitmap(const char *const selection_bitmap)
{
	bool *sb = NULL;

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

			filters -> push_back(new filter_ext(file, par));
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
			bool *sb = load_selection_bitmap(selection_bitmap);

			filters -> push_back(new filter_marker_simple(sm, sb));
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

void version()
{
	printf(NAME " " VERSION "\n");
	printf("(C) 2017 by Folkert van Heusden\n\n");
}

void help()
{
	printf("-c x   select configuration file\n");
	printf("-f     fork into the background\n");
	printf("-V     show version & exit\n");
	printf("-h     this help\n");
}

int main(int argc, char *argv[])
{
	const char *cfg_file = "constatus.conf";
	bool do_fork = false;

	int c = -1;
	while((c = getopt(argc, argv, "c:f:hV")) != -1) {
		switch(c) {
			case 'c':
				cfg_file = optarg;
				break;

			case 'f':
				do_fork = true;
				break;

			case 'h':
				help();
				return 0;

			case 'V':
				version();
				return 0;

			default:
				help();
				return 1;
		}
	}

	signal(SIGCHLD, SIG_IGN);

	log("Loading %s...", cfg_file);

	curl_global_init(CURL_GLOBAL_ALL);

	FILE *fh = fopen(cfg_file, "r");
	if (!fh)
		error_exit(true, "Cannot access configuration file %s", cfg_file);

	std::atomic_bool global_stopflag(false);

	json_error_t error;
	json_t *cfg = json_loadf(fh, 0, &error);
	if (!cfg)
		error_exit(false, "At column %d in line %d the following JSON problem was found in the configuration file: %s (%s", error.column, error.line, error.text, error.source);

	fclose(fh);

	//***

	json_t *j_gen = json_object_get(cfg, "general");
	const char *logfile = json_str(j_gen, "logfile", "file where to store logging");
	setlogfile(logfile[0] ? logfile : NULL);
	log(" *** " NAME " v" VERSION " starting ***");

	//***

	//int w = 352, h = 288; // or -1, -1 for auto detect
	// video device, prefer jpeg, rpi workaround, jpeg quality, width, height
	//source_v4l *s = new source_v4l("/dev/video0", false, false, 75, w, h);
	//
	// a source that retrieves jpegs from a certain url, mainly used by old ip cameras
	//source_http_jpeg *s = new source_http_jpeg("http://192.168.64.139/image.jpg", true, "");
	//
	//source_http_mjpeg *s = new source_http_mjpeg("10.42.42.104", 80, "/mjpg/video.mjpg");

	json_t *j_source = json_object_get(cfg, "source");

	source *s = NULL;
	log("Configuring the video-source...");
	const char *s_type = json_str(j_source, "type", "source-type");
	if (strcasecmp(s_type, "v4l") == 0) {
		bool pref_jpeg = json_bool(j_source, "prefer-jpeg", "if the camera is capable of JPEG, should that be used");
		bool rpi_wa = json_bool(j_source, "rpi-workaround", "the raspberry pi camera has a bug, this enables a workaround");
		int w = json_int(j_source, "width", "width of picture");
		int h = json_int(j_source, "height", "height of picture");
		int jpeg_quality = json_int(j_source, "quality", "JPEG quality, this influences the size");
		int resize_w = json_int(j_source, "resize-width", "resize picture width to this (-1 to disable)");
		int resize_h = json_int(j_source, "resize-height", "resize picture height to this (-1 to disable)");

		s = new source_v4l(json_str(j_source, "device", "linux v4l2 device"), pref_jpeg, rpi_wa, jpeg_quality, w, h, &global_stopflag, resize_w, resize_h);
	}
	else if (strcasecmp(s_type, "jpeg") == 0) {
		bool ign_cert = json_bool(j_source, "ignore-cert", "ignore SSL errors");
		const char *auth = json_str(j_source, "http-auth", "HTTP authentication string");
		const char *url = json_str(j_source, "url", "address of JPEG stream");
		int resize_w = json_int(j_source, "resize-width", "resize picture width to this (-1 to disable)");
		int resize_h = json_int(j_source, "resize-height", "resize picture height to this (-1 to disable)");

		s = new source_http_jpeg(url, ign_cert, auth, &global_stopflag, resize_w, resize_h);
	}
	else if (strcasecmp(s_type, "mjpeg") == 0) {
		const char *url = json_str(j_source, "url", "address of MJPEG stream");
		bool ign_cert = json_bool(j_source, "ignore-cert", "ignore SSL errors");
		int resize_w = json_int(j_source, "resize-width", "resize picture width to this (-1 to disable)");
		int resize_h = json_int(j_source, "resize-height", "resize picture height to this (-1 to disable)");

		s = new source_http_mjpeg(url, ign_cert, &global_stopflag, resize_w, resize_h);
	}
	else if (strcasecmp(s_type, "rtsp") == 0) {
		const char *url = json_str(j_source, "url", "address of JPEG stream");
		int resize_w = json_int(j_source, "resize-width", "resize picture width to this (-1 to disable)");
		int resize_h = json_int(j_source, "resize-height", "resize picture height to this (-1 to disable)");

		s = new source_rtsp(url, &global_stopflag, resize_w, resize_h);
	}
	else {
		log(" no source defined!");
	}

	//***

	std::vector<pthread_t> ths;
	pthread_t th;
	std::vector<filter *> af;

	// listen adapter, listen port, source, fps, jpeg quality, time limit (in seconds)
	log("Configuring the HTTP listener...");
	json_t *j_hl = json_object_get(cfg, "http-listener");
	if (j_hl) {
		const char *listen_adapter = json_str(j_hl, "listen-adapter", "network interface to listen on or 0.0.0.0 for all");
		int listen_port = json_int(j_hl, "listen-port", "port to listen on");
		int fps = json_int(j_hl, "fps", "number of frames per second to record");
		int jpeg_quality = json_int(j_hl, "quality", "JPEG quality, this influences the size");
		int time_limit = json_int(j_hl, "time-limit", "how long (in seconds) to stream before the connection is closed");
		int resize_w = json_int(j_hl, "resize-width", "resize picture width to this (-1 to disable)");
		int resize_h = json_int(j_hl, "resize-height", "resize picture height to this (-1 to disable)");

		std::vector<filter *> *http_filters = load_filters(json_object_get(j_hl, "filters"));
		add_filters(&af, http_filters);

		start_http_server(listen_adapter, listen_port, s, fps, jpeg_quality, time_limit, http_filters, &global_stopflag, resize_w, resize_h, &th);
		ths.push_back(th);
	}
	else {
		log(" no HTTP listener");
	}

	//***

	log("Configuring the video-loopback...");
	json_t *j_vl = json_object_get(cfg, "video-loopback");
	if (j_vl) {
		const char *dev = json_str(j_vl, "device", "Linux v4l2 device to connect to");
		double fps = json_float(j_vl, "fps", "nubmer of frames per second");

		std::vector<filter *> *loopback_filters = load_filters(json_object_get(j_vl, "filters"));
		add_filters(&af, loopback_filters);

		start_p2vl_thread(s, fps, dev, loopback_filters, &global_stopflag, &th);
		ths.push_back(th);
	}
	else {
		log(" no video loopback");
	}


	//***

	// source, jpeg quality, noise factor, pixels changed % trigger, record min n frames, ignore n frames after record, path to store mjpeg files in, filename prefix, max file duration, camera-warm-up(ignore first x frames), pre-record-count(how many frames to store of just-before-the-motion-started)
	//start_motion_trigger_thread(s, 75, 32, 0.6, 15, 5, "./", "motion-", 3600, 10, 15, &filters_before, &filters_after);
	log("Configuring the motion trigger...");
	ext_trigger_t *et = NULL;
	json_t *j_mt = json_object_get(cfg, "motion-trigger");
	if (j_mt) {
		int jpeg_quality = json_int(j_mt, "quality", "JPEG quality, this influences the size");
		int noise_factor = json_int(j_mt, "noise-factor", "at what difference levell is the pixel considered to be changed");
		double pixels_changed_perctange = json_float(j_mt, "pixels-changed-percentage", "what %% of pixels need to be changed before the motion trigger is triggered");
		int min_duration = json_int(j_mt, "min-duration", "minimum number of frames to record");
		int mute_duration = json_int(j_mt, "mute-duration", "how long not to record (in frames) after motion has stopped");
		const char *path = json_str(j_mt, "path", "directory to write to");
		const char *prefix = json_str(j_mt, "prefix", "string to begin filename with");
		int restart_interval = json_int(j_mt, "restart-interval", "after how many seconds should the stream-file be restarted");
		int warmup_duration = json_int(j_mt, "warmup-duration", "how many frames to ignore so that the camera can warm-up");
		int pre_motion_record_duration = json_int(j_mt, "pre-motion-record-duration", "how many frames to record that happened before the motion started");
		int fps = json_int(j_mt, "fps", "number of frames per second to analyze");
		const char *exec_start = json_str(j_mt, "exec-start", "script to start when motion begins");
		const char *exec_cycle = json_str(j_mt, "exec-cycle", "script to start when the output file is restarted");
		const char *exec_end = json_str(j_mt, "exec-end", "script to start when the motion stops");
		const char *selection_bitmap = json_str(j_mt, "selection-bitmap", "bitmaps indicating which pixels to look at. must be same size as webcam image and must be a .pbm-file. leave empty to disable.");

		std::vector<filter *> *filters_before = load_filters(json_object_get(j_mt, "filters-before"));
		add_filters(&af, filters_before);
		std::vector<filter *> *filters_after = load_filters(json_object_get(j_mt, "filters-after"));
		add_filters(&af, filters_after);

		const char *format = json_str(j_mt, "format", "either AVI or JPEG");
		o_format_t of = OF_AVI;
		if (strcasecmp(format, "AVI") == 0)
			of = OF_AVI;
		else if (strcasecmp(format, "JPEG") == 0)
			of = OF_JPEG;

		const char *file = json_str(j_mt, "trigger-plugin-file", "filename of external motion trigger");
		if (file[0]) {
			et = new ext_trigger_t;
			et -> par = json_str(j_mt, "trigger-plugin-parameter", "parameter for external motion trigger");

			et -> library = dlopen(file, RTLD_NOW);
			if (!et -> library)
				error_exit(true, "Failed opening external motion trigger library %s", file);

			et -> init_motion_trigger = (init_filter_t)dlsym(et -> library, "init_motion_trigger");
			if (!et -> init_motion_trigger)
				error_exit(true, "Failed finding external motion trigger \"init_motion_trigger_t\" in %s", file);

			et -> detect_motion = (detect_motion_t)dlsym(et -> library, "detect_motion");
			if (!et -> detect_motion)
				error_exit(true, "Failed finding external motion trigger \"detect_motion\" in %s", file);
		}

		bool *sb = load_selection_bitmap(selection_bitmap);

		start_motion_trigger_thread(s, jpeg_quality, noise_factor, pixels_changed_perctange, min_duration, mute_duration, path, prefix, restart_interval, warmup_duration, pre_motion_record_duration, filters_before, filters_after, fps, exec_start, exec_cycle, exec_end, of, &global_stopflag, sb, et, &th);
		ths.push_back(th);
	}
	else {
		log(" no motion trigger defined");
	}

	//***

	// store all there's to see in a file
	// source, path, filename prefix, jpeg quality, max duration per file, ignore timelapse/fps
	// timelaps/fps value: 0.1 means 10fps, 10 means 1 frame every 10 seconds
	//start_store_thread(s, "./", "all-", 25, 3600, -1, NULL, &filters_after);

	// store all there's to see in a file, timelapse
	// source, path, filename prefix, jpeg quality, max duration per file, time lapse interval/fps
	//start_store_thread(s, "./", "tl-", 100, 86400, 60, NULL, &filters_after);
	log("Configuring the stream-to-disk backend(s)...");
	json_t *j_std = json_object_get(cfg, "stream-to-disk");
	if (j_std) {
		size_t n_std = json_array_size(j_std);
		log(" %zu disk streams", n_std);

		for(size_t i=0; i<n_std; i++) {
			json_t *ae = json_array_get(j_std, i);

			const char *path = json_str(ae, "path", "directory to write to");
			const char *prefix = json_str(ae, "prefix", "string to begin filename with");
			int jpeg_quality = json_int(ae, "quality", "JPEG quality, this influences the size");
			int restart_interval = json_int(ae, "restart-interval", "after how many seconds should the stream-file be restarted");
			double snapshot_interval = json_float(ae, "snapshot-interval", "store a snapshot every x seconds");
			const char *exec_start = json_str(ae, "exec-start", "script to start when motion begins");
			const char *exec_cycle = json_str(ae, "exec-cycle", "script to start when the output file is restarted");
			const char *exec_end = json_str(ae, "exec-end", "script to start when the motion stops");
			const char *format = json_str(ae, "format", "either AVI or JPEG");

			std::vector<filter *> *store_filters = load_filters(json_object_get(ae, "filters"));
			add_filters(&af, store_filters);

			o_format_t of = OF_AVI;
			if (strcasecmp(format, "AVI") == 0)
				of = OF_AVI;
			else if (strcasecmp(format, "JPEG") == 0)
				of = OF_JPEG;

			std::atomic_bool *dummy = NULL;
			start_store_thread(s, path, prefix, jpeg_quality, restart_interval, snapshot_interval, NULL, store_filters, exec_start, exec_cycle,   exec_end, &global_stopflag, of, &dummy, &th);
			ths.push_back(th);
		}
	}
	else {
		log(" no stream-to-disk backends defined");
	}

	if (!s)
		error_exit(false, "No video-source selected");

	if (do_fork) {
		if (daemon(0, 0) == -1)
			error_exit(true, "daemon() failed");

		for(;;)
			sleep(86400);
	}

	log("System started");

	getchar();

	log("Terminating");

	global_stopflag = true;

	log("Waiting for threads to terminate");

	for(pthread_t t : ths) {
		void *dummy = NULL;
		pthread_join(t, &dummy);
	}

	delete s;
	json_decref(cfg);

	if (et) {
		dlclose(et -> library);
		delete et;
	}

	free_filters(&af);

	log("Bye bye");

	return 0;
}
