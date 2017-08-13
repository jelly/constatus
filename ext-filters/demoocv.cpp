// (C) 2017 by folkert van heusden, this file is released in the public domain
#include <algorithm>
#include <stdint.h>
#include <string.h>

//#include <opencv2/core/mat.hpp>
#include <opencv2/core/core.hpp>

extern "C" {

void init_filter(const char *const parameter)
{
	// you can use the parameter for anything you want
	// e.g. the filename of a configuration file or
	// maybe a variable or whatever
}

void apply_filter(const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame, uint8_t *const result)
{
	cv::Mat imageWithData = cv::Mat(w * h * 3, 1, CV_8UC3, (void *)current_frame);
	cv::Mat reshapedImage = imageWithData.reshape(0, h);

	cv::Mat invSrc = cv::Scalar::all(255) - reshapedImage;

	memcpy(result, invSrc.data, w * h * 3);
}

}
