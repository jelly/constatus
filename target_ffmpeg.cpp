#include <unistd.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
}

#include "target_ffmpeg.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"

target_ffmpeg::target_ffmpeg(const std::string & id, const std::string & parameters, source *const s, const std::string & store_path, const std::string & prefix, const int max_time, const double interval, const std::string & type, const int bitrate, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, const int override_fps) : target(id, s, store_path, prefix, max_time, interval, filters, exec_start, exec_cycle, exec_end, override_fps), parameters(parameters), type(type), bitrate(bitrate)
{
	avcodec_register_all();
}

target_ffmpeg::~target_ffmpeg()
{
	stop();
}


static const std::string my_av_err2str(const int nr)
{
	char buffer[AV_ERROR_MAX_STRING_SIZE];

	av_strerror(nr, buffer, sizeof buffer);

        return buffer;
}

// based on https://www.ffmpeg.org/doxygen/2.0/doc_2examples_2muxing_8c-example.html
/*
 * Copyright (c) 2003 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

extern "C" {
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

#define SCALE_FLAGS SWS_BICUBIC

// a wrapper around a single output AVStream
typedef struct OutputStream {
	AVStream *st;
	AVCodecContext *enc;

	/* pts of the next frame that will be generated */
	int64_t next_pts;
	int samples_count;

	AVFrame *frame;
	AVFrame *tmp_frame;

	float t, tincr, tincr2;

	struct SwsContext *sws_ctx;
	struct SwrContext *swr_ctx;
} OutputStream;

static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
	/* rescale output packet timestamp values from codec to stream timebase */
	av_packet_rescale_ts(pkt, *time_base, st->time_base);
	pkt->stream_index = st->index;

	/* Write the compressed frame to the media file. */
	return av_interleaved_write_frame(fmt_ctx, pkt);
}

/* Add an output stream. */
static bool add_stream(OutputStream *ost, AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id, int fps, int bitrate, int w, int h)
{
	AVCodecContext *c;
	int i;

	/* find the encoder */
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec)) {
		log(LL_ERR, "Can't find encoder for '%s'", avcodec_get_name(codec_id));
		return false;
	}

	ost->st = avformat_new_stream(oc, NULL);
	if (!ost->st) {
		log(LL_ERR, "Can't allocate stream");
		return false;
	}
	ost->st->id = oc->nb_streams-1;
	c = avcodec_alloc_context3(*codec);
	if (!c) {
		log(LL_ERR, "Can't alloc an encoding context");
		return false;
	}
	ost->enc = c;

	switch ((*codec)->type) {
		case AVMEDIA_TYPE_AUDIO:
			c->sample_fmt  = (*codec)->sample_fmts ?
				(*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
			c->bit_rate    = 64000;
			c->sample_rate = 44100;
			if ((*codec)->supported_samplerates) {
				c->sample_rate = (*codec)->supported_samplerates[0];
				for (i = 0; (*codec)->supported_samplerates[i]; i++) {
					if ((*codec)->supported_samplerates[i] == 44100)
						c->sample_rate = 44100;
				}
			}
			c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
			c->channel_layout = AV_CH_LAYOUT_STEREO;
			if ((*codec)->channel_layouts) {
				c->channel_layout = (*codec)->channel_layouts[0];
				for (i = 0; (*codec)->channel_layouts[i]; i++) {
					if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
						c->channel_layout = AV_CH_LAYOUT_STEREO;
				}
			}
			c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
			ost->st->time_base = AVRational{ 1, c->sample_rate };
			break;

		case AVMEDIA_TYPE_VIDEO:
			c->codec_id = codec_id;

			c->bit_rate = bitrate;
			/* Resolution must be a multiple of two. */
			c->width    = w;
			c->height   = h;
			/* timebase: This is the fundamental unit of time (in seconds) in terms
			 * of which frame timestamps are represented. For fixed-fps content,
			 * timebase should be 1/framerate and timestamp increments should be
			 * identical to 1. */
			ost->st->time_base = AVRational{1, fps};
			c->time_base       = ost->st->time_base;

			c->gop_size      = std::max(fps / 2, 1); /* emit one intra frame every twelve frames at most */
			c->pix_fmt       = STREAM_PIX_FMT;
			if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
				/* just for testing, we also add B-frames */
				c->max_b_frames = 2;
			}
			if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
				/* Needed to avoid using macroblocks in which some coeffs overflow.
				 * This does not happen with normal video, it just happens here as
				 * the motion of the chroma plane does not match the luma plane. */
				c->mb_decision = 2;
			}
			break;

		default:
			break;
	}

	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	return true;
}

