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
#include <bitconverter.h>
#include <utils.h>
#include <mth_math.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#if defined PLAT_LINUX || defined PLAT_APPLE
#   include <errno.h>
#endif

#include <crc16.h>
#include <crc32.h>
#include <undistort.h>
#include <calc_bilateral.h>
#include <calcXYZ.h>
#include <bta_jpg.h>
#include <bvq_queue.h>



// local prototypes

static void logToFile(BTA_InfoEventInst *infoEventInst, BTA_Status status, const char *msg);
static BTA_Status parseGeomModel(uint8_t* data, uint32_t dataLen, BTA_IntrinsicData*** intrinsicData, uint16_t* intrinsicDataLen, BTA_ExtrinsicData*** extrinsicData, uint16_t* extrinsicDataLen);
static BTA_ChannelId BTAETHgetChannelId(BTA_EthImgMode imgMode, uint8_t channelIndex);
static BTA_DataFormat BTAETHgetDataFormat(BTA_EthImgMode imgMode, uint8_t channelIndex, uint8_t colorMode, uint8_t rawPhaseContent);
static BTA_Unit BTAETHgetUnit(BTA_EthImgMode imgMode, uint8_t channelIndex);
static void insertChannelData(BTA_Channel *channel, uint8_t *data, uint32_t dataLen);
static BTA_Status setMissingAsInvalid(BTA_ChannelId channelId, BTA_DataFormat dataFormat, uint8_t *channelDataStart, int channelDataLength, BTA_FrameToParse *frameToParse);

static void insertChannelDataFromShm(BTA_WrapperInst *winst, BTA_Channel *channel, uint8_t *data, uint32_t dataLen);


static uint64_t timeStart = 0;


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
    ftp->frameSize = frameLen;
    if (frameLen > ftp->frameLen) {
        ftp->frameLen = frameLen;
        free(ftp->frame);
        ftp->frame = (uint8_t *)malloc(frameLen);
        if (!ftp->frame) {
            free(ftp->packetSizes);
            ftp->packetSizes = 0;
            ftp->packetSizesLen = 0;
            free(ftp->packetStartAddrs);
            ftp->packetStartAddrs = 0;
            ftp->packetStartAddrsLen = 0;
            free(ftp->frame);
            ftp->frame = 0;
            ftp->frameLen = 0;
            return BTA_StatusOutOfMemory;
        }
    }

    if (packetCountTotal > ftp->packetStartAddrsLen) {
        ftp->packetStartAddrsLen = packetCountTotal;
        free(ftp->packetStartAddrs);
        ftp->packetStartAddrs = (uint32_t *)calloc(packetCountTotal, sizeof(uint32_t));
        if (!ftp->packetStartAddrs) {
            free(ftp->packetSizes);
            ftp->packetSizes = 0;
            ftp->packetSizesLen = 0;
            free(ftp->packetStartAddrs);
            ftp->packetStartAddrs = 0;
            ftp->packetStartAddrsLen = 0;
            free(ftp->frame);
            ftp->frame = 0;
            ftp->frameLen = 0;
            return BTA_StatusOutOfMemory;
        }
    }
    if (packetCountTotal > ftp->packetSizesLen) {
        ftp->packetSizesLen = packetCountTotal;
        free(ftp->packetSizes);
        ftp->packetSizes = (uint16_t *)calloc(packetCountTotal, sizeof(uint16_t));
        if (!ftp->packetSizes) {
            free(ftp->packetSizes);
            ftp->packetSizes = 0;
            ftp->packetSizesLen = 0;
            free(ftp->packetStartAddrs);
            ftp->packetStartAddrs = 0;
            ftp->packetStartAddrsLen = 0;
            free(ftp->frame);
            ftp->frame = 0;
            ftp->frameLen = 0;
            return BTA_StatusOutOfMemory;
        }
    }
    else {
        memset(ftp->packetStartAddrs, 0, ftp->packetStartAddrsLen * sizeof(uint32_t));
        memset(ftp->packetSizes, 0, ftp->packetSizesLen * sizeof(uint16_t));
    }
    ftp->packetCountGot = 0;
    ftp->packetCountNda = 0;
    ftp->packetCountTotal = packetCountTotal;
    ftp->timeLastPacket = ftp->timestamp;
    ftp->retryTime = ftp->timestamp;
    ftp->retryCount = 0;
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
    free(ftp->packetStartAddrs);
    ftp->packetStartAddrs = 0;
    free(ftp->packetSizes);
    ftp->packetSizes = 0;
    free(ftp);
    *frameToParse = 0;
    return BTA_StatusOk;
}


void BHLPzeroLogTimestamp() {
    timeStart = BTAgetTickCount64();
}


void BTAinfoEventHelper(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, ...) {
    if (!infoEventInst || importance > infoEventInst->verbosity) return;
    va_list args;
    va_start(args, msg);
    char infoEventMsg2[820];
    if (status != BTA_StatusConfigParamError) {
        char infoEventMsg1[800];
        vsnprintf(infoEventMsg1, sizeof(infoEventMsg1), msg, args);
        if (!timeStart) timeStart = BTAgetTickCount64();
        sprintf(infoEventMsg2, "%9.2fs %s", (BTAgetTickCount64() - timeStart) / 1000.0, infoEventMsg1);
    }
    else {
        vsnprintf(infoEventMsg2, sizeof(infoEventMsg2), msg, args);
    }
    va_end(args);
    if (infoEventInst->infoEventEx2) (*infoEventInst->infoEventEx2)(infoEventInst->handle, status, (int8_t *)infoEventMsg2, infoEventInst->userArg);
    if (infoEventInst->infoEventEx) (*infoEventInst->infoEventEx)(infoEventInst->handle, status, (int8_t *)infoEventMsg2);
    else if (infoEventInst->infoEvent) (*infoEventInst->infoEvent)(status, (int8_t *)infoEventMsg2);
    logToFile(infoEventInst, status, infoEventMsg2);
}


