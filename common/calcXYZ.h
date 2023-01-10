#ifndef CALCXYZ_H_INCLUDED
#define CALCXYZ_H_INCLUDED

#include <bta.h>
#include <bta_helper.h>

struct BTA_WrapperInst;

typedef struct BTA_CalcXYZConfig {
} BTA_CalcXYZConfig;


typedef struct BTA_CalcXYZVectors {
    uint16_t xRes;
    uint16_t yRes;
    uint16_t vectorsLen;
    float* vectorsX;
    float *vectorsY;
    float *vectorsZ;
} BTA_CalcXYZVectors;


typedef struct BTA_CalcXYZInst {
    uint8_t enabled;
    BTA_CalcXYZVectors** vectorsList;
    uint16_t vectorsListLen;
    float offset;
} BTA_CalcXYZInst;


BTA_Status BTAcalcXYZInit(BTA_WrapperInst *winst);
BTA_Status BTAcalcXYZClose(BTA_WrapperInst *winst);
BTA_Status BTAcalcXYZApply(BTA_WrapperInst *winst, BTA_Frame *frame);


#endif