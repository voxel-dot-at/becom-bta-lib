#include "undistort.h"
#include <bta_helper.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <mth_math.h>
#include <math.h>



static void initUndistortRectifyMap(BTA_IntrinsicData *intData, uint16_t *xy) {
    if (intData->k4 == 0.0 && intData->k5 == 0.0 && intData->k6 == 0.0) {
        // it's pinhole intrinsic data
        //double ir[9] = { 1/fx,     0,  -cx / fx,
        //                    0,  1/fy,  -cy / fy,
        //                    0,     0,         1 };
        double ir0 = 1 / intData->fx;
        double ir2 = -intData->cx / intData->fx;
        double ir4 = 1 / intData->fy;
        double ir5 = -intData->cy / intData->fy;

        const double k4 = 0.0;
        const double k5 = 0.0;
        const double k6 = 0.0;

        uint16_t *xyTemp = xy;
        for (int row = 0; row < intData->yRes; row++)
        {
            double _x = ir2;              // row * ir[1] + ir[2];  // row * 0    + -cx/fx
            double _y = row * ir4 + ir5;  // row * ir[4] + ir[5];  // row * 1/fy + -cy/fx
            double _z = 1;                // row * ir[7] + ir[8];  // row * 0    + 1

            for (int col = 0; col < intData->xRes; col++)
            {
                double x_1 = _x / _z;
                double y_1 = _y / _z;
                double x2 = x_1 * x_1;
                double y2 = y_1 * y_1;
                double r2 = x2 + y2;
                double _2xy = 2 * x_1 * y_1;
                double kr = (1 + ((intData->k3 * r2 + intData->k2) * r2 + intData->k1) * r2) / (1 + ((k6 * r2 + k5) * r2 + k4) * r2);
                double u = intData->fx * (x_1 * kr + intData->p1 * _2xy + intData->p2 * (r2 + 2 * x2)) + intData->cx;
                double v = intData->fy * (y_1 * kr + intData->p1 * (r2 + 2 * y2) + intData->p2 * _2xy) + intData->cy;
                *xyTemp++ = (uint16_t)round(u);
                *xyTemp++ = (uint16_t)round(v);

                _x += ir0;       // + 1/fx
                //_y += ir[3];   // + 0
                //_z += ir[6];   // + 0
            }
        }
    }
    else if (intData->p1 == 0.0 && intData->p2 == 0.0 && intData->k5 == 0.0 && intData->k6 == 0.0) {
        // it's fisheye intrisic data
        //double ir[9] = { 1/fx,     0,  -cx / fx,
        //                    0,  1/fy,  -cy / fy,
        //                    0,     0,         1 };
        double ir0 = 1 / intData->fx;
        double ir2 = -intData->cx / intData->fx;
        double ir4 = 1 / intData->fy;
        double ir5 = -intData->cy / intData->fy;

        uint16_t *xyTemp = xy;
        for (int row = 0; row < intData->yRes; row++)
        {
            double _x = ir2;              // row * ir[1] + ir[2];  // row * 0    + -cx/fx
            double _y = row * ir4 + ir5;  // row * ir[4] + ir[5];  // row * 1/fy + -cy/fx
            double _z = 1;                // row * ir[7] + ir[8];  // row * 0    + 1

            for (int col = 0; col < intData->xRes; col++)
            {
                double x_1 = _x / _z;
                double y_1 = _y / _z;
                double r = sqrt(x_1 * x_1 + y_1 * y_1);
                double theta = atan(r);
                double theta2 = theta * theta;
                double theta4 = theta2 * theta2;
                double theta6 = theta4 * theta2;
                double theta8 = theta4 * theta4;
                double theta_d = theta * (1 + intData->k1 * theta2 + intData->k2 * theta4 + intData->k3 * theta6 + intData->k4 * theta8);
                double scale = (r == 0) ? 1.0 : theta_d / r;
                double u = intData->fx * x_1 * scale + intData->cx;
                double v = intData->fy * y_1 * scale + intData->cy;
                *xyTemp++ = (uint16_t)round(u);
                *xyTemp++ = (uint16_t)round(v);

                _x += ir0;       // + 1/fx
                //_y += ir[3];   // + 0
                //_z += ir[6];   // + 0
            }
        }
    }
}