/**************************************************************/
/* audio output */
#if 0
static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
		uint64_t channel_layout,
		int sample_rate, int nb_samples)
{
	AVFrame *frame = av_frame_alloc();
	int ret;

	if (!frame) {
		log(LL_ERR, "Error allocating an audio frame\n");
		return NULL;
	}

	frame->format = sample_fmt;
	frame->channel_layout = channel_layout;
	frame->sample_rate = sample_rate;
	frame->nb_samples = nb_samples;

	if (nb_samples) {
		ret = av_frame_get_buffer(frame, 0);
		if (ret < 0) {
			log(LL_ERR, "Error allocating an audio buffer\n");
			av_frame_free(&frame);
			return NULL;
		}
	}

	return frame;
}

static void open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
	AVCodecContext *c;
	int nb_samples;
	int ret;
	AVDictionary *opt = NULL;

	c = ost->enc;

	/* open it */
	av_dict_copy(&opt, opt_arg, 0);
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		log(LL_ERR, "Can't open audio codec: %s", my_av_err2str(ret).c_str());
		exit(1);
	}

	/* init signal generator */
	ost->t     = 0;
	ost->tincr = 2 * M_PI * 110.0 / c->sample_rate;
	/* increment frequency by 110 Hz per second */
	ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;

	if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
		nb_samples = 10000;
	else
		nb_samples = c->frame_size;

	ost->frame     = alloc_audio_frame(c->sample_fmt, c->channel_layout,
			c->sample_rate, nb_samples);
	ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout,
			c->sample_rate, nb_samples);

	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
	if (ret < 0) {
		log(LL_ERR, "Can't copy the stream parameters");
		exit(1);
	}

	/* create resampler context */
	ost->swr_ctx = swr_alloc();
	if (!ost->swr_ctx) {
		log(LL_ERR, "Can't allocate resampler context");
		exit(1);
	}

	/* set options */
	av_opt_set_int       (ost->swr_ctx, "in_channel_count",   c->channels,       0);
	av_opt_set_int       (ost->swr_ctx, "in_sample_rate",     c->sample_rate,    0);
	av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16, 0);
	av_opt_set_int       (ost->swr_ctx, "out_channel_count",  c->channels,       0);
	av_opt_set_int       (ost->swr_ctx, "out_sample_rate",    c->sample_rate,    0);
	av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);

	/* initialize the resampling context */
	if ((ret = swr_init(ost->swr_ctx)) < 0) {
		log(LL_ERR, "Failed to initialize the resampling context\n");
		exit(1);
	}
}

/* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
 * 'nb_channels' channels. */
static AVFrame *get_audio_frame(OutputStream *ost)
{
	AVFrame *frame = ost->tmp_frame;
	int j, i, v;
	int16_t *q = (int16_t*)frame->data[0];

//	/* check if we want to generate more frames */
//	if (av_compare_ts(ost->next_pts, ost->enc->time_base,
//				STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)
		return NULL;

	for (j = 0; j <frame->nb_samples; j++) {
		v = (int)(sin(ost->t) * 10000);
		for (i = 0; i < ost->enc->channels; i++)
			*q++ = v;
		ost->t     += ost->tincr;
		ost->tincr += ost->tincr2;
	}

	frame->pts = ost->next_pts;
	ost->next_pts  += frame->nb_samples;

	return frame;
}

