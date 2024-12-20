#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include <bta.h>
#include <utils.h>
#include "bta_helper.h"
#include "bta_grabbing.h"
#include <bta_oshelper.h>
#include <timing_helper.h>
#include <pthread_helper.h>
#include <mth_math.h>
#include <bitconverter.h>
#include <bta_serialization.h>
#include "memory_area.h"

#include "lzma/LzmaLib.h"


static const int32_t grabbingQueueLength = 1024;
const uint32_t btaGrabVersion = 2;
const char *btaGrabVersionKey = "File format: v";
const char *btaGrabPonKey = "PON: ";
const char *btaGrabSerialNumberKey = "Serial number: ";
const char *btaGrabDeviceTypeKey = "Device type: 0x";
const char *btaGrabFirmwareVersionKey = "Firmware: v";
const char *btaGrabTotalFrameCountKey = "Total frame count: ";
const char *btaGrabSeparator = "----------------------------------------------------------------------------------------------";

///////// Local prototypes
static void *grabRunFunction(void *handle);


BTA_Status BTA_CALLCONV BTAinitGrabbingConfig(BTA_GrabbingConfig *config) {
    if (!config) {
        return BTA_StatusInvalidParameter;
    }
    memset(config, 0, sizeof(BTA_GrabbingConfig));
    return BTA_StatusOk;
}

//BTA_Status BTA_CALLCONV 


BTA_Status BGRBinit(BTA_GrabInst **instPtr, BTA_InfoEventInst *infoEventInst) {
    BTA_Status status;
    if (!instPtr) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BRGBinit: Parameter instPtr missing");
        return BTA_StatusInvalidParameter;
    }

    BTA_GrabInst *inst = (BTA_GrabInst *)calloc(1, sizeof(BTA_GrabInst));
    if (!inst) {
        return BTA_StatusOutOfMemory;
    }

    sprintf((char *)inst->libNameVer, "BltTofApiLib v%d.%d.%d", BTA_VER_MAJ, BTA_VER_MIN, BTA_VER_NON_FUNC);
    inst->lpBltstreamCompressionMode = BTA_CompressionModeNone;
    inst->totalFrameCount = 0;
    inst->infoEventInst = infoEventInst;

    status = BFQinit(grabbingQueueLength, BTA_QueueModeDropOldest, &inst->grabbingQueue);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "BRGBinit: Failed to init queue");
        return BTA_StatusOutOfMemory;
    }
    //status = BTAinitMutex(&inst->grabbingQueueMutex);
    //if (status != BTA_StatusOk) {
    //    free(inst->grabbingQueue);
    //    inst->grabbingQueue = 0;
    //    free(inst->grabbingFilename);
    //    inst->grabbingFilename = 0;
    //    BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "BRGBinit: Failed to init grabbingQueueMutex");
    //    return status;
    //}

    *instPtr = inst;
    return BTA_StatusOk;
}


BTA_Status BGRBstart(BTA_GrabInst *inst, BTA_GrabbingConfig *config, BTA_DeviceInfo *deviceInfo) {
    if (!inst || !config) {
        return BTA_StatusInvalidParameter;
    }
    if (!config->filename) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAstartGrabbing: Parameter filename missing");
        return BTA_StatusInvalidParameter;
    }
    if (inst->grabbingEnabled) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusIllegalOperation, "BTAstartGrabbing: Grabbing is already in progress");
        return BTA_StatusIllegalOperation;
    }

    inst->grabbingFilename = (uint8_t *)malloc(strlen((char *)config->filename) + 1);
    if (!inst->grabbingFilename) {
        return BTA_StatusOutOfMemory;
    }
    strcpy((char *)inst->grabbingFilename, (char *)config->filename);

    void *file;
    BTA_Status status = BTAfLargeOpen((char *)config->filename, "w", &file);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAstartGrabbing: Not able to open file");
        return BTA_StatusInvalidParameter;
    }
    char msg[1000] = { 0 };
    time_t now = time(0);
    strftime(msg + strlen(msg), 100, "%Y-%m-%d %H:%M:%S\n", localtime(&now));
    sprintf(msg + strlen(msg), "%s%d\n", btaGrabVersionKey, btaGrabVersion);
    sprintf(msg + strlen(msg), "Grabbed with %s\n", inst->libNameVer);
    sprintf(msg + strlen(msg), "Original filename: %s\n", config->filename);
    sprintf(msg + strlen(msg), "%s%s\n", btaGrabPonKey, deviceInfo->productOrderNumber);
    sprintf(msg + strlen(msg), "%s%d\n", btaGrabSerialNumberKey, deviceInfo->serialNumber);
    sprintf(msg + strlen(msg), "%s%x\n", btaGrabDeviceTypeKey, deviceInfo->deviceType);
    sprintf(msg + strlen(msg), "%s%d.%d.%d\n", btaGrabFirmwareVersionKey, deviceInfo->firmwareVersionMajor, deviceInfo->firmwareVersionMinor, deviceInfo->firmwareVersionNonFunc);
    sprintf(msg + strlen(msg), "%s%s\n", btaGrabTotalFrameCountKey, "000000000000");
    // if msg.contains(grabSeparator) then replace by something else (but c'mon... never gonna happen)
    sprintf(msg + strlen(msg), "%s\n", btaGrabSeparator);
    BTAfLargeWrite(file, msg, (uint32_t)strlen(msg), 0);
    BTAfLargeClose(file);

    BFQclear(inst->grabbingQueue);
    inst->grabbingEnabled = 1;
    int result = BTAcreateThread(&(inst->grabbingThread), &grabRunFunction, (void *)inst);
    if (result != 0) {
        free(inst->grabbingFilename);
        inst->grabbingFilename = 0;
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAstartGrabbing: Could not start grabbingThread, error: %d", result);
        return BTA_StatusRuntimeError;
    }

    BTAinfoEventHelper(inst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAstartGrabbing: Grabbing started");
    return BTA_StatusOk;
}


