// (C) 2017 by folkert van heusden, this file is released in the public domain
#include <stdint.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/types_c.h>
#include <opencv2/objdetect/objdetect.hpp>

extern "C" {

void * init_motion_trigger(const char *const parameter)
{
	// you can use the parameter for anything you want
	// e.g. the filename of a configuration file or
	// maybe a variable or whatever

	// what you return here, will be given as a parameter
	// to detect_motion

	cv::HOGDescriptor *hog = new cv::HOGDescriptor();

	hog -> setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());

	return hog;
}

bool detect_motion(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, uint8_t *const current_frame, const uint8_t *const pixel_selection_bitmap)
{
	cv::HOGDescriptor *hog = (cv::HOGDescriptor *)arg;

	cv::Mat curFrame = cv::Mat(w * h, 1, CV_8UC3, (void *)current_frame);
	//curFrame = curFrame.reshape(0, h);

	std::vector<cv::Rect> found;
        hog -> detectMultiScale(curFrame, found, 0, cv::Size(8,8), cv::Size(32,32), 1.05, 2);

	return !found.empty();
}

void uninit_motion_trigger(void *arg)
{
	// free memory etc
	delete (cv::HOGDescriptor *)arg;
}

}
