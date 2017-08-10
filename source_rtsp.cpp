// with code from https://stackoverflow.com/questions/39536746/ffmpeg-leak-while-reading-image-files
#include <atomic>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
}

#include "source_rtsp.h"
#include "error.h"

source_rtsp::source_rtsp(const std::string & urlIn, std::atomic_bool *const global_stopflag) : source(-1, global_stopflag), url(urlIn)
{
	th = new std::thread(std::ref(*this));
}

source_rtsp::~source_rtsp()
{
	th -> join();
	delete th;
}

void source_rtsp::operator()()
{
	// Open the initial context variables that are needed
	AVFormatContext* format_ctx = avformat_alloc_context();

	// Register everything
	av_register_all();
	avformat_network_init();

	// open RTSP
	if (avformat_open_input(&format_ctx, url.c_str(), NULL, NULL) != 0)
		error_exit(false, "Cannot open %s", url.c_str());

	if (avformat_find_stream_info(format_ctx, NULL) < 0)
		error_exit(false, "avformat_find_stream_info failed (rtsp)");

	// search video stream
	int video_stream_index = -1;
	for (int i = 0; i < format_ctx->nb_streams; i++) {
		if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
			video_stream_index = i;
	}

	if (video_stream_index == -1)
		error_exit(false, "No video stream in rstp feed");

	AVPacket packet;
	av_init_packet(&packet);

	// open output file
	AVFormatContext* output_ctx = avformat_alloc_context();

	AVStream *stream = NULL;

	//start reading packets from stream and write them to file
	av_read_play(format_ctx);    //play RTSP

	// Get the codec
	AVCodec *codec = NULL;
	codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (!codec)
		error_exit(false, "Decoder AV_CODEC_ID_H264 not found (rtsp)");

	// Add this to allocate the context by codec
	AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);

	avcodec_get_context_defaults3(codec_ctx, codec);

	avcodec_parameters_to_context(codec_ctx, format_ctx->streams[video_stream_index]->codecpar);

	if (avcodec_open2(codec_ctx, codec, NULL) < 0)
		error_exit(false, "avcodec_open2 failed");

	this -> width = codec_ctx -> width;
	this -> height = codec_ctx -> height;
	uint8_t *pixels = (uint8_t *)valloc(this -> width * this -> height * 3);

	SwsContext *img_convert_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt, codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

	int size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, codec_ctx->width, codec_ctx->height, 1);
	uint8_t* picture_buffer = (uint8_t*) (av_malloc(size));
	AVFrame* picture = av_frame_alloc();
	AVFrame* picture_rgb = av_frame_alloc();
	int size2 = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);
	uint8_t* picture_buffer_2 = (uint8_t*) (av_malloc(size2));
	av_image_fill_arrays(picture -> data, picture -> linesize, picture_buffer, AV_PIX_FMT_YUV420P, codec_ctx->width, codec_ctx->height, 1);
	av_image_fill_arrays(picture_rgb -> data, picture_rgb -> linesize, picture_buffer_2, AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);

	while(!*global_stopflag && av_read_frame(format_ctx, &packet) >= 0) {
		if (packet.stream_index == video_stream_index) {    //packet is video

			if (stream == NULL) {    //create stream in file
				printf("Create stream\n");

				stream = avformat_new_stream(output_ctx, NULL);

				avcodec_parameters_to_context(stream->codec, format_ctx->streams[video_stream_index]->codecpar);

				stream->sample_aspect_ratio = format_ctx->streams[video_stream_index]->codecpar->sample_aspect_ratio;
			}

			packet.stream_index = stream->id;

			if (avcodec_send_packet(codec_ctx, &packet) < 0) {
				printf("rtsp error\n");
				break;
			}

			int result = avcodec_receive_frame(codec_ctx, picture); //avcodec_decode_video2(codec_ctx, picture, &check, &packet);
			if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
				printf("rtsp error %d\n", result);
				break;
			}

			sws_scale(img_convert_ctx, picture->data, picture->linesize, 0, codec_ctx->height, picture_rgb->data, picture_rgb->linesize);

			for(int y = 0; y < codec_ctx->height; y++) {
				uint8_t *out_pointer = &pixels[y * this -> width * 3];
				uint8_t *in_pointer = picture_rgb->data[0] + y * picture_rgb->linesize[0];

				for(int x = 0; x < codec_ctx->width * 3; x++)
					out_pointer[x] = in_pointer[x];
			}

			set_frame(E_RGB, pixels, this -> width * this -> height * 3);
		}

		av_frame_unref(picture);

		av_packet_unref(&packet);
		av_init_packet(&packet);
	}

	av_packet_unref(&packet);

	av_free(picture);
	av_free(picture_rgb);
	av_free(picture_buffer);
	av_free(picture_buffer_2);
	av_read_pause(format_ctx);
	avio_close(output_ctx->pb);
	avformat_free_context(output_ctx);
	sws_freeContext(img_convert_ctx);
	avcodec_free_context(&codec_ctx);

	free(pixels);
}
