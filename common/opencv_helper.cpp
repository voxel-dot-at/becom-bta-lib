#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opencv_helper.h"

#ifndef NO_OPENCV

cv::Mat toMat(BTA_Channel *channel) {
    int cvType;
    switch (channel->dataFormat) {
    case BTA_DataFormatFloat32:
        cvType = CV_32F;
        break;
    case BTA_DataFormatUInt32:
    case BTA_DataFormatSInt32:
        cvType = CV_32S;
        break;
    case BTA_DataFormatUInt16:
        cvType = CV_16U;
        break;
    case BTA_DataFormatSInt16:
        cvType = CV_16S;
        break;
    case BTA_DataFormatRgb24:
        cvType = CV_8UC3;
        break;
    case BTA_DataFormatRgb565:
    default:
        assert(0);
        cv::Mat imshowMat = cv::Mat(channel->yRes, channel->xRes, CV_8UC3);
        cv::cvtColor(cv::Mat::zeros(1, 1, CV_8UC2), imshowMat, CV_BGR5652BGR);
        return imshowMat;
    }

    cv::Mat channelMat = cv::Mat(channel->yRes, channel->xRes, cvType, channel->data);
    return channelMat;
}


cv::Mat toPresentableMat(BTA_Channel *channel) {
    uint32_t resizeFactor = 1;
    if (channel->xRes <= 160) {
        resizeFactor = 3;
    }
    else if (channel->xRes <= 240) {
        resizeFactor = 2;
    }
    int inputCvType;
    int outputCvType;
    switch (channel->dataFormat) {
    case BTA_DataFormatFloat32:
        inputCvType = CV_32F;
        outputCvType = CV_8U;
        break;
    case BTA_DataFormatUInt32:
    case BTA_DataFormatSInt32:
        inputCvType = CV_32S;
        outputCvType = CV_8U;
        break;
    case BTA_DataFormatUInt16:
        inputCvType = CV_16U;
        outputCvType = CV_8U;
        break;
    case BTA_DataFormatSInt16:
        inputCvType = CV_16S;
        outputCvType = CV_8U;
        break;
    case BTA_DataFormatRgb565:
        resizeFactor = 1;
        inputCvType = CV_8UC2;
        outputCvType = CV_8UC3;
        break;
	case BTA_DataFormatRgb24:
		resizeFactor = 1;
		inputCvType = CV_8UC3;
		outputCvType = CV_8UC3;
		break;
    case BTA_DataFormatYuv422:
        resizeFactor = 1;
        inputCvType = CV_8UC2;
        outputCvType = CV_8UC3;
        break;
    case BTA_DataFormatYuv444:
        resizeFactor = 1;
        inputCvType = CV_8UC3;
        outputCvType = CV_8UC3;
        break;
    default:
		cv::Mat imshowMat = cv::Mat(channel->yRes, channel->xRes, CV_8UC3);
		cv::cvtColor(cv::Mat::zeros(1, 1, CV_8UC2), imshowMat, CV_BGR5652BGR);
        return imshowMat;
    }

    cv::Mat channelMat = cv::Mat(channel->yRes, channel->xRes, inputCvType, channel->data);
    cv::Mat imshowMat = cv::Mat(channel->yRes, channel->xRes, outputCvType);

    if (channel->id == BTA_ChannelIdDistance ||
        channel->id == BTA_ChannelIdZ) {
        for (int y = 0; y < channel->yRes; y++) {
            for (int x = 0; x < channel->xRes; x++) {
                if (channel->dataFormat == BTA_DataFormatFloat32) {
                    if (channelMat.at<float>(y, x) > 2.0) {
                        imshowMat.at<uint8_t>(y, x) = 0;
                    }
                    else {
                        imshowMat.at<uint8_t>(y, x) = 0xff - (uint8_t)(256.0 * channelMat.at<float>(y, x) / 2.0);
                    }
                }
                else if (channel->dataFormat == BTA_DataFormatUInt16 ||
                         channel->dataFormat == BTA_DataFormatSInt16) {
                    if (channelMat.at<uint16_t>(y, x) == 0xffff) {
                        imshowMat.at<uint8_t>(y, x) = 0;
                    }
                    else if (channelMat.at<uint16_t>(y, x) > 2000) {
                        imshowMat.at<uint8_t>(y, x) = 0;
                    }
                    else if (channelMat.at<uint16_t>(y, x) == 0) {
                        imshowMat.at<uint8_t>(y, x) = 0xff;
                    }
                    else {
                        imshowMat.at<uint8_t>(y, x) = 0xff - (0xff * channelMat.at<uint16_t>(y, x)) / 2000;
                    }
                }
            }
        }
    }
    else if (channel->id == BTA_ChannelIdX ||
             channel->id == BTA_ChannelIdY) {
        for (int y = 0; y < channel->yRes; y++) {
            for (int x = 0; x < channel->xRes; x++) {
                if (channel->dataFormat == BTA_DataFormatFloat32) {
                    if (channelMat.at<float>(y, x) < -1.0 ||
                        channelMat.at<float>(y, x) > 1.0) {
                        imshowMat.at<uint8_t>(y, x) = 0xff;
                    }
                    else {
                        imshowMat.at<uint8_t>(y, x) = (uint8_t)(256.0 * (channelMat.at<float>(y, x) + 1.0) / 2.0);
                    }
                }
                else if (channel->dataFormat == BTA_DataFormatSInt16) {
                    if (channelMat.at<int16_t>(y, x) < -1000 ||
                        channelMat.at<int16_t>(y, x) > 1000) {
                        imshowMat.at<uint8_t>(y, x) = 0xff;
                    }
                    else {
                        imshowMat.at<uint8_t>(y, x) = (0xff * (channelMat.at<int16_t>(y, x) + 1000)) / 2000;
                    }
                }
            }
        }
    }
    else if (channel->id == BTA_ChannelIdAmplitude ||
             channel->id == BTA_ChannelIdCustom01 ||
             channel->id == BTA_ChannelIdCustom02 ||
             channel->id == BTA_ChannelIdCustom03 ||
             channel->id == BTA_ChannelIdCustom04 ||
             channel->id == BTA_ChannelIdCustom05)  {
        float max = FLT_MIN;
        float min = FLT_MAX;
        for (int y = 0; y < channel->yRes; y++) {
            for (int x = 0; x < channel->xRes; x++) {
                if (channel->dataFormat == BTA_DataFormatFloat32) {
                    if (channelMat.at<float>(y, x) > max) {
                        max = channelMat.at<float>(y, x);
                    }
                    if (channelMat.at<float>(y, x) < min) {
                        min = channelMat.at<float>(y, x);
                    }
                }
                else if (channel->dataFormat == BTA_DataFormatUInt16) {
                    if (channelMat.at<uint16_t>(y, x) > max) {
                        max = channelMat.at<uint16_t>(y, x);
                    }
                    if (channelMat.at<uint16_t>(y, x) < min) {
                        min = channelMat.at<uint16_t>(y, x);
                    }
                }
                else if (channel->dataFormat == BTA_DataFormatSInt16) {
                    if (channelMat.at<int16_t>(y, x) > max) {
                        max = channelMat.at<int16_t>(y, x);
                    }
                    if (channelMat.at<int16_t>(y, x) < min) {
                        min = channelMat.at<int16_t>(y, x);
                    }
                }
            }
        }
        for (int y = 0; y < channel->yRes; y++) {
            for (int x = 0; x < channel->xRes; x++) {
                if (channel->dataFormat == BTA_DataFormatFloat32) {
                    imshowMat.at<uint8_t>(y, x) = (uint8_t)(255.0 * (channelMat.at<float>(y, x) - min) / (max - min));
                }
                else if (channel->dataFormat == BTA_DataFormatUInt16) {
                    imshowMat.at<uint8_t>(y, x) = (uint8_t)(255.0 * (channelMat.at<uint16_t>(y, x) - min) / (max - min));
                }
                else if (channel->dataFormat == BTA_DataFormatSInt16) {
                    imshowMat.at<uint8_t>(y, x) = (uint8_t)(255.0 * (channelMat.at<int16_t>(y, x) - min) / (max - min));
                }
            }
        }
    }
    else if (channel->id == BTA_ChannelIdFlags)  {
        for (int y = 0; y < channel->yRes; y++) {
            for (int x = 0; x < channel->xRes; x++) {
                if (channel->dataFormat == BTA_DataFormatUInt32) {
                    // UINT32_T DOES NOT WORK!!! OPENCV THINKS IT'S 8 BYTE WIDE; WHEREAS INT32_T IS KNOWN TO BE 4 BYTE WIDE
                    imshowMat.at<uint8_t>(y, x) = (uint8_t)channelMat.at<int32_t>(y, x) * 0xf;
                }
            }
        }
    }
    else if (channel->id == BTA_ChannelIdPhase0 ||
             channel->id == BTA_ChannelIdPhase90 ||
             channel->id == BTA_ChannelIdPhase180 ||
             channel->id == BTA_ChannelIdPhase270)  {
        double min, max;
        cv::minMaxLoc(channelMat, &min, &max);
        for (int y = 0; y < channel->yRes; y++) {
            for (int x = 0; x < channel->xRes; x++) {
                if (channel->dataFormat == BTA_DataFormatUInt32) {
                    imshowMat.at<uint8_t>(y, x) = (uint8_t)((channelMat.at<uint32_t>(y, x) - min) * 255 / (max - min));
                }
                else if (channel->dataFormat == BTA_DataFormatUInt16) {
                    imshowMat.at<uint8_t>(y, x) = (uint8_t)((channelMat.at<uint16_t>(y, x) - min) * 255 / (max - min));
                }
                else if (channel->dataFormat == BTA_DataFormatSInt16) {
                    //imshowMat.at<uint8_t>(y, x) = (uint8_t)(((uint16_t)channelMat.at<int16_t>(y, x) + 32768) / 100);
                    imshowMat.at<uint8_t>(y, x) = (uint8_t)((channelMat.at<int16_t>(y, x) - min) * 255 / (max - min));
                }
            }
        }
    }
    else if (channel->id == BTA_ChannelIdColor) {
        if (channel->dataFormat == BTA_DataFormatRgb565 && channel->xRes > 0 && channel->yRes > 0) {
            cv::cvtColor(channelMat, imshowMat, CV_BGR5652BGR);
		}
		else if (channel->dataFormat == BTA_DataFormatRgb24 && channel->xRes > 0 && channel->yRes > 0) {
			cv::cvtColor(channelMat, imshowMat, CV_RGB2BGR);
		}
        else if (channel->dataFormat == BTA_DataFormatYuv422 && channel->xRes > 0 && channel->yRes > 0) {
            cv::cvtColor(channelMat, imshowMat, CV_YUV2BGR_Y422);
        }
        else if (channel->dataFormat == BTA_DataFormatYuv444 && channel->xRes > 0 && channel->yRes > 0) {
            //cv::cvtColor(channelMat, imshowMat, CV_YUV2BGR);
            cv::cvtColor(channelMat, imshowMat, CV_YCrCb2RGB);
        }
		else {
			cv::cvtColor(cv::Mat::zeros(channel->yRes, channel->xRes, CV_8UC3), imshowMat, CV_BGR5652BGR);
		}
    }
    else if(channel->id == BTA_ChannelIdGrayScale){
        for (int y = 0; y < channel->yRes; y++) {
            for (int x = 0; x < channel->xRes; x++) {
                if (channel->dataFormat == BTA_DataFormatUInt32) {
                    imshowMat.at<uint8_t>(y, x) = (uint8_t)(channelMat.at<uint32_t>(y, x) / 60);
                }
                else if (channel->dataFormat == BTA_DataFormatUInt16) {
                    imshowMat.at<uint8_t>(y, x) = (uint8_t)(channelMat.at<uint16_t>(y, x) / 60);
                }
                else if (channel->dataFormat == BTA_DataFormatSInt16) {
                    imshowMat.at<uint8_t>(y, x) = (uint8_t)(channelMat.at<int16_t>(y, x) / 60);
                }
            }
        }
    }

    if (resizeFactor > 1) {
        cv::Mat imshowBigMat = cv::Mat(resizeFactor * channel->yRes, resizeFactor * channel->xRes, outputCvType);
        cv::resize(imshowMat, imshowBigMat, imshowBigMat.size(), resizeFactor, resizeFactor, cv::INTER_LINEAR);
        imshowMat.release();
        channelMat.release();
        return imshowBigMat;
    }

    channelMat.release();
    return imshowMat;
}

#endif


