


#ifndef UNDISTORT_H_INCLUDED
#define UNDISTORT_H_INCLUDED

// TODO: remove dependency!!!
#include <bta.h>
#include <bta_helper.h>

struct BTA_WrapperInst;

typedef struct BTA_UndistortConfig {
} BTA_UndistortConfig;


typedef struct BTA_UndistortionMap {
    uint32_t xRes;
    uint32_t yRes;
    uint16_t *xy;
} BTA_UndistortionMap;


typedef struct BTA_UndistortInst {
    uint8_t enabled;
    BTA_UndistortionMap **undistortionMaps;
    uint16_t undistortionMapsLen;
    void *dstBuf;
    uint32_t dstBufLen;
} BTA_UndistortInst;


BTA_Status BTAundistortInit(BTA_WrapperInst *winst);
BTA_Status BTAundistortClose(BTA_WrapperInst *winst);
BTA_Status BTAundistortApply(BTA_WrapperInst *winst, BTA_Frame *frame);


#endif