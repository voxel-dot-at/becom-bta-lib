#ifndef CALCXYZ_H_INCLUDED
#define CALCXYZ_H_INCLUDED

#include <bta.h>
#include <bta_helper.h>

struct BTA_WrapperInst;

//typedef struct BTA_CalcXYZConfig {
//} BTA_CalcXYZConfig;


typedef struct BTA_CalcXYZInst {
    BTA_LensVectors** lensVectorsList;
    uint16_t lensVectorsListLen;
    BTA_InfoEventInst *infoEventInst;
} BTA_CalcXYZInst;


BTA_Status BTAcalcXYZInit(BTA_CalcXYZInst **inst, BTA_InfoEventInst *infoEventInst);
BTA_Status BTAcalcXYZClose(BTA_CalcXYZInst **winst);
BTA_Status BTAcalcXYZApply(BTA_CalcXYZInst *inst, BTA_WrapperInst *winst, BTA_Frame *frame, float offset);


#endif