static void logToFile(BTA_InfoEventInst *infoEventInst, BTA_Status status, const char *msg) {
    if (infoEventInst->infoEventFilename) {
        void *file;
        const char *statusString = BTAstatusToString2(status);
        char *msgTemp = (char *)malloc(strlen(msg) + 512);
        if (msgTemp) {
            sprintf(msgTemp, "%-100s %19s handle0x%p\n", msg, statusString, infoEventInst->handle);
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





void BTApostprocess(BTA_WrapperInst *winst, BTA_Frame *frame) {
#   ifndef BTA_WO_LIBJPEG
    if (winst->lpJpgDecodeEnabled) {
        BTAjpegFrameToRgb24(frame);
    }
#   endif
    if (winst->lpBilateralFilterWindow) {
        BTAcalcBilateralApply(winst, frame, winst->lpBilateralFilterWindow);
    }
    if (winst->lpCalcXyzEnabled) {
        BTAcalcXYZApply(winst->calcXYZInst, winst, frame, winst->lpCalcXyzOffset);
    }
    if (winst->lpColorFromTofEnabled) {
        BTAcalcMonochromeFromAmplitude(frame);
    }
    if (winst->lpUndistortRgbEnabled) {
        BTAundistortApply(winst->undistortInst, winst, frame);
    }
}


void BTAcallbackEnqueue(BTA_WrapperInst *winst, BTA_Frame *frame) {
    uint8_t userFreesFrame = 0;
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
            userFreesFrame = winst->frameArrivedInst->frameArrivedReturnOptions->userFreesFrame;
        }
    }

    if (!userFreesFrame) {
        if (winst->frameQueue) {
            BVQenqueue(winst->frameQueue, frame);
        }
        else {
            BTAfreeFrame(&frame);
        }
    }
}


BTA_Status BTAparsePostprocessGrabCallbackEnqueue(BTA_WrapperInst *winst, BTA_FrameToParse *frameToParse) {
    BTA_Frame *frame;
    BTA_Status status = BTAparseFrame(winst, frameToParse, &frame);
    if (status != BTA_StatusOk) {
        // BTAparseFrame itself calls infoEvent on error
        return status;
    }
    BTApostprocessGrabCallbackEnqueue(winst, frame);
    return BTA_StatusOk;
}


void BTApostprocessGrabCallbackEnqueue(BTA_WrapperInst *winst, BTA_Frame *frame) {
    BTApostprocess(winst, frame);
    BGRBgrab(winst->grabInst, frame);
    BTAcallbackEnqueue(winst, frame);
}


void BTAgetFlashCommand(BTA_FlashUpdateConfig *flashUpdateConfig, BTA_EthCommand *cmd, BTA_EthSubCommand *subCmd) {
    switch (flashUpdateConfig->target) {
    case BTA_FlashTargetBootloader:
        *cmd = BTA_EthCommandFlashBootloader;
        *subCmd = BTA_EthSubCommandNone;
        return;
    case BTA_FlashTargetApplication:
        *cmd = BTA_EthCommandFlashApplication;
        *subCmd = BTA_EthSubCommandNone;
        return;
    case BTA_FlashTargetGeneric:
        *cmd = BTA_EthCommandFlashGeneric;
        switch (flashUpdateConfig->flashId) {
        case BTA_FlashIdSpi:
            *subCmd = BTA_EthSubCommandSpiFlash;
            return;
        case BTA_FlashIdParallel:
            *subCmd = BTA_EthSubCommandParallelFlash;
            return;
        default:
            return;
        }
    case BTA_FlashTargetLensCalibration:
        *cmd = BTA_EthCommandFlashLensCalib;
        *subCmd = BTA_EthSubCommandNone;
        return;
    case BTA_FlashTargetOtp:
        *cmd = BTA_EthCommandFlashGeneric;
        *subCmd = BTA_EthSubCommandOtpFlash;
        return;
    case BTA_FlashTargetFactoryConfig:
        *cmd = BTA_EthCommandFlashFactoryConfig;
        *subCmd = BTA_EthSubCommandNone;
        return;
    case BTA_FlashTargetWigglingCalibration:
        *cmd = BTA_EthCommandFlashWigglingCalib;
        *subCmd = BTA_EthSubCommandNone;
        return;
    case BTA_FlashTargetIntrinsicTof:                   // obsolete
        *cmd = BTA_EthCommandFlashIntrinsicTof;         // obsolete
        *subCmd = BTA_EthSubCommandNone;                // obsolete
        return;                                         // obsolete
    case BTA_FlashTargetIntrinsicColor:                 // obsolete
        *cmd = BTA_EthCommandFlashIntrinsicColor;       // obsolete
        *subCmd = BTA_EthSubCommandNone;                // obsolete
        return;                                         // obsolete
    case BTA_FlashTargetExtrinsic:                      // obsolete
        *cmd = BTA_EthCommandFlashIntrinsicStereo;      // obsolete
        *subCmd = BTA_EthSubCommandNone;                // obsolete
        return;                                         // obsolete
    case BTA_FlashTargetAmpCompensation:                // obsolete
        *cmd = BTA_EthCommandFlashAmpCompensation;      // obsolete
        *subCmd = BTA_EthSubCommandNone;                // obsolete
        return;                                         // obsolete
    case BTA_FlashTargetFpn:
        *cmd = BTA_EthCommandFlashFPN;
        *subCmd = BTA_EthSubCommandNone;
        return;
    case BTA_FlashTargetFppn:
        *cmd = BTA_EthCommandFlashFPPN;
        *subCmd = BTA_EthSubCommandNone;
        return;
    case BTA_FlashTargetGeometricModelParameters:
        *cmd = BTA_EthCommandFlashGeometricModelParameters;
        *subCmd = (BTA_EthSubCommand)flashUpdateConfig->address; // address is misused so we don't need to change 
        return;
    case BTA_FlashTargetOverlayCalibration:
        *cmd = BTA_EthCommandFlashOverlayCalibration;
        *subCmd = BTA_EthSubCommandNone;
        return;
    case BTA_FlashTargetPredefinedConfig:
        *cmd = BTA_EthCommandFlashPredefinedConfig;
        *subCmd = BTA_EthSubCommandNone;
        return;
    case BTA_FlashTargetDeadPixelList:
        *cmd = BTA_EthCommandFlashDeadPixelList;
        *subCmd = BTA_EthSubCommandNone;
        return;
    case BTA_FlashTargetXml:
        *cmd = BTA_EthCommandFlashXml;
        *subCmd = BTA_EthSubCommandNone;
        return;
    case BTA_FlashTargetLogFiles:
        *cmd = (BTA_EthCommand)(BTA_EthCommandReadLogFiles - 100); // use the flash write code always (even if it doesn't exist)
        *subCmd = BTA_EthSubCommandNone;
        return;
    default:
        *cmd = BTA_EthCommandNone;
        *subCmd = BTA_EthSubCommandNone;
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
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusCrcError, "BTAflashUpdate: Packet crc error / file crc error %d", fileUpdateStatus);
        *finished = 1;
        return BTA_StatusCrcError;

    case 0:  // idle
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: idle %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 2:  // max_filesize_exceeded
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: max_filesize_exceeded %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 3:  // out_of_memory
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: out_of_memory %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 4:  // buffer_overrun
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: buffer_overrun %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 11: // erasing_failed
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: erasing_failed %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 12: // flashing_failed
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: flashing_failed %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 13: // verifying_failed
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: verifying_failed %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 15: // wrong_packet_nr
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: wrong_packet_nr %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 16: // header_version_conflict
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: header_version_conflict %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 17: // missing_fw_identifier
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: missing_fw_identifier %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 18: // wrong_fw_identifier
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: wrong_fw_identifier %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 19: // flash_boundary_exceeded
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: flash_boundary_exceeded %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 20: // data_inconsistent
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: data_inconsistent %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    case 255: // protocol_violation
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Failed. Response: protocol_violation %d", fileUpdateStatus);
        if (progressReport) (*progressReport)(BTA_StatusRuntimeError, 0);
        *finished = 1;
        return BTA_StatusRuntimeError;
    default:
        if (progressReport) (*progressReport)(BTA_StatusUnknown, 0);
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusUnknown, "BTAflashUpdate: Failed. The device reported an unknown status %d", fileUpdateStatus);
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
    BTAinitFlashUpdateConfig(&flashUpdateConfig);
    flashUpdateConfig.target = BTA_FlashTargetIntrinsicColor;
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
        if (!quiet) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "Intrinsic data has wrong preamble: %d", metriStreamItemId);
        free(flashUpdateConfig.data);
        flashUpdateConfig.data = 0;
        return BTA_StatusCrcError;
    }
    if (flashUpdateConfig.dataLen < 44) {
        if (!quiet) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "Intrinsic data is too short: %d", flashUpdateConfig.dataLen);
        free(flashUpdateConfig.data);
        flashUpdateConfig.data = 0;
        return BTA_StatusInvalidParameter;
    }
    intData->yRes = (uint16_t)((uint32_t *)flashUpdateConfig.data)[1];
    intData->xRes = (uint16_t)((uint32_t *)flashUpdateConfig.data)[2];
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
    if (!intrinsicData == !!intrinsicDataLen) {  // checks if both or none are given
        return BTA_StatusInvalidParameter;
    }
    if (!extrinsicData == !!extrinsicDataLen) {  // checks if both or none are given
        return BTA_StatusInvalidParameter;
    }
    const int resultLenMax = 24;
    BTA_IntrinsicData *ids[resultLenMax];
    BTA_ExtrinsicData *eds[resultLenMax];
    for (int i = 0; i < resultLenMax; i++) {
        ids[i] = 0;
        eds[i] = 0;
    }
    uint16_t idsLen = 0, edsLen = 0;
    for (int lensIndex = 0; lensIndex < resultLenMax; lensIndex++) {
        BTA_FlashUpdateConfig flashUpdateConfig;
        BTAinitFlashUpdateConfig(&flashUpdateConfig);
        flashUpdateConfig.target = BTA_FlashTargetGeometricModelParameters;
        flashUpdateConfig.address = lensIndex;
        BTA_Status status;
        if (quiet) status = winst->flashRead(winst, &flashUpdateConfig, 0, 1);
        else status = BTAflashRead(winst, &flashUpdateConfig, 0);
        if (status == BTA_StatusIllegalOperation || status == BTA_StatusUnknown) { // TODO: do not accept BTA_StatusUnknown
            // There is no calibration data for this index
            continue;
            //if (lensIndex == 0) {
            //    // This camera may not accept index 0 and need an explicit lens index
            //    continue;
            //}
            //if (lensIndex == 1) {
            //    // Index zero and one both failed
            //    if (!quiet) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "No lens calibration found (geometric model)");
            //    free(flashUpdateConfig.data);
            //    flashUpdateConfig.data = 0;
            //    return status;
            //}
            //if (lensIndex > 1) {
            //    // We apparently read all available data
            //    //break; Lets t
            //}
        }
        else if (status != BTA_StatusOk) {
            if (!quiet) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, status, "Failed to read lens calibration (geometric model)");
            free(flashUpdateConfig.data);
            flashUpdateConfig.data = 0;
            for (int i = 0; i < idsLen; i++) {
                free(ids[i]);
                ids[i] = 0;
            }
            for (int i = 0; i < edsLen; i++) {
                free(eds[i]);
                eds[i] = 0;
            }
            return status;
        }
        BTA_IntrinsicData** intDataTemp = 0;
        BTA_ExtrinsicData** extDataTemp = 0;
        uint16_t intDataLenTemp = 0, extDataLenTemp = 0;
        status = parseGeomModel(flashUpdateConfig.data, flashUpdateConfig.dataLen, &intDataTemp, &intDataLenTemp, &extDataTemp, &extDataLenTemp);
        free(flashUpdateConfig.data);
        flashUpdateConfig.data = 0;
        if (status != BTA_StatusOk) {
            // We got data from the camera but somehow it's not parseable
            if (!quiet) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, status, "Failed to parse lens calibration (geometric model)");
            for (int i = 0; i < idsLen; i++) {
                free(ids[i]);
                ids[i] = 0;
            }
            for (int i = 0; i < edsLen; i++) {
                free(eds[i]);
                eds[i] = 0;
            }
            return status;
        }
        assert(intDataLenTemp > 0 || extDataLenTemp > 0);
        if (intDataLenTemp > 0) {
            for (int i = 0; i < intDataLenTemp && idsLen < resultLenMax; i++) {
                ids[idsLen++] = intDataTemp[i];
            }
        }
        free(intDataTemp);
        if (extDataLenTemp > 0) {
            for (int i = 0; i < extDataLenTemp && edsLen < resultLenMax; i++) {
                eds[edsLen++] = extDataTemp[i];
            }
        }
        free(extDataTemp);
        if (lensIndex == 0) {
            // We got data without index, so don't try indexing at all
            break;
        }
    }
    if (idsLen && intrinsicData && intrinsicDataLen) {
        *intrinsicDataLen = idsLen;
        *intrinsicData = (BTA_IntrinsicData **)malloc(idsLen * sizeof(BTA_IntrinsicData *));
        if (!*intrinsicData) {
            for (int i = 0; i < idsLen; i++) {
                free(ids[i]);
                ids[i] = 0;
            }
            for (int i = 0; i < edsLen; i++) {
                free(eds[i]);
                eds[i] = 0;
            }
            return BTA_StatusOutOfMemory;
        }
        for (int i = 0; i < idsLen; i++) {
            (*intrinsicData)[i] = ids[i];
        }
    }
    if (edsLen && extrinsicData && extrinsicDataLen) {
        *extrinsicDataLen = edsLen;
        *extrinsicData = (BTA_ExtrinsicData **)malloc(*extrinsicDataLen * sizeof(BTA_ExtrinsicData *));
        if (!*extrinsicData) {
            for (int i = 0; i < idsLen; i++) {
                free(ids[i]);
                ids[i] = 0;
            }
            for (int i = 0; i < edsLen; i++) {
                free(eds[i]);
                eds[i] = 0;
            }
            if (intrinsicData) {
                free(*intrinsicData);
                *intrinsicData = 0;
            }
            return BTA_StatusOutOfMemory;
        }
        for (int i = 0; i < edsLen; i++) {
            (*extrinsicData)[i] = eds[i];
        }
    }
    if ((edsLen && extrinsicData && extrinsicDataLen) || (idsLen && intrinsicData && intrinsicDataLen)) {
        return BTA_StatusOk;
    }
    return BTA_StatusIllegalOperation;
}


