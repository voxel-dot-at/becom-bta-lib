#include "undistort.h"
#include <bta_helper.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <math.h>



void initUndistortRectifyMapPh(BTA_IntrinsicData *intData, uint16_t *xy) {
    //double ir[9] = { 1/fx,     0,  -cx / fx,
    //                    0,  1/fy,  -cy / fy,
    //                    0,     0,         1 };
    double ir0 = 1 / intData->fx;
    double ir2 = -intData->cx / intData->fx;
    double ir4 = 1 / intData->fy;
    double ir5 = -intData->cy / intData->fy;

    const double k4 = 0.;
    const double k5 = 0.;
    const double k6 = 0.;

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


void initUndistortRectifyMapFe(double k1, double k2, double k3, double k4, double fx, double fy, double cx, double cy, int xRes, int yRes, uint16_t *xy) {
    //double ir[9] = { 1/fx,     0,  -cx / fx,
    //                    0,  1/fy,  -cy / fy,
    //                    0,     0,         1 };
    double ir0 = 1 / fx;
    double ir2 = -cx / fx;
    double ir4 = 1 / fy;
    double ir5 = -cy / fy;

    uint16_t *xyTemp = xy;
    for (int row = 0; row < yRes; row++)
    {
        double _x = ir2;              // row * ir[1] + ir[2];  // row * 0    + -cx/fx
        double _y = row * ir4 + ir5;  // row * ir[4] + ir[5];  // row * 1/fy + -cy/fx
        double _z = 1;                // row * ir[7] + ir[8];  // row * 0    + 1

        for (int col = 0; col < xRes; col++)
        {
            double x_1 = _x / _z;
            double y_1 = _y / _z;
            double r = sqrt(x_1 * x_1 + y_1 * y_1);
            double theta = atan(r);
            double theta2 = theta*theta;
            double theta4 = theta2*theta2;
            double theta6 = theta4*theta2;
            double theta8 = theta4*theta4;
            double theta_d = theta * (1 + k1 * theta2 + k2 * theta4 + k3 * theta6 + k4 * theta8);
            double scale = (r == 0) ? 1.0 : theta_d / r;
            double u = fx * x_1 * scale + cx;
            double v = fy * y_1 * scale + cy;
            *xyTemp++ = (uint16_t)round(u);
            *xyTemp++ = (uint16_t)round(v);

            _x += ir0;       // + 1/fx
            //_y += ir[3];   // + 0
            //_z += ir[6];   // + 0
        }
    }
}


void cvRemapRgb24(uint8_t *src, uint8_t *dst, uint16_t xRes, uint16_t yRes, uint16_t *xy) {
    uint16_t *xyTemp = (uint16_t *)xy;
    uint8_t *D = dst;
    for (int i = 0; i < xRes * yRes; i++) {
        int xTemp = (int)*xyTemp++;
        int yTemp = (int)*xyTemp++;
        if (xTemp >= 0 && xTemp < xRes && yTemp >= 0 && yTemp < yRes) {
            uint8_t *S = src + (xTemp + yTemp * xRes) * 3;
            *D++ = *S++;
            *D++ = *S++;
            *D++ = *S;
        }
    }
}


void cvRemapRgb565(uint16_t *src, uint16_t *dst, uint16_t xRes, uint16_t yRes, uint16_t *xy) {
    uint16_t *xyTemp = (uint16_t *)xy;
    uint16_t *D = dst;
    int dsize = xRes * yRes;
    for (int i = 0; i < dsize; i++) {
        int xTemp = (int)*xyTemp++;
        int yTemp = (int)*xyTemp++;
        if (xTemp >= 0 && xTemp < xRes && yTemp >= 0 && yTemp < yRes) {
            uint16_t *S = src + xTemp + yTemp * xRes;
            *D++ = *S;
        }
    }
}



//-----------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------



BTA_Status BTAundistortInit(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    winst->undistortInst = (BTA_UndistortInst *)calloc(1, sizeof(BTA_UndistortInst));
    BTA_UndistortInst *inst = winst->undistortInst;
    if (!inst) {
        return BTA_StatusOutOfMemory;
    }
    inst->enabled = 1;
    inst->undistortionMaps = 0;
    inst->undistortionMapsLen = 0;
    inst->dstBuf = 0;
    inst->dstBufLen = 0;
    return BTA_StatusOk;
}


static BTA_Status addUndistortionMap(BTA_WrapperInst *winst, BTA_IntrinsicData *intData) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UndistortInst *inst = (BTA_UndistortInst *)winst->undistortInst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UndistortionMap *map = (BTA_UndistortionMap *)malloc(sizeof(BTA_UndistortionMap));
    if (!map) {
        return BTA_StatusOutOfMemory;
    }
    map->xRes = intData->xRes;
    map->yRes = intData->yRes;
    map->xy = (uint16_t *)malloc(intData->yRes * intData->xRes * 2 * sizeof(uint16_t));
    if (!map->xy) {
        free(map);
        return BTA_StatusOutOfMemory;
    }
    initUndistortRectifyMapPh(intData, map->xy);
    if (!inst->undistortionMaps) {
        inst->undistortionMaps = (BTA_UndistortionMap **)malloc(sizeof(BTA_UndistortionMap *));
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


static BTA_Status addEmptyUndistortionMap(BTA_WrapperInst *winst, uint16_t xRes, uint16_t yRes) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UndistortInst *inst = (BTA_UndistortInst *)winst->undistortInst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UndistortionMap *map = (BTA_UndistortionMap *)malloc(sizeof(BTA_UndistortionMap));
    if (!map) {
        return BTA_StatusOutOfMemory;
    }
    map->xRes = xRes;
    map->yRes = yRes;
    map->xy = 0;
    if (!inst->undistortionMaps) {
        inst->undistortionMaps = (BTA_UndistortionMap **)malloc(sizeof(BTA_UndistortionMap *));
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


BTA_Status BTAundistortClose(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UndistortInst *inst = (BTA_UndistortInst *)winst->undistortInst;
    winst->undistortInst = 0;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    if (inst) {
        for (int i = 0; i < inst->undistortionMapsLen; i++) {
            free(inst->undistortionMaps[i]->xy);
            free(inst->undistortionMaps[i]);
        }
        free(inst->undistortionMaps);
        free(inst->dstBuf);
        free(inst);
    }
    return BTA_StatusOk;
}


static BTA_Status undistortApply(BTA_UndistortInst *inst, BTA_Channel *channel) {
    for (int i = 0; i < inst->undistortionMapsLen; i++) {
        BTA_UndistortionMap *map = inst->undistortionMaps[i];
        if (channel->xRes == map->xRes && channel->yRes == map->yRes) {
            if (!map->xy) {
                return BTA_StatusOk;
            }
            if (inst->dstBufLen != channel->dataLen) {
                free(inst->dstBuf);
                inst->dstBufLen = channel->dataLen;
                inst->dstBuf = malloc(inst->dstBufLen);
                if (!inst->dstBuf) {
                    inst->dstBufLen = 0;
                    inst->dstBuf = 0;
                    return BTA_StatusOutOfMemory;
                }
            }
            if (channel->dataFormat == BTA_DataFormatRgb24) {
                cvRemapRgb24(channel->data, (uint8_t *)inst->dstBuf, map->xRes, map->yRes, map->xy);
            }
            else if (channel->dataFormat == BTA_DataFormatRgb565) {
                cvRemapRgb565((uint16_t *)channel->data, (uint16_t *)inst->dstBuf, map->xRes, map->yRes, map->xy);
            }
            void *temp = channel->data;
            channel->data = (uint8_t *)inst->dstBuf;
            inst->dstBuf = temp;
            return BTA_StatusOk;
        }
    }
    return BTA_StatusInvalidData;
}


static void tryToLoadMap(BTA_WrapperInst *winst, uint16_t xRes, uint16_t yRes) {
    // No undistortion map found for this resolution -> try to read from camera
    BTA_IntrinsicData **intData = 0;
    uint16_t intDataLen = 0;
    BTA_Status status = BTAreadGeomModelFromFlash(winst, &intData, &intDataLen, 0, 0, 1);
    if (status == BTA_StatusOk && intDataLen > 1) {
        if (intDataLen >= 2 && intData[1] && xRes == intData[1]->xRes && yRes == intData[1]->yRes) {
            // It is defined, that the RGB stream to distort comes from lens index 1, thus only
            // check lens parameters at index 1
            addUndistortionMap(winst, intData[1]);
            BTAinfoEventHelperII(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Loaded undistortion data for RGB %dx%d", xRes, yRes);
            BTAfreeIntrinsicData(&intData, intDataLen);
            return;
        }
        BTAfreeIntrinsicData(&intData, intDataLen);
    }
    BTA_IntrinsicData intDataTemp;
    status = BTAreadMetrilusFromFlash(winst, &intDataTemp, 1);
    if (status == BTA_StatusOk) {
        if (xRes == intDataTemp.xRes && yRes == intDataTemp.yRes) {
            addUndistortionMap(winst, &intDataTemp);
            BTAinfoEventHelperII(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Loaded undistortion data for RGB %dx%d (legacy format)", xRes, yRes);
            return;
        }
    }
    addEmptyUndistortionMap(winst, xRes, yRes);
}


BTA_Status BTAundistortApply(BTA_WrapperInst *winst, BTA_Frame *frame) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UndistortInst *inst = (BTA_UndistortInst *)winst->undistortInst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    if (!inst->enabled) {
        return BTA_StatusOk;
    }
    if (!frame) {
        return BTA_StatusInvalidParameter;
    }
    for (int chIn = 0; chIn < frame->channelsLen; chIn++) {
        BTA_Channel *channel = frame->channels[chIn];
        if (channel->id == BTA_ChannelIdColor && channel->xRes > 0 && channel->yRes > 0) {
            BTA_Status status = undistortApply(inst, channel);
            if (status == BTA_StatusInvalidData) {
                tryToLoadMap(winst, channel->xRes, channel->yRes);
                return undistortApply(inst, channel);
            }
            return status;
        }
    }
    return BTA_StatusOk;
}