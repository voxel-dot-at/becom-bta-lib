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
#include <bta_frame.h>
//#include <bta_helper.h>

struct BTA_Frame;

BTA_Status BTAjpegFrameToRgb24(BTA_Frame *frame);

#endif