/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_audio_frame(AVFormatContext *oc, OutputStream *ost)
{
	AVCodecContext *c;
	AVPacket pkt = { 0 }; // data and size must be 0;
	AVFrame *frame;
	int ret;
	int got_packet;
	int dst_nb_samples;

	av_init_packet(&pkt);
	c = ost->enc;

	frame = get_audio_frame(ost);

	if (frame) {
		/* convert samples from native format to destination codec format, using the resampler */
		/* compute destination number of samples */
		dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
				c->sample_rate, c->sample_rate, AV_ROUND_UP);
		av_assert0(dst_nb_samples == frame->nb_samples);

		/* when we pass a frame to the encoder, it may keep a reference to it
		 * internally;
		 * make sure we do not overwrite it here
		 */
		ret = av_frame_make_writable(ost->frame);
		if (ret < 0)
			exit(1);

		/* convert to destination format */
		ret = swr_convert(ost->swr_ctx,
				ost->frame->data, dst_nb_samples,
				(const uint8_t **)frame->data, frame->nb_samples);
		if (ret < 0) {
			log(LL_ERR, "Error while converting\n");
			exit(1);
		}
		frame = ost->frame;

		frame->pts = av_rescale_q(ost->samples_count, AVRational{1, c->sample_rate}, c->time_base);
		ost->samples_count += dst_nb_samples;
	}

	ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
	if (ret < 0) {
		log(LL_ERR, "Error encoding audio frame: %s", my_av_err2str(ret).c_str());
		exit(1);
	}

	if (got_packet) {
		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
		if (ret < 0) {
			log(LL_ERR, "Error while writing audio frame: %s", my_av_err2str(ret).c_str());
			exit(1);
		}
	}

	return (frame || got_packet) ? 0 : 1;
}
#endif

/**************************************************************/
/* video output */

static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame *picture;
	int ret;

	picture = av_frame_alloc();
	if (!picture)
		return NULL;

	picture->format = pix_fmt;
	picture->width  = width;
	picture->height = height;

	/* allocate the buffers for the frame data */
	ret = av_frame_get_buffer(picture, 32);
	if (ret < 0) {
		log(LL_ERR, "Can't allocate frame data");
		av_frame_free(&picture);
		return NULL;
	}

	return picture;
}

static bool open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
	int ret;
	AVCodecContext *c = ost->enc;
	AVDictionary *opt = NULL;

	av_dict_copy(&opt, opt_arg, 0);

	/* open the codec */
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		log(LL_ERR, "Can't open video codec: %s", my_av_err2str(ret).c_str());
		return false;
	}

	/* allocate and init a re-usable frame */
	ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
	if (!ost->frame) {
		log(LL_ERR, "Can't allocate video frame");
		return false;
	}

	/* If the output format is not YUV420P, then a temporary YUV420P
	 * picture is needed too. It is then converted to the required
	 * output format. */
	ost->tmp_frame = NULL;
	if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
		ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
		if (!ost->tmp_frame) {
			log(LL_ERR, "Can't allocate temporary picture");
			return false;
		}
	}

	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
	if (ret < 0) {
		log(LL_ERR, "Can't copy the stream parameters\n");
		return false;
	}

	return true;
}

