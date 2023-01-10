/**  @file bta_jpg.h
*
*    @brief Header file for bta_jpg.c
*
*    BLT_DISCLAIMER
*
*    @author Alex Falkensteiner
*
*    @cond svn
*
*    Information of last commit
*    $Rev::               $:  Revision of last commit
*    $Author::            $:  Author of last commit
*    $Date::              $:  Date of last commit
*
*    @endcond
*/



#ifndef BTA_JPG_H_INCLUDED
#define BTA_JPG_H_INCLUDED

#include <stdint.h>
// TODO: remove dependency!!!
#include <bta_status.h>
#include <bta_helper.h>


typedef struct BTA_JpgInst {
    uint8_t enabled;
} BTA_JpgInst;


BTA_Status BTAjpgInit(BTA_WrapperInst *winst);
BTA_Status BTAjpgClose(BTA_WrapperInst *winst);
BTA_Status BTAjpgEnable(BTA_WrapperInst *winst, uint8_t enable);
BTA_Status BTAjpgIsEnabled(BTA_WrapperInst *winst, uint8_t *enabled);
BTA_Status BTAdecodeJpgToRgb24(BTA_WrapperInst *winst, uint8_t *dataIn, uint32_t dataInLen, uint8_t *dataOut, uint32_t dataOutLen);

#endif

