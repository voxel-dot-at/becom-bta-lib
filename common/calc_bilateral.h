#ifndef CALCBILATERAL_H_INCLUDED
#define CALCBILATERAL_H_INCLUDED

#include <bta.h>
#include <bta_helper.h>

#define BILAT_SIGMA_S                     20
#define BILAT_SIGMA_R                     30
#define BILAT_TOL                         0.01

BTA_Status BTAcalcBilateralApply(BTA_WrapperInst *winst, BTA_Frame *frame, uint8_t windowSize);

#endif