/**  @file bta_helper.c
*
*    @brief The main c for BltTofApi. Implements device independent interface functions and
*           wrapps device specific functions
*           Also the definition of the config struct organisation.
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

#include "bta_helper.h"
#include <bta_oshelper.h>
#include <timing_helper.h>
#include <utils.h>
#include "configuration.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#ifdef PLAT_APPLE
#   include <stdbool.h>
#endif

#include <crc16.h>
#include <crc32.h>
#include <undistort.h>
#ifndef WO_LIBJPEG
#   include <bta_jpg.h>
#endif
#include <bvq_queue.h>


// local prototypes

static BTA_ChannelId BTAETHgetChannelId(BTA_EthImgMode imgMode, uint8_t channelIndex);
static BTA_DataFormat BTAETHgetDataFormat(BTA_EthImgMode imgMode, uint8_t channelIndex, uint8_t colorMode, uint8_t rawPhaseContent);
static BTA_Unit BTAETHgetUnit(BTA_EthImgMode imgMode, uint8_t channelIndex);
static void BTAinsertTestPattern(BTA_Frame *frame, uint16_t testPattern);
static BTA_Status insertChannelData(BTA_WrapperInst *winst, BTA_ChannelId id, BTA_DataFormat dataFormat, uint8_t *data, uint16_t width, uint16_t height, uint32_t dataLen, uint8_t decodingEnabled, BTA_Channel *channel);
static void setMissingAsInvalid(BTA_ChannelId channelId, BTA_DataFormat dataFormat, uint8_t *channelDataStart, int channelDataLength, BTA_FrameToParse *frameToParse);
static void logToFile(BTA_InfoEventInst *infoEventInst, BTA_Status status, const char *msg);


static uint32_t timeStampLast = 0;
static uint16_t frameCounterLast = 0;


BTA_Status BTAcreateFrameToParse(BTA_FrameToParse **frameToParse) {
    BTA_FrameToParse *ftp = (BTA_FrameToParse *)calloc(1, sizeof(BTA_FrameToParse));
    if (!ftp) {
        *frameToParse = 0;
        return BTA_StatusOutOfMemory;
    }
    *frameToParse = ftp;
    return BTA_StatusOk;
}


BTA_Status BTAinitFrameToParse(BTA_FrameToParse **frameToParse, uint64_t timestamp, uint16_t frameCounter, uint32_t frameLen, uint16_t packetCountTotal) {
    if (!frameToParse || !frameLen || !packetCountTotal) {
        return BTA_StatusInvalidParameter;
    }
    BTA_FrameToParse *ftp = *frameToParse;
    if (!ftp) {
        return BTA_StatusInvalidParameter;
    }
    ftp->timestamp = timestamp;
    ftp->frameCounter = frameCounter;
    if (frameLen > ftp->frameLen) {
        ftp->frame = (uint8_t *)malloc(frameLen);
        if (!ftp->frame) {
            free(ftp);
            *frameToParse = 0;
            return BTA_StatusOutOfMemory;
        }
    }
    ftp->frameLen = frameLen;

    if (packetCountTotal > ftp->packetCountTotal) {
        free(ftp->packetStartAddr);
        ftp->packetStartAddr = (uint32_t *)calloc(packetCountTotal, sizeof(uint32_t));
        if (!ftp->packetStartAddr) {
            free(ftp->frame);
            ftp->frame = 0;
            free(ftp);
            *frameToParse = 0;
            return BTA_StatusOutOfMemory;
        }
        free(ftp->packetSize);
        ftp->packetSize = (uint16_t *)calloc(packetCountTotal, sizeof(uint16_t));
        if (!ftp->packetSize) {
            free(ftp->packetStartAddr);
            ftp->packetStartAddr = 0;
            free(ftp->frame);
            ftp->frame = 0;
            free(ftp);
            *frameToParse = 0;
            return BTA_StatusOutOfMemory;
        }
    }
    else {
        memset(ftp->packetStartAddr, 0, packetCountTotal * sizeof(uint32_t));
        memset(ftp->packetSize, 0, packetCountTotal * sizeof(uint16_t));
    }
    ftp->packetCountGot = 0;
    ftp->packetCountNda = 0;
    ftp->packetCountTotal = packetCountTotal;
    ftp->timeLastPacket = ftp->timestamp;
    ftp->retryTime = ftp->timestamp;
    ftp->retryCount = 0;
    *frameToParse = ftp;
    return BTA_StatusOk;
}


BTA_Status BTAfreeFrameToParse(BTA_FrameToParse **frameToParse) {
    if (!frameToParse) {
        return BTA_StatusInvalidParameter;
    }
    BTA_FrameToParse *ftp = *frameToParse;
    if (!ftp) {
        return BTA_StatusInvalidParameter;
    }
    free(ftp->frame);
    ftp->frame = 0;
    free(ftp->packetStartAddr);
    ftp->packetStartAddr = 0;
    free(ftp->packetSize);
    ftp->packetSize = 0;
    free(ftp);
    *frameToParse = 0;
    return BTA_StatusOk;
}


void BTAinfoEventHelperIIIIIIII(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, int par2, int par3, int par4, int par5, int par6, int par7, int par8) {
#   ifndef DEBUG
    if (status == BTA_StatusDebug) return;
#   endif
    if (!infoEventInst) return;
    if (importance > infoEventInst->verbosity) return;
    char *msgTemp = (char *)malloc(strlen(msg) + 512);
    if (msgTemp) {
        sprintf(msgTemp, msg, par1, par2, par3, par4, par5, par6, par7, par8);
        BTAinfoEventHelper(infoEventInst, importance, status, msgTemp);
        free(msgTemp);
        msgTemp = 0;
    }
    else {
        BTAinfoEventHelper(infoEventInst, importance, status, msg);
    }
}


void BTAinfoEventHelperISIIIII(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, uint8_t *par2, int par3, int par4, int par5, int par6, int par7) {
#   ifndef DEBUG
    if (status == BTA_StatusDebug) return;
#   endif
    if (!infoEventInst) return;
    if (importance > infoEventInst->verbosity) return;
    char *msgTemp = (char *)malloc(strlen(msg) + 512);
    if (msgTemp) {
        sprintf(msgTemp, msg, par1, par2, par3, par4, par5, par6, par7);
        BTAinfoEventHelper(infoEventInst, importance, status, msgTemp);
        free(msgTemp);
        msgTemp = 0;
    }
    else {
        BTAinfoEventHelper(infoEventInst, importance, status, msg);
    }
}


void BTAinfoEventHelperIIIVI(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, int par2, int par3, void *par4, int par5) {
#   ifndef DEBUG
    if (status == BTA_StatusDebug) return;
#   endif
    if (!infoEventInst) return;
    if (importance > infoEventInst->verbosity) return;
    char *msgTemp = (char *)malloc(strlen(msg) + 512);
    if (msgTemp) {
        sprintf(msgTemp, msg, par1, par2, par3, par4, par5);
        BTAinfoEventHelper(infoEventInst, importance, status, msgTemp);
        free(msgTemp);
        msgTemp = 0;
    }
    else {
        BTAinfoEventHelper(infoEventInst, importance, status, msg);
    }
}


void BTAinfoEventHelperIIIII(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, int par2, int par3, int par4, int par5) {
#   ifndef DEBUG
    if (status == BTA_StatusDebug) return;
#   endif
    if (!infoEventInst) return;
    if (importance > infoEventInst->verbosity) return;
    char *msgTemp = (char *)malloc(strlen(msg) + 512);
    if (msgTemp) {
        sprintf(msgTemp, msg, par1, par2, par3, par4, par5);
        BTAinfoEventHelper(infoEventInst, importance, status, msgTemp);
        free(msgTemp);
        msgTemp = 0;
    }
    else {
        BTAinfoEventHelper(infoEventInst, importance, status, msg);
    }
}


void BTAinfoEventHelperIIII(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, int par2, int par3, int par4) {
#   ifndef DEBUG
    if (status == BTA_StatusDebug) return;
#   endif
    if (!infoEventInst) return;
    if (importance > infoEventInst->verbosity) return;
    char *msgTemp = (char *)malloc(strlen(msg) + 512);
    if (msgTemp) {
        sprintf(msgTemp, msg, par1, par2, par3, par4);
        BTAinfoEventHelper(infoEventInst, importance, status, msgTemp);
        free(msgTemp);
        msgTemp = 0;
    }
    else {
        BTAinfoEventHelper(infoEventInst, importance, status, msg);
    }
}


void BTAinfoEventHelperIII(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, int par2, int par3) {
#   ifndef DEBUG
    if (status == BTA_StatusDebug) return;
#   endif
    if (!infoEventInst) return;
    if (importance > infoEventInst->verbosity) return;
    char *msgTemp = (char *)malloc(strlen(msg) + 512);
    if (msgTemp) {
        sprintf(msgTemp, msg, par1, par2, par3);
        BTAinfoEventHelper(infoEventInst, importance, status, msgTemp);
        free(msgTemp);
        msgTemp = 0;
    }
    else {
        BTAinfoEventHelper(infoEventInst, importance, status, msg);
    }
}


void BTAinfoEventHelperISF(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, uint8_t *par2, float par3) {
#   ifndef DEBUG
    if (status == BTA_StatusDebug) return;
#   endif
    if (!infoEventInst) return;
    if (importance > infoEventInst->verbosity) return;
    char *msgTemp = (char *)malloc(strlen(msg) + 512);
    if (msgTemp) {
        sprintf(msgTemp, msg, par1, par2, par3);
        BTAinfoEventHelper(infoEventInst, importance, status, msgTemp);
        free(msgTemp);
        msgTemp = 0;
    }
    else {
        BTAinfoEventHelper(infoEventInst, importance, status, msg);
    }
}


void BTAinfoEventHelperSF(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, uint8_t *par1, float par2) {
#   ifndef DEBUG
    if (status == BTA_StatusDebug) return;
#   endif
    if (!infoEventInst) return;
    if (importance > infoEventInst->verbosity) return;
    char *msgTemp = (char *)malloc(strlen(msg) + 512);
    if (msgTemp) {
        sprintf(msgTemp, msg, par1, par2);
        BTAinfoEventHelper(infoEventInst, importance, status, msgTemp);
        free(msgTemp);
        msgTemp = 0;
    }
    else {
        BTAinfoEventHelper(infoEventInst, importance, status, msg);
    }
}


void BTAinfoEventHelperSV(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, uint8_t *par1, void *par2) {
#   ifndef DEBUG
    if (status == BTA_StatusDebug) return;
#   endif
    if (!infoEventInst) return;
    if (importance > infoEventInst->verbosity) return;
    char *msgTemp = (char *)malloc(strlen(msg) + 512);
    if (msgTemp) {
        sprintf(msgTemp, msg, par1, par2);
        BTAinfoEventHelper(infoEventInst, importance, status, msgTemp);
        free(msgTemp);
        msgTemp = 0;
    }
    else {
        BTAinfoEventHelper(infoEventInst, importance, status, msg);
    }
}


void BTAinfoEventHelperSI(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, uint8_t *par1, int par2) {
#   ifndef DEBUG
    if (status == BTA_StatusDebug) return;
#   endif
    if (!infoEventInst) return;
    if (importance > infoEventInst->verbosity) return;
    char *msgTemp = (char *)malloc(strlen(msg) + 512);
    if (msgTemp) {
        sprintf(msgTemp, msg, par1, par2);
        BTAinfoEventHelper(infoEventInst, importance, status, msgTemp);
        free(msgTemp);
        msgTemp = 0;
    }
    else {
        BTAinfoEventHelper(infoEventInst, importance, status, msg);
    }
}


void BTAinfoEventHelperIF(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, float par2) {
#   ifndef DEBUG
    if (status == BTA_StatusDebug) return;
#   endif
    if (!infoEventInst) return;
    if (importance > infoEventInst->verbosity) return;
    char *msgTemp = (char *)malloc(strlen(msg) + 512);
    if (msgTemp) {
        sprintf(msgTemp, msg, par1, par2);
        BTAinfoEventHelper(infoEventInst, importance, status, msgTemp);
        free(msgTemp);
        msgTemp = 0;
    }
    else {
        BTAinfoEventHelper(infoEventInst, importance, status, msg);
    }
}


void BTAinfoEventHelperII(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, int par2) {
#   ifndef DEBUG
    if (status == BTA_StatusDebug) return;
#   endif
    if (!infoEventInst) return;
    if (importance > infoEventInst->verbosity) return;
    char *msgTemp = (char *)malloc(strlen(msg) + 512);
    if (msgTemp) {
        sprintf(msgTemp, msg, par1, par2);
        BTAinfoEventHelper(infoEventInst, importance, status, msgTemp);
        free(msgTemp);
        msgTemp = 0;
    }
    else {
        BTAinfoEventHelper(infoEventInst, importance, status, msg);
    }
}


void BTAinfoEventHelperIS(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, uint8_t *par2) {
#   ifndef DEBUG
    if (status == BTA_StatusDebug) return;
#   endif
    if (!infoEventInst) return;
    if (importance > infoEventInst->verbosity) return;
    char *msgTemp = (char *)malloc(strlen(msg) + 512);
    if (msgTemp) {
        sprintf(msgTemp, msg, par1, par2);
        BTAinfoEventHelper(infoEventInst, importance, status, msgTemp);
        free(msgTemp);
        msgTemp = 0;
    }
    else {
        BTAinfoEventHelper(infoEventInst, importance, status, msg);
    }
}


void BTAinfoEventHelperS(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, uint8_t *par1) {
#   ifndef DEBUG
    if (status == BTA_StatusDebug) return;
#   endif
    if (!infoEventInst) return;
    if (importance > infoEventInst->verbosity) return;
    char *msgTemp = (char *)malloc(strlen(msg) + 512);
    if (msgTemp) {
        sprintf(msgTemp, msg, par1);
        BTAinfoEventHelper(infoEventInst, importance, status, msgTemp);
        free(msgTemp);
        msgTemp = 0;
    }
    else {
        BTAinfoEventHelper(infoEventInst, importance, status, msg);
    }
}


void BTAinfoEventHelperF(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, float par1) {
#   ifndef DEBUG
    if (status == BTA_StatusDebug) return;
#   endif
    if (!infoEventInst) return;
    if (importance > infoEventInst->verbosity) return;
    char *msgTemp = (char *)malloc(strlen(msg) + 512);
    if (msgTemp) {
        sprintf(msgTemp, msg, par1);
        BTAinfoEventHelper(infoEventInst, importance, status, msgTemp);
        free(msgTemp);
        msgTemp = 0;
    }
    else {
        BTAinfoEventHelper(infoEventInst, importance, status, msg);
    }
}


void BTAinfoEventHelperI(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1) {
#   ifndef DEBUG
    if (status == BTA_StatusDebug) return;
#   endif
    if (!infoEventInst) return;
    if (importance > infoEventInst->verbosity) return;
    char *msgTemp = (char *)malloc(strlen(msg) + 512);
    if (msgTemp) {
        sprintf(msgTemp, msg, par1);
        BTAinfoEventHelper(infoEventInst, importance, status, msgTemp);
        free(msgTemp);
        msgTemp = 0;
    }
    else {
        BTAinfoEventHelper(infoEventInst, importance, status, msg);
    }
}


void BTAinfoEventHelper(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg) {
    static uint64_t timeStart = 0;
    if (!timeStart) timeStart = BTAgetTickCount64();
#   ifndef DEBUG
    if (status == BTA_StatusDebug) return;
#   endif
    if (!infoEventInst) return;
    if (importance > infoEventInst->verbosity) return;
    char* msgTemp = (char *)malloc(strlen(msg) + 512);
    if (msgTemp) {
        sprintf(msgTemp, "%9.3f: %s", (BTAgetTickCount64() - timeStart) / 1000.0, msg);
        if (infoEventInst->infoEventEx2) (*infoEventInst->infoEventEx2)(infoEventInst->handle, status, (int8_t *)msgTemp, infoEventInst->userArg);
        if (infoEventInst->infoEventEx) (*infoEventInst->infoEventEx)(infoEventInst->handle, status, (int8_t *)msgTemp);
        else if (infoEventInst->infoEvent) (*infoEventInst->infoEvent)(status, (int8_t *)msgTemp);
        logToFile(infoEventInst, status, msgTemp);
        free(msgTemp);
        msgTemp = 0;
    }
    else {
        if (infoEventInst->infoEventEx2) (*infoEventInst->infoEventEx2)(infoEventInst->handle, status, (int8_t *)msg, infoEventInst->userArg);
        else if (infoEventInst->infoEventEx) (*infoEventInst->infoEventEx)(infoEventInst->handle, status, (int8_t *)msg);
        else if (infoEventInst->infoEvent) (*infoEventInst->infoEvent)(status, (int8_t *)msg);
        logToFile(infoEventInst, status, msg);
    }
}


static void logToFile(BTA_InfoEventInst *infoEventInst, BTA_Status status, const char *msg) {
    if (infoEventInst->infoEventFilename) {
        void *file;
        const char *statusString = BTAstatusToString2(status);
        char *msgTemp = (char *)malloc(strlen(msg) + 512);
        if (msgTemp) {
            sprintf(msgTemp, "%-100s%19s handle0x%p\n", msg, statusString, infoEventInst->handle);
            BTA_Status status2 = BTAfopen((char *)infoEventInst->infoEventFilename, "a", &file);
            if (status2 == BTA_StatusOk) {
                BTAfwrite(file, msgTemp, (uint32_t)strlen(msgTemp), 0);
                BTAfclose(file);
            }
            free(msgTemp);
            msgTemp = 0;
        }
        else {
            BTA_Status status2 = BTAfopen((char *)infoEventInst->infoEventFilename, "a", &file);
            if (status2 == BTA_StatusOk) {
                BTAfwrite(file, (char *)msg, (uint32_t)strlen(msg), 0);
                BTAfclose(file);
            }
        }
    }
}


BTA_Status BTAparseGrabCallbackEnqueue(BTA_WrapperInst *winst, BTA_FrameToParse *frameToParse) {
    assert(winst);
    assert(frameToParse);

    winst->lpDataStreamPacketsReceivedCount += frameToParse->packetCountGot;
    winst->lpDataStreamPacketsMissedCount += frameToParse->packetCountTotal - frameToParse->packetCountGot;

    BTA_Status status;
    BTA_Frame *frame;
    status = BTAparseFrame(winst, frameToParse, &frame);
    if (status != BTA_StatusOk) {
        // BTAparseFrame itself calls infoEvent on error
        return status;
    }
    winst->lpDataStreamFramesParsedCount++;
    BVQenqueue(winst->lpDataStreamFramesParsedPerSecFrametimes, (void *)(uint64_t)(frame->timeStamp - timeStampLast), 0);
    timeStampLast = frame->timeStamp;
    winst->lpDataStreamFramesParsedPerSecUpdated = BTAgetTickCount();

    BTAundistortApply(winst, frame);

    if (winst->grabInst) {
        BGRBgrab(winst->grabInst, frame);
    }

    if (winst->frameArrivedInst) {
        if (winst->frameArrivedInst->frameArrived) {
            (*(winst->frameArrivedInst->frameArrived))(frame);
        }
        if (winst->frameArrivedInst->frameArrivedEx) {
            (*(winst->frameArrivedInst->frameArrivedEx))(winst->frameArrivedInst->handle, frame);
        }
        if (winst->frameArrivedInst->frameArrivedEx2) {
            winst->frameArrivedInst->frameArrivedReturnOptions->userFreesFrame = 0;
            (*(winst->frameArrivedInst->frameArrivedEx2))(winst->frameArrivedInst->handle, frame, winst->frameArrivedInst->userArg, winst->frameArrivedInst->frameArrivedReturnOptions);
        }
    }

    if (!winst->frameArrivedInst->frameArrivedReturnOptions->userFreesFrame) {
        if (winst->frameQueue) {
            status = BVQenqueue(winst->frameQueue, frame, (BTA_Status(*)(void **))&BTAfreeFrame);
        }
        else {
            BTAfreeFrame(&frame);
            status = BTA_StatusOk;
        }
    }

    return status;
}


BTA_Status BTAgrabCallbackEnqueue(BTA_WrapperInst *winst, BTA_Frame *frame) {
    assert(winst);
    assert(frame);

    winst->lpDataStreamFramesParsedCount++;
    BVQenqueue(winst->lpDataStreamFramesParsedPerSecFrametimes, (void *)(uint64_t)(frame->timeStamp - timeStampLast), 0);
    timeStampLast = frame->timeStamp;
    winst->lpDataStreamFramesParsedPerSecUpdated = BTAgetTickCount();

    if (winst->grabInst) {
        BGRBgrab(winst->grabInst, frame);
    }

    if (winst->frameArrivedInst) {
        if (winst->frameArrivedInst->frameArrived) {
            (*(winst->frameArrivedInst->frameArrived))(frame);
        }
        if (winst->frameArrivedInst->frameArrivedEx) {
            (*(winst->frameArrivedInst->frameArrivedEx))(winst->frameArrivedInst->handle, frame);
        }
        if (winst->frameArrivedInst->frameArrivedEx2) {
            winst->frameArrivedInst->frameArrivedReturnOptions->userFreesFrame = 0;
            (*(winst->frameArrivedInst->frameArrivedEx2))(winst->frameArrivedInst->handle, frame, winst->frameArrivedInst->userArg, winst->frameArrivedInst->frameArrivedReturnOptions);
        }
    }

    BTA_Status status = BTA_StatusOk;
    if (!winst->frameArrivedInst->frameArrivedReturnOptions->userFreesFrame) {
        if (winst->frameQueue) {
            status = BVQenqueue(winst->frameQueue, frame, (BTA_Status(*)(void **))&BTAfreeFrame);
        }
        else {
            BTAfreeFrame(&frame);
        }
    }

    return status;
}


void BTAgetFlashCommand(BTA_FlashTarget flashTarget, BTA_FlashId flashId, BTA_EthCommand *cmd, BTA_EthSubCommand *subCmd) {
    switch (flashTarget) {
    case BTA_FlashTargetBootloader:
        *cmd = BTA_EthCommandFlashBootloader;
        *subCmd = BTA_EthSubCommandNone;
        break;
    case BTA_FlashTargetApplication:
        *cmd = BTA_EthCommandFlashApplication;
        *subCmd = BTA_EthSubCommandNone;
        break;
    case BTA_FlashTargetGeneric:
        *cmd = BTA_EthCommandFlashGeneric;
        switch (flashId) {
        case BTA_FlashIdSpi:
            *subCmd = BTA_EthSubCommandSpiFlash;
            break;
        case BTA_FlashIdParallel:
            *subCmd = BTA_EthSubCommandParallelFlash;
            break;
        default:
            return;
        }
        break;
    case BTA_FlashTargetLensCalibration:
        *cmd = BTA_EthCommandFlashLensCalib;
        *subCmd = BTA_EthSubCommandNone;
        break;
    case BTA_FlashTargetOtp:
        *cmd = BTA_EthCommandFlashGeneric;
        *subCmd = BTA_EthSubCommandOtpFlash;
        break;
    case BTA_FlashTargetFactoryConfig:
        *cmd = BTA_EthCommandFlashFactoryConfig;
        *subCmd = BTA_EthSubCommandNone;
        break;
    case BTA_FlashTargetWigglingCalibration:
        *cmd = BTA_EthCommandFlashWigglingCalib;
        *subCmd = BTA_EthSubCommandNone;
        break;
    case BTA_FlashTargetIntrinsicTof:
        *cmd = BTA_EthCommandFlashIntrinsicTof;
        *subCmd = BTA_EthSubCommandNone;
        break;
    case BTA_FlashTargetIntrinsicColor:
        *cmd = BTA_EthCommandFlashIntrinsicColor;
        *subCmd = BTA_EthSubCommandNone;
        break;
    case BTA_FlashTargetExtrinsic:
        *cmd = BTA_EthCommandFlashIntrinsicStereo;
        *subCmd = BTA_EthSubCommandNone;
        break;
    case BTA_FlashTargetAmpCompensation:
        *cmd = BTA_EthCommandFlashAmpCompensation;
        *subCmd = BTA_EthSubCommandNone;
        break;
    case BTA_FlashTargetGeometricModelParameters:
        *cmd = BTA_EthCommandFlashGeometricModelParameters;
        *subCmd = BTA_EthSubCommandNone;
        break;
    case BTA_FlashTargetOverlayCalibration:
        *cmd = BTA_EthCommandFlashOverlayCalibration;
        *subCmd = BTA_EthSubCommandNone;
        break;
    case BTA_FlashTargetPredefinedConfig:
        *cmd = BTA_EthCommandFlashPredefinedConfig;
        *subCmd = BTA_EthSubCommandNone;
        break;
    default:
        return;
    }
}


BTA_Status BTAhandleFileUpdateStatus(uint32_t fileUpdateStatus, FN_BTA_ProgressReport progressReport, BTA_InfoEventInst *infoEventInst, uint8_t *finished) {
    switch (fileUpdateStatus) {
    case 21: // in progress
        BTAinfoEventHelper(infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: In progress");
        return BTA_StatusOk;
    case 7: // file ok
        if (progressReport) (*progressReport)(BTA_StatusOk, 80);
        BTAinfoEventHelper(infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: File ok");
        return BTA_StatusOk;
    case 8: // erasing flash
        if (progressReport) (*progressReport)(BTA_StatusOk, 85);
        BTAinfoEventHelper(infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: Erasing flash");
        return BTA_StatusOk;
    case 9: // flashing
        if (progressReport) (*progressReport)(BTA_StatusOk, 90);
        BTAinfoEventHelper(infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: Flashing");
        return BTA_StatusOk;
    case 10: // verifying
        if (progressReport) (*progressReport)(BTA_StatusOk, 95);
        BTAinfoEventHelper(infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: Verifying");
        return BTA_StatusOk;

    case 1:  // ok
    case 14: // update success
        if (progressReport) (*progressReport)(BTA_StatusOk, 100);
        BTAinfoEventHelper(infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: Update success");
        *finished = 1;
        return BTA_StatusOk;

    case 5: // packet_crc_error
    case 6: // file_crc_error
        if (progressReport) (*progressReport)(BTA_StatusCrcError, 0);
        BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusCrcError, "BTAflashUpdate: Packet crc error / file crc error %d", fileUpdateStatus);
        *finished = 1;
        return BTA_StatusCrcError;

    case 0:  // idle
        BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: idle %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 2:  // max_filesize_exceeded
        BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: max_filesize_exceeded %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 3:  // out_of_memory
        BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: out_of_memory %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 4:  // buffer_overrun
        BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: buffer_overrun %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 11: // erasing_failed
        BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: erasing_failed %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 12: // flashing_failed
        BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: flashing_failed %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 13: // verifying_failed
        BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: verifying_failed %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 15: // wrong_packet_nr
        BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: wrong_packet_nr %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 16: // header_version_conflict
        BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: header_version_conflict %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 17: // missing_fw_identifier
        BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: missing_fw_identifier %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 18: // wrong_fw_identifier
        BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: wrong_fw_identifier %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 19: // flash_boundary_exceeded
        BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: flash_boundary_exceeded %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 20: // data_inconsistent
        BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: data_inconsistent %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 255: // protocol_violation
        BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: protocol_violation %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    default:
        if (progressReport) (*progressReport)(BTA_StatusUnknown, 0);
        BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusUnknown, "BTAflashUpdate: Failed. The device reported an unknown status %d", fileUpdateStatus);
        *finished = 1;
        return BTA_StatusUnknown;
    }
}


BTA_Status BTAsetVideoMode(BTA_Handle handle, uint8_t *flagVideoMode) {
    uint32_t mode0;
    BTA_Status status = BTAreadRegister(handle, 0x0001, &mode0, 0);
    if (status != BTA_StatusOk) return status;
    uint8_t flagVideoModeToSet = *flagVideoMode;
    *flagVideoMode = mode0 & 0x1;
    mode0 &= ~0x1;
    mode0 |= flagVideoModeToSet;
    status = BTAwriteRegister(handle, 0x0001, &mode0, 0);
    if (status != BTA_StatusOk) return status;
    return BTA_StatusOk;
}


BTA_Status BTAsetSoftwareTrigger(BTA_Handle handle) {
    uint32_t mode0;
    BTA_Status status = BTAreadRegister(handle, 0x0001, &mode0, 0);
    if (status != BTA_StatusOk) return status;
    mode0 |= 0x10;
    status = BTAwriteRegister(handle, 0x0001, &mode0, 0);
    if (status != BTA_StatusOk) return status;
    return BTA_StatusOk;
}


BTA_Status BTAreadMetrilusFromFlash(BTA_WrapperInst *winst, BTA_IntrinsicData *intData, uint8_t quiet) {
    if (!winst || !intData) {
        return BTA_StatusInvalidParameter;
    }
    memset(intData, 0, sizeof(BTA_IntrinsicData));
    BTA_FlashUpdateConfig flashUpdateConfig;
    memset(&flashUpdateConfig, 0, sizeof(BTA_FlashUpdateConfig));
    flashUpdateConfig.target = BTA_FlashTargetIntrinsicColor;
    flashUpdateConfig.dataLen = 1024;
    flashUpdateConfig.data = (uint8_t *)malloc(flashUpdateConfig.dataLen);
    if (!flashUpdateConfig.data) {
        return BTA_StatusOutOfMemory;
    }
    BTA_Status status;
    if (quiet) status = winst->flashRead(winst, &flashUpdateConfig, 0, 1);
    else status = BTAflashRead(winst, &flashUpdateConfig, 0);
    if (status == BTA_StatusIllegalOperation) {
        if (!quiet) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "No intrinsic lens parameters for color sensor found");
        free(flashUpdateConfig.data);
        flashUpdateConfig.data = 0;
        return BTA_StatusNotSupported;
    }
    else if (status != BTA_StatusOk) {
        if (!quiet) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, status, "Failed to read intrinsic data for color sensor");
        free(flashUpdateConfig.data);
        flashUpdateConfig.data = 0;
        return status;
    }
    uint32_t metriStreamItemId = ((uint32_t*)flashUpdateConfig.data)[0];
    if (metriStreamItemId != 0x2EB27A52) {
        if (!quiet) BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "Intrinsic data has wrong preamble %d", status);
        free(flashUpdateConfig.data);
        flashUpdateConfig.data = 0;
        return BTA_StatusCrcError;
    }
    if (flashUpdateConfig.dataLen < 44) {
        if (!quiet) BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "Intrinsic data is too short %d", status);
        free(flashUpdateConfig.data);
        flashUpdateConfig.data = 0;
        return BTA_StatusInvalidParameter;
    }
    intData->yRes = ((uint32_t *)flashUpdateConfig.data)[1];
    intData->xRes = ((uint32_t *)flashUpdateConfig.data)[2];
    intData->fx = ((float *)flashUpdateConfig.data)[3];
    intData->fy = ((float *)flashUpdateConfig.data)[4];
    intData->cx = ((float *)flashUpdateConfig.data)[5];
    intData->cy = ((float *)flashUpdateConfig.data)[6];
    intData->k1 = ((float *)flashUpdateConfig.data)[7];
    intData->k2 = ((float *)flashUpdateConfig.data)[8];
    intData->p1 = ((float *)flashUpdateConfig.data)[10];
    intData->p2 = ((float *)flashUpdateConfig.data)[11];
    intData->k3 = ((float *)flashUpdateConfig.data)[9];
    intData->lensIndex = 0;
    intData->lensId = 0;
    free(flashUpdateConfig.data);
    flashUpdateConfig.data = 0;
    return BTA_StatusOk;
}


BTA_Status BTAreadGeomModelFromFlash(BTA_WrapperInst *winst, BTA_IntrinsicData ***intrinsicData, uint16_t *intrinsicDataLen, BTA_ExtrinsicData ***extrinsicData, uint16_t *extrinsicDataLen, uint8_t quiet) {
    if (intrinsicData) {
        *intrinsicData = 0;
    }
    if (intrinsicDataLen) {
        *intrinsicDataLen = 0;
    }
    if (extrinsicData) {
        *extrinsicData = 0;
    }
    if (extrinsicDataLen) {
        *extrinsicDataLen = 0;
    }
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_FlashUpdateConfig flashUpdateConfig;
    memset(&flashUpdateConfig, 0, sizeof(BTA_FlashUpdateConfig));
    flashUpdateConfig.target = BTA_FlashTargetGeometricModelParameters;
    flashUpdateConfig.dataLen = 1024;
    flashUpdateConfig.data = (uint8_t *)malloc(flashUpdateConfig.dataLen);
    if (!flashUpdateConfig.data) {
        return BTA_StatusOutOfMemory;
    }
    BTA_Status status;
    if (quiet) status = winst->flashRead(winst, &flashUpdateConfig, 0, 1);
    else status = BTAflashRead(winst, &flashUpdateConfig, 0);
    if (status == BTA_StatusIllegalOperation) {
        if (!quiet) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "No intrinsic lens parameters found (geometric model)");
        free(flashUpdateConfig.data);
        flashUpdateConfig.data = 0;
        return status;
    }
    else if (status != BTA_StatusOk) {
        if (!quiet) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, status, "Failed to read intrinsic data (geometric model)");
        free(flashUpdateConfig.data);
        flashUpdateConfig.data = 0;
        return status;
    }

    if (flashUpdateConfig.dataLen < 6) {
        return BTA_StatusInvalidData;
    }

    // TODO !!! make function parseGeomModel() !!!

    uint16_t *ptu16 = (uint16_t *)flashUpdateConfig.data;
    float *ptf32 = (float *)flashUpdateConfig.data;
    int i = 0;
    uint16_t preamble0 = ptu16[i++];
    uint16_t preamble1 = ptu16[i++];
    if (preamble0 != 0x4742 || preamble1 != 0x4c54) {
        return BTA_StatusInvalidData;
    }
    uint16_t version = ptu16[i++];
    if (version == 1) {
        if (flashUpdateConfig.dataLen != 356) {
            return BTA_StatusInvalidData;
        }
        BTA_IntrinsicData *intDataTof = 0;
        uint16_t lensIdTof = ptu16[i++];
        uint16_t lensIdRgb = ptu16[i++];
        uint16_t xResTof = ptu16[i++];
        uint16_t yResTof = ptu16[i++];
        uint16_t xResRgb = ptu16[i++];
        uint16_t yResRgb = ptu16[i++];
        uint16_t descr = ptu16[i++];
        // header end
        if (descr & (1 << 0)) {
            // Bit 0: Intrinsic parameters for 3D valid(fx, fy, cx, cy and distortion coefficients)
            intDataTof = (BTA_IntrinsicData *)malloc(sizeof(BTA_IntrinsicData));
            intDataTof->lensIndex = 0;
            intDataTof->lensId = lensIdTof;
            intDataTof->xRes = xResTof;
            intDataTof->yRes = yResTof;
            i = 16;
            intDataTof->fx = ptf32[i++];
            intDataTof->fy = ptf32[i++];
            intDataTof->cx = ptf32[i++];
            intDataTof->cy = ptf32[i++];
            i = 24;
            intDataTof->k1 = ptf32[i++];
            intDataTof->k2 = ptf32[i++];
            intDataTof->k3 = ptf32[i++];
            intDataTof->k4 = ptf32[i++];
            intDataTof->k5 = ptf32[i++];
            intDataTof->k6 = ptf32[i++];
            intDataTof->p1 = ptf32[i++];
            intDataTof->p2 = ptf32[i++];
        }
        BTA_IntrinsicData *intDataRgb = 0;
        if (descr & (1 << 1)) {
            // Bit 1: Intrinsic parameters for 2D valid(fx, fy, cx, cy and distortion coefficients)
            intDataRgb = (BTA_IntrinsicData *)malloc(sizeof(BTA_IntrinsicData));
            intDataRgb->lensIndex = 0;
            intDataRgb->lensId = lensIdRgb;
            intDataRgb->xRes = xResRgb;
            intDataRgb->yRes = yResRgb;
            i = 20;
            intDataRgb->fx = ptf32[i++];
            intDataRgb->fy = ptf32[i++];
            intDataRgb->cx = ptf32[i++];
            intDataRgb->cy = ptf32[i++];
            i = 32;
            intDataRgb->k1 = ptf32[i++];
            intDataRgb->k2 = ptf32[i++];
            intDataRgb->k3 = ptf32[i++];
            intDataRgb->k4 = ptf32[i++];
            intDataRgb->k5 = ptf32[i++];
            intDataRgb->k6 = ptf32[i++];
            intDataRgb->p1 = ptf32[i++];
            intDataRgb->p2 = ptf32[i++];
        }
        if (descr & (1 << 2)) {
            // Bit 2: 3D intrinsic calibration is Fisheye model
        }
        else {
            // Bit 2: 3D intrinsic calibration is Pinhole model
        }
        if (descr & (1 << 3)) {
            // Bit 3: 2D intrinsic calibration is Fisheye model
        }
        else {
            // Bit 3: 2D intrinsic calibration is Pinhole model
        }
        BTA_ExtrinsicData *extDataTof = 0;
        if (descr & (1 << 4)) {
            // Bit 4: Extrinsic rotation-translation matrix for 3D valid
            extDataTof = (BTA_ExtrinsicData *)malloc(sizeof(BTA_ExtrinsicData));
            i = 40;
            extDataTof->rot[0] = ptf32[i++];
            extDataTof->rot[1] = ptf32[i++];
            extDataTof->rot[2] = ptf32[i++];
            extDataTof->trl[0] = ptf32[i++];
            extDataTof->rot[3] = ptf32[i++];
            extDataTof->rot[4] = ptf32[i++];
            extDataTof->rot[5] = ptf32[i++];
            extDataTof->trl[1] = ptf32[i++];
            extDataTof->rot[6] = ptf32[i++];
            extDataTof->rot[7] = ptf32[i++];
            extDataTof->rot[8] = ptf32[i++];
            extDataTof->trl[2] = ptf32[i++];
            extDataTof->lensIndex = 0;
            extDataTof->lensId = lensIdTof;
        }
        BTA_ExtrinsicData *extDataRgb = 0;
        if (descr & (1 << 5)) {
            // Bit 5: Extrinsic rotation-translation matrix for 2D valid
            extDataRgb = (BTA_ExtrinsicData *)malloc(sizeof(BTA_ExtrinsicData));
            i = 52;
            extDataRgb->rot[0] = ptf32[i++];
            extDataRgb->rot[1] = ptf32[i++];
            extDataRgb->rot[2] = ptf32[i++];
            extDataRgb->trl[0] = ptf32[i++];
            extDataRgb->rot[3] = ptf32[i++];
            extDataRgb->rot[4] = ptf32[i++];
            extDataRgb->rot[5] = ptf32[i++];
            extDataRgb->trl[1] = ptf32[i++];
            extDataRgb->rot[6] = ptf32[i++];
            extDataRgb->rot[7] = ptf32[i++];
            extDataRgb->rot[8] = ptf32[i++];
            extDataRgb->trl[2] = ptf32[i++];
            extDataRgb->lensIndex = 0;
            extDataRgb->lensId = lensIdRgb;
        }
        // Bit 6: Extrinsic inverse rotation-translation matrix for 3D valid
        // Bit 7: Extrinsic inverse rotation-translation matrix for 2D valid
        if (intrinsicData && intrinsicDataLen) {
            *intrinsicDataLen = 2; // (intDataTof ? 1 : 0) + (intDataRgb ? 1 : 0);
            if (*intrinsicDataLen) {
                *intrinsicData = (BTA_IntrinsicData **)malloc(*intrinsicDataLen * sizeof(BTA_IntrinsicData *));
                (*intrinsicData)[0] = intDataTof;
                (*intrinsicData)[1] = intDataRgb;
            }
        }
        if (extrinsicData && extrinsicDataLen) {
            *extrinsicDataLen = 2; // (extDataTof ? 1 : 0) + (extDataRgb ? 1 : 0);
            if (*extrinsicDataLen) {
                *extrinsicData = (BTA_ExtrinsicData **)malloc(*extrinsicDataLen * sizeof(BTA_ExtrinsicData *));
                (*extrinsicData)[0] = extDataTof;
                (*extrinsicData)[1] = extDataRgb;
            }
        }
        return BTA_StatusOk;
    }
    else {
        return BTA_StatusInvalidVersion;
    }
}


BTA_Status BTAfreeIntrinsicData(BTA_IntrinsicData ***intData, uint16_t intDataLen) {
    if (intData) {
        for (int i = 0; i < intDataLen; i++) {
            free((*intData)[i]);
            (*intData)[i] = 0;
        }
        *intData = 0;
        free(*intData);
    }
    return BTA_StatusOk;
}


BTA_Status BTAfreeExtrinsicData(BTA_ExtrinsicData ***extData, uint16_t extDataLen) {
    if (extData) {
        for (int i = 0; i < extDataLen; i++) {
            free((*extData)[i]);
            (*extData)[i] = 0;
        }
        free(*extData);
        *extData = 0;
    }
    return BTA_StatusOk;
}


BTA_Status flashUpdate(BTA_WrapperInst *winst, const uint8_t *filename, FN_BTA_ProgressReport progressReport, BTA_FlashTarget target) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }

    void *file;
    uint64_t filesize;
    BTA_Status status = BTAfopen((char *)filename, "rb", &file);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, status, "BTAfirmwareUpdate: Could not open file %d", status);
        return status;
    }
    status = BTAfseek(file, 0, BTA_SeekOriginEnd, &filesize);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, status, "BTAfirmwareUpdate: fseek failed %d", status);
        BTAfclose(file);
        return status;
    }
    status = BTAfseek(file, 0, BTA_SeekOriginBeginning, 0);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, status, "BTAfirmwareUpdate: fseek failed %d", status);
        BTAfclose(file);
        return status;
    }

    uint8_t *cdata = (unsigned char *)malloc((uint32_t)filesize);
    if (!cdata) {
        BTAfclose(file);
        return BTA_StatusOutOfMemory;
    }

    uint32_t fileSizeRead;
    status = BTAfread(file, cdata, (uint32_t)filesize, &fileSizeRead);
    if (status != BTA_StatusOk || fileSizeRead != filesize) {
        BTAfclose(file);
        free(cdata);
        cdata = 0;
        BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, status, "BTAfirmwareUpdate: Error reading firmware file %d", status);
        return status;
    }
    BTAfclose(file);

    BTA_FlashUpdateConfig config;
    memset(&config, 0, sizeof(BTA_FlashUpdateConfig));
    config.target = target;
    config.data = cdata;
    config.dataLen = (uint32_t)filesize;
    status = winst->flashUpdate(winst, &config, progressReport);
    free(cdata);
    cdata = 0;
    return status;
}


BTA_Status BTAtoByteStream(BTA_EthCommand cmd, BTA_EthSubCommand subCmd, uint32_t addr, void *data, uint32_t length, uint8_t crcEnabled, uint8_t **result, uint32_t *resultLen,
                             uint8_t callbackIpAddrVer, uint8_t *callbackIpAddr, uint8_t callbackIpAddrLen, uint16_t callbackPort,
                             uint32_t packetNumber, uint32_t fileSize, uint32_t fileCrc32) {
    uint32_t i;
    if (!result || !resultLen) {
        return BTA_StatusInvalidParameter;
    }
    *resultLen = 0;
    uint16_t flags = 1;
    if (crcEnabled ||
        cmd == BTA_EthCommandFlashApplication || cmd == BTA_EthCommandFlashBootloader || cmd == BTA_EthCommandFlashGeneric ||
        cmd == BTA_EthCommandFlashLensCalib || cmd == BTA_EthCommandFlashFactoryConfig || cmd == BTA_EthCommandFlashPredefinedConfig || cmd == BTA_EthCommandFlashWigglingCalib ||
        cmd == BTA_EthCommandFlashIntrinsicTof || cmd == BTA_EthCommandFlashIntrinsicColor || cmd == BTA_EthCommandFlashIntrinsicStereo ||
        cmd == BTA_EthCommandFlashAmpCompensation || cmd == BTA_EthCommandFlashGeometricModelParameters || cmd == BTA_EthCommandFlashOverlayCalibration ||
        cmd == BTA_EthCommandReadApplication || cmd == BTA_EthCommandReadBootloader || cmd == BTA_EthCommandReadGeneric ||
        cmd == BTA_EthCommandReadLensCalib || cmd == BTA_EthCommandReadFactoryConfig || cmd == BTA_EthCommandReadWigglingCalib ||
        cmd == BTA_EthCommandReadIntrinsicTof || cmd == BTA_EthCommandReadIntrinsicColor || cmd == BTA_EthCommandReadIntrinsicStereo ||
        cmd == BTA_EthCommandReadAmpCompensation || cmd == BTA_EthCommandReadGeometricModelParameters || cmd == BTA_EthCommandReadOverlayCalibration) {
        flags &= ~1;
    }

    uint8_t headerData0 = 0;
    uint8_t headerData1 = 0;
    uint8_t headerData2 = 0;
    uint8_t headerData3 = 0;
    if (cmd == BTA_EthCommandFlashGeneric || cmd == BTA_EthCommandReadGeneric) {
        headerData0 = (uint8_t)(addr >> 24);
        headerData1 = (uint8_t)(addr >> 16);
        headerData2 = (uint8_t)(addr >> 8);
        headerData3 = (uint8_t)(addr);
    }
    else if (cmd == BTA_EthCommandRead || cmd == BTA_EthCommandWrite || cmd == BTA_EthCommandRetransmissionRequest) {
        headerData0 = (uint8_t)(addr >> 8);
        headerData1 = (uint8_t)(addr);
    }

    // Prepare payload data and length field
    uint32_t payloadLen = 0;
    switch (cmd) {
    case BTA_EthCommandWrite:
    case BTA_EthCommandFlashApplication:
    case BTA_EthCommandFlashBootloader:
    case BTA_EthCommandFlashGeneric:
    case BTA_EthCommandFlashLensCalib:
    case BTA_EthCommandFlashFactoryConfig:
    case BTA_EthCommandFlashPredefinedConfig:
    case BTA_EthCommandFlashWigglingCalib:
    case BTA_EthCommandFlashAmpCompensation:
    case BTA_EthCommandFlashGeometricModelParameters:
    case BTA_EthCommandFlashOverlayCalibration:
    case BTA_EthCommandFlashIntrinsicTof:
    case BTA_EthCommandFlashIntrinsicColor:
    case BTA_EthCommandFlashIntrinsicStereo:
    case BTA_EthCommandRetransmissionRequest:
        payloadLen = length;
        break;
    case BTA_EthCommandRead:
    case BTA_EthCommandReset:
    case BTA_EthCommandReadApplication:
    case BTA_EthCommandReadBootloader:
    case BTA_EthCommandReadGeneric:
    case BTA_EthCommandReadLensCalib:
    case BTA_EthCommandReadFactoryConfig:
    case BTA_EthCommandReadWigglingCalib:
    case BTA_EthCommandReadAmpCompensation:
    case BTA_EthCommandReadGeometricModelParameters:
    case BTA_EthCommandReadOverlayCalibration:
    case BTA_EthCommandReadIntrinsicTof:
    case BTA_EthCommandReadIntrinsicColor:
    case BTA_EthCommandReadIntrinsicStereo:
    case BTA_EthCommandKeepAliveMsg:
        payloadLen = 0;
        break;
    default:
        return BTA_StatusInvalidParameter;
        break;
    }

    *result = (uint8_t *)calloc(1, payloadLen + BTA_ETH_HEADER_SIZE);
    if (!*result) {
        return BTA_StatusOutOfMemory;
    }
    *resultLen = payloadLen + BTA_ETH_HEADER_SIZE;
    uint32_t resultIndex = 0;
    (*result)[resultIndex++] = (uint8_t)BTA_ETH_PREAMBLE_0;
    (*result)[resultIndex++] = (uint8_t)BTA_ETH_PREAMBLE_1;
    (*result)[resultIndex++] = (uint8_t)BTA_ETH_CONTROL_PROTOCOL_VERSION;
    (*result)[resultIndex++] = (uint8_t)cmd;
    (*result)[resultIndex++] = (uint8_t)subCmd;
    (*result)[resultIndex++] = (uint8_t)BTA_StatusOk;
    (*result)[resultIndex++] = (uint8_t)(flags >> 8);
    (*result)[resultIndex++] = (uint8_t)flags;
    (*result)[resultIndex++] = (uint8_t)(length >> 24);
    (*result)[resultIndex++] = (uint8_t)(length >> 16);
    (*result)[resultIndex++] = (uint8_t)(length >> 8);
    (*result)[resultIndex++] = (uint8_t)length;
    (*result)[resultIndex++] = (uint8_t)headerData0;
    (*result)[resultIndex++] = (uint8_t)headerData1;
    (*result)[resultIndex++] = (uint8_t)headerData2;
    (*result)[resultIndex++] = (uint8_t)headerData3;


    // udp related part___________________________________________________________
    // all this should only write anything other than 0 when using udp
    if (callbackIpAddr) {
        (*result)[resultIndex++] = callbackIpAddrVer;
        for (i = 0; i < callbackIpAddrLen; i++) {
            (*result)[resultIndex++] = callbackIpAddr[i];
        }
    }
    else {
        (*result)[resultIndex++] = 4;
        for (i = 0; i < 4; i++) {
            (*result)[resultIndex++] = 0;
        }
    }
    (*result)[resultIndex++] = (uint8_t)(callbackPort >> 8);
    (*result)[resultIndex++] = (uint8_t)callbackPort;

    if ((cmd == BTA_EthCommandFlashApplication || cmd == BTA_EthCommandFlashBootloader || cmd == BTA_EthCommandFlashGeneric ||
        cmd == BTA_EthCommandFlashLensCalib || cmd == BTA_EthCommandFlashFactoryConfig || cmd == BTA_EthCommandFlashPredefinedConfig || cmd == BTA_EthCommandFlashWigglingCalib ||
        cmd == BTA_EthCommandFlashIntrinsicTof || cmd == BTA_EthCommandFlashIntrinsicColor || cmd == BTA_EthCommandFlashIntrinsicStereo ||
        cmd == BTA_EthCommandFlashAmpCompensation || cmd == BTA_EthCommandFlashGeometricModelParameters || cmd == BTA_EthCommandFlashOverlayCalibration)) {
        // file transfer special
        (*result)[resultIndex++] = (uint8_t)(packetNumber >> 24);
        (*result)[resultIndex++] = (uint8_t)(packetNumber >> 16);
        (*result)[resultIndex++] = (uint8_t)(packetNumber >> 8);
        (*result)[resultIndex++] = (uint8_t)packetNumber;
        (*result)[resultIndex++] = (uint8_t)(fileSize >> 24);
        (*result)[resultIndex++] = (uint8_t)(fileSize >> 16);
        (*result)[resultIndex++] = (uint8_t)(fileSize >> 8);
        (*result)[resultIndex++] = (uint8_t)fileSize;
        (*result)[resultIndex++] = (uint8_t)(fileCrc32 >> 24);
        (*result)[resultIndex++] = (uint8_t)(fileCrc32 >> 16);
        (*result)[resultIndex++] = (uint8_t)(fileCrc32 >> 8);
        (*result)[resultIndex++] = (uint8_t)fileCrc32;
    }

    // reserved bytes
    while (resultIndex < BTA_ETH_HEADER_SIZE - 6) {
        (*result)[resultIndex++] = 0;
    }

    if (cmd == BTA_EthCommandWrite) {
        for (i = 0; i < payloadLen / 2; i++) {
            (*result)[BTA_ETH_HEADER_SIZE + 2 * i] = ((uint32_t *)data)[i] >> 8;
            (*result)[BTA_ETH_HEADER_SIZE + 2 * i + 1] = ((uint32_t *)data)[i];
        }
    }
    else {
        memcpy(*result + BTA_ETH_HEADER_SIZE, data, payloadLen);
    }
    uint32_t payloadCrc32 = 0;
    if (!(flags & 1) && (payloadLen > 0)) {
        payloadCrc32 = (uint32_t)CRC32ccitt(*result + BTA_ETH_HEADER_SIZE, payloadLen);
    }
    (*result)[resultIndex++] = payloadCrc32 >> 24;
    (*result)[resultIndex++] = payloadCrc32 >> 16;
    (*result)[resultIndex++] = payloadCrc32 >> 8;
    (*result)[resultIndex++] = payloadCrc32;

    uint32_t headerCrc16 = crc16_ccitt(*result + 2, BTA_ETH_HEADER_SIZE - 4);
    (*result)[resultIndex++] = headerCrc16 >> 8;
    (*result)[resultIndex++] = headerCrc16;

    return BTA_StatusOk;
}


BTA_Status BTAparseControlHeader(uint8_t *request, uint8_t *data, uint32_t *payloadLength, uint32_t *flags, uint32_t *dataCrc32, BTA_InfoEventInst *infoEventInst) {
    if (!request || !data || !payloadLength || !flags || !dataCrc32) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAparseControlHeader: Parameters missing");
        return BTA_StatusInvalidParameter;
    }
    if (data[0] != request[0] || data[1] != request[1]) {     //preamble
        BTAinfoEventHelperIIII(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Eth: parseControlHeader: Preamble is 0x%x%x. Expected 0x%x%x", data[1], data[0], request[1], request[0]);
        return BTA_StatusRuntimeError;
    }
    if (data[2] != request[2]) {        //version
        BTAinfoEventHelperII(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Eth: parseControlHeader: Version is %d. Expected %d", data[2], request[2]);
        return BTA_StatusRuntimeError;
    }
    if (data[3] != request[3]) {       //cmd
        BTAinfoEventHelperII(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Eth: parseControlHeader: Cmd is %d. Expected %d", data[3], request[3]);
        return BTA_StatusRuntimeError;
    }
    if (data[4] != request[4]) {       //sub cmd
        BTAinfoEventHelperII(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Eth: parseControlHeader: SubCmd is %d. Expected %d", data[4], request[4]);
        return BTA_StatusRuntimeError;
    }
    if (data[12] != request[12] || data[13] != request[13] || data[14] != request[14] || data[15] != request[15]) {     //header data
        BTAinfoEventHelperIIIIIIII(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Eth: parseControlHeader: Data is 0x%x%x%x%x. Expected 0x%x%x%x%x", data[15], data[14], data[13], data[12], request[15], request[14], request[13], request[12]);
        return BTA_StatusRuntimeError;
    }
    if (data[5] != 0) {
        switch (data[5]) {
        case 0x0f: //ERR_TCI_ILLEGAL_REG_WRITE
        case 0x10: //ERR_TCI_ILLEGAL_REG_READ
            BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusIllegalOperation, "Device response: %d: Illegal read/write", data[5]);
            return BTA_StatusIllegalOperation;
        case 0x11: //ERR_TCI_REGISTER_END_REACHED
            BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "Device response: %d: Register end reached", data[5]);
            return BTA_StatusInvalidParameter;
        case 0xfa: //CIT_STATUS_LENGTH_EXCEEDS_MAX
            BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Device response: %d: Length exceeds max", data[5]);
            return BTA_StatusRuntimeError;
        case 0xfb: //CIT_STATUS_HEADER_CRC_ERR
            BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Device response: %d: header crc error", data[5]);
            return BTA_StatusRuntimeError;
        case 0xfc: //CIT_STATUS_DATA_CRC_ERR
            BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Device response: %d: Data crc error", data[5]);
            return BTA_StatusRuntimeError;
        case 0xfd: //CIT_STATUS_INVALID_LENGTH_EQ0
            BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Device response: %d: Invalid length equal 0", data[5]);
            return BTA_StatusRuntimeError;
        case 0xfe: //CIT_STATUS_INVALID_LENGTH_GT0
            BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Device response: %d: Invalid length greater than 0", data[5]);
            return BTA_StatusRuntimeError;
        case 0xff: //CIT_STATUS_UNKNOWN_COMMAND
            BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Device response: %d: unknown command", data[5]);
            return BTA_StatusRuntimeError;
        default:
            BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusUnknown, "Device response: %d: Unrecognized error", data[5]);
            return BTA_StatusUnknown;
        }
    }
    *payloadLength = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];
    *flags = ((uint16_t)data[6] << 8) | (uint16_t)data[7];
    if (*payloadLength > 0 && !(*flags & 1) && dataCrc32) {
        *dataCrc32 = ((uint32_t)data[0x3a] << 24) | ((uint32_t)data[0x3b] << 16) | ((uint32_t)data[0x3c] << 8) | (uint32_t)data[0x3d];
    }
    uint16_t headerCrc16 = crc16_ccitt(data + 2, BTA_ETH_HEADER_SIZE - 4);
    if (headerCrc16 != (((uint16_t)data[0x3e] << 8) | (uint16_t)data[0x3f]) /*|| headerCrc16 == 0xffff  ???*/) {
        BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, BTA_StatusCrcError, "Control header: The header CRC check failed %d", data[5]);
        return BTA_StatusCrcError;
    }
    return BTA_StatusOk;
}