static BTA_Status parseGeomModel(uint8_t *data, uint32_t dataLen, BTA_IntrinsicData*** intrinsicData, uint16_t* intrinsicDataLen, BTA_ExtrinsicData*** extrinsicData, uint16_t* extrinsicDataLen) {
    if (!intrinsicData || !intrinsicDataLen || !extrinsicData || !extrinsicDataLen) {
        return BTA_StatusInvalidParameter;
    }
    *intrinsicData = 0;
    *extrinsicData = 0;
    *intrinsicDataLen = *extrinsicDataLen = 0;

    if (dataLen < 64) {
        return BTA_StatusInvalidData;
    }

    uint16_t* ptu16 = (uint16_t*)data;
    float* ptf32 = (float*)data;
    int i = 0;
    uint16_t preamble0 = ptu16[i++];
    uint16_t preamble1 = ptu16[i++];
    if (preamble0 != 0x4742 || preamble1 != 0x4c54) {
        return BTA_StatusInvalidData;
    }

    uint16_t crc16 = crc16_ccitt(data, 62);
    if (ptu16[31] != crc16) {
        return BTA_StatusCrcError;
    }

    uint32_t crc32 = (uint32_t)CRC32ccitt(data, dataLen - 4);
    uint16_t crc32Low = ptu16[(dataLen - 4) / 2];
    uint16_t crc32High = ptu16[(dataLen - 2) / 2];
    if ((uint32_t)(crc32Low | (crc32High << 16)) != crc32) {
        return BTA_StatusCrcError;
    }

    uint16_t version = ptu16[i++];
    if (version == 1) {
        if (dataLen != 356) {
            return BTA_StatusInvalidData;
        }
        BTA_IntrinsicData* intDataTof = 0;
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
            intDataTof = (BTA_IntrinsicData*)malloc(sizeof(BTA_IntrinsicData));
            if (!intDataTof) {
                return BTA_StatusOutOfMemory;
            }
            intDataTof->lensIndex = 1;      // ToF should have index 1 by best guess
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
        BTA_IntrinsicData* intDataRgb = 0;
        if (descr & (1 << 1)) {
            // Bit 1: Intrinsic parameters for 2D valid(fx, fy, cx, cy and distortion coefficients)
            intDataRgb = (BTA_IntrinsicData*)malloc(sizeof(BTA_IntrinsicData));
            if (!intDataRgb) {
                free(intDataTof);
                intDataTof = 0;
                return BTA_StatusOutOfMemory;
            }
            intDataRgb->lensIndex = 2;      // RGB should have index 2 by best guess
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
        BTA_ExtrinsicData* extDataTof = 0;
        if (descr & (1 << 4)) {
            // Bit 4: Extrinsic rotation-translation matrix for 3D valid
            extDataTof = (BTA_ExtrinsicData*)malloc(sizeof(BTA_ExtrinsicData));
            if (!extDataTof) {
                free(intDataTof);
                free(intDataRgb);
                intDataTof = intDataRgb = 0;
                return BTA_StatusOutOfMemory;
            }
            i = 40;
            extDataTof->lensIndex = 1;      // ToF should have index 1 by best guess
            extDataTof->lensId = lensIdTof;
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
        }
        BTA_ExtrinsicData* extDataRgb = 0;
        if (descr & (1 << 5)) {
            // Bit 5: Extrinsic rotation-translation matrix for 2D valid
            extDataRgb = (BTA_ExtrinsicData*)malloc(sizeof(BTA_ExtrinsicData));
            if (!extDataRgb) {
                free(intDataTof);
                free(intDataRgb);
                free(extDataTof);
                intDataTof = intDataRgb = 0;
                extDataTof = 0;
                return BTA_StatusOutOfMemory;
            }
            i = 52;
            extDataRgb->lensIndex = 2;      // RGB should have index 2 by best guess
            extDataRgb->lensId = lensIdRgb;
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
        }
        if (descr & (1 << 6)) {
            // Bit 6: Extrinsic inverse rotation-translation matrix for 3D valid
            if (!extDataTof) {
                extDataTof = (BTA_ExtrinsicData*)malloc(sizeof(BTA_ExtrinsicData));
                if (!extDataTof) {
                    free(intDataTof);
                    free(intDataRgb);
                    intDataTof = intDataRgb = 0;
                    return BTA_StatusOutOfMemory;
                }
            }
            i = 64;
            extDataTof->lensIndex = 1;      // ToF should have index 1 by best guess
            extDataTof->lensId = lensIdTof;
            extDataTof->rotTrlInv[0] = ptf32[i++];
            extDataTof->rotTrlInv[1] = ptf32[i++];
            extDataTof->rotTrlInv[2] = ptf32[i++];
            extDataTof->rotTrlInv[3] = ptf32[i++];
            extDataTof->rotTrlInv[4] = ptf32[i++];
            extDataTof->rotTrlInv[5] = ptf32[i++];
            extDataTof->rotTrlInv[6] = ptf32[i++];
            extDataTof->rotTrlInv[7] = ptf32[i++];
            extDataTof->rotTrlInv[8] = ptf32[i++];
            extDataTof->rotTrlInv[9] = ptf32[i++];
            extDataTof->rotTrlInv[10] = ptf32[i++];
            extDataTof->rotTrlInv[11] = ptf32[i++];
        }
        if (descr & (1 << 5)) {
            // Bit 7: Extrinsic inverse rotation-translation matrix for 2D valid
            if (!extDataRgb) {
                extDataRgb = (BTA_ExtrinsicData*)malloc(sizeof(BTA_ExtrinsicData));
                if (!extDataRgb) {
                    free(intDataTof);
                    free(intDataRgb);
                    free(extDataTof);
                    intDataTof = intDataRgb = 0;
                    extDataTof = 0;
                    return BTA_StatusOutOfMemory;
                }
            }
            i = 76;
            extDataRgb->lensIndex = 2;      // RGB should have index 2 by best guess
            extDataRgb->lensId = lensIdRgb;
            extDataRgb->rotTrlInv[0] = ptf32[i++];
            extDataRgb->rotTrlInv[1] = ptf32[i++];
            extDataRgb->rotTrlInv[2] = ptf32[i++];
            extDataRgb->rotTrlInv[3] = ptf32[i++];
            extDataRgb->rotTrlInv[4] = ptf32[i++];
            extDataRgb->rotTrlInv[5] = ptf32[i++];
            extDataRgb->rotTrlInv[6] = ptf32[i++];
            extDataRgb->rotTrlInv[7] = ptf32[i++];
            extDataRgb->rotTrlInv[8] = ptf32[i++];
            extDataRgb->rotTrlInv[9] = ptf32[i++];
            extDataRgb->rotTrlInv[10] = ptf32[i++];
            extDataRgb->rotTrlInv[11] = ptf32[i++];
        }
        *intrinsicDataLen = (intDataTof ? 1 : 0) + (intDataRgb ? 1 : 0);
        if (*intrinsicDataLen) {
            *intrinsicData = (BTA_IntrinsicData**)malloc(*intrinsicDataLen * sizeof(BTA_IntrinsicData *));
            if (!*intrinsicData) {
                free(intDataTof);
                free(intDataRgb);
                free(extDataTof);
                free(extDataRgb);
                intDataTof = intDataRgb = 0;
                extDataTof = extDataRgb = 0;
                return BTA_StatusOutOfMemory;
            }
            (*intrinsicData)[0] = intDataTof;
            (*intrinsicData)[1] = intDataRgb;
        }
        else {
            free(intDataTof);
            free(intDataRgb);
            intDataTof = intDataRgb = 0;
        }
        *extrinsicDataLen = (extDataTof ? 1 : 0) + (extDataRgb ? 1 : 0);
        if (*extrinsicDataLen) {
            *extrinsicData = (BTA_ExtrinsicData**)malloc(*extrinsicDataLen * sizeof(BTA_ExtrinsicData*));
            if (!*extrinsicData) {
                free(intDataTof);
                free(intDataRgb);
                free(extDataTof);
                free(extDataRgb);
                intDataTof = intDataRgb = 0;
                extDataTof = extDataRgb = 0;
                free(*intrinsicData);
                *intrinsicData = 0;
                return BTA_StatusOutOfMemory;
            }
            (*extrinsicData)[0] = extDataTof;
            (*extrinsicData)[1] = extDataRgb;
        }
        else {
            free(extDataTof);
            free(extDataRgb);
            extDataTof = extDataRgb = 0;
        }
        return BTA_StatusOk;
    }
    else if (version == 2) {
        if (dataLen < 236) {
            return BTA_StatusInvalidData;
        }
        uint16_t lensIndex = ptu16[i++];
        uint16_t lensId = ptu16[i++];
        uint16_t xRes = ptu16[i++];
        uint16_t yRes = ptu16[i++];
        uint16_t model = ptu16[i++];
        uint16_t fileContent = ptu16[i++];
        //uint16_t vectorFormat = ptu16[i++];
        BTA_IntrinsicData *intData = 0;
        // header end
        if (fileContent & (1 << 0)) {
            // Bit 0: Intrinsic parameters valid(fx, fy, cx, cy and distortion coefficients)
            intData = (BTA_IntrinsicData *)malloc(sizeof(BTA_IntrinsicData));
            if (!intData) {
                return BTA_StatusOutOfMemory;
            }
            intData->lensIndex = lensIndex;
            intData->lensId = lensId;
            intData->xRes = xRes;
            intData->yRes = yRes;
            i = 16;
            intData->fx = ptf32[i++];
            intData->fy = ptf32[i++];
            intData->cx = ptf32[i++];
            intData->cy = ptf32[i++];
            intData->k1 = ptf32[i++];
            intData->k2 = ptf32[i++];
            intData->k3 = ptf32[i++];
            intData->k4 = ptf32[i++];
            intData->k5 = ptf32[i++];
            intData->k6 = ptf32[i++];
            intData->p1 = ptf32[i++];
            intData->p2 = ptf32[i++];
            if (model == 0) {
                // make sure what we read is pinhole
            }
            else if (model == 1) {
                // make sure what we read is fisheye
            }
        }
        if (fileContent & (1 << 1)) {
            // Bit 2: Intrinsic base vectors valid
            // TODO: parse lenscalib pixel vectors
        }
        BTA_ExtrinsicData *extData = 0;
        if (fileContent & (1 << 2)) {
            // Bit 2: Extrinsic rotation-translation matrix valid
            extData = (BTA_ExtrinsicData *)malloc(sizeof(BTA_ExtrinsicData));
            if (!extData) {
                free(intData);
                intData = 0;
                return BTA_StatusOutOfMemory;
            }
            extData->lensIndex = lensIndex;
            extData->lensId = lensId;
            i = 34;
            extData->rot[0] = ptf32[i++];
            extData->rot[1] = ptf32[i++];
            extData->rot[2] = ptf32[i++];
            extData->trl[0] = ptf32[i++];
            extData->rot[3] = ptf32[i++];
            extData->rot[4] = ptf32[i++];
            extData->rot[5] = ptf32[i++];
            extData->trl[1] = ptf32[i++];
            extData->rot[6] = ptf32[i++];
            extData->rot[7] = ptf32[i++];
            extData->rot[8] = ptf32[i++];
            extData->trl[2] = ptf32[i++];
        }
        if (fileContent & (1 << 3)) {
            // Bit 3: Extrinsic inverse rotation-translation matrix valid
            if (!extData) {
                extData = (BTA_ExtrinsicData *)malloc(sizeof(BTA_ExtrinsicData));
                if (!extData) {
                    free(intData);
                    intData = 0;
                    return BTA_StatusOutOfMemory;
                }
            }
            extData->lensIndex = lensIndex;
            extData->lensId = lensId;
            i = 46;
            extData->rotTrlInv[0] = ptf32[i++];
            extData->rotTrlInv[1] = ptf32[i++];
            extData->rotTrlInv[2] = ptf32[i++];
            extData->rotTrlInv[3] = ptf32[i++];
            extData->rotTrlInv[4] = ptf32[i++];
            extData->rotTrlInv[5] = ptf32[i++];
            extData->rotTrlInv[6] = ptf32[i++];
            extData->rotTrlInv[7] = ptf32[i++];
            extData->rotTrlInv[8] = ptf32[i++];
            extData->rotTrlInv[9] = ptf32[i++];
            extData->rotTrlInv[10] = ptf32[i++];
            extData->rotTrlInv[11] = ptf32[i++];
        }
        if (intrinsicData && intrinsicDataLen && intData) {
            *intrinsicDataLen = 1;
            *intrinsicData = (BTA_IntrinsicData **)malloc(sizeof(BTA_IntrinsicData *));
            if (!*intrinsicData) {
                free(intData);
                free(extData);
                intData = 0;
                extData = 0;
                return BTA_StatusOutOfMemory;
            }
            **intrinsicData = intData;
        }
        if (extrinsicData && extrinsicDataLen && extData) {
            *extrinsicDataLen = 1;
            *extrinsicData = (BTA_ExtrinsicData **)malloc(sizeof(BTA_ExtrinsicData *));
            if (!*extrinsicData) {
                free(intData);
                free(extData);
                intData = 0;
                extData = 0;
                free(*intrinsicData);
                *intrinsicData = 0;
                return BTA_StatusOutOfMemory;
            }
            **extrinsicData = extData;
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
        free(*intData);
        *intData = 0;
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
    BTA_Status status = BTAfopen((char *)filename, "rb", &file);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAfirmwareUpdate: Could not open file: %s", BTAstatusToString2(status));
        return status;
    }
    uint32_t filesize;
    status = BTAfseek(file, 0, BTA_SeekOriginEnd, &filesize);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAfirmwareUpdate: fseek failed: %s", BTAstatusToString2(status));
        BTAfclose(file);
        return status;
    }
    status = BTAfseek(file, 0, BTA_SeekOriginBeginning, 0);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAfirmwareUpdate: fseek failed: %s", BTAstatusToString2(status));
        BTAfclose(file);
        return status;
    }

    uint8_t *cdata = (unsigned char *)malloc(filesize);
    if (!cdata) {
        BTAfclose(file);
        return BTA_StatusOutOfMemory;
    }

    uint32_t fileSizeRead;
    status = BTAfread(file, cdata, filesize, &fileSizeRead);
    if (status != BTA_StatusOk || fileSizeRead != filesize) {
        BTAfclose(file);
        free(cdata);
        cdata = 0;
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAfirmwareUpdate: Error reading firmware file: %s. bytes read: %d", BTAstatusToString2(status), fileSizeRead);
        return status;
    }
    BTAfclose(file);

    BTA_FlashUpdateConfig flashUpdateConfig;
    BTAinitFlashUpdateConfig(&flashUpdateConfig);
    flashUpdateConfig.target = target;
    flashUpdateConfig.data = cdata;
    flashUpdateConfig.dataLen = filesize;
    status = winst->flashUpdate(winst, &flashUpdateConfig, progressReport);
    free(cdata);
    cdata = 0;
    return status;
}


