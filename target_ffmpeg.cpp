#include <unistd.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include "target_ffmpeg.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"

target_ffmpeg::target_ffmpeg(const std::string & id, source *const s, const std::string & store_path, const std::string & prefix, const int max_time, const double interval, const std::vector<filter *> *const filters, const char *const exec_start, const char *const exec_cycle, const char *const exec_end) : target(id, s, store_path, prefix, max_time, interval, filters, exec_start, exec_cycle, exec_end)
{
}

target_ffmpeg::~target_ffmpeg()
{
	stop();
}

// this function is based on https://stackoverflow.com/questions/9465815/rgb-to-yuv420-algorithm-efficiency
static void rgb_to_yuv2(AVFrame *d, uint8_t *rgb, size_t width, size_t height)
{
	for(size_t line = 0; line < height; ++line)
	{
		uint8_t *Y = &d->data[0][line * d->linesize[0] + 0];
		uint8_t *U = &d->data[1][(line / 2)* d->linesize[1] + 0];
		uint8_t *V = &d->data[2][(line / 2)* d->linesize[2] + 0];
		size_t i = 0, upos = 0, vpos = 0;

		unsigned int offset = line * width * 3;
		uint8_t *p = &rgb[offset];

		if (line & 1) {
			for(size_t x = 0; x < width; x += 1) {
				uint8_t r = *p++;
				uint8_t g = *p++;
				uint8_t b = *p++;

				Y[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;
			}
		}
		else {
			for(size_t x = 0; x < width; x += 2) {
				uint8_t r = *p++;
				uint8_t g = *p++;
				uint8_t b = *p++;

				Y[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;

				U[upos++] = ((-38*r + -74*g + 112*b) >> 8) + 128;
				V[vpos++] = ((112*r + -94*g + -18*b) >> 8) + 128;

				r = *p++;
				g = *p++;
				b = *p++;

				Y[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;
			}
		}
	}
}

// this code is based on https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/encode_video.c
static bool encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt, FILE *outfile)
{
	int ret = avcodec_send_frame(enc_ctx, frame);
	if (ret < 0) {
		log(LL_ERR, "Error sending a frame for encoding");
		return false;
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(enc_ctx, pkt);

		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return true;

		if (ret < 0) {
			log(LL_ERR, "Error during encoding");
			return false;
		}

		fwrite(pkt->data, 1, pkt->size, outfile);

		av_packet_unref(pkt);
	}

	return true;
}

void target_ffmpeg::operator()()
{
	set_thread_name("storef_" + prefix);

	s -> register_user();

	uint64_t prev_ts = get_us();
	int width = -1, height = -1;

	for(;!local_stop_flag;) {
		uint8_t *work = NULL;
		size_t work_len = 0;
		if (s -> get_frame(E_RGB, -1, &prev_ts, &width, &height, &work, &work_len)) {
			free(work);
			break;
		}
	}

	const char *filename = "test.mp4", *codec_name = "libx264";
	const AVCodec *codec;
	AVCodecContext *c= NULL;
	int ret;
	FILE *f;
	AVFrame *frame;
	AVPacket *pkt;
	uint8_t endcode[] = { 0, 0, 1, 0xb7 };

	avcodec_register_all();

	/* find the mpeg1video encoder */
	codec = avcodec_find_encoder_by_name(codec_name);
	if (!codec) {
		fprintf(stderr, "Codec '%s' not found\n", codec_name);
		exit(1);
	}

	c = avcodec_alloc_context3(codec);
	if (!c) {
		fprintf(stderr, "Could not allocate video codec context\n");
		exit(1);
	}

	pkt = av_packet_alloc();
	if (!pkt)
		exit(1);

	/* put sample parameters */
	c->bit_rate = 200000;
	/* resolution must be a multiple of two */
	c->width = width;
	c->height = height;

	int fps = 1.0 / interval;
	/* frames per second */
	c->time_base = (AVRational){1, fps};
	c->framerate = (AVRational){fps, 1};

	/* emit one intra frame every ten frames
	 * check frame pict_type before passing frame
	 * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
	 * then gop_size is ignored and the output of encoder
	 * will always be I frame irrespective to gop_size
	 */
	c->gop_size = 10;
	c->max_b_frames = 1;
	c->pix_fmt = AV_PIX_FMT_YUV420P;

	if (codec->id == AV_CODEC_ID_H264)
		av_opt_set(c->priv_data, "preset", "slow", 0);

	/* open it */
	ret = avcodec_open2(c, codec, NULL);
	if (ret < 0) {
		//fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
		exit(1);
	}

	f = fopen(filename, "wb");
	if (!f) {
		fprintf(stderr, "Could not open %s\n", filename);
		exit(1);
	}

	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}
	frame->format = c->pix_fmt;
	frame->width  = c->width;
	frame->height = c->height;

	ret = av_frame_get_buffer(frame, 32);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate the video frame data\n");
		exit(1);
	}

	/* encode 1 second of video */
	unsigned int t = 0;
	for(;!local_stop_flag;) {
		int w = -1, h = -1;
		uint8_t *work = NULL;
		size_t work_len = 0;
		if (!s -> get_frame(E_RGB, -1, &prev_ts, &w, &h, &work, &work_len))
			continue;

		/* make sure the frame data is writable */
		ret = av_frame_make_writable(frame);
		if (ret < 0)
			exit(1);

		rgb_to_yuv2(frame, work, w, h);
		free(work);

		frame->pts = t++;

		/* encode the image */
		if (!encode(c, frame, pkt, f))
			break;
	}

	/* flush the encoder */
	encode(c, NULL, pkt, f);

	/* add sequence end code to have a real MPEG file */
	fwrite(endcode, 1, sizeof(endcode), f);
	fclose(f);

	avcodec_free_context(&c);
	av_frame_free(&frame);
	av_packet_free(&pkt);

	s -> unregister_user();
}