static void cvRemap24bit(BTA_UndistortInst *inst, BTA_Channel *channel, uint16_t *xy) {
    if (inst->dstBufLen < channel->dataLen) {
        inst->dstBufLen = channel->dataLen;
        free(inst->dstBuf);
        inst->dstBuf = malloc(inst->dstBufLen);
        if (!inst->dstBuf) {
            inst->dstBufLen = 0;
            inst->dstBuf = 0;
            BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Cannot allocate buffer for undistortion");
            return;
        }
    }
    uint16_t *xyTemp = (uint16_t *)xy;
    uint8_t *dst = (uint8_t *)inst->dstBuf;
    for (int i = 0; i < channel->xRes * channel->yRes; i++) {
        int xTemp = (int)*xyTemp++;
        int yTemp = (int)*xyTemp++;
        if (xTemp >= 0 && xTemp < channel->xRes && yTemp >= 0 && yTemp < channel->yRes) {
            uint8_t *S = channel->data + (xTemp + yTemp * channel->xRes) * 3;
            *dst++ = *S++;
            *dst++ = *S++;
            *dst++ = *S;
        }
    }
}


static void cvRemap16bit(BTA_UndistortInst *inst, BTA_Channel *channel, uint16_t *xy) {
    if (inst->dstBufLen < channel->dataLen) {
        inst->dstBufLen = channel->dataLen;
        free(inst->dstBuf);
        inst->dstBuf = malloc(inst->dstBufLen);
        if (!inst->dstBuf) {
            inst->dstBufLen = 0;
            inst->dstBuf = 0;
            BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Cannot allocate buffer for undistortion");
            return;
        }
    }
    uint16_t *xyTemp = (uint16_t *)xy;
    uint16_t *dst = (uint16_t *)inst->dstBuf;
    for (int i = 0; i < channel->xRes * channel->yRes; i++) {
        int xTemp = (int)*xyTemp++;
        int yTemp = (int)*xyTemp++;
        if (xTemp >= 0 && xTemp < channel->xRes && yTemp >= 0 && yTemp < channel->yRes) {
            uint16_t *S = (uint16_t *)channel->data + xTemp + yTemp * channel->xRes;
            *dst++ = *S;
        }
    }
}


static void cvRemap8bit(BTA_UndistortInst *inst, BTA_Channel *channel, uint16_t *xy) {
    if (inst->dstBufLen < channel->dataLen) {
        inst->dstBufLen = channel->dataLen;
        free(inst->dstBuf);
        inst->dstBuf = malloc(inst->dstBufLen);
        if (!inst->dstBuf) {
            inst->dstBufLen = 0;
            inst->dstBuf = 0;
            BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Cannot allocate buffer for undistortion");
            return;
        }
    }
    uint16_t *xyTemp = (uint16_t *)xy;
    uint8_t *dst = (uint8_t *)inst->dstBuf;
    for (int i = 0; i < channel->xRes * channel->yRes; i++) {
        int xTemp = (int)*xyTemp++;
        int yTemp = (int)*xyTemp++;
        if (xTemp >= 0 && xTemp < channel->xRes && yTemp >= 0 && yTemp < channel->yRes) {
            uint8_t *S = channel->data + xTemp + yTemp * channel->xRes;
            *dst++ = *S;
        }
    }
}


static void cvYuv422ToRgb24(BTA_UndistortInst *inst, BTA_Channel *channel, uint16_t *xy) {
    // YUV422 shares information over several pixels! Convert to RGB first
    if (inst->dstBufLen < (uint32_t)(3 * channel->xRes * channel->yRes)) {
        inst->dstBufLen = 3 * channel->xRes * channel->yRes;
        free(inst->dstBuf);
        inst->dstBuf = malloc(inst->dstBufLen);
        if (!inst->dstBuf) {
            inst->dstBufLen = 0;
            inst->dstBuf = 0;
            BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Cannot allocate buffer for undistortion");
            return;
        }
    }
    uint8_t *dst = (uint8_t *)inst->dstBuf;
    int C1 = 0, D = 0, C2 = 0, E = 0;
    uint8_t alter = 1;
    uint16_t *src = (uint16_t *)channel->data;
    for (int i = 0; i < channel->xRes * channel->yRes; i++) {
        if (alter)
        {
            C1 = (*src >> 8) - 16;
            D = (*src & 0xff) - 128;
        }
        else
        {
            C2 = (*src >> 8) - 16;
            E = (*src & 0xff) - 128;
            *dst++ = (uint8_t)MTHmax(0, MTHmin(255, (298 * C1 + 409 * E + 128) >> 8));
            *dst++ = (uint8_t)MTHmax(0, MTHmin(255, (298 * C1 - 100 * D - 208 * E + 128) >> 8));
            *dst++ = (uint8_t)MTHmax(0, MTHmin(255, (298 * C1 + 516 * D + 128) >> 8));
            *dst++ = (uint8_t)MTHmax(0, MTHmin(255, (298 * C2 + 409 * E + 128) >> 8));
            *dst++ = (uint8_t)MTHmax(0, MTHmin(255, (298 * C2 - 100 * D - 208 * E + 128) >> 8));
            *dst++ = (uint8_t)MTHmax(0, MTHmin(255, (298 * C2 + 516 * D + 128) >> 8));
        }
        alter =  1 - alter;
        src++;
    }

    free(channel->data);
    channel->dataLen = 3 * channel->xRes * channel->yRes;
    channel->data = (uint8_t *)malloc(channel->dataLen);
    if (!channel->data) {
        channel->data = 0;
        channel->dataLen = 0;
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Cannot allocate buffer for undistortion");
        return;
    }
    uint16_t *xyTemp = (uint16_t *)xy;
    dst = (uint8_t *)channel->data;
    for (int i = 0; i < channel->xRes * channel->yRes; i++) {
        int xTemp = (int)*xyTemp++;
        int yTemp = (int)*xyTemp++;
        if (xTemp >= 0 && xTemp < channel->xRes && yTemp >= 0 && yTemp < channel->yRes) {
            uint8_t *S = (uint8_t *)inst->dstBuf + (xTemp + yTemp * channel->xRes) * 3;
            *dst++ = *S++;
            *dst++ = *S++;
            *dst++ = *S;
        }
    }
    channel->dataFormat = BTA_DataFormatRgb24;
}