BTA_Status BTAtoByteStream(BTA_EthCommand cmd, BTA_EthSubCommand subCmd, uint32_t addr, void *data, uint32_t length, uint8_t crcEnabled, uint8_t **result, uint32_t *resultLen,
                           uint8_t callbackIpAddrVer, uint8_t *callbackIpAddr, uint8_t callbackIpAddrLen, uint16_t callbackPort,
                           uint32_t packetNumber, uint32_t fileSize, uint32_t fileCrc32) {
    if (!result || !resultLen) {
        return BTA_StatusInvalidParameter;
    }
    *resultLen = 0;
    uint16_t flags = 1;
    if (crcEnabled || ((int)cmd >= (int)BTA_EthCommandFlashBootloader && (int)cmd < (int)BTA_EthCommandRetransmissionRequest)) {
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
    case BTA_EthCommandWrite:                           // 4
    case BTA_EthCommandFlashBootloader:                 // 11
    case BTA_EthCommandFlashApplication:                // 12
    case BTA_EthCommandFlashGeneric:                    // 13
    case BTA_EthCommandFlashLensCalib:                  // 21
    case BTA_EthCommandFlashWigglingCalib:              // 22
    case BTA_EthCommandFlashAmpCompensation:            // obsolete
    case BTA_EthCommandFlashGeometricModelParameters:   // 24
    case BTA_EthCommandFlashOverlayCalibration:         // 25
    case BTA_EthCommandFlashFPPN:                       // 26
    case BTA_EthCommandFlashFPN:                        // 27
    case BTA_EthCommandFlashDeadPixelList:              // 28
    case BTA_EthCommandFlashFactoryConfig:              // 31
    case BTA_EthCommandFlashPredefinedConfig:           // 32
    case BTA_EthCommandFlashXml:                        // 33
    case BTA_EthCommandFlashIntrinsicTof:               // obsolete
    case BTA_EthCommandFlashIntrinsicColor:             // obsolete
    case BTA_EthCommandFlashIntrinsicStereo:            // obsolete
    case BTA_EthCommandRetransmissionRequest:           // 241
        payloadLen = length;
        break;
    case BTA_EthCommandRead:                            // 3
    case BTA_EthCommandReset:                           // 7
    case BTA_EthCommandReadBootloader:                  // 111
    case BTA_EthCommandReadApplication:                 // 112
    case BTA_EthCommandReadGeneric:                     // 113
    case BTA_EthCommandReadLensCalib:                   // 121
    case BTA_EthCommandReadWigglingCalib:               // 122
    case BTA_EthCommandReadAmpCompensation:             // obsolete
    case BTA_EthCommandReadGeometricModelParameters:    // 124
    case BTA_EthCommandReadOverlayCalibration:          // 125
    case BTA_EthCommandReadFPPN:                        // 126
    case BTA_EthCommandReadFPN:                         // 127
    case BTA_EthCommandReadDeadPixelList:               // 128
    case BTA_EthCommandReadFactoryConfig:               // 131
    case BTA_EthCommandReadXml:                         // 133
    case BTA_EthCommandReadLogFiles:                    // 134
    case BTA_EthCommandReadIntrinsicTof:                // obsolete
    case BTA_EthCommandReadIntrinsicColor:              // obsolete
    case BTA_EthCommandReadIntrinsicStereo:             // obsolete
    case BTA_EthCommandKeepAliveMsg:                    // 254
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
        for (uint8_t i = 0; i < callbackIpAddrLen; i++) {
            (*result)[resultIndex++] = callbackIpAddr[i];
        }
    }
    else {
        // BUG!! Above is stated that only zeros should be written!
        (*result)[resultIndex++] = 4;
        for (int i = 0; i < 4; i++) {
            (*result)[resultIndex++] = 0;
        }
    }
    (*result)[resultIndex++] = (uint8_t)(callbackPort >> 8);
    (*result)[resultIndex++] = (uint8_t)callbackPort;

    if (cmd == BTA_EthCommandFlashApplication || cmd == BTA_EthCommandFlashBootloader || cmd == BTA_EthCommandFlashGeneric ||
        cmd == BTA_EthCommandFlashLensCalib || cmd == BTA_EthCommandFlashFactoryConfig || cmd == BTA_EthCommandFlashPredefinedConfig || cmd == BTA_EthCommandFlashWigglingCalib ||
        cmd == BTA_EthCommandFlashIntrinsicTof || cmd == BTA_EthCommandFlashIntrinsicColor || cmd == BTA_EthCommandFlashIntrinsicStereo || cmd == BTA_EthCommandFlashAmpCompensation ||     // obsolete
        cmd == BTA_EthCommandFlashGeometricModelParameters || cmd == BTA_EthCommandFlashOverlayCalibration ||
        cmd == BTA_EthCommandFlashFPN || cmd == BTA_EthCommandFlashFPPN || cmd == BTA_EthCommandFlashDeadPixelList || cmd == BTA_EthCommandFlashXml) {
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
        for (uint32_t i = 0; i < payloadLen / 2; i++) {
            (*result)[BTA_ETH_HEADER_SIZE + 2 * i] = (uint8_t)(((uint32_t *)data)[i] >> 8);
            (*result)[BTA_ETH_HEADER_SIZE + 2 * i + 1] = (uint8_t)(((uint32_t *)data)[i]);
        }
    }
    else {
        memcpy(*result + BTA_ETH_HEADER_SIZE, data, payloadLen);
    }
    uint32_t payloadCrc32 = 0;
    if (!(flags & 1) && (payloadLen > 0)) {
        payloadCrc32 = (uint32_t)CRC32ccitt(*result + BTA_ETH_HEADER_SIZE, payloadLen);
    }
    (*result)[resultIndex++] = (uint8_t)(payloadCrc32 >> 24);
    (*result)[resultIndex++] = (uint8_t)(payloadCrc32 >> 16);
    (*result)[resultIndex++] = (uint8_t)(payloadCrc32 >> 8);
    (*result)[resultIndex++] = (uint8_t)(payloadCrc32);

    uint32_t headerCrc16 = crc16_ccitt(*result + 2, BTA_ETH_HEADER_SIZE - 4);
    (*result)[resultIndex++] = (uint8_t)(headerCrc16 >> 8);
    (*result)[resultIndex++] = (uint8_t)(headerCrc16);

    return BTA_StatusOk;
}


BTA_Status BTAparseControlHeader(uint8_t *request, uint8_t *data, uint32_t *payloadLength, uint32_t *flags, uint32_t *dataCrc32, uint8_t *parseError, BTA_InfoEventInst *infoEventInst) {
    if (!request || !data || !payloadLength || !flags || !parseError || !dataCrc32) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAparseControlHeader: Parameters missing");
        return BTA_StatusInvalidParameter;
    }
    *parseError = 0;
    if (data[0] != request[0] || data[1] != request[1]) {     //preamble
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Eth: parseControlHeader: Preamble is 0x%x%x. Expected 0x%x%x", data[1], data[0], request[1], request[0]);
        *parseError = 1;
        return BTA_StatusRuntimeError;
    }
    if (data[2] != request[2]) {        //version
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Eth: parseControlHeader: Version is %d. Expected %d", data[2], request[2]);
        *parseError = 1;
        return BTA_StatusRuntimeError;
    }
    if (data[3] != request[3]) {       //cmd
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Eth: parseControlHeader: Cmd is %d. Expected %d", data[3], request[3]);
        *parseError = 1;
        return BTA_StatusRuntimeError;
    }
    if (request[3] != BTA_EthCommandDiscovery && data[4] != request[4]) {       //sub cmd (let's ignore a wrong sub-command when discovering because Pulse 
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Eth: parseControlHeader: SubCmd is %d. Expected %d", data[4], request[4]);
        *parseError = 1;
        return BTA_StatusRuntimeError;
    }
    if (data[12] != request[12] || data[13] != request[13] || data[14] != request[14] || data[15] != request[15]) {     //header data
        *parseError = 1;
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Eth: parseControlHeader: Data is 0x%x%x%x%x. Expected 0x%x%x%x%x", data[15], data[14], data[13], data[12], request[15], request[14], request[13], request[12]);
        return BTA_StatusRuntimeError;
    }
    if (data[5] != 0) {
        switch (data[5]) {
        case 0x0f: //ERR_TCI_ILLEGAL_REG_WRITE
        case 0x10: //ERR_TCI_ILLEGAL_REG_READ
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusIllegalOperation, "Device response: %d: Illegal read/write", data[5]);
            return BTA_StatusIllegalOperation;
        case 0x11: //ERR_TCI_REGISTER_END_REACHED
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "Device response: %d: Register end reached", data[5]);
            return BTA_StatusInvalidParameter;
        case 0xfa: //CIT_STATUS_LENGTH_EXCEEDS_MAX
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Device response: %d: Length exceeds max", data[5]);
            return BTA_StatusRuntimeError;
        case 0xfb: //CIT_STATUS_HEADER_CRC_ERR
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Device response: %d: header crc error", data[5]);
            return BTA_StatusRuntimeError;
        case 0xfc: //CIT_STATUS_DATA_CRC_ERR
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Device response: %d: Data crc error", data[5]);
            return BTA_StatusRuntimeError;
        case 0xfd: //CIT_STATUS_INVALID_LENGTH_EQ0
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Device response: %d: Invalid length equal 0", data[5]);
            return BTA_StatusRuntimeError;
        case 0xfe: //CIT_STATUS_INVALID_LENGTH_GT0
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Device response: %d: Invalid length greater than 0", data[5]);
            return BTA_StatusRuntimeError;
        case 0xff: //CIT_STATUS_UNKNOWN_COMMAND
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Device response: %d: unknown command", data[5]);
            return BTA_StatusRuntimeError;
        default:
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusUnknown, "Device response: %d: Unrecognized error", data[5]);
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
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusCrcError, "Control header: The header CRC check failed %d", data[5]);
        return BTA_StatusCrcError;
    }
    return BTA_StatusOk;
}