static void setMissingAsInvalid(BTA_ChannelId channelId, BTA_DataFormat dataFormat, uint8_t *channelDataStart, int channelDataLength, BTA_FrameToParse *frameToParse) {
    if (!frameToParse) return;
    assert(frameToParse->packetSize[0]); // this implementation relies on first packet presence

    uint8_t *channelDataEnd = channelDataStart + channelDataLength - 1;
    for (int pInd1 = 0; pInd1 < frameToParse->packetCountTotal - 1; pInd1++) {
        if (frameToParse->packetSize[pInd1 + 1] && frameToParse->packetSize[pInd1 + 1] != UINT16_MAX) continue;
        // pInd1 is now index of a present packet before a non-present packet
        uint8_t *blockStart = frameToParse->frame + frameToParse->packetStartAddr[pInd1] + frameToParse->packetSize[pInd1];
        int pInd2;
        for (pInd2 = pInd1 + 1; pInd2 < frameToParse->packetCountTotal; pInd2++) {
            if (frameToParse->packetSize[pInd2] && frameToParse->packetSize[pInd2] != UINT16_MAX) break;
        }
        int blockLength;
        if (pInd2 >= frameToParse->packetCountTotal) {
            // We are missing packets until the end
            blockLength = frameToParse->frameLen - (frameToParse->packetStartAddr[pInd1] + frameToParse->packetSize[pInd1]);
        }
        else {
            // pInd2 is now index of first present packet after pInd1
            blockLength = frameToParse->packetStartAddr[pInd2] - (frameToParse->packetStartAddr[pInd1] + frameToParse->packetSize[pInd1]);
        }
        uint8_t *blockEnd = blockStart + blockLength - 1;
        int length = 0;
        uint8_t *start = 0;
        if (blockStart < channelDataStart && blockEnd > channelDataEnd) {
            // The missing block starts before AND ends after this channel's data
            start = channelDataStart;
            length = channelDataLength;
        }
        else if (blockStart >= channelDataStart && blockStart <= channelDataEnd && blockEnd >= channelDataStart && blockEnd <= channelDataEnd) {
            // The missing block starts AND ends inside this channel's data
            start = blockStart;
            length = blockLength - (int)(start - blockStart);
        }
        else if (blockStart >= channelDataStart && blockStart <= channelDataEnd) {
            // The missing block starts inside this channel's data
            start = blockStart;
            length = BTAmin(blockLength, channelDataLength - (int)(blockStart - channelDataStart));
        }
        else if (blockEnd >= channelDataStart && blockEnd <= channelDataEnd) {
            // The missing block ends inside this channel's data
            start = BTAmax(blockStart, channelDataStart);
            length = blockLength - (int)(start - blockStart);
        }
        if (length) {
            //int bytesPerPixel = dataFormat & 0xf;
            //sprintf(invalid + strlen(invalid), "([%d %d] %d %d) ", pInd1, pInd2, (start - channelDataStart) / bytesPerPixel, ((start - channelDataStart) + length) / bytesPerPixel);

            switch (dataFormat) {

            case BTA_DataFormatUInt8:
            case BTA_DataFormatUInt16:
            //case BTA_DataFormatUInt32:// jump to previous aligned address
                switch (channelId) {
                case BTA_ChannelIdAmplitude:
                case BTA_ChannelIdFlags:
                    memset(start, 0xff, length);
                    break;
                default:
                    memset(start, 0, length);
                    break;
                }
                break;

            case BTA_DataFormatSInt16:
                switch (channelId) {
                    case BTA_ChannelIdDistance:
                    case BTA_ChannelIdX:
                    case BTA_ChannelIdY:
                    case BTA_ChannelIdZ: {
                        int16_t *ptr = (int16_t *)start;
                        for (int i = 0; i < length; i += 2) {
                            *ptr++ = INT16_MIN;
                        }
                        break;
                    }
                    default:
                        memset(start, 0, length);
                        break;
                }
                break;

            //case BTA_DataFormatSInt32:
            //    switch (channelId) {
            //    case BTA_ChannelIdDistance:
            //    case BTA_ChannelIdX:
            //    case BTA_ChannelIdY:
            //    case BTA_ChannelIdZ: {
            // jump to previous aligned address
            //        int32_t *ptr = (int32_t *)start;
            //        for (int i = 0; i < length; i += 4) {
            //            *ptr++ = INT32_MIN;
            //        }
            //        break;
            //    }
            //    break;

            case BTA_DataFormatRgb565:
            case BTA_DataFormatRgb24:
            case BTA_DataFormatJpeg:
                memset(start, 0, length);
                break;

            case BTA_DataFormatYuv422: {  // uyvy
                int firstPixelIndex = (int)(start - channelDataStart) / 4;
                uint8_t *ptr = channelDataStart + firstPixelIndex * 4;
                if (length % 4) {
                    length += 4 - length % 4;
                }
                for (int i = 0; i < length; i += 4) {
                    assert(ptr < channelDataStart + channelDataLength);
                    *ptr++ = 128;
                    assert(ptr < channelDataStart + channelDataLength);
                    *ptr++ = 0;
                    assert(ptr < channelDataStart + channelDataLength);
                    *ptr++ = 128;
                    assert(ptr < channelDataStart + channelDataLength);
                    *ptr++ = 0;
                }
                break;
            }

            case BTA_DataFormatYuv444: {  // yuv
                int firstPixelIndex = (int)(start - channelDataStart) / 3;
                uint8_t *ptr = channelDataStart + firstPixelIndex * 3;
                if (length % 3) {
                    length += 3 - length % 3;
                }
                for (int i = 0; i < length; i += 3) {
                    assert(ptr < channelDataStart + channelDataLength);
                    *ptr++ = 0;
                    assert(ptr < channelDataStart + channelDataLength);
                    *ptr++ = 128;
                    assert(ptr < channelDataStart + channelDataLength);
                    *ptr++ = 128;
                }
                break;
            }

            default:
                memset(start, 0, length);
            }
        }
        pInd1 = pInd2 - 1; // continue loop from a present packet index
    }


    //if (dataFormat == BTA_DataFormatYuv422) {
    //    char msg[1000000] = { 0 };
    //    uint8_t *dataYuv422 = channelDataStart;
    //    //for (int i = 0; i < channelDataLength; i += 4) {
    //    //    int D = *dataYuv422++ - 128;
    //    //    int C1 = *dataYuv422++ - 16;
    //    //    int E = *dataYuv422++ - 128;
    //    //    int C2 = *dataYuv422++ - 16;
    //    //    int r = ((298 * C1 + 409 * E + 128) >> 8);
    //    //    int g = ((298 * C1 - 100 * D - 208 * E + 128) >> 8);
    //    //    int b = ((298 * C1 + 516 * D + 128) >> 8);
    //    //    int c1 = (r << 16) + (g << 8) + b;
    //    //    //if (r == 0 && g == 135 && b == 0) {            }
    //    //    r = ((298 * C2 + 409 * E + 128) >> 8);
    //    //    g = ((298 * C2 - 100 * D - 208 * E + 128) >> 8);
    //    //    b = ((298 * C2 + 516 * D + 128) >> 8);
    //    //    int c2 = (r << 16) + (g << 8) + b;
    //    //    if (c1 == 0x8700) {
    //    //        printf("");
    //    //    }
    //    //}

    //    int d1 = 0, d2 = 1, d3 = 2, d4 = 3, d5 = 4, d6 = 5, d7 = 6;
    //    for (int xy = 0; xy < channelDataLength; xy += 4)
    //    {
    //        uint8_t u = *dataYuv422++;
    //        uint8_t y1 = *dataYuv422++;
    //        uint8_t v = *dataYuv422++;
    //        uint8_t y2 = *dataYuv422++;
    //        d7 = d5;
    //        d6 = d4;
    //        d5 = d3;
    //        d4 = d2;
    //        d3 = d1;
    //        d2 = (y1 << 16) + (u << 8) + v;
    //        d1 = (y2 << 16) + (u << 8) + v;
    //        if (d1 == d2 && d1 == d3 && d1 == d4 && d1 == d5 && d1 == d6 && d1 == d7) {
    //            if (strlen(msg) > 900000) {
    //                break;
    //            }
    //            sprintf(msg + strlen(msg), "%d ", xy);
    //        }
    //    }
    //    if (msg[0] != 0) {
    //        printf("");
    //    }
    //}

    //if (dataFormat == BTA_DataFormatYuv422) {
    //    for (int i = 0; i < channelDataLength - 3; i += 4) {
    //        uint8_t b1 = channelDataStart[i];
    //        uint8_t b2 = channelDataStart[i + 1];
    //        uint8_t b3 = channelDataStart[i + 2];
    //        uint8_t b4 = channelDataStart[i + 3];
    //        if (b1 == 0 && b2 == 0 && b3 == 0 && b3 == 0) {
    //            uint32_t w = b1;
    //        }
    //        if (b1 == 171 && b2 == 183 && b3 == 183 && b4 == 183) {
    //            uint32_t w = b1;
    //        }
    //    }
    //}
}