//-----------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------



BTA_Status BTAundistortInit(BTA_UndistortInst **inst, BTA_InfoEventInst *infoEventInst) {
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    *inst = (BTA_UndistortInst *)calloc(1, sizeof(BTA_UndistortInst));
    if (!*inst) {
        return BTA_StatusOutOfMemory;
    }
    (*inst)->infoEventInst = infoEventInst;
    (*inst)->undistortionMaps = 0;
    (*inst)->undistortionMapsLen = 0;
    (*inst)->dstBuf = 0;
    (*inst)->dstBufLen = 0;
    return BTA_StatusOk;
}


static BTA_Status addUndistortionMap(BTA_UndistortInst *inst, BTA_IntrinsicData *intData) {
    if (!inst || !intData) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UndistortionMap *map = (BTA_UndistortionMap *)calloc(1, sizeof(BTA_UndistortionMap));
    if (!map) {
        return BTA_StatusOutOfMemory;
    }
    map->lensIndex = intData->lensIndex;
    map->xRes = intData->xRes;
    map->yRes = intData->yRes;
    map->xy = (uint16_t *)malloc(intData->yRes * intData->xRes * 2 * sizeof(uint16_t));
    if (!map->xy) {
        free(map);
        return BTA_StatusOutOfMemory;
    }
    initUndistortRectifyMap(intData, map->xy);
    if (!inst->undistortionMaps) {
        inst->undistortionMaps = (BTA_UndistortionMap **)calloc(1, sizeof(BTA_UndistortionMap *));
        inst->undistortionMapsLen = 1;
    }
    else {
        inst->undistortionMapsLen++;
        BTA_UndistortionMap **temp = inst->undistortionMaps;
        inst->undistortionMaps = (BTA_UndistortionMap **)realloc(inst->undistortionMaps, inst->undistortionMapsLen * sizeof(BTA_UndistortionMap *));
        if (!inst->undistortionMaps) {
            inst->undistortionMapsLen--;
            inst->undistortionMaps = temp;
            free(map->xy);
            free(map);
            return BTA_StatusOutOfMemory;
        }
    }
    inst->undistortionMaps[inst->undistortionMapsLen - 1] = map;
    return BTA_StatusOk;
}


static BTA_Status addEmptyUndistortionMap(BTA_UndistortInst *inst, uint16_t lensIndex, uint16_t xRes, uint16_t yRes) {
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UndistortionMap *map = (BTA_UndistortionMap *)calloc(1, sizeof(BTA_UndistortionMap));
    if (!map) {
        return BTA_StatusOutOfMemory;
    }
    map->lensIndex = lensIndex;
    map->xRes = xRes;
    map->yRes = yRes;
    map->xy = 0;
    if (!inst->undistortionMaps) {
        inst->undistortionMaps = (BTA_UndistortionMap **)calloc(1, sizeof(BTA_UndistortionMap *));
        inst->undistortionMapsLen = 1;
    }
    else {
        inst->undistortionMapsLen++;
        BTA_UndistortionMap **temp = inst->undistortionMaps;
        inst->undistortionMaps = (BTA_UndistortionMap **)realloc(inst->undistortionMaps, inst->undistortionMapsLen * sizeof(BTA_UndistortionMap *));
        if (!inst->undistortionMaps) {
            inst->undistortionMapsLen--;
            inst->undistortionMaps = temp;
            free(map->xy);
            free(map);
            return BTA_StatusOutOfMemory;
        }
    }
    inst->undistortionMaps[inst->undistortionMapsLen - 1] = map;
    return BTA_StatusOk;
}


