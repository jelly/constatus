// (C) 2017 by folkert van heusden, this file is released under the AGPL v3.0 license.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "../source.h"
#include "../log.h"
#include "../utils.h"

static void reconnect(const std::string & host, const int port, std::atomic_bool *const stop_flag, int *const fd, int *const vnc_width, int *const vnc_height)
{
	log(LL_INFO, "(Re-)connect to %s:%d", host.c_str(), port);

	*fd = -1;

	for(;;) {
		if (*fd != -1) {
			usleep(751000);
			close(fd);
		}

		*fd = connect_to(host, port, stop_flag);
		if (*fd == -1) {
			log(LL_INFO, "Cannot connect to " + host_details + " (" + get_errno_string() + ")");
			continue;
		}

		// receiver VNC-server protocol version
		char ProtocolVersion[12 + 1];
		if (READ(*fd, ProtocolVersion, 12) != 12) {
			log(LL_WARNING, host_details + ": protocol version handshake short read (" + get_errno_string() + ")");
			continue;
		}
		ProtocolVersion[12] = 0;

		log(LL_DEBUG(host_details + ": protocol version: " + ProtocolVersion);

		// send VNC-client protocol version
		char my_prot_version[] = "RFB 003.003\n";
		if (WRITE(*fd, my_prot_version, 12) != 12) {
			log(LL_WARNING, host_details + ": protocol version handshake short write (" + get_errno_string() + ")");
			continue;
		}

		char auth[4];
		if (READ(*fd, auth, 4) != 4) {
			log(LL_WARNING, host_details + ": auth method short read (" + get_errno_string() + ")");
			continue;
		}

		if (auth[3] == 0) { // CONNECTION FAILED
			std::string reason;

			for(;;) {
				unsigned char auth_err_len[4];
				if (READ(*fd, reinterpret_cast<char *>(auth_err_len), 4) != 4)
					break;

				int len = GET_CARD16(auth_err_len);
				if (len <= 0)
					break;

				char *msg = new char[len];
				if (READ(*fd, msg, len) != len) {
					delete [] msg;
					break;
				}

				reason = msg;
				delete [] msg;

				break;
			}

			log(LL_WARNING, host_details + ": connection failed (" + reason + ")");

			continue;
		}
		else if (auth[3] == 1) { // NO AUTHENTICATION
			// do nothing
			log(LL_INFO, host_details + ": log-on without password");
		}
#if 0
		else if (auth[3] == 2) // AUTHENTICATION
		{
			unsigned char challenge[16 + 1];
			if (READ(*fd, reinterpret_cast<char *>(challenge), 16) != 16)
			{
				error(host_details + ": challenge short read (" + get_errno_string() + ")");
				break;
			}

			// retrieve first 8 characters of password and pad with 0x00
			const_DES_cblock password_padded;
			memset(password_padded, 0x00, sizeof password_padded);
			memcpy(password_padded, vnc_password.c_str(), std::min(8, int(vnc_password.size())));
			// then vnc swaps the bits in the password
			for(int index=0; index<8; index++)
			{ 
				int cur = password_padded[index];
				password_padded[index] = static_cast<unsigned char>(((cur & 0x01)<<7) + ((cur & 0x02)<<5) + ((cur & 0x04)<<3) + ((cur & 0x08)<<1) + ((cur & 0x10)>>1) + ((cur & 0x20)>>3) + ((cur & 0x40)>>5) + ((cur & 0x80)>>7));
			}
			// setup key schedule
			DES_key_schedule key_schedule;
			DES_set_key_unchecked(&password_padded, &key_schedule);

			// encrypt
			unsigned char result[16];
			const_DES_cblock ch1, ch2;
			memcpy(ch1, &challenge[0], 8);
			memcpy(ch2, &challenge[8], 8);
			DES_cblock r1, r2;
			DES_ecb_encrypt(&ch1, &r1, &key_schedule, DES_ENCRYPT);
			DES_ecb_encrypt(&ch2, &r2, &key_schedule, DES_ENCRYPT);
			memcpy(&result[0], r1, 8);
			memcpy(&result[8], r2, 8);
			// transmit result to server
			if (WRITE(*fd, reinterpret_cast<char *>(result), 16) != 16)
			{
				error(host_details + ": auth response short write (" + get_errno_string() + ")");
				break;
			}

			char auth_answer[4];
			if (READ(*fd, auth_answer, 4) != 4)
			{
				error(host_details + ": auth response short read (" + get_errno_string() + ")");
				break;
			}

			if (auth_answer[3] != 0)
			{
				error(host_details + ": authentication failed");
				break;
			}

			logger -> dolog(host_details + ": authentication succeeded");
		}
#endif
		else
		{
			log(LL_ERR, host_details + ": connection failed, unknown authentication");
			continue;
		}

		char do_shared = 1;
		if (WRITE(*fd, &do_shared, 1) != 1) {
			log(LL_ERR, host_details + ": enable shared failed (" + get_errno_string() + ")");
			continue;
		}

		unsigned char parameters[2 + 2 + 16 + 4];
		if (READ(*fd, reinterpret_cast<char *>(parameters), 24) != 24) {
			log(LL_ERR, host_details + ": parameters short read (" + get_errno_string() + ")");
			break;
		}

		swap_rgb = parameters[6] != 0;
		*vnc_width = GET_CARD16(&parameters[0]);
		*vnc_height = GET_CARD16(&parameters[2]);

		logger -> dolog("%s: w/h: %dx%d", host_details.c_str(), vnc_width, vnc_height);

		char *vnc_name = new char[parameters[23] + 1];
		if (READ(*fd, vnc_name, parameters[23]) != parameters[23]) {
			delete [] vnc_name;
			log(LL_ERR, host_details + ": name short read (" + get_errno_string() + ")");
			continue;
		}

		vnc_name[parameters[23]] = 0x00;
		log(LL_DEBUG, host_details + ": session name: " + std::string(vnc_name));
		delete [] vnc_name;

		log(LL_DEBUG, "bits per pixel: %d, depth: %d, r/g/b shift: %d/%d/%d", parameters[4], parameters[5], parameters[14], parameters[15], parameters[16]);

		// printf("%d, %d, %d\n", GET_CARD16(&parameters[4+4]), GET_CARD16(&parameters[6+4]), GET_CARD16(&parameters[8+4]));
		if (parameters[4] != 32) {
			log(LL_ERR, host_details + ": only 32 bit/pixel supported");
			continue;
		}

		// set encoding
		// char encoding[] = { 2, 0, 0, 5, 0, 0, 0, 5, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 4, 0, 0, 0, 0 };
		char encoding[] = { 2, 0, 0, 4, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 4, 0, 0, 0, 0 };
		if (WRITE(*fd, encoding, sizeof encoding) != sizeof encoding) {
			log(LL_ERR, host_details + ": short write for encoding message (" + get_errno_string() + ")");
			continue;
		}

		log(LL_INFO, host_details + ": session started");
		incremental = false;

		break;
	}
}

bool draw_vnc(int cur_fd)
{
	unsigned char header[4];

	if (READ(cur_fd, reinterpret_cast<char *>(header), 1) != 1)
		return false;

	if (header[0] == 2) { // BELL
		log(LL_DEBUG, "VNC: bell");
		return true;
	}

	if (header[0] != 0) {
		log(LL_DEBUG, "Unknown VNC server command %d\n", header[0]);
		return true; // don't know how to handle but just continue, maybe it works
	}

	if (READ(cur_fd, reinterpret_cast<char *>(&header[1]), 3) != 3)
		return false;

	unsigned int n_rec = GET_CARD16(&header[2]);

	//printf("# rectangles: %d\n", n_rec);
	char *pixels = static_cast<char *>(cur_fb -> pixels);
	for(unsigned int nr=0; nr<n_rec; nr++) {
		unsigned char rectangle_header[12];

		if (READ(cur_fd, reinterpret_cast<char *>(rectangle_header), 12) != 12)
			return false;

		unsigned int cur_xo= GET_CARD16(&rectangle_header[0]);
		unsigned int cur_yo= GET_CARD16(&rectangle_header[2]);
		unsigned int cur_w = GET_CARD16(&rectangle_header[4]);
		unsigned int cur_h = GET_CARD16(&rectangle_header[6]);

		int encoding = GET_CARD32(&rectangle_header[8]);
		//printf("encoding: %d\n", encoding);
		if (encoding == 0) // RAW
		{
			//printf("%d,%d %dx%d\n", cur_xo, cur_yo, cur_w, cur_h);

			int n_bytes = cur_w * 4;
			for(unsigned int y=cur_yo; y<(cur_yo + cur_h); y++)
			{
				if (READ(cur_fd, &pixels[y * vnc_width * 4 + cur_xo * 4], n_bytes) != n_bytes)
					break;
			}
			//printf(" updated\n");
		}
		else if (encoding == 1) // copy rect
		{
			unsigned char src[4];
			if (READ(cur_fd, reinterpret_cast<char *>(src), 4) != 4)
				return false;

			unsigned int src_x = GET_CARD16(&src[0]);
			unsigned int src_y = GET_CARD16(&src[2]);
			//printf("%d,%d %dx%d from %d,%d\n", cur_xo, cur_yo, cur_w, cur_h, src_x, src_y);

			for(unsigned int y=0; y<cur_h; y++)
			{
				memmove(&pixels[(y + cur_yo) * vnc_width * 4 + cur_xo * 4], &pixels[(y + src_y) * vnc_width * 4 + src_x * 4], cur_w * 4);
			}
			// printf(" updated\n");
		}
		else if (encoding == 2) // RRE
		{
			unsigned char sub_header[4];
			if (READ(cur_fd, reinterpret_cast<char *>(sub_header), 4) != 4)
				return false;
			if (READ(cur_fd, reinterpret_cast<char *>(bg_color), 4) != 4)
				return false;

			unsigned int n_subrect = GET_CARD32(sub_header);
			// printf("# sub rectangles: %d\n", n_subrect);

			for(unsigned int y=cur_yo; y<(cur_yo + cur_h); y++)
			{
				for(unsigned int x=cur_xo; x<(cur_xo + cur_w); x++)
					memcpy(&pixels[y * vnc_width * 4 + x * 4], bg_color, 4);
			}

			for(unsigned int rnr=0; rnr<n_subrect; rnr++)
			{
				if (READ(cur_fd, reinterpret_cast<char *>(fg_color), 4) != 4)
					return false;
				unsigned char subrect[8];
				if (READ(cur_fd, reinterpret_cast<char *>(subrect), 8) != 8)
					return false;
				unsigned int x_pos = GET_CARD16(&subrect[0]) + cur_xo;
				unsigned int y_pos = GET_CARD16(&subrect[2]) + cur_yo;
				unsigned int scur_w = GET_CARD16(&subrect[4]);
				unsigned int scur_h = GET_CARD16(&subrect[6]);

				for(unsigned int y=y_pos; y<(y_pos + scur_h); y++)
				{
					for(unsigned int x=x_pos; x<(x_pos + scur_w); x++)
						memcpy(&pixels[y * vnc_width * 4 + x * 4], fg_color, 4);
				}
			}
			// printf(" updated\n");
		}
		else if (encoding == 4) // CORRE
		{
			unsigned char sub_header[4];
			if (READ(cur_fd, reinterpret_cast<char *>(sub_header), 4) != 4)
				return false;
			if (READ(cur_fd, reinterpret_cast<char *>(bg_color), 4) != 4)
				return false;

			unsigned int n_subrect = GET_CARD32(sub_header);
			// printf("# sub rectangles: %d\n", n_subrect);

			for(unsigned int y=cur_yo; y<(cur_yo + cur_h); y++)
			{
				for(unsigned int x=cur_xo; x<(cur_xo + cur_w); x++)
					memcpy(&pixels[y * vnc_width * 4 + x * 4], bg_color, 4);
			}

			for(unsigned int rnr=0; rnr<n_subrect; rnr++)
			{
				if (READ(cur_fd, reinterpret_cast<char *>(fg_color), 4) != 4)
					return false;
				unsigned char subrect[4];
				if (READ(cur_fd, reinterpret_cast<char *>(subrect), 4) != 4)
					return false;
				unsigned int x_pos = subrect[0] + cur_xo;
				unsigned int y_pos = subrect[1] + cur_yo;
				unsigned int scur_w = subrect[2];
				unsigned int scur_h = subrect[3];

				for(unsigned int y=y_pos; y<(y_pos + scur_h); y++)
				{
					for(unsigned int x=x_pos; x<(x_pos + scur_w); x++)
						memcpy(&pixels[y * vnc_width * 4 + x * 4], fg_color, 4);
				}
			}
			// printf(" updated\n");
		}
		else if (encoding == 5) // HEXTILE
		{
			unsigned int ht_w = cur_w / 16 + (cur_w & 15 ? 1 : 0);
			unsigned int ht_h = cur_h / 16 + (cur_h & 15 ? 1 : 0);

			for(unsigned int ht_y=0; ht_y<ht_h; ht_y++)
			{
				for(unsigned int ht_x=0; ht_x<ht_w; ht_x++)
				{
					unsigned char subencoding = 0;
					if (READ(cur_fd, reinterpret_cast<char *>(&subencoding), 1) != 1)
						return false;

					if (subencoding & 1) // RAW, just read bytes & set
					{
						char pixelscols[16 * 16 * 4];
						if (READ(cur_fd, pixelscols, sizeof pixelscols) != sizeof pixelscols)
							return false;

						int pixel_index = 0;
						for(unsigned int y=cur_yo + ht_y * 16; y<std::min(cur_yo + ht_y * 16 + 16, vnc_height); y++)
						{
							for(unsigned int x=cur_xo + ht_x * 16; x<std::min(cur_xo + ht_x * 16 + 16, vnc_width); x++)
							{
								memcpy(&pixels[y * vnc_width * 4 + x * 4], &pixelscols[pixel_index], 4);
								pixel_index += 4;
							}
						}
						memcpy(fg_color, &pixelscols[pixel_index - 4], 4);

						continue;
					}
					if (subencoding & 2) // background
					{
						if (READ(cur_fd, reinterpret_cast<char *>(bg_color), 4) != 4)
							return false;
					}
					if (subencoding & 4) // foreground
					{
						if (READ(cur_fd, reinterpret_cast<char *>(fg_color), 4) != 4)
							return false;
					}
					bool any_subrects = (subencoding & 8) ? true : false;
					bool subrects_color = (subencoding & 16) ? true : false;

					if (!any_subrects) // ! / *
					{
						for(unsigned int y=cur_yo + ht_y * 16; y<std::min(cur_yo + ht_y * 16 + 16, vnc_height); y++)
						{
							for(unsigned int x=cur_xo + ht_x * 16; x<std::min(cur_xo + ht_x * 16 + 16, vnc_width); x++)
								memcpy(&pixels[y * vnc_width * 4 + x * 4], bg_color, 4);
						}

						continue;
					}

					unsigned char subsubrects = 0;
					if (READ(cur_fd, reinterpret_cast<char *>(&subsubrects), 1) != 1)
						return false;
					for(unsigned int ss_index=0; ss_index<subsubrects; ss_index++)
					{
						if (subrects_color) // T / T
						{
							if (READ(cur_fd, reinterpret_cast<char *>(fg_color), 4) != 4)
								return false;
						}

						unsigned char xywh[2];
						if (READ(cur_fd, reinterpret_cast<char *>(xywh), 2) != 2)
							return false;

						unsigned int ssx = xywh[0] >> 4;
						unsigned int ssy = xywh[0] & 15;
						unsigned int ssw = (xywh[1] >> 4) + 1;
						unsigned int ssh = (xywh[1] & 15) + 1;

						for(unsigned int y=cur_yo + ht_y * 16 + ssy; y<std::min(cur_yo + ht_y * 16 + ssy + ssh, vnc_height); y++)
						{
							for(unsigned int x=cur_xo + ht_x * 16 + ssx; x<std::min(cur_xo + ht_x * 16 + ssx + ssw, vnc_width); x++)
								memcpy(&pixels[y * vnc_width * 4 + x * 4], fg_color, 4);
						}
					}
				}
			}
			printf(" updated\n");
		}
	}

	return true;
}

bool request_update(int cur_fd)
{
	// request update
	//printf("request update %dx%d\n", vnc_width, vnc_height);
	char upd_request[10];
	upd_request[0] = 3;
	upd_request[1] = incremental ? 1 : 0;
	incremental = true;
	upd_request[2] = upd_request[3] = 0; // x
	upd_request[4] = upd_request[5] = 0; // y
	upd_request[6] = char(vnc_width >> 8);
	upd_request[7] = char(vnc_width & 255);
	upd_request[8] = char(vnc_height >> 8);
	upd_request[9] = char(vnc_height & 255);

	if (WRITE(cur_fd, upd_request, 10) != 10)
		return false;

	return true;
}

typedef struct
{
	source *s;
	std::atomic_bool stop_flag;
	pthread_t th;
} my_data_t;

void * thread(void *arg)
{
	log(LL_INFO, "source plugin thread started");

	my_data_t *md = (my_data_t *)arg;

	while(!md -> stop_flag) {
		usleep(10000);
	set_thread_name(name);

	bool msg = true;
	while(!md -> stop_flag) {
		if (msg) {
			data_lock.lock();
			if (bg)
			{
				SDL_Color fg = { 255, 0, 0, 255 };
				draw_and_put_text(bg, 5, bg -> h / 3, error_font, fg, "Connecting to:", blended, false);
				draw_and_put_text(bg, 5, (bg -> h / 3) * 2, error_font, fg, host_details.c_str(), blended, false);
				msg = false;
			}
			data_lock.unlock();
		}

		//printf("reconnect\n");
		if (fd == -1 && !reconnect())
		{
			// this check is so that we don't do a sleep
			// when we're supposed to exit
			if (end_thread_flag -> exchange(false))
				break;

			if (my_usleep(end_thread_flag, reconnect_sleep * 1000000))
				break;
			if (reconnect_sleep < 16)
				reconnect_sleep++;
			msg = true;
			continue;
		}
		reconnect_sleep = 1;

		double cur_ts = get_rel_ts(), end_ts = cur_ts + 1.0 / fps;
		double start_ts = get_rel_ts();
		bool upd_req = true;
		bool got_update = false;
		for(;;)
		{
			if (upd_req)
			{
				upd_req = false;

				if (!request_update(fd))
					break;
			}

			struct timeval tv;
			double ts_diff = std::max(0.001, end_ts - cur_ts);
			if (ts_diff > EXIT_POLL_MS / 1000.0)
				ts_diff = EXIT_POLL_MS / 1000.0;
			tv.tv_sec = long(ts_diff);
			tv.tv_usec = long((ts_diff - (double)tv.tv_sec) * 1000000.0);

			fd_set rfds;
			FD_ZERO(&rfds);
			FD_SET(fd, &rfds);

			if (select(fd + 1, &rfds, NULL, NULL, &tv) == -1)
			{
				if (errno == EINTR || errno == EAGAIN)
					continue;

				close(fd);
				error(host_details + ": connection problems (" + get_errno_string() + ")");

				break;
			}
			cur_ts = get_rel_ts();
			if (FD_ISSET(fd, &rfds))
			{
				if (!draw_vnc(fd, fb))
					break;

				got_update = true;
			}

			if (end_thread_flag -> exchange(false))
			{
				end_thread_flag -> store(true);
				break;
			}

			if (cur_ts >= end_ts && fb != NULL && got_update)
			{
				got_update = false;

				upd_req = true;

				if (swap_rgb) 
				{
					for(unsigned int index=0; index<(vnc_width * vnc_height * 4); index += 4)
					{
						// unsigned char p1 = ((char *)fb -> pixels)[index + 0];
						unsigned char p2 = static_cast<char *>(fb -> pixels)[index + 1];
						unsigned char p3 = static_cast<char *>(fb -> pixels)[index + 2];
						unsigned char p4 = static_cast<char *>(fb -> pixels)[index + 3];
						static_cast<char *>(fb -> pixels)[index + 0] = p4;
						static_cast<char *>(fb -> pixels)[index + 1] = p3;
						static_cast<char *>(fb -> pixels)[index + 2] = p2;
						static_cast<char *>(fb -> pixels)[index + 3] = alpha_value;
					}
				}
				else
				{
					for(unsigned int index=0; index<(vnc_width * vnc_height * 4); index += 4)
						static_cast<char *>(fb -> pixels)[index + 3] = alpha_value;
				}

				tweak_image(static_cast<unsigned char *>(fb -> pixels));
				set_bitmap(static_cast<unsigned char *>(fb -> pixels), vnc_width, vnc_height);

				frame_loaded_flag -> store(true);

				avg_frame_count = update_fps_counter(start_ts, cur_ts);

				start_ts = cur_ts;
				end_ts = cur_ts + 1.0 / fps;
			}
		}

		if (fd != -1)
		{
			close(fd);
			fd = -1;
		}
	}

	if (fd != -1)
	{
		close(fd);
		fd = -1;
	}

	logger -> dolog("Thread for VNC " + name + " ends");
	}

	free(pic);

	log(LL_INFO, "source plugin thread ending");

	return NULL;
}


extern "C" void *init_plugin(const char *const argument)
{
	my_data_t *md = new my_data_t;
	md -> s = s;
	md -> stop_flag = false;

	pthread_create(&md -> th, NULL, thread, md);

	return md;
}

extern "C" void uninit_plugin(void *arg)
{
	my_data_t *md = (my_data_t *)arg;

	md -> stop_flag = true;

	pthread_join(md -> th, NULL);

	delete md;
}