static AVFrame *get_video_frame(OutputStream *ost, source *const s, uint64_t *const prev_ts, const std::vector<filter *> *const filters, uint8_t **prev_frame, std::vector<frame_t> **pre_record)
{
	AVCodecContext *c = ost->enc;

//	/* check if we want to generate more frames */
//	if (av_compare_ts(ost->next_pts, c->time_base,
//				STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)
//		return NULL;

	/* when we pass a frame to the encoder, it may keep a reference to it
	 * internally; make sure we do not overwrite it here */
	int ret = -1;
	if ((ret = av_frame_make_writable(ost->frame)) < 0) {
		log(LL_ERR, "Can't initialize the conversion context %s", my_av_err2str(ret).c_str());
		return NULL;
	}

	uint8_t *work = NULL;
	int w = -1, h = -1;

	if (!*pre_record) {
		for(;;) {
			size_t work_len = 0;
			if (s -> get_frame(E_RGB, -1, prev_ts, &w, &h, &work, &work_len))
				break;
		}
	}
	else {
		if (!(*pre_record) -> empty()) {
			work = (*pre_record) -> front().data;
			w = (*pre_record) -> front().w;
			h = (*pre_record) -> front().h;
			*prev_ts = (*pre_record) -> front().ts;

			(*pre_record) -> erase((*pre_record) -> begin());
		}

		if ((*pre_record) -> empty()) {
			delete *pre_record;
			*pre_record = NULL;
		}
	}

	apply_filters(filters, *prev_frame, work, *prev_ts, w, h);

	/* as we only generate a YUV420P picture, we must convert it
	 * to the codec pixel format if needed */
	if (!ost->sws_ctx) {
		ost->sws_ctx = sws_getContext(c->width, c->height, AV_PIX_FMT_RGB24, c->width, c->height, c->pix_fmt, SCALE_FLAGS, NULL, NULL, NULL);
		if (!ost->sws_ctx) {
			log(LL_ERR, "Can't initialize the conversion context\n");
			return NULL;
		}
	}

	int line_size = c -> width * 3;
	sws_scale(ost->sws_ctx, (const uint8_t * const *)&work, &line_size, 0, c->height, ost->frame->data, ost->frame->linesize);

	free(*prev_frame);
	*prev_frame = work;

	ost->frame->pts = ost->next_pts++;

	return ost->frame;
}

