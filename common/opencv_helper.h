#ifndef OPENCV_HELPER_H_INCLUDED
#define OPENCV_HELPER_H_INCLUDED

#include <bta.h>

#ifndef NO_OPENCV
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>


cv::Mat toPresentableMat(BTA_Channel *channel);
#endif

#endif