BTA_Status BGRBgrab(BTA_GrabInst *inst, BTA_Frame *frame) {
    if (!inst || !frame) {
        return BTA_StatusInvalidParameter;
    }
    if (!inst->grabbingEnabled) {
        return BTA_StatusOk;
    }

    BTA_Frame *frameCopy;
    BTAcloneFrame(frame, &frameCopy);
    BTA_Status status = BFQenqueue(inst->grabbingQueue, frameCopy);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_WARNING, status, "Grabbing: Queue is full, not grabbing frame #%d", frame->frameCounter);
        BTAfreeFrame(&frameCopy);
    }
    return status;
}


static void *grabRunFunction(void *handle) {
    BTA_GrabInst *inst = (BTA_GrabInst *)handle;
    void *file;
    BTA_Status status = BTAfLargeOpen((char *)inst->grabbingFilename, "ab", &file);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "Grabbing: Not able to open file");
        return 0;
    }
    while (1) {
        BTA_Frame *frame;
        status = BFQdequeue(inst->grabbingQueue, &frame, 100);
        if (status == BTA_StatusOk) {
            int32_t frameSerializedLen;
            status = BTAgetSerializedLength(frame, (uint32_t *)&frameSerializedLen);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "Grabbing: Error in BTAgetSerializedLength");
                BTAfLargeClose(file);
                return 0;
            }
            uint8_t *frameSerialized = (uint8_t *)malloc(sizeof(uint32_t) + frameSerializedLen + sizeof(uint32_t));
            if (!frameSerialized) {
                BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Grabbing: Cannot allocate");
                BTAfLargeClose(file);
                return 0;
            }
            int32_t frameSerializedLenTemp = frameSerializedLen;
            status = BTAserializeFrame(frame, frameSerialized + sizeof(uint32_t), (uint32_t *)&frameSerializedLenTemp);
            if (status != BTA_StatusOk) {
                free(frameSerialized);
                frameSerialized = 0;
                BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "Grabbing: Error in BTAserializeFrame");
                BTAfLargeClose(file);
                return 0;
            }
            if (frameSerializedLenTemp != frameSerializedLen) {
                free(frameSerialized);
                frameSerialized = 0;
                BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Grabbing: Error, BTAserializeFrame result %d differs from BTAgetSerializedLength %d", frameSerializedLenTemp, frameSerializedLen);
                BTAfLargeClose(file);
                return 0;
            }

            if (inst->lpBltstreamCompressionMode) {
                // Allocate for compressed size, compression preamble, uncompressed size, props, data and compressed size (again)
                uint32_t frameSerializedCompressedLen = sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint32_t) + LZMA_PROPS_SIZE + frameSerializedLen + sizeof(uint32_t);
                uint8_t *frameSerializedCompressed = (uint8_t *)malloc(frameSerializedCompressedLen);
                if (!frameSerializedCompressed) {
                    free(frameSerialized);
                    frameSerialized = 0;
                    BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Grabbing: Cannot allocate for compression");
                    BTAfLargeClose(file);
                    return 0;
                }
                uint8_t *dst = frameSerializedCompressed + sizeof(uint32_t);
                uint32_t dstLen = frameSerializedCompressedLen - sizeof(uint32_t);
                uint64_t timeStart = BTAgetTickCount64();
                status = BTAcompressSerializedFrame(frameSerialized + sizeof(uint32_t), frameSerializedLen, inst->lpBltstreamCompressionMode, dst, &dstLen);
                uint64_t timeEnd = BTAgetTickCount64();
                free(frameSerialized);
                frameSerialized = 0;
                if (status != BTA_StatusOk) {
                    BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Grabbing: Error in BTAcompressSerializedFrame");
                    BTAfLargeClose(file);
                    return 0;
                }
                BTAinfoEventHelper(inst->infoEventInst, VERBOSE_DEBUG, BTA_StatusInformation, "Grabbing: compressed bltframe %d : %d = %f%% in %lums", dstLen, frameSerializedLen, (float)dstLen / frameSerializedLen * 100.0f, timeEnd - timeStart);
                frameSerialized = frameSerializedCompressed;
                frameSerializedLen = dstLen;
            }

            uint32_t dataLen = 0;
            BTAbitConverterFromUInt32(frameSerializedLen, frameSerialized, &dataLen);
            dataLen += frameSerializedLen;
            BTAbitConverterFromUInt32(frameSerializedLen, frameSerialized, &dataLen);

            uint32_t bytesWritten = 0;
            while (bytesWritten < dataLen) {
                uint32_t bytesWrittenTemp = 0;
                status = BTAfLargeWrite(file, frameSerialized + bytesWritten, dataLen - bytesWritten, &bytesWrittenTemp);
                if (status == BTA_StatusOk) {
                    bytesWritten += bytesWrittenTemp;
                }
                else {
                    BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "Grabbing: Error writing to file!");
                    if (!inst->grabbingEnabled) {
                        // Not able to write and grabbing disabled -> abort
                        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "Grabbing: Aborting!");
                        BTAfLargeClose(file);
                        return 0;
                    }
                }
            }
            inst->totalFrameCount++;
            free(frameSerialized);
            frameSerialized = 0;
        }
        else if (!inst->grabbingEnabled) {
            // Nothing in queue and grabbing disabled -> We're done
            BTAfLargeClose(file);
            return 0;
        }
    }
}