BTA_Status BTAparseFrame(BTA_WrapperInst *winst, BTA_FrameToParse *frameToParse, BTA_Frame **framePtr) {
    uint64_t timeParseFrame = BTAgetTickCountNano() / 1000;
    assert(winst);
    assert(frameToParse);

    frameToParse->timestamp = 0;

    uint8_t *data = frameToParse->frame;
    uint32_t dataLen = frameToParse->frameLen;
    *framePtr = 0;
    if (dataLen < 64) {
        BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame: data too short: %d", dataLen);
        return BTA_StatusOutOfMemory;
    }
    if (frameToParse->packetCountGot > 0 && (!frameToParse->packetSize[0] || frameToParse->packetSize[0] == UINT16_MAX)) {
        BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidData, "Parsing frame %d: First packet is missing, abort", frameToParse->frameCounter);
        return BTA_StatusInvalidData;
    }
    BTA_Frame *frame = (BTA_Frame *)malloc(sizeof(BTA_Frame));
    if (!frame) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame: Could not allocate a");
        return BTA_StatusOutOfMemory;
    }

    // 2 bytes 'dont care'
    uint32_t i = 2;
    uint16_t protocolVersion = (data[i] << 8) | data[i + 1];
    i += 2;
    switch (protocolVersion) {

    case 3: {
        uint16_t crc16 = (uint16_t)((data[62] << 8) | data[63]);
        if (crc16 != crc16_ccitt(data + 2, 60)) {
            free(frame);
            frame = 0;
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusCrcError, "Parsing frame: CRC check failed");
            return BTA_StatusCrcError;
        }

        uint16_t xRes = (data[i] << 8) | data[i + 1];
        i += 2;
        uint16_t yRes = (data[i] << 8) | data[i + 1];
        i += 2;
        frame->channelsLen = data[i++];
        i++; //bytesPerPixel = data[i++]; unused!
        BTA_EthImgMode imgMode = (BTA_EthImgMode)(((data[i] << 8) | data[i + 1]) >> 3);
        i += 2;
        frame->timeStamp = (data[i] << 24) | (data[i + 1] << 16) | (data[i + 2] << 8) | data[i + 3];
        i += 4;
        frame->frameCounter = (data[i] << 8) | data[i + 1];
        i += 2;

        // count frame counter gaps
        if (frame->frameCounter != (uint32_t)(frameCounterLast + 1) && timeStampLast != 0) {
            winst->lpDataStreamFrameCounterGapsCount++;
        }
        frameCounterLast = frame->frameCounter;

        // only relevant for imgMode == BTA_EthImgModeRawPhases || imgMode == BTA_EthImgModeRawQI
        uint8_t preMetaData = data[i++];
        uint8_t postMetaData = data[i++];

        i = 0x1a;
        frame->mainTemp = ((float)data[i++]);
        if (frame->mainTemp != 0xff) {
            frame->mainTemp -= 50;
        }
        frame->ledTemp = ((float)data[i++]);
        if (frame->ledTemp != 0xff) {
            frame->ledTemp -= 50;
        }
        uint16_t firmwareVersion = (data[i] << 8) | data[i + 1];
        i += 2;
        frame->firmwareVersionMajor = firmwareVersion >> 11;
        frame->firmwareVersionMinor = (firmwareVersion >> 6) & 0x1f;
        frame->firmwareVersionNonFunc = firmwareVersion & 0x3f;

        int headerVersion = 30; /* v3.0 */
                                /* Detect header version */
        if (((data[i] << 8) | data[i + 1]) == 0x3331) {
            headerVersion = 31; /* v3.1 */
        }
        else if (((data[i] << 8) | data[i + 1]) == 0xcc32) {
            headerVersion = 32; /* v3.2 */
        }
        else if (((data[i] << 8) | data[i + 1]) == 0x5533) {
            headerVersion = 33; /* v3.3 */
        }
        i += 2;

        uint32_t integrationTime = 0;
        uint32_t modulationFrequency = 0;
        uint8_t sequenceCounter = 0;
        uint8_t colorChannelMode = 0;
        uint16_t xResColorChannel = 0;
        uint16_t yResColorChannel = 0;
        if (headerVersion >= 31) {
            integrationTime = (data[i] << 8) | data[i + 1];
            i += 2;
            modulationFrequency = ((data[i] << 8) | data[i + 1]) * 10000;
            i += 2;
            frame->genericTemp = ((float)data[i++]);
            if (frame->genericTemp != 0xff)
            {
                frame->genericTemp -= 50;
            }
            colorChannelMode = data[i++];   //header v3.1 supports 1=RGB565 only, v3.2 added 2=JPEG too, v3.3 added 3=YUV422, YUV444
            xResColorChannel = (data[i] << 8) | data[i + 1];
            i += 2;
            yResColorChannel = (data[i] << 8) | data[i + 1];
            i += 2;
            sequenceCounter = data[i++];
        }
        else {
            frame->genericTemp = 0;
            frame->sequenceCounter = 0;
        }
        uint32_t lengthColorChannel = 0;
        if (headerVersion >= 32) {
            int addr = 0x2c;
            lengthColorChannel = (data[addr] << 24) | (data[addr + 1] << 16) | (data[addr + 2] << 8) | data[addr + 3];
        }
        uint32_t lengthColorChannelAdditional = 0;
        if (headerVersion >= 33) {
            int addr = 0x34;
            lengthColorChannelAdditional = (data[addr] << 24) | (data[addr + 1] << 16) | (data[addr + 2] << 8) | data[addr + 3];
        }

        // only relevant for imgMode == BTA_EthImgModeRawPhases || imgMode == BTA_EthImgModeRawQI
        i = 0x30;
        uint32_t rawPhaseContent32 = (data[i] << 24) | (data[i + 1] << 16) | (data[i + 2] << 8) | data[i + 3];

        i = BTA_ETH_FRAME_DATA_HEADER_SIZE;

        frame->channels = (BTA_Channel **)malloc(frame->channelsLen * sizeof(BTA_Channel *));
        if (!frame->channels) {
            BTAfreeFrame(&frame);
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame: Could not allocate b");
            return BTA_StatusOutOfMemory;
        }
        for (uint8_t chInd = 0; chInd < frame->channelsLen; chInd++) {
            uint8_t rawPhaseContent = (rawPhaseContent32 >> (4 * chInd)) & 0xf;
            BTA_Channel *channel = (BTA_Channel *)malloc(sizeof(BTA_Channel));
            channel->metadata = 0;      // pointer nullen hot do Michi gsog
            channel->metadataLen = 0;
            channel->lensIndex = 0;
            channel->flags = 0;
            channel->gain = 0;
            frame->channels[chInd] = channel;
            if (!channel) {
                // free channels created so far
                frame->channelsLen = chInd;
                BTAfreeFrame(&frame);
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame: Could not allocate c");
                return BTA_StatusOutOfMemory;
            }
            channel->id = BTAETHgetChannelId(imgMode, chInd);
            if (channel->id == BTA_ChannelIdColor) {
                channel->xRes = xResColorChannel;
                channel->yRes = yResColorChannel;
            }
            else {
                channel->xRes = xRes;
                channel->yRes = yRes;
            }
            channel->integrationTime = integrationTime;
            channel->modulationFrequency = modulationFrequency;
            channel->sequenceCounter = sequenceCounter;
            channel->dataFormat = BTAETHgetDataFormat(imgMode, chInd, colorChannelMode, rawPhaseContent);
            channel->unit = BTAETHgetUnit(imgMode, chInd);

            // Calculate dataLen and channelDataInputLen
            uint32_t channelDataInputLen;
#           ifndef WO_LIBJPEG
                uint8_t jpgDecodingEnabled = winst->jpgInst && winst->jpgInst->enabled;
#           else
                uint8_t jpgDecodingEnabled = 0;
#           endif
            if (channel->dataFormat == BTA_DataFormatJpeg) {
                if (!jpgDecodingEnabled) {
                    channel->dataLen = lengthColorChannel;
                    channelDataInputLen = lengthColorChannel;
                }
                else {
                    // Going to convert it to RGB24, so reserve that space
                    channel->dataLen = channel->xRes * channel->yRes * 3;
                    channelDataInputLen = lengthColorChannel;
                }
                if (lengthColorChannelAdditional) {
                    // next color channel will get this length
                    lengthColorChannel = lengthColorChannelAdditional;
                }
            }
            else if (imgMode == BTA_EthImgModeRawPhases || imgMode == BTA_EthImgModeRawQI) {
                if (preMetaData == 1) {
                    channel->yRes--;
                    // read first line of metadata
                    uint32_t metadataLen = channel->xRes * sizeof(uint16_t);
                    void *metadata = malloc(metadataLen);
                    memcpy(metadata, data + i, metadataLen);
                    i += metadataLen;
                    BTAinsertMetadataDataIntoChannel(channel, BTA_MetadataIdMlxMeta1, metadata, metadataLen);
                }
                if (postMetaData & 1) {
                    channel->yRes--;
                }
                if (postMetaData & 2) {
                    channel->yRes -= 8;
                }
                if (postMetaData & 4) {
                    channel->yRes--;
                }
                channel->dataLen = channel->xRes * channel->yRes * sizeof(uint16_t);
                channelDataInputLen = channel->dataLen;
            }
            else {
                channel->dataLen = channel->xRes * channel->yRes * (channel->dataFormat & 0xf);
                channelDataInputLen = channel->dataLen;
            }

            channel->data = (uint8_t *)malloc(channel->dataLen);
            if (!channel->data) {
                free(channel);
                channel = 0;
                // free channels created so far
                frame->channelsLen = chInd;
                BTAfreeFrame(&frame);
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame: Could not allocate d");
                return BTA_StatusOutOfMemory;
            }

            if (!winst->lpTestPatternEnabled) {
                // before the memcopy check if there is enough input data
                if (dataLen < i + channelDataInputLen) {
                    free(channel);
                    channel = 0;
                    // free channels created so far
                    frame->channelsLen = chInd;
                    BTAfreeFrame(&frame);
                    BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame: data too short %d", dataLen);
                    return BTA_StatusOutOfMemory;
                }

                //---------------------------------------------------------------------
                // copy data with special cases (also convert from SentisTofM100 coordinate system to BltTofApi coordinat system)
                if (channel->id == BTA_ChannelIdX) {
                    channel->id = BTA_ChannelIdZ;
                    memcpy(channel->data, data + i, channel->dataLen);
                }
                else if (channel->id == BTA_ChannelIdY) {
                    channel->id = BTA_ChannelIdX;
                    int16_t *channelDataTempSrc = (int16_t *)(data + i);
                    int16_t *channelDataTempDst = (int16_t *)channel->data;
                    for (int32_t j = 0; j < channel->xRes * channel->yRes; j++) {
                        *channelDataTempDst++ = -*channelDataTempSrc++;
                    }
                }
                else if (channel->id == BTA_ChannelIdZ) {
                    channel->id = BTA_ChannelIdY;
                    int16_t *channelDataTempSrc = (int16_t *)(data + i);
                    int16_t *channelDataTempDst = (int16_t *)channel->data;
                    for (int32_t j = 0; j < channel->xRes * channel->yRes; j++) {
                        *channelDataTempDst++ = -*channelDataTempSrc++;
                    }
                }
                else if (channel->dataFormat == BTA_DataFormatJpeg) {
                    //uint32_t millisStart = BTAgetTickCount();
                    BTA_Status status = BTA_StatusNotSupported;
#                   ifndef WO_LIBJPEG
                    if (jpgDecodingEnabled) {
                        status = BTAdecodeJpgToRgb24(winst, data + i, channelDataInputLen, channel->data, channel->dataLen);
                    }
#                   endif
                    if (status == BTA_StatusOk) {
                        channel->dataFormat = BTA_DataFormatRgb24;
                    }
                    else {
                        free(channel->data);
                        channel->data = 0;
                        channel->data = (uint8_t *)malloc(channelDataInputLen);
                        if (!channel->data) {
                            free(channel);
                            channel = 0;
                            // free channels created so far
                            frame->channelsLen = chInd;
                            BTAfreeFrame(&frame);
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame: Could not allocate e");
                            return BTA_StatusOutOfMemory;
                        }
                        memcpy(channel->data, data + i, channelDataInputLen);
                    }
                }
                else if (channel->dataFormat == BTA_DataFormatUInt16Mlx12S) {
                    int16_t *channelDataTempSrc = (int16_t *)(data + i);
                    int16_t *channelDataTempDst = (int16_t *)channel->data;
                    for (int32_t j = 0; j < channel->xRes * channel->yRes; j++) {
                        //*channelDataTempDst++ = (*channelDataTempSrc++ << 4) >> 4;
                        *channelDataTempDst++ = (*channelDataTempSrc & 0x0800) ? (*channelDataTempSrc | 0xf000) : *channelDataTempSrc;
                        channelDataTempSrc++;
                    }
                }
                else if (channel->dataFormat == BTA_DataFormatUInt16Mlx12U) {
                    memcpy(channel->data, data + i, channel->dataLen);
                }
                else if (channel->dataFormat == BTA_DataFormatUInt16Mlx1C11S) {
                    int16_t *channelDataTempSrc = (int16_t *)(data + i);
                    int16_t *channelDataTempDst = (int16_t *)channel->data;
                    for (int32_t j = 0; j < channel->xRes * channel->yRes; j++) {
                        //*channelDataTempDst++ = (*channelDataTempSrc++ << 5) >> 5;
                        *channelDataTempDst++ = (*channelDataTempSrc & 0x0400) ? (*channelDataTempSrc | 0xfc00) : *channelDataTempSrc;
                        channelDataTempSrc++;
                    }
                }
                else if (channel->dataFormat == BTA_DataFormatUInt16Mlx1C11U) {
                    uint16_t *channelDataTempSrc = (uint16_t *)(data + i);
                    uint16_t *channelDataTempDst = (uint16_t *)channel->data;
                    for (int32_t j = 0; j < channel->xRes * channel->yRes; j++) {
                        *channelDataTempDst++ = *channelDataTempSrc++ & 0x07ff;
                    }
                }
                else {
                    memcpy(channel->data, data + i, channel->dataLen);
                }

            }

            // advance input data index
            i += channelDataInputLen;

            channel->metadata = 0;
            channel->metadataLen = 0;
            if (imgMode == BTA_EthImgModeRawPhases || imgMode == BTA_EthImgModeRawQI) {
                // read last lines of metadata
                if (postMetaData & 2) {
                    uint32_t metadataLen = 8 * channel->xRes * sizeof(uint16_t);
                    void *metadata = malloc(metadataLen);
                    memcpy(metadata, data + i, metadataLen);
                    i += metadataLen;
                    BTAinsertMetadataDataIntoChannel(channel, BTA_MetadataIdMlxTest, metadata, metadataLen);
                }
                if (postMetaData & 4) {
                    uint32_t metadataLen = channel->xRes * sizeof(uint16_t);
                    void *metadata = malloc(metadataLen);
                    memcpy(metadata, data + i, metadataLen);
                    i += metadataLen;
                    BTAinsertMetadataDataIntoChannel(channel, BTA_MetadataIdMlxAdcData, metadata, metadataLen);
                }
                if (postMetaData & 1) {
                    uint32_t metadataLen = channel->xRes * sizeof(uint16_t);
                    void *metadata = malloc(metadataLen);
                    memcpy(metadata, data + i, metadataLen);
                    i += metadataLen;
                    BTAinsertMetadataDataIntoChannel(channel, BTA_MetadataIdMlxMeta2, metadata, metadataLen);
                }
            }
        }

        if (winst->lpTestPatternEnabled) {
            BTAinsertTestPattern(frame, winst->lpTestPatternEnabled);
        }

        // just reorder X, Y, Z channelpointer, so they are alphabetical
        if (imgMode == BTA_EthImgModeXYZ || imgMode == BTA_EthImgModeXYZAmp || imgMode == BTA_EthImgModeXYZColor ||
            imgMode == BTA_EthImgModeXYZConfColor || imgMode == BTA_EthImgModeXYZAmpColorOverlay) {
            BTA_Channel *channelTemp = frame->channels[0];
            frame->channels[0] = frame->channels[1];
            frame->channels[1] = frame->channels[2];
            frame->channels[2] = channelTemp;
        }
        else if (imgMode == BTA_EthImgModeDistXYZ) {
            BTA_Channel *channelTemp = frame->channels[1];
            frame->channels[1] = frame->channels[2];
            frame->channels[2] = frame->channels[3];
            frame->channels[3] = channelTemp;
        }
        if (i != dataLen) {
            BTAinfoEventHelperII(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Parsing frame: Unexpected payload length, i: %d  dataLen: %d", i, dataLen);
        }
        *framePtr = frame;

        uint64_t durParseFrame = BTAgetTickCountNano() / 1000 - timeParseFrame;
        winst->lpDataStreamParseFrameDuration = (float)BTAmax(durParseFrame, (uint64_t)winst->lpDataStreamParseFrameDuration);

        return BTA_StatusOk;
    }

	case 4: {
        uint8_t *dataHeader = data + 4;
        uint16_t headerLength = *((uint16_t *)dataHeader);
        dataHeader += 2;
        if (dataLen < headerLength) {
            free(frame);  // no channels created so far, so only free this one memory
            frame = 0;
            BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing header v4: data too short: %d", dataLen);
            return BTA_StatusOutOfMemory;
        }
        uint16_t crc16 = *((uint16_t *)(data + headerLength - 2));
        if (crc16 != crc16_ccitt(data + 2, headerLength - 4)) {
            free(frame);  // no channels created so far, so only free this one memory
            frame = 0;
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusCrcError, "Parsing frame v4: CRC check failed");
            return BTA_StatusCrcError;
        }
        uint8_t *dataStream = data + headerLength;

        // Count and check descriptors
        uint8_t infoValid = 0;
        uint8_t channelCount = 0;
        uint8_t *dataHeaderTemp = dataHeader;
        while (1) {
            BTA_Data4DescBase *data4DescBase = (BTA_Data4DescBase *)dataHeaderTemp;
            switch (data4DescBase->descriptorType) {
                case 1:  // FrameInfoV1
                    infoValid = 1;
                    dataHeaderTemp += data4DescBase->descriptorLen;
                    break;
                case 2:  // TofV1
                case 3:  // TofWithMetadataV1
                case 4:  // ColorV1
                    channelCount++;
                    dataHeaderTemp += data4DescBase->descriptorLen;
                    break;
                case 0xfffe:  // EOF
                    break;
                default:
                    free(frame);  // no channels created so far, so only free this one memory
                    frame = 0;
                    BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidVersion, "Parsing frame v4: Descriptor %d not supported", data4DescBase->descriptorType);
                    return BTA_StatusInvalidVersion;
            }
            if (data4DescBase->descriptorType == 0xfffe) {
                dataHeaderTemp += 2;
                break;
            }
        }
        dataHeaderTemp += 2; // crc16
        if (dataHeaderTemp - data != headerLength) {
            BTAinfoEventHelperII(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidData, "Parsing header v4: Unexpected header length: i: %d  headerLen: %d", (int)(dataHeaderTemp - data), headerLength);
        }

        if (!infoValid) {
            memset(frame, 0, sizeof(BTA_Frame));
        }
        if (channelCount) {
            frame->channels = (BTA_Channel **)malloc(channelCount * sizeof(BTA_Channel *));
            if (!frame->channels) {
                free(frame);  // no channels created so far, so only free this one memory
                frame = 0;
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v4: Could not allocate b");
                return BTA_StatusOutOfMemory;
            }
            frame->channelsLen = channelCount;
        }
        else {
            frame->channels = 0;
            frame->channelsLen = 0;
        }

        // Parse descriptors
        int chInd = 0;