static bool write_video_frame(AVFormatContext *oc, OutputStream *ost, source *const s, uint64_t *const prev_ts, const std::vector<filter *> *const filters, uint8_t **prev_frame, std::vector<frame_t> **pre_record)
{
	AVCodecContext *c = ost->enc;

	AVFrame *frame = get_video_frame(ost, s, prev_ts, filters, prev_frame, pre_record);
	if (!frame)
		return false;

	AVPacket pkt = { 0 };
	av_init_packet(&pkt);

	int ret = avcodec_send_frame(c, frame);
	if (ret < 0) {
		log(LL_ERR, "Error sending a frame for encoding %s", my_av_err2str(ret).c_str());
		return false;
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(c, &pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		else if (ret < 0) {
			log(LL_ERR, "Error during encoding %s", my_av_err2str(ret).c_str());
			return false;
		}

		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
		if (ret < 0) {
			log(LL_ERR, "Error while writing video frame: %s", my_av_err2str(ret).c_str());
			return false;
		}

		av_packet_unref(&pkt);
	}

	return true;
}

static void close_stream(AVFormatContext *oc, OutputStream *ost)
{
	avcodec_free_context(&ost->enc);
	av_frame_free(&ost->frame);
	av_frame_free(&ost->tmp_frame);
	sws_freeContext(ost->sws_ctx);
	swr_free(&ost->swr_ctx);
}

void target_ffmpeg::operator()()
{
	set_thread_name("storef_" + prefix);

	s -> register_user();

	const int fps = interval <= 0 ? 25 : std::max(1, int(1.0 / interval));

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

	av_register_all();

	std::string name;
	unsigned f_nr = 0;
	bool is_start = true;

	uint8_t *prev_frame = NULL;

	////////////////////////////////////////////////////////////////////////////

	for(;!local_stop_flag;) {
		OutputStream video_st = { 0 }, audio_st = { 0 };
		AVOutputFormat *fmt = NULL;
		AVFormatContext *oc = NULL;
		AVCodec *audio_codec = NULL, *video_codec = NULL;
		int ret = -1;
		int have_video = 0, have_audio = 0;
		int encode_video = 0, encode_audio = 0;
		AVDictionary *opt = NULL;

		/* Initialize libavcodec, and register all codecs and formats. */
		name = gen_filename(store_path, prefix, type, get_us(), f_nr++);

		if (!exec_start.empty() && is_start) {
			exec(exec_start, name);
			is_start = false;
		}
		else if (!exec_cycle.empty()) {
			exec(exec_cycle, name);
		}

		/* allocate the output media context */
		avformat_alloc_output_context2(&oc, NULL, NULL, name.c_str());
		if (!oc) {
			printf("Can't deduce output format from file extension: using MPEG");
			avformat_alloc_output_context2(&oc, NULL, "mpeg", name.c_str());
		}

		if ((ret = av_set_options_string(opt, parameters.c_str(), "=", ":")) < 0)
			error_exit(false, "ffmpeg parameters are incorrect");

		fmt = oc->oformat;

		/* Add the audio and video streams using the default format codecs
		 * and initialize the codecs. */
		if (fmt->video_codec != AV_CODEC_ID_NONE) {
			add_stream(&video_st, oc, &video_codec, fmt->video_codec, override_fps != -1 ? override_fps : fps, bitrate, width, height);
			have_video = 1;
			encode_video = 1;
		}
		//if (fmt->audio_codec != AV_CODEC_ID_NONE) {
		//	add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec, fps, bitrate, width, height);
		//	have_audio = 1;
		//	encode_audio = 1;
		//}

		if (!have_video)
			error_exit(false, "Target encoder does not have video capabilities");

		/* Now that all the parameters are set, we can open the audio and
		 * video codecs and allocate the necessary encode buffers. */
		if (have_video) {
			av_opt_set(opt, "preset", "fast", 0);
			open_video(oc, video_codec, &video_st, opt);
		}

		//if (have_audio)
		//	open_audio(oc, audio_codec, &audio_st, opt);

		av_dump_format(oc, 0, name.c_str(), 1);

		/* open the output file, if needed */
		if (!(fmt->flags & AVFMT_NOFILE)) {
			ret = avio_open(&oc->pb, name.c_str(), AVIO_FLAG_WRITE);
			if (ret < 0) {
				log(LL_ERR, "Can't open '%s': %s", name.c_str(), my_av_err2str(ret).c_str());
				break;
			}
		}

		/* Write the stream header, if any. */
		ret = avformat_write_header(oc, &opt);
		if (ret < 0) {
			log(LL_ERR, "Error occurred when opening output file: %s", my_av_err2str(ret).c_str());
			break;
		}

		time_t cut_ts = time(NULL) + max_time;

		while(!local_stop_flag) {
			if (max_time > 0 && time(NULL) >= cut_ts)
				break;

			/* select the stream to encode */
			//	if (encode_video &&
			//			(!encode_audio || av_compare_ts(video_st.next_pts, video_st.enc->time_base,
			//							audio_st.next_pts, audio_st.enc->time_base) <= 0)) {
			//encode_video = !write_video_frame(oc, &video_st, s, &prev_ts);
			if (!write_video_frame(oc, &video_st, s, &prev_ts, filters, &prev_frame, &pre_record))
				break;
			//	} else {
			//		encode_audio = !write_audio_frame(oc, &audio_st);
			//	}

			if (!pre_record || pre_record -> empty())
				mysleep(interval, &local_stop_flag, s);
		}

		/* Write the trailer, if any. The trailer must be written before you
		 * close the CodecContexts open when you wrote the header; otherwise
		 * av_write_trailer() may try to use memory that was freed on
		 * av_codec_close(). */
		av_write_trailer(oc);

		/* Close each codec. */
		if (have_video)
			close_stream(oc, &video_st);
		//if (have_audio)
		//	close_stream(oc, &audio_st);

		if (!(fmt->flags & AVFMT_NOFILE))
			/* Close the output file. */
			avio_closep(&oc->pb);

		/* free the stream */
		avformat_free_context(oc);

		////////////////////////////////////////////////////////////////////////////
	}

	if (!exec_end.c_str())
		exec(exec_end, name);

	free(prev_frame);

	s -> unregister_user();
}