BTA_Status BGRBstop(BTA_GrabInst *inst) {
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    inst->grabbingEnabled = 0;
    BTAinfoEventHelper(inst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Stopping grabbing");
    BTA_Status status = BTAjoinThread(inst->grabbingThread);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "Grabbing: Failed to join grabbingThread");
        return status;
    }

    // Write the total frame count into header
    void *file;
    status = BTAfLargeOpen((char *)inst->grabbingFilename, "r+", &file);
    if (status == BTA_StatusOk) {
        while (1) {
            uint64_t filePos;
            status = BTAfLargeTell(file, &filePos);
            if (status != BTA_StatusOk) {
                break;
            }
            char *line;
            status = BTAfLargeReadLine(file, &line);
            if (status == BTA_StatusOk) {
                if (UTILstartsWith(btaGrabTotalFrameCountKey, line)) {
                    free(line);
                    char msg[100] = { 0 };
                    sprintf(msg, "%s%012d\n", btaGrabTotalFrameCountKey, inst->totalFrameCount);
                    BTAfLargeSeek(file, filePos, BTA_SeekOriginBeginning, 0);
                    BTAfLargeWrite(file, msg, (uint32_t)strlen(msg), 0);
                    break;
                }
                free(line);
            }
        }
        BTAfLargeClose(file);
    }
    free(inst->grabbingFilename);
    inst->grabbingFilename = 0;
    BTAinfoEventHelper(inst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Grabbing stopped");
    return BTA_StatusOk;
}


BTA_Status BGRBclose(BTA_GrabInst **instPtr) {
    if (!instPtr || !*instPtr) {
        return BTA_StatusInvalidParameter;
    }
    BTA_GrabInst *inst = *instPtr;
    if (inst->grabbingEnabled) {
        BGRBstop(inst);
    }

    //status = BTAcloseMutex(inst->grabbingQueueMutex);
    //if (status != BTA_StatusOk) {
    //    BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "Grabbing: Failed to close grabbingQueueMutex");
    //    return status;
    //}
    //inst->grabbingQueueMutex = 0;

    BFQclose(&inst->grabbingQueue);
    free(inst);
    *instPtr = 0;
    return BTA_StatusOk;
}