#       ifndef WO_LIBJPEG
            uint8_t jpgDecodingEnabled = winst->jpgInst && winst->jpgInst->enabled;
#       else
            uint8_t jpgDecodingEnabled = 0;
#       endif
        while (1) {
            BTA_Data4DescBase *data4DescBase = (BTA_Data4DescBase *)dataHeader;
            dataHeader += data4DescBase->descriptorLen;
            switch (data4DescBase->descriptorType) {
                case 1: {  // FrameInfoV1
                    BTA_Data4DescFrameInfoV1 *data4DescFrameInfoV1 = (BTA_Data4DescFrameInfoV1 *)data4DescBase;
                    frame->frameCounter = data4DescFrameInfoV1->frameCounter;
                    frame->timeStamp = data4DescFrameInfoV1->timestamp;
                    frame->mainTemp = data4DescFrameInfoV1->mainTemp / 100.0f;
                    frame->ledTemp = data4DescFrameInfoV1->ledTemp / 100.0f;
                    frame->genericTemp = data4DescFrameInfoV1->genericTemp / 100.0f;
                    frame->firmwareVersionMajor = data4DescFrameInfoV1->firmwareVersion >> 11;
                    frame->firmwareVersionMinor = (data4DescFrameInfoV1->firmwareVersion >> 6) & 0x1f;
                    frame->firmwareVersionNonFunc = data4DescFrameInfoV1->firmwareVersion & 0x3f;

                    // count frame counter gaps
                    if (frame->frameCounter != (uint32_t)(frameCounterLast + 1) && timeStampLast != 0) {
                        winst->lpDataStreamFrameCounterGapsCount++;
                    }
                    frameCounterLast = frame->frameCounter;
                    break;
                }
                case 2: {  // TofV1
                    BTA_Data4DescTofV1 *data4DescTofV1 = (BTA_Data4DescTofV1 *)data4DescBase;
                    BTA_Channel *channel = (BTA_Channel *)malloc(sizeof(BTA_Channel));
                    if (!channel) {
                        // free channels created so far
                        frame->channelsLen = chInd;
                        BTAfreeFrame(&frame);
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v4 TofV1: Could not allocate channel");
                        return BTA_StatusOutOfMemory;
                    }
                    frame->channels[chInd] = channel;
                    channel->lensIndex = data4DescTofV1->lensIndex;
                    channel->flags = data4DescTofV1->flags;
                    channel->unit = (BTA_Unit)data4DescTofV1->unit;
                    channel->sequenceCounter = data4DescTofV1->sequenceCounter;
                    channel->integrationTime = data4DescTofV1->integrationTime;
                    channel->modulationFrequency = data4DescTofV1->modulationFrequency * 10000;
                    channel->gain = 0;
                    channel->metadata = 0;
                    channel->metadataLen = 0;
                    if (dataStream + data4DescTofV1->dataLen > data + dataLen) {
                        // free channels created so far
                        frame->channelsLen = chInd;
                        BTAfreeFrame(&frame);
                        BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v4: data too short: %d", dataLen);
                        return BTA_StatusOutOfMemory;
                    }
                    if (!winst->lpTestPatternEnabled) {
                        setMissingAsInvalid((BTA_ChannelId)data4DescTofV1->channelId, (BTA_DataFormat)data4DescTofV1->dataFormat, dataStream, data4DescTofV1->dataLen, frameToParse);
                        insertChannelData(winst, (BTA_ChannelId)data4DescTofV1->channelId, (BTA_DataFormat)data4DescTofV1->dataFormat, dataStream,
                                          data4DescTofV1->width, data4DescTofV1->height, data4DescTofV1->dataLen, 0, channel);
                    }
                    else {
                        channel->data = 0;
                        channel->dataLen = 0;
                    }
                    dataStream += data4DescTofV1->dataLen;
                    chInd++;
                    break;
                }
                case 4: {  // ColorV1
                    BTA_Data4DescColorV1 *data4DescColorV1 = (BTA_Data4DescColorV1 *)data4DescBase;
                    BTA_Channel *channel = (BTA_Channel *)malloc(sizeof(BTA_Channel));
                    if (!channel) {
                        // free channels created so far
                        frame->channelsLen = chInd;
                        BTAfreeFrame(&frame);
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v4 ColorV1: Could not allocate channel");
                        return BTA_StatusOutOfMemory;
                    }
                    frame->channels[chInd] = channel;
                    channel->lensIndex = data4DescColorV1->lensIndex;
                    channel->flags = data4DescColorV1->flags;
                    channel->unit = BTA_UnitUnitLess;
                    channel->sequenceCounter = 0;
                    channel->integrationTime = data4DescColorV1->integrationTime;
                    channel->modulationFrequency = 0;
                    channel->gain = data4DescColorV1->gain;
                    channel->metadata = 0;
                    channel->metadataLen = 0;
                    if (dataStream + data4DescColorV1->dataLen > data + dataLen) {
                        // free channels created so far
                        frame->channelsLen = chInd;
                        BTAfreeFrame(&frame);
                        BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v4: data too short: %d", dataLen);
                        return BTA_StatusOutOfMemory;
                    }
                    if (!winst->lpTestPatternEnabled) {
                        setMissingAsInvalid(BTA_ChannelIdColor, (BTA_DataFormat)data4DescColorV1->colorFormat, dataStream, data4DescColorV1->dataLen, frameToParse);
                        insertChannelData(winst, BTA_ChannelIdColor, (BTA_DataFormat)data4DescColorV1->colorFormat, dataStream,
                                          data4DescColorV1->width, data4DescColorV1->height, data4DescColorV1->dataLen, jpgDecodingEnabled, channel);
                    }
                    else {
                        channel->data = 0;
                        channel->dataLen = 0;
                    }
                    dataStream += data4DescColorV1->dataLen;
                    chInd++;
                    break;
                }
                case 0xfffe:
                    break;

                default:
                    // unreachable because the descriptor was checked before
                    assert(0);
                    return BTA_StatusIllegalOperation;
            }
            if (data4DescBase->descriptorType == 0xfffe) {
                frame->sequenceCounter = 0;
                break;
            }
        }

        if (winst->lpTestPatternEnabled) {
            BTAinsertTestPattern(frame, winst->lpTestPatternEnabled);
        }

        if ((int)(dataStream - data) != (int)dataLen) {
            BTAinfoEventHelperII(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "Parsing frame v4: Unexpected payload length, i: %d  dataLen: %d", (int)(dataStream - data), dataLen);
        }
        *framePtr = frame;

        uint64_t durParseFrame = BTAgetTickCountNano() / 1000 - timeParseFrame;
        winst->lpDataStreamParseFrameDuration = (float)BTAmax(durParseFrame, (uint64_t)winst->lpDataStreamParseFrameDuration);

        return BTA_StatusOk;
	}

    default:
        free(frame);
        frame = 0;
        BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidVersion, "Parsing frame: Version not supported: %d", protocolVersion);
        return BTA_StatusInvalidVersion;
    }
}