static BTA_Status setMissingAsInvalid(BTA_ChannelId channelId, BTA_DataFormat dataFormat, uint8_t *channelDataStart, int channelDataLength, BTA_FrameToParse *frameToParse) {
    if (!frameToParse) {
        return BTA_StatusInvalidParameter;
    }
    assert(frameToParse->packetSizes[0]); // this implementation relies on first packet presence

    uint8_t *channelDataEnd = channelDataStart + channelDataLength - 1;
    for (int pInd1 = 0; pInd1 < frameToParse->packetCountTotal - 1; pInd1++) {
        if (frameToParse->packetSizes[pInd1 + 1] && frameToParse->packetSizes[pInd1 + 1] != UINT16_MAX) continue;
        // pInd1 is now index of a present packet before a non-present packet
        uint8_t *blockStart = frameToParse->frame + frameToParse->packetStartAddrs[pInd1] + frameToParse->packetSizes[pInd1];
        int pInd2;
        for (pInd2 = pInd1 + 1; pInd2 < frameToParse->packetCountTotal; pInd2++) {
            if (frameToParse->packetSizes[pInd2] && frameToParse->packetSizes[pInd2] != UINT16_MAX) break;
        }
        int blockLength;
        if (pInd2 >= frameToParse->packetCountTotal) {
            // We are missing packets until the end
            blockLength = frameToParse->frameSize - (frameToParse->packetStartAddrs[pInd1] + frameToParse->packetSizes[pInd1]);
        }
        else {
            // pInd2 is now index of first present packet after pInd1
            blockLength = frameToParse->packetStartAddrs[pInd2] - (frameToParse->packetStartAddrs[pInd1] + frameToParse->packetSizes[pInd1]);
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
            length = MTHmin(blockLength, channelDataLength - (int)(blockStart - channelDataStart));
        }
        else if (blockEnd >= channelDataStart && blockEnd <= channelDataEnd) {
            // The missing block ends inside this channel's data
            start = MTHmax(blockStart, channelDataStart);
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
                    case BTA_ChannelIdZ:
                    case BTA_ChannelIdHeightMap: {
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
                memset(start, 0, length);
                break;
            case BTA_DataFormatJpeg:
                return BTA_StatusIllegalOperation;

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

            case BTA_DataFormatYuv444UYV: {
                int firstPixelIndex = (int)(start - channelDataStart) / 3;
                uint8_t *ptr = channelDataStart + firstPixelIndex * 3;
                if (length % 3) {
                    length += 3 - length % 3;
                }
                for (int i = 0; i < length; i += 3) {
                    assert(ptr < channelDataStart + channelDataLength);
                    *ptr++ = 128;
                    assert(ptr < channelDataStart + channelDataLength);
                    *ptr++ = 0;
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
    return BTA_StatusOk;

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

    winst->lpDataStreamPacketsReceivedCount += frameToParse->packetCountGot;
    winst->lpDataStreamPacketsMissedCount += frameToParse->packetCountTotal - frameToParse->packetCountGot;

    frameToParse->timestamp = 0;

    uint8_t *data = frameToParse->frame;
    uint32_t dataLen = frameToParse->frameSize;
    *framePtr = 0;
    if (dataLen < 64) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame: data too short: %d", dataLen);
        return BTA_StatusOutOfMemory;
    }
    if (frameToParse->packetCountGot > 0 && (!frameToParse->packetSizes[0] || frameToParse->packetSizes[0] == UINT16_MAX)) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusInvalidData, "Parsing frame %d: First packet is missing, abort", frameToParse->frameCounter);
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
        if (frameToParse->packetCountGot < frameToParse->packetCountTotal) {
            // hack for TimIrs firmware that supports udp protocol v2 but old v3 frame protocol
            // don't parse incomplete frames!
            // TODO:
            //if (winst->lpAllowIncompleteFrames > 0) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusInvalidData, "Parsing frame v3 %d: %d packet(s) missing, abort", frameToParse->frameCounter, frameToParse->packetCountTotal - frameToParse->packetCountGot);
            free(frame);
            frame = 0;
            return BTA_StatusInvalidData;
        }

        uint16_t crc16 = (uint16_t)((data[62] << 8) | data[63]);
        if (crc16 != crc16_ccitt(data + 2, 60)) {
            free(frame);
            frame = 0;
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusCrcError, "Parsing frame v3: CRC check failed");
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
        frame->sequenceCounter = 0;

        // count frame counter gaps
        if (frame->frameCounter > winst->frameCounterLast + (uint32_t)winst->lpDataStreamFrameCounterGap && winst->timeStampLast != 0) {
            winst->lpDataStreamFrameCounterGapsCount++;
        }
        winst->frameCounterLast = frame->frameCounter;

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
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v3: Could not allocate b");
            return BTA_StatusOutOfMemory;
        }
        for (uint8_t chInd = 0; chInd < frame->channelsLen; chInd++) {
            uint8_t rawPhaseContent = (rawPhaseContent32 >> (4 * chInd)) & 0xf;
            BTA_Channel *channel = (BTA_Channel *)malloc(sizeof(BTA_Channel));
            if (!channel) {
                // free channels created so far
                frame->channelsLen = chInd;
                BTAfreeFrame(&frame);
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v3: Could not allocate c");
                return BTA_StatusOutOfMemory;
            }
            frame->channels[chInd] = channel;
            channel->metadata = 0;
            channel->metadataLen = 0;
            channel->gain = 0;
            channel->id = BTAETHgetChannelId(imgMode, chInd);
            if (channel->id == BTA_ChannelIdColor) {
                channel->flags = 0;
                channel->lensIndex = 2;
                channel->xRes = xResColorChannel;
                channel->yRes = yResColorChannel;
            }
            else {
                channel->flags = 2;  // While parsing we make sure that the resulting coordinate system is BTA conform. Non-cartesian channels also shall use flag bit1
                channel->lensIndex = 1;
                channel->xRes = xRes;
                channel->yRes = yRes;
            }
            channel->integrationTime = integrationTime;
            channel->modulationFrequency = modulationFrequency;
            channel->sequenceCounter = sequenceCounter;
            channel->dataFormat = BTAETHgetDataFormat(imgMode, chInd, colorChannelMode, rawPhaseContent);
            channel->unit = BTAETHgetUnit(imgMode, chInd);

            // Calculate dataLen
            if (channel->id == BTA_ChannelIdColor) {
                channel->dataLen = lengthColorChannel;
                if (lengthColorChannelAdditional) {
                    // next color channel will get this length
                    lengthColorChannel = lengthColorChannelAdditional;
                }
            }
            else if (channel->id == BTA_ChannelIdRawPhase || channel->id == BTA_ChannelIdRawI || channel->id == BTA_ChannelIdRawQ) { //(imgMode == BTA_EthImgModeRawPhases || imgMode == BTA_EthImgModeRawQI)
                if (preMetaData == 1) {
                    channel->yRes--;
                    // read first line of metadata
                    uint32_t metadataLen = channel->xRes * sizeof(uint16_t);
                    void *metadata = malloc(metadataLen);
                    if (!metadata) {
                        // free channels created so far
                        frame->channelsLen = chInd + 1;
                        BTAfreeFrame(&frame);
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v3: Could not allocate d");
                        return BTA_StatusOutOfMemory;
                    }
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
            }
            else {
                channel->dataLen = channel->xRes * channel->yRes * (channel->dataFormat & 0xf);
            }

            channel->data = (uint8_t *)malloc(channel->dataLen);
            if (!channel->data) {
                free(channel);
                channel = 0;
                // free channels created so far
                frame->channelsLen = chInd;
                BTAfreeFrame(&frame);
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v3: Could not allocate d");
                return BTA_StatusOutOfMemory;
            }

            // before the memcopy check if there is enough input data
            if (dataLen < i + channel->dataLen) {
                free(channel);
                channel = 0;
                // free channels created so far
                frame->channelsLen = chInd;
                BTAfreeFrame(&frame);
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v3: data too short %d", dataLen);
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
            else if (channel->dataFormat == BTA_DataFormatSInt16Mlx12S) {
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
            else if (channel->dataFormat == BTA_DataFormatSInt16Mlx1C11S) {
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

            // advance input data index
            i += channel->dataLen;

            channel->metadata = 0;
            channel->metadataLen = 0;
            if (channel->id == BTA_ChannelIdRawPhase || channel->id == BTA_ChannelIdRawI || channel->id == BTA_ChannelIdRawQ) { //(imgMode == BTA_EthImgModeRawPhases || imgMode == BTA_EthImgModeRawQI)
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
        frame->metadataLen = 0;
        frame->metadata = 0;

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
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Parsing frame v3: Unexpected payload length, i: %d  dataLen: %d", i, dataLen);
        }
        *framePtr = frame;

        uint64_t durParseFrame = BTAgetTickCountNano() / 1000 - timeParseFrame;
        winst->lpDataStreamParseFrameDuration = (float)MTHmax(durParseFrame, (uint64_t)winst->lpDataStreamParseFrameDuration);

        BVQenqueue(winst->lpDataStreamFramesParsedPerSecFrametimes, (void *)(size_t)(frame->timeStamp - winst->timeStampLast));
        winst->timeStampLast = frame->timeStamp;
        winst->lpDataStreamFramesParsedPerSecUpdated = BTAgetTickCount64();
        winst->lpDataStreamFramesParsedCount++;

        return BTA_StatusOk;
    }

    case 4: {
        if (frameToParse->packetCountGot < frameToParse->packetCountTotal && winst->lpAllowIncompleteFrames < 1) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusInvalidData, "Parsing frame v4 %d: A packet is missing, abort", frameToParse->frameCounter);
            free(frame);
            frame = 0;
            return BTA_StatusInvalidData;
        }

        uint8_t *dataHeader = data + 4;
        uint16_t headerLength = *((uint16_t *)dataHeader);
        dataHeader += 2;
        if (dataLen < headerLength) {
            free(frame);  // no channels created so far, so only free this one memory
            frame = 0;
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing header v4: data too short! dataLen %d, headerLen %d", dataLen, headerLength);
            return BTA_StatusOutOfMemory;
        }
        uint16_t crc16 = *((uint16_t *)(data + headerLength - 2));
        if (crc16 != crc16_ccitt(data + 2, headerLength - 4)) {
            free(frame);  // no channels created so far, so only free this one memory
            frame = 0;
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusCrcError, "Parsing frame v4: CRC check failed! read 0x%x, calculated 0x%x", crc16, crc16_ccitt(data + 2, headerLength - 4));
            return BTA_StatusCrcError;
        }
        uint8_t *dataStream = data + headerLength;

        // Count and check descriptors
        uint8_t infoValid = 0;
        uint8_t aliveMsg = 0;
        uint8_t channelCount = 0;
        uint8_t metadataCount = 0;
        uint8_t *dataHeaderTemp = dataHeader;
        while (1) {
            BTA_Data4DescBase *data4DescBase = (BTA_Data4DescBase *)dataHeaderTemp;
            switch (data4DescBase->descriptorType) {
            case btaData4DescriptorTypeFrameInfoV1:
                infoValid = 1;
                dataHeaderTemp += data4DescBase->descriptorLen;
                break;
            case btaData4DescriptorTypeTofV1:
            case btaData4DescriptorTypeTofWithMetadataV1:
            case btaData4DescriptorTypeColorV1:
                channelCount++;
                dataHeaderTemp += data4DescBase->descriptorLen;
                break;
            case btaData4DescriptorTypeAliveMsgV1:
                aliveMsg++;
                dataHeaderTemp += data4DescBase->descriptorLen;
                break;
            case btaData4DescriptorTypeMetadataV1:
                metadataCount++;
                dataHeaderTemp += data4DescBase->descriptorLen;
                break;
            case btaData4DescriptorTypeEof:
                break;
            default:
                free(frame);  // no channels created so far, so only free this one memory
                frame = 0;
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidVersion, "Parsing frame v4: Descriptor %d not supported", data4DescBase->descriptorType);
                return BTA_StatusInvalidVersion;
            }
            if (data4DescBase->descriptorType == btaData4DescriptorTypeEof) {
                dataHeaderTemp += 2;
                break;
            }
        }
        dataHeaderTemp += 2; // crc16
        if (dataHeaderTemp - data != headerLength) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidData, "Parsing header v4: Unexpected header length: i: %d  headerLen: %d", (int)(dataHeaderTemp - data), headerLength);
        }

        if (!infoValid && aliveMsg && !channelCount && !metadataCount) {
            // alive message
            free(frame);
            frame = 0;
            return BTA_StatusOk;
        }

        if (!infoValid) {
            memset(frame, 0, sizeof(BTA_Frame));
        }
        if (channelCount) {
            frame->channels = (BTA_Channel **)calloc(channelCount, sizeof(BTA_Channel *));
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
        if (metadataCount) {
            frame->metadata = (BTA_Metadata **)calloc(metadataCount, sizeof(BTA_Metadata *));
            if (!frame->metadata) {
                BTAfreeFrame(&frame);
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v4: Could not allocate c");
                return BTA_StatusOutOfMemory;
            }
            frame->metadataLen = metadataCount;
        }
        else {
            frame->metadata = 0;
            frame->metadataLen = 0;
        }

        // Parse descriptors
        uint8_t chInd = 0;
        int mdInd = 0;
        while (1) {
            BTA_Data4DescBase *data4DescBase = (BTA_Data4DescBase *)dataHeader;
            dataHeader += data4DescBase->descriptorLen;
            switch (data4DescBase->descriptorType) {
            case btaData4DescriptorTypeFrameInfoV1: {
                BTA_Data4DescFrameInfoV1 *data4DescFrameInfoV1 = (BTA_Data4DescFrameInfoV1 *)data4DescBase;
                frame->frameCounter = data4DescFrameInfoV1->frameCounter;
                frame->sequenceCounter = 0;
                frame->timeStamp = data4DescFrameInfoV1->timestamp;
                frame->mainTemp = ((int16_t)data4DescFrameInfoV1->mainTemp) / 100.0f;
                frame->ledTemp = ((int16_t)data4DescFrameInfoV1->ledTemp) / 100.0f;
                frame->genericTemp = ((int16_t)data4DescFrameInfoV1->genericTemp) / 100.0f;
                frame->firmwareVersionMajor = data4DescFrameInfoV1->firmwareVersion >> 11;
                frame->firmwareVersionMinor = (data4DescFrameInfoV1->firmwareVersion >> 6) & 0x1f;
                frame->firmwareVersionNonFunc = data4DescFrameInfoV1->firmwareVersion & 0x3f;

                // count frame counter gaps
                if (frame->frameCounter != winst->frameCounterLast + 1 && winst->timeStampLast != 0) {
                    winst->lpDataStreamFrameCounterGapsCount++;
                }
                winst->frameCounterLast = frame->frameCounter;
                break;
            }
            case btaData4DescriptorTypeTofV1: {
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
                channel->id = (BTA_ChannelId)data4DescTofV1->channelId;
                assert(channel->id != BTA_ChannelIdUnknown);
                channel->xRes = data4DescTofV1->width;
                channel->yRes = data4DescTofV1->height;
                channel->dataFormat = (BTA_DataFormat)data4DescTofV1->dataFormat;
                channel->unit = (BTA_Unit)data4DescTofV1->unit;
                channel->integrationTime = data4DescTofV1->integrationTime;
                channel->modulationFrequency = data4DescTofV1->modulationFrequency * 10000;
                channel->metadata = 0;
                channel->metadataLen = 0;
                channel->lensIndex = data4DescTofV1->lensIndex;
                channel->flags = data4DescTofV1->flags;
                channel->sequenceCounter = data4DescTofV1->sequenceCounter;
                channel->gain = 0;
                if (dataStream + data4DescTofV1->dataLen > data + dataLen) {
                    // free channels created so far
                    frame->channelsLen = chInd;
                    BTAfreeFrame(&frame);
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v4: data too short: %d", dataLen);
                    return BTA_StatusOutOfMemory;
                }
                BTA_Status status = setMissingAsInvalid((BTA_ChannelId)data4DescTofV1->channelId, (BTA_DataFormat)data4DescTofV1->dataFormat, dataStream, data4DescTofV1->dataLen, frameToParse);
                if (status == BTA_StatusOk) {
                    insertChannelData(channel, dataStream, data4DescTofV1->dataLen);
                }
                else {
                    channel->xRes = 0;
                    channel->yRes = 0;
                    insertChannelData(channel, 0, 0);
                }
                dataStream += data4DescTofV1->dataLen;
                chInd++;
                break;
            }
            case btaData4DescriptorTypeColorV1: {
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
                channel->id = BTA_ChannelIdColor;
                channel->xRes = data4DescColorV1->width;
                channel->yRes = data4DescColorV1->height;
                channel->dataFormat = (BTA_DataFormat)data4DescColorV1->colorFormat;
                channel->unit = BTA_UnitUnitLess;
                channel->integrationTime = data4DescColorV1->integrationTime;
                channel->modulationFrequency = 0;
                channel->metadata = 0;
                channel->metadataLen = 0;
                channel->lensIndex = data4DescColorV1->lensIndex;
                channel->flags = data4DescColorV1->flags;
                channel->sequenceCounter = 0;
                channel->gain = data4DescColorV1->gain;
                if (dataStream + data4DescColorV1->dataLen > data + dataLen) {
                    // free channels created so far
                    frame->channelsLen = chInd;
                    BTAfreeFrame(&frame);
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v4: data too short: %d", dataLen);
                    return BTA_StatusOutOfMemory;
                }
                BTA_Status status = setMissingAsInvalid(BTA_ChannelIdColor, (BTA_DataFormat)data4DescColorV1->colorFormat, dataStream, data4DescColorV1->dataLen, frameToParse);
                if (status == BTA_StatusOk) {
                    insertChannelData(channel, dataStream, data4DescColorV1->dataLen);
                }
                else {
                    channel->xRes = 0;
                    channel->yRes = 0;
                    insertChannelData(channel, 0, 0);
                }
                dataStream += data4DescColorV1->dataLen;
                chInd++;
                break;
            }
            case btaData4DescriptorTypeAliveMsgV1:
                //BTA_Data4DescBase *data4DescMsgV1 = (BTA_Data4DescBase *)data4DescBase;
                // nothing to do
                break;
            case btaData4DescriptorTypeMetadataV1: {
                BTA_Data4DescMetadataV1 *data4DescMetadataV1 = (BTA_Data4DescMetadataV1 *)data4DescBase;
                BTA_Metadata *metadata = (BTA_Metadata *)malloc(sizeof(BTA_Metadata));
                if (!metadata) {
                    // free metadatas created so far
                    frame->metadataLen = mdInd;
                    BTAfreeFrame(&frame);
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v4 ColorV1: Could not allocate channel");
                    return BTA_StatusOutOfMemory;
                }
                metadata->id = (BTA_MetadataId)data4DescMetadataV1->metadataId;
                metadata->dataLen = data4DescMetadataV1->dataLen;
                if (dataStream + metadata->dataLen > data + dataLen) {
                    // free metadatas created so far
                    frame->metadataLen = mdInd;
                    BTAfreeFrame(&frame);
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v4: data too short: %d", dataLen);
                    return BTA_StatusOutOfMemory;
                }
                metadata->data = (uint8_t *)malloc(metadata->dataLen);
                if (!metadata->data) {
                    free(metadata);
                    frame->metadataLen = mdInd;
                    BTAfreeFrame(&frame);
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v4: data too short: %d", dataLen);
                    return BTA_StatusOutOfMemory;
                }
                memcpy(metadata->data, dataStream, metadata->dataLen);
                frame->metadata[mdInd] = metadata;
                dataStream += data4DescMetadataV1->dataLen;
                mdInd++;
                break;
            }
            case btaData4DescriptorTypeEof:
                break;

            default:
                // unreachable because the descriptor was checked before
                assert(0);
                return BTA_StatusIllegalOperation;
            }
            if (data4DescBase->descriptorType == btaData4DescriptorTypeEof) {
                break;
            }
        }

        if ((int)(dataStream - data) != (int)dataLen) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "Parsing frame v4: Unexpected payload length, i: %d  dataLen: %d", (int)(dataStream - data), dataLen);
        }
        *framePtr = frame;

        uint64_t durParseFrame = BTAgetTickCountNano() / 1000 - timeParseFrame;
        winst->lpDataStreamParseFrameDuration = (float)MTHmax(durParseFrame, (uint64_t)winst->lpDataStreamParseFrameDuration);

        BVQenqueue(winst->lpDataStreamFramesParsedPerSecFrametimes, (void *)(size_t)(frame->timeStamp - winst->timeStampLast));
        winst->timeStampLast = frame->timeStamp;
        winst->lpDataStreamFramesParsedPerSecUpdated = BTAgetTickCount64();
        winst->lpDataStreamFramesParsedCount++;

        return BTA_StatusOk;
    }

    default:
        free(frame);
        frame = 0;
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidVersion, "Parsing frame: Version not supported: %d", protocolVersion);
        return BTA_StatusInvalidVersion;
    }
}


