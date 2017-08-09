// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <jansson.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <vector>

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
#include "filter_grayscale.h"
#include "filter_boost_contrast.h"
#include "filter_marker_simple.h"
#include "push_to_vloopback.h"
#include "filter_overlay.h"

const char *json_str(const json_t *const in, const char *const key, const char *const descr)
{
	json_t *j_value = json_object_get(in, key);

	if (!j_value)
		error_exit(false, "%s missing (%s)", key, descr);

	return json_string_value(j_value);
}

bool json_bool(const json_t *const in, const char *const key, const char * const descr)
{
	json_t *j_value = json_object_get(in, key);

	if (!j_value)
		error_exit(false, "%s missing (%s)", key, descr);

	return json_boolean_value(j_value);
}

int json_int(const json_t *const in, const char *const key, const char * const descr)
{
	json_t *j_value = json_object_get(in, key);

	if (!j_value)
		error_exit(false, "%s missing (%s)", key, descr);

	return json_integer_value(j_value);
}

double json_float(const json_t *const in, const char *const key, const char * const descr)
{
	json_t *j_value = json_object_get(in, key);

	if (!j_value)
		error_exit(false, "%s missing (%s)", key, descr);

	return json_real_value(j_value);
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
		else if (strcasecmp(s_type, "marker") == 0) {
			const char *s_position = json_str(ae, "mode", "marker mode, e.g. red, red-invert or invert");

			sm_mode_t sm = m_red;

			if (strcasecmp(s_position, "red") == 0)
				sm = m_red;
			else if (strcasecmp(s_position, "red-invert") == 0)
				sm = m_red_invert;
			else if (strcasecmp(s_position, "invert") == 0)
				sm = m_invert;

			filters -> push_back(new filter_marker_simple(sm));
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

	printf("Loading %s...\n", cfg_file);

	FILE *fh = fopen(cfg_file, "r");
	if (!fh)
		error_exit(true, "Cannot access configuration file %s", cfg_file);

	std::atomic_bool global_stopflag(false);

	json_error_t error;
	json_t *cfg = json_loadf(fh, 0, &error);
	if (!cfg)
		error_exit(false, "At column %d in line %d the following JSON problem was found in the configuration file: %s (%s", error.column, error.line, error.text, error.source);

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
	printf("Configuring the video-source...\n");
	const char *s_type = json_str(j_source, "type", "source-type");
	if (strcasecmp(s_type, "v4l") == 0) {
		bool pref_jpeg = json_bool(j_source, "prefer-jpeg", "if the camera is capable of JPEG, should that be used");
		bool rpi_wa = json_bool(j_source, "rpi-workaround", "the raspberry pi camera has a bug, this enables a workaround");
		int w = json_int(j_source, "width", "width of picture");
		int h = json_int(j_source, "height", "height of picture");
		int jpeg_quality = json_int(j_source, "quality", "JPEG quality, this influences the size");

		s = new source_v4l(json_str(j_source, "device", "linux v4l2 device"), pref_jpeg, rpi_wa, jpeg_quality, w, h, &global_stopflag);
	}
	else if (strcasecmp(s_type, "jpeg") == 0) {
		bool ign_cert = json_bool(j_source, "ignore-cert", "ignore SSL errors");
		const char *auth = json_str(j_source, "http-auth", "HTTP authentication string");
		const char *url = json_str(j_source, "url", "address of JPEG stream");
		int jpeg_quality = json_int(j_source, "quality", "JPEG quality, this influences the size");

		s = new source_http_jpeg(url, ign_cert, auth, jpeg_quality, &global_stopflag);
	}
	else if (strcasecmp(s_type, "mjpeg") == 0) {
		const char *host = json_str(j_source, "host", "IP address/hostname of the camera");
		int port = json_int(j_source, "port", "Port the camera listens on (usually 80)");
		const char *file = json_str(j_source, "file", "Name of the file, e.g. \"/stream.mjpeg\"");
		int jpeg_quality = json_int(j_source, "quality", "JPEG quality, this influences the size");

		s = new source_http_mjpeg(host, port, file, jpeg_quality, &global_stopflag);
	}
	else if (strcasecmp(s_type, "rtsp") == 0) {
		const char *url = json_str(j_source, "url", "address of JPEG stream");

		s = new source_rtsp(url, &global_stopflag);
	}
	else {
		printf(" no source defined!\n");
	}

	//***

	// listen adapter, listen port, source, fps, jpeg quality, time limit (in seconds)
	printf("Configuring the HTTP listener...\n");
	json_t *j_hl = json_object_get(cfg, "http-listener");
	if (j_hl) {
		const char *listen_adapter = json_str(j_hl, "listen-adapter", "network interface to listen on or 0.0.0.0 for all");
		int listen_port = json_int(j_hl, "listen-port", "port to listen on");
		int fps = json_int(j_hl, "fps", "number of frames per second to record");
		int jpeg_quality = json_int(j_hl, "quality", "JPEG quality, this influences the size");
		int time_limit = json_int(j_hl, "time-limit", "how long (in seconds) to stream before the connection is closed");

		std::vector<filter *> *http_filters = load_filters(json_object_get(j_hl, "filters"));

		start_http_server(listen_adapter, listen_port, s, fps, jpeg_quality, time_limit, http_filters, &global_stopflag);
	}
	else {
		printf(" no HTTP listener\n");
	}

	//***

	printf("Configuring the video-loopback...\n");
	json_t *j_vl = json_object_get(cfg, "video-loopback");
	if (j_vl) {
		const char *dev = json_str(j_vl, "device", "Linux v4l2 device to connect to");
		double fps = json_float(j_vl, "fps", "nubmer of frames per second");

		std::vector<filter *> *loopback_filters = load_filters(json_object_get(j_vl, "filters"));

		start_p2vl_thread(s, fps, dev, loopback_filters, &global_stopflag);
	}
	else {
		printf(" no video loopback\n");
	}


	//***

	// source, jpeg quality, noise factor, pixels changed % trigger, record min n frames, ignore n frames after record, path to store mjpeg files in, filename prefix, max file duration, camera-warm-up(ignore first x frames), pre-record-count(how many frames to store of just-before-the-motion-started)
	//start_motion_trigger_thread(s, 75, 32, 0.6, 15, 5, "./", "motion-", 3600, 10, 15, &filters_before, &filters_after);
	printf("Configuring the motion trigger...\n");
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

		std::vector<filter *> *filters_before = load_filters(json_object_get(j_mt, "filters-before"));
		std::vector<filter *> *filters_after = load_filters(json_object_get(j_mt, "filters-after"));

		start_motion_trigger_thread(s, jpeg_quality, noise_factor, pixels_changed_perctange, min_duration, mute_duration, path, prefix, restart_interval, warmup_duration, pre_motion_record_duration, filters_before, filters_after, fps, exec_start, exec_cycle, exec_end, &global_stopflag);
	}
	else {
		printf(" no motion trigger defined\n");
	}

	//***

	// store all there's to see in a file
	// source, path, filename prefix, jpeg quality, max duration per file, ignore timelapse/fps
	// timelaps/fps value: 0.1 means 10fps, 10 means 1 frame every 10 seconds
	//start_store_thread(s, "./", "all-", 25, 3600, -1, NULL, &filters_after);

	// store all there's to see in a file, timelapse
	// source, path, filename prefix, jpeg quality, max duration per file, time lapse interval/fps
	//start_store_thread(s, "./", "tl-", 100, 86400, 60, NULL, &filters_after);
	printf("Configuring the stream-to-disk backend(s)...\n");
	json_t *j_std = json_object_get(cfg, "stream-to-disk");
	if (j_std) {
		size_t n_std = json_array_size(j_std);
		printf("%zu disk-streams\n", n_std);

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

			std::vector<filter *> *store_filters = load_filters(json_object_get(ae, "filters"));

			start_store_thread(s, path, prefix, jpeg_quality, restart_interval, snapshot_interval, NULL, store_filters, exec_start, exec_cycle,   exec_end, &global_stopflag);
		}
	}
	else {
		printf(" no stream-to-disk backends defined\n");
	}

	if (!s)
		error_exit(false, "No video-source selected");

	if (do_fork) {
		if (daemon(0, 0) == -1)
			error_exit(true, "daemon() failed");

		for(;;)
			sleep(86400);
	}

	printf("System started. Press enter to exit.\n");

	getchar();

	global_stopflag = true;

	delete s;
	json_decref(cfg);

	return 0;
}