//---------------------------------------------------------------------
// copy data with special cases (also convert from SentisTofM100 coordinate system to BltTofApi coordinat system)
// channel->data must already be allocated (considering jpeg and possible decompression)
static BTA_Status insertChannelData(BTA_WrapperInst *winst, BTA_ChannelId id, BTA_DataFormat dataFormat, uint8_t *data, uint16_t width, uint16_t height, uint32_t dataLen, uint8_t decodingEnabled, BTA_Channel *channel) {

    channel->id = id;
    channel->dataFormat = dataFormat;
    channel->xRes = width;
    channel->yRes = height;
    channel->dataLen = dataLen;

    if (id == BTA_ChannelIdX) {
        channel->id = BTA_ChannelIdZ;
        channel->data = (uint8_t *)malloc(channel->dataLen);
        if (!channel->data) {
            return BTA_StatusOutOfMemory;
        }
        memcpy(channel->data, data, channel->dataLen);
    }
    else if (id == BTA_ChannelIdY) {
        channel->id = BTA_ChannelIdX;
        channel->data = (uint8_t *)malloc(channel->dataLen);
        if (!channel->data) {
            return BTA_StatusOutOfMemory;
        }
        int16_t *channelDataTempSrc = (int16_t *)data;
        int16_t *channelDataTempDst = (int16_t *)channel->data;
        int px = dataLen / (dataFormat & 0xf);
        for (int32_t j = 0; j < px; j++) {
            *channelDataTempDst++ = -*channelDataTempSrc++;
        }
    }
    else if (id == BTA_ChannelIdZ) {
        channel->id = BTA_ChannelIdY;
        channel->data = (uint8_t *)malloc(channel->dataLen);
        if (!channel->data) {
            return BTA_StatusOutOfMemory;
        }
        int16_t *channelDataTempSrc = (int16_t *)data;
        int16_t *channelDataTempDst = (int16_t *)channel->data;
        int px = dataLen / (dataFormat & 0xf);
        for (int32_t j = 0; j < px; j++) {
            *channelDataTempDst++ = -*channelDataTempSrc++;
        }
    }
    else if (decodingEnabled && dataFormat == BTA_DataFormatJpeg) {
        channel->dataFormat = BTA_DataFormatRgb24;
        channel->dataLen = width * height * 3;
        channel->data = (uint8_t *)malloc(channel->dataLen);
        if (!channel->data) {
            return BTA_StatusOutOfMemory;
        }
        BTA_Status status = BTA_StatusNotSupported;
#       ifndef WO_LIBJPEG
            status = BTAdecodeJpgToRgb24(winst, data, dataLen, channel->data, channel->dataLen);
#       endif
        if (status != BTA_StatusOk) {
            memset(channel->data, 0, channel->dataLen);
        }
    }
    else if (dataFormat == BTA_DataFormatUInt16Mlx12S) {
        channel->data = (uint8_t *)malloc(channel->dataLen);
        if (!channel->data) {
            return BTA_StatusOutOfMemory;
        }
        int16_t *channelDataTempSrc = (int16_t *)data;
        int16_t *channelDataTempDst = (int16_t *)channel->data;
        int px = dataLen / (dataFormat & 0xf);
        for (int32_t j = 0; j < px; j++) {
            *channelDataTempDst++ = (*channelDataTempSrc & 0x0800) ? (*channelDataTempSrc | 0xf800) : *channelDataTempSrc;
            channelDataTempSrc++;
        }
    }
    else if (dataFormat == BTA_DataFormatUInt16Mlx1C11S) {
        channel->data = (uint8_t *)malloc(channel->dataLen);
        if (!channel->data) {
            return BTA_StatusOutOfMemory;
        }
        int16_t *channelDataTempSrc = (int16_t *)data;
        int16_t *channelDataTempDst = (int16_t *)channel->data;
        int px = dataLen / (dataFormat & 0xf);
        for (int32_t j = 0; j < px; j++) {
            *channelDataTempDst++ = (*channelDataTempSrc & 0x0400) ? (*channelDataTempSrc | 0xfc00) : *channelDataTempSrc;
            channelDataTempSrc++;
        }
    }
    else if (dataFormat == BTA_DataFormatUInt16Mlx1C11U) {
        channel->data = (uint8_t *)malloc(channel->dataLen);
        if (!channel->data) {
            return BTA_StatusOutOfMemory;
        }
        uint16_t *channelDataTempSrc = (uint16_t *)data;
        uint16_t *channelDataTempDst = (uint16_t *)channel->data;
        int px = dataLen / (dataFormat & 0xf);
        for (int32_t j = 0; j < px; j++) {
            *channelDataTempDst++ = *channelDataTempSrc++ & 0x07ff;
        }
    }
    else {
        channel->data = (uint8_t *)malloc(channel->dataLen);
        if (!channel->data) {
            return BTA_StatusOutOfMemory;
        }
        memcpy(channel->data, data, channel->dataLen);
    }
    return BTA_StatusOk;
}