//---------------------------------------------------------------------
// copy data with special cases (also convert from SentisTofM100 coordinate system to BltTofApi coordinate system)
// channel->data must already be allocated
static void insertChannelData(BTA_Channel *channel, uint8_t *data, uint32_t dataLen) {
    channel->dataLen = dataLen;
    if (!(channel->flags & 0x2) && channel->id == BTA_ChannelIdX) {
        // Transform coordinate system from camera to bta spec
        channel->id = BTA_ChannelIdZ;
        channel->data = (uint8_t *)malloc(channel->dataLen);
        if (!channel->data) {
            channel->dataLen = 0;
            return;
        }
        memcpy(channel->data, data, channel->dataLen);
        channel->flags &= ~2;
    }
    else if (!(channel->flags & 0x2) && channel->id == BTA_ChannelIdY) {
        // Transform coordinate system from camera to bta spec
        channel->id = BTA_ChannelIdX;
        channel->data = (uint8_t *)malloc(channel->dataLen);
        if (!channel->data) {
            channel->dataLen = 0;
            return;
        }
        int16_t *channelDataTempSrc = (int16_t *)data;
        int16_t *channelDataTempDst = (int16_t *)channel->data;
        int px = dataLen / (channel->dataFormat & 0xf);
        for (int32_t j = 0; j < px; j++) {
            *channelDataTempDst++ = -*channelDataTempSrc++;
        }
        channel->flags &= ~2;
    }
    else if (!(channel->flags & 0x2) && channel->id == BTA_ChannelIdZ) {
        // Transform coordinate system from camera to bta spec
        channel->id = BTA_ChannelIdY;
        channel->data = (uint8_t *)malloc(channel->dataLen);
        if (!channel->data) {
            channel->dataLen = 0;
            return;
        }
        int16_t *channelDataTempSrc = (int16_t *)data;
        int16_t *channelDataTempDst = (int16_t *)channel->data;
        int px = dataLen / (channel->dataFormat & 0xf);
        for (int32_t j = 0; j < px; j++) {
            *channelDataTempDst++ = -*channelDataTempSrc++;
        }
        channel->flags &= ~2;
    }
    else if (channel->dataFormat == BTA_DataFormatSInt16Mlx12S) {
        channel->data = (uint8_t *)malloc(channel->dataLen);
        if (!channel->data) {
            channel->dataLen = 0;
            return;
        }
        int16_t *channelDataTempSrc = (int16_t *)data;
        int16_t *channelDataTempDst = (int16_t *)channel->data;
        int px = dataLen / (channel->dataFormat & 0xf);
        for (int32_t j = 0; j < px; j++) {
            *channelDataTempDst++ = (*channelDataTempSrc & 0x0800) ? (*channelDataTempSrc | 0xf800) : *channelDataTempSrc;
            channelDataTempSrc++;
        }
    }
    else if (channel->dataFormat == BTA_DataFormatSInt16Mlx1C11S) {
        channel->data = (uint8_t *)malloc(channel->dataLen);
        if (!channel->data) {
            channel->dataLen = 0;
            return;
        }
        int16_t *channelDataTempSrc = (int16_t *)data;
        int16_t *channelDataTempDst = (int16_t *)channel->data;
        int px = dataLen / (channel->dataFormat & 0xf);
        for (int32_t j = 0; j < px; j++) {
            *channelDataTempDst++ = (*channelDataTempSrc & 0x0400) ? (*channelDataTempSrc | 0xfc00) : *channelDataTempSrc;
            channelDataTempSrc++;
        }
    }
    else if (channel->dataFormat == BTA_DataFormatUInt16Mlx1C11U) {
        channel->data = (uint8_t *)malloc(channel->dataLen);
        if (!channel->data) {
            channel->dataLen = 0;
            return;
        }
        uint16_t *channelDataTempSrc = (uint16_t *)data;
        uint16_t *channelDataTempDst = (uint16_t *)channel->data;
        int px = dataLen / (channel->dataFormat & 0xf);
        for (int32_t j = 0; j < px; j++) {
            *channelDataTempDst++ = *channelDataTempSrc++ & 0x07ff;
        }
    }
    else {
        channel->data = (uint8_t *)malloc(channel->dataLen);
        if (!channel->data) {
            channel->dataLen = 0;
            return;
        }
        memcpy(channel->data, data, channel->dataLen);
    }
}