BTA_Status BTAundistortClose(BTA_UndistortInst **inst) {
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    if (!*inst) {
        // not even opened
        return BTA_StatusOk;
    }
    for (int i = 0; i < (*inst)->undistortionMapsLen; i++) {
        free((*inst)->undistortionMaps[i]->xy);
        (*inst)->undistortionMaps[i]->xy = 0;
        free((*inst)->undistortionMaps[i]);
        (*inst)->undistortionMaps[i] = 0;
    }
    free((*inst)->undistortionMaps);
    (*inst)->undistortionMaps = 0;
    free((*inst)->dstBuf);
    (*inst)->dstBuf = 0;
    free(*inst);
    *inst = 0;
    return BTA_StatusOk;
}


static BTA_UndistortionMap *getUndistortionMap(BTA_UndistortInst *inst, BTA_WrapperInst *winst, uint16_t lensIndex, uint16_t xRes, uint16_t yRes) {
    for (int i = 0; i < inst->undistortionMapsLen; i++) {
        BTA_UndistortionMap *map = inst->undistortionMaps[i];
        if (lensIndex == map->lensIndex && xRes == map->xRes && yRes == map->yRes) {
            return map;
        }
    }
    BTA_IntrinsicData **intData = 0;
    uint16_t intDataLen = 0;
    BTA_Status status = BTAreadGeomModelFromFlash(winst, &intData, &intDataLen, 0, 0, 1);
    if (status == BTA_StatusOk && intDataLen > 0) {
        for (int i = 0; i < intDataLen; i++) {
            if (intData[i]->lensIndex == lensIndex && xRes == intData[i]->xRes && yRes == intData[i]->yRes) {
                addUndistortionMap(inst, intData[i]);
                BTAinfoEventHelper(inst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Loaded undistortion data for RGB %dx%d, lensIndex %d", xRes, yRes, lensIndex);
                BTAfreeIntrinsicData(&intData, intDataLen);
                return inst->undistortionMaps[inst->undistortionMapsLen - 1];
            }
        }
        BTAfreeIntrinsicData(&intData, intDataLen);
    }
    BTA_IntrinsicData intDataTemp;
    status = BTAreadMetrilusFromFlash(winst, &intDataTemp, 1);
    if (status == BTA_StatusOk) {
        if (xRes == intDataTemp.xRes && yRes == intDataTemp.yRes) {
            intDataTemp.lensIndex = lensIndex;
            addUndistortionMap(inst, &intDataTemp);
            BTAinfoEventHelper(inst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Loaded undistortion data for RGB %dx%d, lensIndex %d (legacy format)", xRes, yRes, lensIndex);
            return inst->undistortionMaps[inst->undistortionMapsLen - 1];
        }
    }
    addEmptyUndistortionMap(inst, (uint16_t)lensIndex, xRes, yRes);
    BTAinfoEventHelper(inst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "No undistortion data available for RGB %dx%d, lensIndex %d", xRes, yRes, lensIndex);
    return inst->undistortionMaps[inst->undistortionMapsLen - 1];
}


BTA_Status BTAundistortApply(BTA_UndistortInst *inst, BTA_WrapperInst *winst, BTA_Frame *frame) {
    if (!inst || !winst) {
        return BTA_StatusInvalidParameter;
    }
    if (!frame) {
        return BTA_StatusInvalidParameter;
    }
    for (int chIn = 0; chIn < frame->channelsLen; chIn++) {
        BTA_Channel *channel = frame->channels[chIn];
        if (channel->id != BTA_ChannelIdColor || channel->xRes == 0 || channel->yRes == 0 || (channel->flags & 1) || (channel->flags & 2)) {
            // channel isn't RGB, resolution isn't valid, an overlay or already undistorted
            continue;
        }
        BTA_UndistortionMap *map = getUndistortionMap(inst, winst, channel->lensIndex, channel->xRes, channel->yRes);
        if (!map->xy) {
            // No calibration data available
            continue;
        }
        if (channel->dataFormat == BTA_DataFormatRgb24) {
            cvRemap24bit(inst, channel, map->xy);
            channel->flags |= 2;
        }
        else if (channel->dataFormat == BTA_DataFormatRgb565) {
            cvRemap16bit(inst, channel, map->xy);
            channel->flags |= 2;
        }
        else if (channel->dataFormat == BTA_DataFormatUInt8) {
            cvRemap8bit(inst, channel, map->xy);
            channel->flags |= 2;
        }
        else if (channel->dataFormat == BTA_DataFormatYuv422) {
            cvYuv422ToRgb24(inst, channel, map->xy);
        }
        else {
            continue;
        }
    }
    return BTA_StatusOk;
}