static void BTAinsertTestPattern(BTA_Frame *frame, uint16_t testPattern) {
    uint32_t counter = frame->frameCounter % 100;
    int chInd;
    if (testPattern == 3) {
        for (chInd = 0; chInd < frame->channelsLen; chInd++) {
            BTA_Channel *channel = frame->channels[chInd];
            channel->xRes *= 2;
            channel->yRes *= 2;
            channel->dataLen *= 4;
            free(channel->data);
            channel->data = (uint8_t *)malloc(channel->dataLen);
        }
        testPattern = 2;
    }
    else if (testPattern == 4) {
        for (chInd = 0; chInd < frame->channelsLen; chInd++) {
            BTA_Channel *channel = frame->channels[chInd];
            channel->xRes *= 3;
            channel->yRes *= 3;
            channel->dataLen *= 9;
            free(channel->data);
            channel->data = (uint8_t *)malloc(channel->dataLen);
        }
        testPattern = 2;
    }
    else if (testPattern == 5) {
        for (chInd = 0; chInd < frame->channelsLen; chInd++) {
            BTA_Channel *channel = frame->channels[chInd];
            channel->xRes *= 4;
            channel->yRes *= 4;
            channel->dataLen *= 16;
            free(channel->data);
            channel->data = (uint8_t *)malloc(channel->dataLen);
        }
        testPattern = 2;
    }
    else if (testPattern == 6) {
        for (chInd = 0; chInd < frame->channelsLen; chInd++) {
            BTA_Channel *channel = frame->channels[chInd];
            if (channel->dataFormat & 0xf) {
                channel->xRes = 320;
                channel->yRes = 240;
                channel->dataLen = channel->xRes * channel->yRes * (channel->dataFormat & 0xf);
                free(channel->data);
                channel->data = (uint8_t *)malloc(channel->dataLen);
            }
        }
        testPattern = 2;
    }
    else if (testPattern == 7) {
        for (chInd = 0; chInd < frame->channelsLen; chInd++) {
            BTA_Channel *channel = frame->channels[chInd];
            if (channel->dataFormat & 0xf) {
                channel->xRes = 640;
                channel->yRes = 480;
                channel->dataLen = channel->xRes * channel->yRes * (channel->dataFormat & 0xf);
                free(channel->data);
                channel->data = (uint8_t *)malloc(channel->dataLen);
            }
        }
        testPattern = 2;
    }
    if (testPattern != 2) {
        testPattern = 1;
    }

    for (chInd = 0; chInd < frame->channelsLen; chInd++) {
        BTA_Channel *channel = frame->channels[chInd];
        int j;
        switch (channel->dataFormat) {
        case BTA_DataFormatUInt8: {
            uint8_t *channelDataTempDst = (uint8_t *)channel->data;
            if (testPattern == 1) {
                for (j = 0; j < channel->xRes * channel->yRes; j++) {
                    *channelDataTempDst++ = j;
                }
            }
            else if (testPattern == 2) {
                for (j = 0; j < channel->xRes * channel->yRes; j++) {
                    *channelDataTempDst++ = counter;
                }
            }
            break;
        }
        case BTA_DataFormatSInt16:
        case BTA_DataFormatUInt16:
        case BTA_DataFormatRgb565:
        case BTA_DataFormatUInt16Mlx12S:
        case BTA_DataFormatUInt16Mlx12U:
        case BTA_DataFormatUInt16Mlx1C11S:
        case BTA_DataFormatUInt16Mlx1C11U:
        case BTA_DataFormatYuv422: {
            uint16_t *channelDataTempDst = (uint16_t *)channel->data;
            if (testPattern == 1) {
                for (j = 0; j < channel->xRes * channel->yRes; j++) {
                    *channelDataTempDst++ = j;
                }
            }
            else if (testPattern == 2) {
                for (j = 0; j < channel->xRes * channel->yRes; j++) {
                    *channelDataTempDst++ = counter;
                }
            }
            break;
        }
        case BTA_DataFormatSInt32:
        case BTA_DataFormatUInt32: {
            uint32_t *channelDataTempDst = (uint32_t *)channel->data;
            if (testPattern == 1) {
                for (j = 0; j < channel->xRes * channel->yRes; j++) {
                    *channelDataTempDst++ = j;
                }
            }
            else if (testPattern == 2) {
                for (j = 0; j < channel->xRes * channel->yRes; j++) {
                    *channelDataTempDst++ = counter;
                }
            }
            break;
        }
        case BTA_DataFormatFloat32: {
            float *channelDataTempDst = (float *)channel->data;
            if (testPattern == 1) {
                for (j = 0; j < channel->xRes * channel->yRes; j++) {
                    *channelDataTempDst++ = (float)j;
                }
            }
            else if (testPattern == 2) {
                for (j = 0; j < channel->xRes * channel->yRes; j++) {
                    *channelDataTempDst++ = (float)counter;
                }
            }
            break;
        }
        case BTA_DataFormatFloat64: {
            double *channelDataTempDst = (double *)channel->data;
            if (testPattern == 1) {
                for (j = 0; j < channel->xRes * channel->yRes; j++) {
                    *channelDataTempDst++ = (double)j;
                }
            }
            else if (testPattern == 2) {
                for (j = 0; j < channel->xRes * channel->yRes; j++) {
                    *channelDataTempDst++ = (double)counter;
                }
            }
            break;
        }
        case BTA_DataFormatJpeg:
        case BTA_DataFormatRgb24:
        case BTA_DataFormatYuv444: {
            channel->dataFormat = BTA_DataFormatRgb24;
            uint8_t *channelDataTempDst = (uint8_t *)channel->data;
            int x, y;
            //uint8_t rnd = frame->timeStamp / 30000;
            for (y = 0; y < channel->yRes; y++) {
                for (x = 0; x < channel->xRes; x++) {
                    *channelDataTempDst++ = 255 * x / channel->xRes;
                    *channelDataTempDst++ = 255 * y / channel->yRes;
                    *channelDataTempDst++ = 128 * x / channel->xRes + 128 * y / channel->yRes;
                }
            }
            break;
        }
        case BTA_DataFormatUnknown:
            assert(0);
            //BTAinfoEventHelper1(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "parseFrame: DataFormat not supported %d", dataFormat);
            break;
        }
    }
}