static BTA_ChannelId BTAETHgetChannelId(BTA_EthImgMode imgMode, uint8_t channelIndex) {
    switch (imgMode) {
    case BTA_EthImgModeRawdistAmp:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdRawDist;
        case 1:
            return BTA_ChannelIdAmplitude;
        default:
            assert(0);
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeDistAmp:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdDistance;
        case 1:
            return BTA_ChannelIdAmplitude;
        default:
            assert(0);
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
            assert(0);
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
            assert(0);
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
            assert(0);
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
            assert(0);
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
            assert(0);
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
            assert(0);
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
            assert(0);
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeXAmp:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdX;
        case 1:
            return BTA_ChannelIdAmplitude;
        default:
            assert(0);
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
            assert(0);
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeDist:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdDistance;
        default:
            assert(0);
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
            assert(0);
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModePhase90_270:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdPhase90;
        case 1:
            return BTA_ChannelIdPhase270;
        default:
            assert(0);
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModePhase0:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdPhase0;
        default:
            assert(0);
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModePhase90:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdPhase90;
        default:
            assert(0);
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModePhase180:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdPhase180;
        default:
            assert(0);
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModePhase270:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdPhase270;
        default:
            assert(0);
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeIntensities:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdColor;
        default:
            assert(0);
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
        assert(0);
        return BTA_ChannelIdUnknown;
    case BTA_EthImgModeRawQI:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdRawI;
        case 1:
            return BTA_ChannelIdRawQ;
        default:
            assert(0);
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeDistConfExt:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdDistance;
        case 1:
            return BTA_ChannelIdConfidence;
        default:
            assert(0);
            return BTA_ChannelIdUnknown;
        }
    case BTA_EthImgModeAmp:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdAmplitude;
        default:
            assert(0);
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
        assert(0);
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
    case BTA_EthImgModeIntensities:
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
            return BTA_DataFormatSInt16Mlx1C11S;
        }
        if (rawPhaseContent == 1) {
            return BTA_DataFormatSInt16Mlx12S;
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


int BTAgetBytesPerPixelSum(BTA_EthImgMode imgMode) {
    switch (imgMode) {
    case BTA_EthImgModeAmp:
    case BTA_EthImgModeDist:
    case BTA_EthImgModePhase0:
    case BTA_EthImgModePhase90:
    case BTA_EthImgModePhase180:
    case BTA_EthImgModePhase270:
    case BTA_EthImgModeIntensities:
        return 2;
    case BTA_EthImgModeDistConfExt:
        return 3;
    case BTA_EthImgModeDistAmp:
    case BTA_EthImgModeXAmp:
    case BTA_EthImgModeRawdistAmp:
    case BTA_EthImgModePhase0_180:
        case BTA_EthImgModePhase90_270:
        return 4;
    case BTA_EthImgModeDistAmpConf:
        return 5;
    case BTA_EthImgModeDistAmpBalance:
    case BTA_EthImgModeXYZ:
        return 6;
    case BTA_EthImgModeXYZAmp:
    case BTA_EthImgModeDistXYZ:
    case BTA_EthImgModeTest:
    case BTA_EthImgModePhase0_90_180_270:
    case BTA_EthImgModePhase270_180_90_0:
        return 8;
    case BTA_EthImgModeDistAmpColor:
    case BTA_EthImgModeDistAmpConfColor:
    case BTA_EthImgModeXYZConfColor:
    case BTA_EthImgModeXYZAmpColorOverlay:
    case BTA_EthImgModeXYZColor:
    case BTA_EthImgModeDistColor:
    case BTA_EthImgModeColor:
    case BTA_EthImgModeRawPhases:
    case BTA_EthImgModeRawQI:
        // ERROR!!! NOT SUPPORTED!!!!
        return 1;
    default:
        return 0;
    }
}


BTA_Status BTAparseLenscalib(uint8_t* data, uint32_t dataLen, BTA_LensVectors** lensVectors, BTA_InfoEventInst *infoEventInst) {
    if (!data || !lensVectors) {
        return BTA_StatusInvalidParameter;
    }
    if (dataLen < 50) {
        return BTA_StatusInvalidData;
    }
    BTA_LenscalibHeader* lenscalibHeader = (BTA_LenscalibHeader*)data;
    if (lenscalibHeader->preamble0 != 0xf011 || lenscalibHeader->preamble1 != 0xb1ad) {
        return BTA_StatusInvalidData;
    }

    BTA_LensVectors* vectors;
    switch (lenscalibHeader->version) {
    case 1:
    case 2:
    {
        uint16_t xRes = lenscalibHeader->xRes;
        uint16_t yRes = lenscalibHeader->yRes;
        vectors = (BTA_LensVectors*)calloc(1, sizeof(BTA_LensVectors));
        if (!vectors) {
            return BTA_StatusOutOfMemory;
        }
        vectors->lensIndex = 1; // Not supported by lenscalib, but all products using this file format only have one ToF-sensor anyway
        vectors->lensId = lenscalibHeader->lensId;
        vectors->xRes = xRes;
        vectors->yRes = yRes;
        vectors->vectorsX = (float*)calloc(sizeof(float), yRes * xRes);
        vectors->vectorsY = (float*)calloc(sizeof(float), yRes * xRes);
        vectors->vectorsZ = (float*)calloc(sizeof(float), yRes * xRes);
        if (!vectors->vectorsX || !vectors->vectorsY || !vectors->vectorsZ) {
            free(vectors->vectorsX);
            free(vectors->vectorsY);
            free(vectors->vectorsZ);
            free(vectors);
            return BTA_StatusOutOfMemory;
        }
        if (lenscalibHeader->bytesPerPixel == 2) {
            uint16_t coordSysId = lenscalibHeader->coordSysId;
            float expansionfactor = (float)lenscalibHeader->expasionFactor;
            int16_t* dataXYZ = (int16_t*)(data + sizeof(BTA_LenscalibHeader));
            float* dataX = vectors->vectorsX;
            float* dataY = vectors->vectorsY;
            float* dataZ = vectors->vectorsZ;
            if (coordSysId) {
                for (int i = 0; i < xRes * yRes; i++) {
                    *dataX++ = *dataXYZ++ / expansionfactor;
                    *dataY++ = *dataXYZ++ / expansionfactor;
                    *dataZ++ = *dataXYZ++ / expansionfactor;
                }
            }
            else {
                for (int i = 0; i < xRes * yRes; i++) {
                    *dataZ++ = *dataXYZ++ / expansionfactor;
                    *dataX++ = -*dataXYZ++ / expansionfactor;
                    *dataY++ = -*dataXYZ++ / expansionfactor;
                }
            }
            *lensVectors = vectors;
            return BTA_StatusOk;
        }
        else {
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "addLenscalib: bytesPerPixel %d not supported!", lenscalibHeader->bytesPerPixel);
            return BTA_StatusNotSupported;
        }
    }

    default:
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "addLenscalib: version %d not supported!", lenscalibHeader->version);
        return BTA_StatusNotSupported;
    }
}









int initShm(uint32_t shmKeyNum, uint32_t shmSize, int32_t *shmFd, uint8_t **bufShmBase, sem_t **semFullWrite, sem_t **semFullRead, sem_t **semEmptyWrite, sem_t **semEmptyRead, fifo_t **fifoFull, fifo_t **fifoEmpty, uint8_t **bufDataBase, BTA_InfoEventInst *infoEventInst) {
#   if defined PLAT_LINUX
    const int nameLen = 222;
    char name[nameLen];
    snprintf(name, nameLen, "/%d_shm", shmKeyNum);
    *shmFd = shm_open(name, O_RDWR, S_IRUSR | S_IWUSR);
    if (*shmFd == -1) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "error in shm_open, errno %s (%d)", strerror(errno), errno);
        return 0;
    }
    *bufShmBase = (uint8_t *)mmap(0, shmSize, PROT_WRITE | PROT_READ, MAP_SHARED, *shmFd, 0);
    if (*bufShmBase == MAP_FAILED) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "error in mmap, errno %s (%d)", strerror(errno), errno);
        closeShm(0, 0, 0, 0, 0, 0, shmFd, shmKeyNum, infoEventInst);
        return 0;
    }
    snprintf(name, nameLen, "%d_sem_full_write", shmKeyNum);
    *semFullWrite = sem_open(name, 0);
    if (*semFullWrite == SEM_FAILED) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "error in sem_open sem_full_write, errno %s (%d)", strerror(errno), errno);
        closeShm(0, 0, 0, semFullWrite, bufShmBase, shmSize, shmFd, shmKeyNum, infoEventInst);
        return 0;
    }
    snprintf(name, nameLen, "%d_sem_full_read", shmKeyNum);
    *semFullRead = sem_open(name, 0);
    if (*semFullRead == SEM_FAILED) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "error in sem_open sem_full_read, errno %s (%d)", strerror(errno), errno);
        closeShm(0, 0, semFullRead, semFullWrite, bufShmBase, shmSize, shmFd, shmKeyNum, infoEventInst);
        return 0;
    }
    snprintf(name, nameLen, "%d_sem_empty_write", shmKeyNum);
    *semEmptyWrite = sem_open(name, 0);
    if (*semEmptyWrite == SEM_FAILED) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "error in sem_open sem_empty_write, errno %s (%d)", strerror(errno), errno);
        closeShm(0, semEmptyWrite, semFullRead, semFullWrite, bufShmBase, shmSize, shmFd, shmKeyNum, infoEventInst);
        return 0;
    }
    snprintf(name, nameLen, "%d_sem_empty_read", shmKeyNum);
    *semEmptyRead = sem_open(name, 0);
    if (*semEmptyRead == SEM_FAILED) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "error in sem_open sem_empty_read, errno %s (%d)", strerror(errno), errno);
        closeShm(semEmptyRead, semEmptyWrite, semFullRead, semFullWrite, bufShmBase, shmSize, shmFd, shmKeyNum, infoEventInst);
        return 0;
    }
    *fifoFull = (fifo_t *)*bufShmBase;
    *fifoEmpty = (fifo_t *)(*bufShmBase + getSize(*fifoFull));
    *bufDataBase = *bufShmBase + getSize(*fifoFull) + getSize(*fifoEmpty);
    return 1;
#   else
    return 0;
#   endif
}