static BTA_ChannelId BTAETHgetChannelId(BTA_EthImgMode imgMode, uint8_t channelIndex) {
    switch (imgMode) {
    case BTA_EthImgModeRawdistAmp:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdPhase;
        case 1:
            return BTA_ChannelIdAmplitude;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeDistAmp:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdDistance;
        case 1:
            return BTA_ChannelIdAmplitude;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeDistAmpConf:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdDistance;
        case 1:
            return BTA_ChannelIdAmplitude;
        case 2:
            return BTA_ChannelIdConfidence;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeDistAmpBalance:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdDistance;
        case 1:
            return BTA_ChannelIdAmplitude;
        case 2:
            return BTA_ChannelIdBalance;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeXYZ:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdX;
        case 1:
            return BTA_ChannelIdY;
        case 2:
            return BTA_ChannelIdZ;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeXYZAmp:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdX;
        case 1:
            return BTA_ChannelIdY;
        case 2:
            return BTA_ChannelIdZ;
        case 3:
            return BTA_ChannelIdAmplitude;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeXYZColor:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdX;
        case 1:
            return BTA_ChannelIdY;
        case 2:
            return BTA_ChannelIdZ;
        default:
            return BTA_ChannelIdColor;
        }
    case BTA_EthImgModeDistColor:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdDistance;
        default:
            return BTA_ChannelIdColor;
        }
    case BTA_EthImgModePhase0_90_180_270:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdPhase0;
        case 1:
            return BTA_ChannelIdPhase90;
        case 2:
            return BTA_ChannelIdPhase180;
        case 3:
            return BTA_ChannelIdPhase270;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModePhase270_180_90_0:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdPhase270;
        case 1:
            return BTA_ChannelIdPhase180;
        case 2:
            return BTA_ChannelIdPhase90;
        case 3:
            return BTA_ChannelIdPhase0;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeDistXYZ:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdDistance;
        case 1:
            return BTA_ChannelIdX;
        case 2:
            return BTA_ChannelIdY;
        case 3:
            return BTA_ChannelIdZ;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeXAmp:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdX;
        case 1:
            return BTA_ChannelIdAmplitude;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeTest:
        switch (channelIndex) {
        case 0:
        case 1:
        case 2:
        case 3:
            return BTA_ChannelIdTest;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeDist:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdDistance;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeDistAmpColor:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdDistance;
        case 1:
            return BTA_ChannelIdAmplitude;
        default:
            return BTA_ChannelIdColor;
        }
    case BTA_EthImgModePhase0_180:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdPhase0;
        case 1:
            return BTA_ChannelIdPhase180;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModePhase90_270:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdPhase90;
        case 1:
            return BTA_ChannelIdPhase270;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModePhase0:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdPhase0;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModePhase90:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdPhase90;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModePhase180:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdPhase180;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModePhase270:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdPhase270;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeIntensity:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdGrayScale;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeDistAmpConfColor:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdDistance;
        case 1:
            return BTA_ChannelIdAmplitude;
        case 2:
            return BTA_ChannelIdConfidence;
        default:
            return BTA_ChannelIdColor;
        }
    case BTA_EthImgModeColor:
        return BTA_ChannelIdColor;
    case BTA_EthImgModeRawPhases:
        if (channelIndex >= 0 && channelIndex <= 7) {
            return BTA_ChannelIdRawPhase;
        }
        return BTA_ChannelIdUnknown;
    case BTA_EthImgModeRawQI:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdRawI;
        case 1:
            return BTA_ChannelIdRawQ;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeDistConfExt:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdDistance;
        case 1:
            return BTA_ChannelIdConfidence;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeAmp:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdAmplitude;
        default:
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeXYZConfColor:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdX;
        case 1:
            return BTA_ChannelIdY;
        case 2:
            return BTA_ChannelIdZ;
        case 3:
            return BTA_ChannelIdConfidence;
        default:
            return BTA_ChannelIdColor;
        }
    case BTA_EthImgModeXYZAmpColorOverlay:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdX;
        case 1:
            return BTA_ChannelIdY;
        case 2:
            return BTA_ChannelIdZ;
        case 3:
            return BTA_ChannelIdAmplitude;
        default:
            return BTA_ChannelIdColor;
        }

    default:
        return BTA_ChannelIdUnknown;
    }
}