void closeShm(sem_t **semEmptyRead, sem_t **semEmptyWrite, sem_t **semFullRead, sem_t **semFullWrite, uint8_t **bufShmBase, uint32_t shmSize, int32_t *shmFd, uint32_t shmKeyNum, BTA_InfoEventInst *infoEventInst) {
#   if defined PLAT_LINUX
    if (shmKeyNum) {
        const int nameLen = 222;
        char name[nameLen];
        if (semEmptyRead) {
            snprintf(name, nameLen, "%d_sem_empty_read", shmKeyNum);
            int err = sem_unlink(name);
            if (err) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "sem_unlink returned %d, errno %s (%d)", err, strerror(errno), errno);
            }
            err = sem_close(*semEmptyRead);
            if (err) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "sem_close returned %d, errno %s (%d)", err, strerror(errno), errno);
            }
            *semEmptyRead = 0;
        }
        if (semEmptyWrite) {
            snprintf(name, nameLen, "%d_sem_empty_write", shmKeyNum);
            int err = sem_unlink(name);
            if (err) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "sem_unlink returned %d, errno %s (%d)", err, strerror(errno), errno);
            }
            err = sem_close(*semEmptyWrite);
            if (err) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "sem_close returned %d, errno %s (%d)", err, strerror(errno), errno);
            }
            *semEmptyWrite = 0;
        }
        if (semFullRead) {
            snprintf(name, nameLen, "%d_sem_full_read", shmKeyNum);
            int err = sem_unlink(name);
            if (err) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "sem_unlink returned %d, errno %s (%d)", err, strerror(errno), errno);
            }
            err = sem_close(*semFullRead);
            if (err) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "sem_close returned %d, errno %s (%d)", err, strerror(errno), errno);
            }
            *semFullRead = 0;
        }
        if (semFullWrite) {
            snprintf(name, nameLen, "%d_sem_full_write", shmKeyNum);
            int err = sem_unlink(name);
            if (err) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "sem_unlink returned %d, errno %s (%d)", err, strerror(errno), errno);
            }
            err = sem_close(*semFullWrite);
            if (err) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "sem_close returned %d, errno %s (%d)", err, strerror(errno), errno);
            }
            *semFullWrite = 0;
        }
        if (bufShmBase && shmSize) {
            munmap(*bufShmBase, shmSize);
        }
        if (shmFd) {
            close(*shmFd);
            *shmFd = 0;
        }
        snprintf(name, nameLen, "/%d_shm", shmKeyNum);
        shm_unlink(name);
    }
#   endif
}


BTA_Status BTAparseFrameFromShm(BTA_Handle handle, uint8_t *data, BTA_Frame **framePtr) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }

    uint64_t timeParseFrame = BTAgetTickCountNano() / 1000;
    *framePtr = 0;
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

    case 4: {
        uint8_t *dataHeader = data + 4;
        uint16_t headerLength = *((uint16_t *)dataHeader);
        dataHeader += 2;
        uint8_t *dataStream = data + headerLength;

        // Count and check descriptors
        uint8_t infoValid = 0;
        uint8_t aliveMsg = 0;
        uint8_t channelCount = 0;
        uint8_t metadataCount = 0;
        uint8_t *dataHeaderTemp = dataHeader;
        while (1) {
            BTA_Data4DescBase *data4DescBase = (BTA_Data4DescBase *)dataHeaderTemp;
            switch (data4DescBase->descriptorType) {
            case btaData4DescriptorTypeFrameInfoV1:
                infoValid = 1;
                dataHeaderTemp += data4DescBase->descriptorLen;
                break;
            case btaData4DescriptorTypeTofV1:
            case btaData4DescriptorTypeTofWithMetadataV1:
            case btaData4DescriptorTypeColorV1:
                channelCount++;
                dataHeaderTemp += data4DescBase->descriptorLen;
                break;
            case btaData4DescriptorTypeAliveMsgV1:
                aliveMsg = 1;
                dataHeaderTemp += data4DescBase->descriptorLen;
                break;
            case btaData4DescriptorTypeMetadataV1:
                metadataCount++;
                dataHeaderTemp += data4DescBase->descriptorLen;
                break;
            case btaData4DescriptorTypeEof:
                break;
            default:
                free(frame);  // no channels created so far, so only free this one memory
                frame = 0;
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidVersion, "Parsing frame v4: Descriptor %d not supported", data4DescBase->descriptorType);
                return BTA_StatusInvalidVersion;
            }
            if (data4DescBase->descriptorType == btaData4DescriptorTypeEof) {
                dataHeaderTemp += 2;
                break;
            }
        }
        dataHeaderTemp += 2; // crc16
        if (dataHeaderTemp - data != headerLength) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidData, "Parsing header v4: Unexpected header length: i: %d  headerLen: %d", (int)(dataHeaderTemp - data), headerLength);
        }

        if (!infoValid && aliveMsg && !channelCount && !metadataCount) {
            // alive message
            free(frame);
            frame = 0;
            return BTA_StatusOk;
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
        if (metadataCount) {
            frame->metadata = (BTA_Metadata **)malloc(metadataCount * sizeof(BTA_Metadata *));
            if (!frame->metadata) {
                free(frame);  // no metadata created so far, so only free this one memory
                frame = 0;
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v4: Could not allocate c");
                return BTA_StatusOutOfMemory;
            }
            frame->metadataLen = metadataCount;
        }
        else {
            frame->metadata = 0;
            frame->metadataLen = 0;
        }

        // Parse descriptors
        uint8_t chInd = 0;
        int mdInd = 0;
        while (1) {
            BTA_Data4DescBase *data4DescBase = (BTA_Data4DescBase *)dataHeader;
            dataHeader += data4DescBase->descriptorLen;
            switch (data4DescBase->descriptorType) {
            case btaData4DescriptorTypeFrameInfoV1: {
                BTA_Data4DescFrameInfoV1 *data4DescFrameInfoV1 = (BTA_Data4DescFrameInfoV1 *)data4DescBase;
                frame->frameCounter = data4DescFrameInfoV1->frameCounter;
                frame->sequenceCounter = 0;
                frame->timeStamp = data4DescFrameInfoV1->timestamp;
                frame->mainTemp = data4DescFrameInfoV1->mainTemp / 100.0f;
                frame->ledTemp = data4DescFrameInfoV1->ledTemp / 100.0f;
                frame->genericTemp = data4DescFrameInfoV1->genericTemp / 100.0f;
                frame->firmwareVersionMajor = data4DescFrameInfoV1->firmwareVersion >> 11;
                frame->firmwareVersionMinor = (data4DescFrameInfoV1->firmwareVersion >> 6) & 0x1f;
                frame->firmwareVersionNonFunc = data4DescFrameInfoV1->firmwareVersion & 0x3f;

                // count frame counter gaps
                if (frame->frameCounter != winst->frameCounterLast + (uint32_t)winst->lpDataStreamFrameCounterGap && winst->timeStampLast != 0) {
                    winst->lpDataStreamFrameCounterGapsCount++;
                }
                winst->frameCounterLast = frame->frameCounter;
                break;
            }
            case btaData4DescriptorTypeTofV1: {
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

                channel->id = (BTA_ChannelId)data4DescTofV1->channelId;
                channel->xRes = data4DescTofV1->width;
                channel->yRes = data4DescTofV1->height;
                channel->dataFormat = (BTA_DataFormat)data4DescTofV1->dataFormat;
                channel->unit = (BTA_Unit)data4DescTofV1->unit;
                channel->integrationTime = data4DescTofV1->integrationTime;
                channel->modulationFrequency = data4DescTofV1->modulationFrequency * 10000;
                channel->metadata = 0;
                channel->metadataLen = 0;
                channel->lensIndex = data4DescTofV1->lensIndex;
                channel->flags = data4DescTofV1->flags;
                channel->sequenceCounter = data4DescTofV1->sequenceCounter;
                channel->gain = 0;
                insertChannelDataFromShm(winst, channel, dataStream, data4DescTofV1->dataLen);
                dataStream += data4DescTofV1->dataLen;
                chInd++;
                break;
            }
            case btaData4DescriptorTypeColorV1: {
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

                channel->id = BTA_ChannelIdColor;
                channel->xRes = data4DescColorV1->width;
                channel->yRes = data4DescColorV1->height;
                channel->dataFormat = (BTA_DataFormat)data4DescColorV1->colorFormat;
                channel->unit = BTA_UnitUnitLess;
                channel->integrationTime = data4DescColorV1->integrationTime;
                channel->modulationFrequency = 0;
                channel->metadata = 0;
                channel->metadataLen = 0;
                channel->lensIndex = data4DescColorV1->lensIndex;
                channel->flags = data4DescColorV1->flags;
                channel->sequenceCounter = 0;
                channel->gain = data4DescColorV1->gain;
                insertChannelDataFromShm(winst, channel, dataStream, data4DescColorV1->dataLen);
                dataStream += data4DescColorV1->dataLen;
                chInd++;
                break;
            }
            case btaData4DescriptorTypeAliveMsgV1:
                //BTA_Data4DescBase *data4DescMsgV1 = (BTA_Data4DescBase *)data4DescBase;
                // nothing to do
                break;

            case btaData4DescriptorTypeMetadataV1: {
                BTA_Data4DescMetadataV1 *data4DescMetadataV1 = (BTA_Data4DescMetadataV1 *)data4DescBase;
                BTA_Metadata *metadata = (BTA_Metadata *)malloc(sizeof(BTA_Metadata));
                if (!metadata) {
                    // free metadatas created so far
                    frame->metadataLen = mdInd;
                    BTAfreeFrame(&frame);
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Parsing frame v4 ColorV1: Could not allocate channel");
                    return BTA_StatusOutOfMemory;
                }
                frame->metadata[mdInd] = metadata;
                metadata->id = (BTA_MetadataId)data4DescMetadataV1->metadataId;
                metadata->dataLen = data4DescMetadataV1->dataLen;
                metadata->id = (BTA_MetadataId)data4DescMetadataV1->metadataId;
                metadata->data = dataStream;
                dataStream += data4DescMetadataV1->dataLen;
                mdInd++;
                break;
            }
            case btaData4DescriptorTypeEof:
                break;

            default:
                // unreachable because the descriptor was checked before
                assert(0);
                return BTA_StatusIllegalOperation;
            }
            if (data4DescBase->descriptorType == btaData4DescriptorTypeEof) {
                break;
            }
        }

        *framePtr = frame;

        uint64_t durParseFrame = BTAgetTickCountNano() / 1000 - timeParseFrame;
        winst->lpDataStreamParseFrameDuration = (float)MTHmax(durParseFrame, (uint64_t)winst->lpDataStreamParseFrameDuration);

        BVQenqueue(winst->lpDataStreamFramesParsedPerSecFrametimes, (void *)(size_t)(frame->timeStamp - winst->timeStampLast));
        winst->timeStampLast = frame->timeStamp;
        winst->lpDataStreamFramesParsedPerSecUpdated = BTAgetTickCount64();
        winst->lpDataStreamFramesParsedCount++;

        return BTA_StatusOk;
    }

    default:
        free(frame);
        frame = 0;
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidVersion, "Parsing frame: Version not supported: %d", protocolVersion);
        return BTA_StatusInvalidVersion;
    }
}


BTA_Status BTAgrabCallbackEnqueueFromShm(BTA_WrapperInst *winst, BTA_Frame *frame) {
    assert(winst);
    assert(frame);

    //BTAcalcXYZApply(winst, frame);
    //BTAundistortApply(winst, frame);

    if (winst->grabInst) {
        BGRBgrab(winst->grabInst, frame);
    }

    uint8_t userFreesFrame = 0;
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
            userFreesFrame = winst->frameArrivedInst->frameArrivedReturnOptions->userFreesFrame;
        }
    }

    if (userFreesFrame) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInvalidParameter, "userFreesFrame is not supported with shared memory interface!!!");
    }
    return BTAfreeFrameFromShm(&frame);
}


static void insertChannelDataFromShm(BTA_WrapperInst *winst, BTA_Channel *channel, uint8_t *data, uint32_t dataLen) {
    channel->dataLen = dataLen;

    if (channel->id != BTA_ChannelIdColor && !(channel->flags & 0x2)) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "insertChannelDataFromShm: coordinate system should be transformed! (avoided because we want zero memcopies)");
    }
    else if (channel->dataFormat == BTA_DataFormatSInt16Mlx12S || channel->dataFormat == BTA_DataFormatSInt16Mlx1C11S || channel->dataFormat == BTA_DataFormatUInt16Mlx1C11U) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "insertChannelDataFromShm: bit operations should be performed! (avoided because we want zero memcopies)");
    }
    channel->data = data;
    return;
}