static BTA_DataFormat BTAETHgetDataFormat(BTA_EthImgMode imgMode, uint8_t channelIndex, uint8_t colorChannelMode, uint8_t rawPhaseContent) {
    switch (imgMode) {
    case BTA_EthImgModeDistAmp:
    case BTA_EthImgModeRawdistAmp:
        switch (channelIndex) {
        case 0:
        case 1:
            return BTA_DataFormatUInt16;
        default:
            return BTA_DataFormatUnknown;
        }
    case BTA_EthImgModeDistAmpConf:
        switch (channelIndex) {
        case 0:
        case 1:
            return BTA_DataFormatUInt16;
        case 2:
            return BTA_DataFormatUInt8;
        default:
            return BTA_DataFormatUnknown;
        }
    case BTA_EthImgModeDistAmpBalance:
        switch (channelIndex) {
        case 0:
        case 1:
            return BTA_DataFormatUInt16;
        case 2:
            return BTA_DataFormatSInt16;
        default:
            return BTA_DataFormatUnknown;
        }
    case BTA_EthImgModeXYZ:
        switch (channelIndex) {
        case 0:
        case 1:
        case 2:
            return BTA_DataFormatSInt16;
        default:
            return BTA_DataFormatUnknown;
        }
    case BTA_EthImgModeXYZAmp:
        switch (channelIndex) {
        case 0:
        case 1:
        case 2:
            return BTA_DataFormatSInt16;
        case 3:
            return BTA_DataFormatUInt16;
        default:
            return BTA_DataFormatUnknown;
        }
    case BTA_EthImgModeXYZColor:
        switch (channelIndex) {
        case 0:
        case 1:
        case 2:
            return BTA_DataFormatSInt16;
        default:
            if (colorChannelMode == 1) {
                return BTA_DataFormatRgb565;
            }
            else if (colorChannelMode == 2) {
                return BTA_DataFormatJpeg;
            }
            else if (colorChannelMode == 3) {
                return BTA_DataFormatYuv422;
            }
            else if (colorChannelMode == 4) {
                return BTA_DataFormatYuv444;
            }
            else {
                return BTA_DataFormatUnknown;
            }
        }
    case BTA_EthImgModeDistColor:
        switch (channelIndex) {
        case 0:
            return BTA_DataFormatUInt16;
        default:
            if (colorChannelMode == 1) {
                return BTA_DataFormatRgb565;
            }
            else if (colorChannelMode == 2) {
                return BTA_DataFormatJpeg;
            }
            else if (colorChannelMode == 3) {
                return BTA_DataFormatYuv422;
            }
            else if (colorChannelMode == 4) {
                return BTA_DataFormatYuv444;
            }
            else {
                return BTA_DataFormatUnknown;
            }
        }
    case BTA_EthImgModePhase0_90_180_270:
    case BTA_EthImgModePhase270_180_90_0:
        switch (channelIndex) {
        case 0:
        case 1:
        case 2:
        case 3:
            return BTA_DataFormatUInt16;
        default:
            return BTA_DataFormatUnknown;
        }
    case BTA_EthImgModePhase0_180:
    case BTA_EthImgModePhase90_270:
        switch (channelIndex) {
        case 0:
        case 1:
            return BTA_DataFormatUInt16;
        default:
            return BTA_DataFormatUnknown;
        }
    case BTA_EthImgModePhase0:
    case BTA_EthImgModePhase90:
    case BTA_EthImgModePhase180:
    case BTA_EthImgModePhase270:
        switch (channelIndex) {
        case 0:
            return BTA_DataFormatUInt16;
        default:
            return BTA_DataFormatUnknown;
        }
    case BTA_EthImgModeDistXYZ:
        switch (channelIndex) {
        case 0:
            return BTA_DataFormatUInt16;
        case 1:
        case 2:
        case 3:
            return BTA_DataFormatSInt16;
        default:
            return BTA_DataFormatUnknown;
        }
    case BTA_EthImgModeXAmp:
        switch (channelIndex) {
        case 0:
            return BTA_DataFormatSInt16;
        case 1:
            return BTA_DataFormatUInt16;
        default:
            return BTA_DataFormatUnknown;
        }
    case BTA_EthImgModeTest:
        switch (channelIndex) {
        case 0:
        case 1:
        case 2:
        case 3:
            return BTA_DataFormatUInt16;
        default:
            return BTA_DataFormatUnknown;
        }
    case BTA_EthImgModeDist:
        switch (channelIndex) {
        case 0:
            return BTA_DataFormatUInt16;
        default:
            return BTA_DataFormatUnknown;
        }
    case BTA_EthImgModeDistAmpColor:
        switch (channelIndex) {
        case 0:
        case 1:
            return BTA_DataFormatUInt16;
        default:
            if (colorChannelMode == 1) {
                return BTA_DataFormatRgb565;
            }
            else if (colorChannelMode == 2) {
                return BTA_DataFormatJpeg;
            }
            else if (colorChannelMode == 3) {
                return BTA_DataFormatYuv422;
            }
            else if (colorChannelMode == 4) {
                return BTA_DataFormatYuv444;
            }
            else {
                return BTA_DataFormatUnknown;
            }
        }
    case BTA_EthImgModeIntensity:
        switch (channelIndex) {
        case 0:
            return BTA_DataFormatUInt16;
        default:
            return BTA_DataFormatUnknown;
        }
    case BTA_EthImgModeDistAmpConfColor:
        switch (channelIndex) {
        case 0:
        case 1:
            return BTA_DataFormatUInt16;
        case 2:
            return BTA_DataFormatUInt8;
        default:
            if (colorChannelMode == 1) {
                return BTA_DataFormatRgb565;
            }
            else if (colorChannelMode == 2) {
                return BTA_DataFormatJpeg;
            }
            else if (colorChannelMode == 3) {
                return BTA_DataFormatYuv422;
            }
            else if (colorChannelMode == 4) {
                return BTA_DataFormatYuv444;
            }
            else {
                return BTA_DataFormatUnknown;
            }
        }
    case BTA_EthImgModeColor:
        if (colorChannelMode == 1) {
            return BTA_DataFormatRgb565;
        }
        else if (colorChannelMode == 2) {
            return BTA_DataFormatJpeg;
        }
        else if (colorChannelMode == 3) {
            return BTA_DataFormatYuv422;
        }
        else if (colorChannelMode == 4) {
            return BTA_DataFormatYuv444;
        }
        else {
            return BTA_DataFormatUnknown;
        }
    case BTA_EthImgModeRawPhases:
    case BTA_EthImgModeRawQI:
        if (rawPhaseContent == 0) {
            return BTA_DataFormatUInt16Mlx1C11S;
        }
        if (rawPhaseContent == 1) {
            return BTA_DataFormatUInt16Mlx12S;
        }
        if (rawPhaseContent == 2) {
            return BTA_DataFormatUInt16Mlx1C11U;
        }
        if (rawPhaseContent == 3) {
            return BTA_DataFormatUInt16Mlx12U;
        }
        if (rawPhaseContent == 4) {
            return BTA_DataFormatSInt16;
        }
        return BTA_DataFormatUnknown;
    case BTA_EthImgModeDistConfExt:
        switch (channelIndex) {
        case 0:
            return BTA_DataFormatUInt16;
        case 1:
            return BTA_DataFormatUInt8;
        default:
            return BTA_DataFormatUnknown;
        }
    case BTA_EthImgModeAmp:
        switch (channelIndex) {
        case 0:
            return BTA_DataFormatUInt16;
        default:
            return BTA_DataFormatUnknown;
        }
    case BTA_EthImgModeXYZConfColor:
    case BTA_EthImgModeXYZAmpColorOverlay:
        switch (channelIndex) {
        case 0:
        case 1:
        case 2:
            return BTA_DataFormatSInt16;
        case 3:
            return BTA_DataFormatUInt16;
        default:
            if (colorChannelMode == 1) {
                return BTA_DataFormatRgb565;
            }
            else if (colorChannelMode == 2) {
                return BTA_DataFormatJpeg;
            }
            else if (colorChannelMode == 3) {
                return BTA_DataFormatYuv422;
            }
            else if (colorChannelMode == 4) {
                return BTA_DataFormatYuv444;
            }
            else {
                return BTA_DataFormatUnknown;
            }
        }

    default:
        return BTA_DataFormatUnknown;
    }
}


static BTA_Unit BTAETHgetUnit(BTA_EthImgMode imgMode, uint8_t channelIndex) {
    switch (imgMode) {
    case BTA_EthImgModeDistAmp:
        switch (channelIndex) {
        case 0:
            return BTA_UnitMillimeter;
        default:
            return BTA_UnitUnitLess;
        }
    case BTA_EthImgModeDistAmpConf:
        switch (channelIndex) {
        case 0:
            return BTA_UnitMillimeter;
        default:
            return BTA_UnitUnitLess;
        }
    case BTA_EthImgModeDistAmpBalance:
        switch (channelIndex) {
        case 0:
            return BTA_UnitMillimeter;
        default:
            return BTA_UnitUnitLess;
        }
    case BTA_EthImgModeXYZ:
        switch (channelIndex) {
        case 0:
        case 1:
        case 2:
            return BTA_UnitMillimeter;
        default:
            return BTA_UnitUnitLess;
        }
    case BTA_EthImgModeXYZAmp:
        switch (channelIndex) {
        case 0:
        case 1:
        case 2:
            return BTA_UnitMillimeter;
        default:
            return BTA_UnitUnitLess;
        }
    case BTA_EthImgModeXYZColor:
        switch (channelIndex) {
        case 0:
        case 1:
        case 2:
            return BTA_UnitMillimeter;
        default:
            return BTA_UnitUnitLess;
        }
    case BTA_EthImgModeDistColor:
        switch (channelIndex) {
        case 0:
            return BTA_UnitMillimeter;
        default:
            return BTA_UnitUnitLess;
        }
    case BTA_EthImgModeDistXYZ:
        switch (channelIndex) {
        case 0:
        case 1:
        case 2:
        case 3:
            return BTA_UnitMillimeter;
        default:
            return BTA_UnitUnitLess;
        }
    case BTA_EthImgModeXAmp:
        switch (channelIndex) {
        case 0:
            return BTA_UnitMillimeter;
        default:
            return BTA_UnitUnitLess;
        }
    case BTA_EthImgModeDist:
        switch (channelIndex) {
        case 0:
            return BTA_UnitMillimeter;
        default:
            return BTA_UnitUnitLess;
        }
    case BTA_EthImgModeDistAmpColor:
        switch (channelIndex) {
        case 0:
            return BTA_UnitMillimeter;
        default:
            return BTA_UnitUnitLess;
        }
    case BTA_EthImgModeDistAmpConfColor:
        switch (channelIndex) {
        case 0:
            return BTA_UnitMillimeter;
        default:
            return BTA_UnitUnitLess;
        }
    case BTA_EthImgModeDistConfExt:
        switch (channelIndex) {
        case 0:
            return BTA_UnitMillimeter;
        case 1:
        default:
            return BTA_UnitUnitLess;
        }
    case BTA_EthImgModeXYZConfColor:
    case BTA_EthImgModeXYZAmpColorOverlay:
        switch (channelIndex) {
        case 0:
        case 1:
        case 2:
            return BTA_UnitMillimeter;
        default:
            return BTA_UnitUnitLess;
        }

    case BTA_EthImgModeRawdistAmp:
    case BTA_EthImgModeAmp:
    default:
        return BTA_UnitUnitLess;
    }
}
