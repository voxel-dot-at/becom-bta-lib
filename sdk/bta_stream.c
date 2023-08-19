#ifndef BTA_WO_STREAM

// BTAopen with different combinations of PON and serialNumber

#include <bta.h>
#include <bta_helper.h>
#include <bta_oshelper.h>
#include <timing_helper.h>
#include <pthread_helper.h>

#include "bta_stream.h"

#ifdef PLAT_WINDOWS
#   include <Windows.h>
#else
#   include <netdb.h>
#   include <errno.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


#define BTAgt0(x) (x > 0 ? 1 : 0)

static const int32_t frameQueueInternalLength = 100;


typedef struct FrameAndIndex {
    uint32_t index;
    BTA_Frame *frame;
} FrameAndIndex;


/////////// Local prototypes
static void *bufferRunFunction(void *handle);
static void *streamRunFunction(void *handle);
static BTA_Status getFrameFromFile(BTA_WrapperInst *winst, int32_t index, BTA_Frame **frame);
static BTA_Status freeFrameAndIndex(FrameAndIndex **frameAndIndex);
////////////////////////////////////////////////////////////////////////////////


static int getLastError() {
#ifdef PLAT_WINDOWS
    return GetLastError();
#else
    return errno;
#endif
}


BTA_Status BTASTREAMopen(BTA_Config *config, BTA_WrapperInst *winst) {

    if (!config || !winst) {
        return BTA_StatusInvalidParameter;
    }

    if (!config->bltstreamFilename) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusConfigParamError, "BTAopen stream: Parameter bltstreamFilename missing");
        return BTA_StatusInvalidParameter;
    }

    if (!config->frameArrived && !config->frameArrivedEx && !config->frameArrivedEx2 && config->frameQueueMode == BTA_QueueModeDoNotQueue) {
        // No way to get frames without queueing or callback
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusConfigParamError, "BTAopen stream: Queueing and frameArrived callback are disabled");
        return BTA_StatusInvalidParameter;
    }

    winst->inst = calloc(1, sizeof(BTA_StreamLibInst));
    BTA_StreamLibInst *inst = (BTA_StreamLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusOutOfMemory;
    }

    inst->autoPlaybackSpeed = 0.0f;
    inst->frameIndexToSeek = -1;
    inst->frameIndex = -1;

    BTA_Status status;

    status = BVQinit(frameQueueInternalLength, BTA_QueueModeAvoidDrop, (FN_FreeItem)&freeFrameAndIndex, &inst->frameAndIndexQueueInst);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAopen stream: Not able to init internal frame queue");
        BTASTREAMclose(winst);
        return status;
    }

    inst->inputFilename = (uint8_t *)malloc(strlen((char *)config->bltstreamFilename) + 1);
    if (!inst->inputFilename) {
        BTASTREAMclose(winst);
        return BTA_StatusOutOfMemory;
    }
    strcpy((char *)inst->inputFilename, (char *)config->bltstreamFilename);

    //// Try to parse file with BTAdeserializeFrame in case it is not a stream but a single frame
    //status = BTAfopen((char*)inst->inputFilename, "r", &file);
    //if (status == BTA_StatusOk) {
    //    uint32_t filesize;
    //    status = BTAfseek(file, 0, BTA_SeekOriginEnd, &filesize);
    //    if (status == BTA_StatusOk) {
    //        status = BTAfseek(file, 0, BTA_SeekOriginBeginning, 0);
    //        if (status == BTA_StatusOk) {
    //            uint8_t* cdata = (unsigned char*)malloc(filesize);
    //            if (cdata) {
    //                uint32_t fileSizeRead;
    //                status = BTAfread(file, cdata, filesize, &fileSizeRead);
    //                if (status == BTA_StatusOk || fileSizeRead == filesize) {
    //                    BTA_Frame* frame;
    //                    status = BTAdeserializeFrame(&frame, cdata, &fileSizeRead);
    //                    if (status == BTA_StatusOk) {
    //                        // Yes, we were able to parse a frame!
    //                    }
    //                }
    //            }
    //        }
    //    }
    //    BTAfclose(file);
    //}

    void *file;
    status = BTAfLargeOpen((char *)inst->inputFilename, "r", &file);
    if (status != BTA_StatusOk) {
        uint8_t cwd[1024];
        BTAgetCwd(cwd, 1024);
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAopen stream: Not able to open file %s (working dir: %s)! error: %d", (char *)inst->inputFilename, cwd, getLastError());
        BTASTREAMclose(winst);
        return status;
    }

    char *line;
    char format[50], pon[50];
    while ((status = BTAfLargeReadLine(file, &line)) == BTA_StatusOk) {
        if (!strcmp(line, btaGrabSeparator)) {
            free(line);
            break;
        }
        sprintf(format, "%s%%s", btaGrabPonKey);
        if (sscanf(line, format, pon)) {
            inst->pon = (uint8_t *)malloc(strlen(pon) + 1);
            if (!inst->pon) {
                BTAfLargeClose(file);
                BTASTREAMclose(winst);
            }
            sprintf((char *)inst->pon, "%s", pon);
        }
        sprintf(format, "%s%%d", btaGrabVersionKey);
        sscanf(line, format, &inst->fileFormatVersion);
        sprintf(format, "%s%%x", btaGrabDeviceTypeKey);
        sscanf(line, format, &inst->deviceType);
        sprintf(format, "%s%%d", btaGrabSerialNumberKey);
        sscanf(line, format, &inst->serialNumber);
        sprintf(format, "%s%%d.%%d.%%d", btaGrabFirmwareVersionKey);
        sscanf(line, format, &inst->firmwareVersionMajor, &inst->firmwareVersionMinor, &inst->firmwareVersionNonFunc);
        sprintf(format, "%s%%d", btaGrabTotalFrameCountKey);
        sscanf(line, format, &inst->totalFrameCount);
        free(line);
    }
    if (status != BTA_StatusOk) {
        BTAfLargeClose(file);
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAopen stream: Error in BTAfLargeReadLine %d", getLastError());
        BTASTREAMclose(winst);
        return BTA_StatusInvalidParameter;
    }

    // The position in the file is right after the separator
    status = BTAfLargeTell(file, &inst->filePosMin);
    if (status != BTA_StatusOk) {
        BTAfLargeClose(file);
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAopen stream: Error in BTAfLargeTell 1 %d", getLastError());
        BTASTREAMclose(winst);
        return BTA_StatusRuntimeError;
    }
    status = BTAfLargeSeek(file, 0, BTA_SeekOriginEnd, &inst->filePosMax);
    if (status != BTA_StatusOk) {
        BTAfLargeClose(file);
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAopen stream: Error in BTAfLargeSeek 1 %d", getLastError());
        BTASTREAMclose(winst);
        return BTA_StatusRuntimeError;
    }
    BTAfLargeClose(file);

    if (inst->fileFormatVersion != 1 && inst->fileFormatVersion != 2) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusConfigParamError, "BTAopen stream: File format version unknown");
        BTASTREAMclose(winst);
        return BTA_StatusInvalidVersion;
    }

    // Now open in binary mode
    status = BTAfLargeOpen((char *)inst->inputFilename, "rb", &inst->file);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAopen stream: Not able to open file '%s' in binary mode! error: %d", (char *)inst->inputFilename, getLastError());
        BTASTREAMclose(winst);
        return BTA_StatusRuntimeError;
    }
    status = BTAfLargeSeek(inst->file, inst->filePosMin, BTA_SeekOriginBeginning, (uint64_t *)&inst->filePos);
    if (status != BTA_StatusOk) {
        BTAfLargeClose(inst->file);
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAopen stream: Error in BTAfLargeSeek 2 %d", getLastError());
        BTASTREAMclose(winst);
        return BTA_StatusRuntimeError;
    }

    status = BTAcreateThread(&(inst->parseThread), &streamRunFunction, (void *)winst);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAopen stream: Could not start parseThread");
        BTASTREAMclose(winst);
        return status;
    }

    return BTA_StatusOk;
}


BTA_Status BTASTREAMclose(BTA_WrapperInst *winst) {
    if (!winst) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAclose stream: winst missing!");
        return BTA_StatusInvalidParameter;
    }
    BTA_StreamLibInst *inst = (BTA_StreamLibInst *)winst->inst;
    if (!inst) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAclose stream: inst missing!");
        return BTA_StatusInvalidParameter;
    }
    inst->closing = 1;
    BTA_Status status = BTAjoinThread(inst->parseThread);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose stream: Failed to join parseThread");
    }
    BTAfLargeClose(inst->file);
    free(inst->inputFilename);
    inst->inputFilename = 0;

    status = BVQclose(&(inst->frameAndIndexQueueInst));
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose stream: Failed to close internal frame queue");
    }

    free(inst->pon);
    free(inst);
    winst->inst = 0;
    return BTA_StatusOk;
}


BTA_Status BTASTREAMgetDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_StreamLibInst *inst = (BTA_StreamLibInst *)winst->inst;
    BTA_DeviceInfo *deviceInfoTemp;
    if (!inst || ! deviceInfo) {
        return BTA_StatusInvalidParameter;
    }
    deviceInfoTemp = (BTA_DeviceInfo *)calloc(1, sizeof(BTA_DeviceInfo));
    if (!deviceInfoTemp) {
        return BTA_StatusOutOfMemory;
    }
    deviceInfoTemp->deviceType = inst->deviceType;
    if (inst->pon) {
        deviceInfoTemp->productOrderNumber = (uint8_t *)malloc(strlen((char *)inst->pon) + 1);
        if (!deviceInfoTemp->productOrderNumber) {
            free(deviceInfoTemp);
            deviceInfoTemp = 0;
            return BTA_StatusOutOfMemory;
        }
        sprintf((char *)deviceInfoTemp->productOrderNumber, "%s", inst->pon);
    }
    deviceInfoTemp->serialNumber = inst->serialNumber;
    deviceInfoTemp->firmwareVersionMajor = inst->firmwareVersionMajor;
    deviceInfoTemp->firmwareVersionMinor = inst->firmwareVersionMinor;
    deviceInfoTemp->firmwareVersionNonFunc = inst->firmwareVersionNonFunc;
    deviceInfoTemp->bltstreamFilename = (uint8_t*)malloc(strlen((char *)inst->inputFilename) + 1);
    if (!deviceInfoTemp->bltstreamFilename) {
        free(deviceInfoTemp);
        deviceInfoTemp = 0;
        return BTA_StatusOutOfMemory;
    }
    strcpy((char *)deviceInfoTemp->bltstreamFilename, (char*)inst->inputFilename);
    *deviceInfo = deviceInfoTemp;
    return BTA_StatusOk;
}


BTA_Status BTASTREAMgetDeviceType(BTA_WrapperInst *winst, BTA_DeviceType *deviceType) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_StreamLibInst *inst = (BTA_StreamLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    *deviceType = inst->deviceType;
    return BTA_StatusOk;
}


uint8_t BTASTREAMisRunning(BTA_WrapperInst *winst) {
    if (!winst) {
        return 0;
    }
    BTA_StreamLibInst *inst = (BTA_StreamLibInst *)winst->inst;
    if (!inst) {
        return 0;
    }
    //if (inst->parseThread) {
        return 1;
    //}
    //return 0;
}


uint8_t BTASTREAMisConnected(BTA_WrapperInst *winst) {
    return BTASTREAMisRunning(winst);
}



static BTA_Status getFrameFromFile(BTA_WrapperInst *winst, int32_t index, BTA_Frame **frame) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_StreamLibInst *inst = (BTA_StreamLibInst *)winst->inst;
    if (!inst || !frame) {
        return BTA_StatusInvalidParameter;
    }
    if (inst->bufferThread /*<-Auto playback enabled!*/) {
        assert(0);
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Stream: Auto playback enabled!");
        return BTA_StatusRuntimeError;
    }
    BTA_Status status;
    uint32_t frameHeaderLen = 0;
    uint32_t frameFooterLen = 4;
    if (inst->fileFormatVersion == 2) {
        frameHeaderLen = 4;
    }
    if (index < (int32_t)abs((int32_t)index - (int32_t)inst->frameIndexAtFilePos)) {
        // The new index in nearer to the beginning than to the current position
        status = BTAfLargeSeek(inst->file, inst->filePosMin, BTA_SeekOriginBeginning, &inst->filePos);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Error in BTAfLargeSeek 1 %d", getLastError());
            return status;
        }
        inst->frameIndexAtFilePos = 0;
    }

    while (index < inst->frameIndexAtFilePos) {
        // seek backward one index
        status = BTAfLargeSeek(inst->file, -(int64_t)frameFooterLen, BTA_SeekOriginCurrent, &inst->filePos);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Error in BTAfLargeSeek 2 %d", getLastError());
            return status;
        }
        uint32_t frameLen;
        status = BTAfLargeRead(inst->file, &frameLen, frameFooterLen, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Error in BTAfLargeRead 1 %d", getLastError());
            return status;
        }
        if (inst->fileFormatVersion == 1) {
            // v1 stores the frame length incl footer
            status = BTAfLargeSeek(inst->file, -(int64_t)frameLen, BTA_SeekOriginCurrent, &inst->filePos);
        }
        else {
            status = BTAfLargeSeek(inst->file, -(int64_t)frameFooterLen - (int64_t)frameLen - (int64_t)frameHeaderLen, BTA_SeekOriginCurrent, &inst->filePos);
        }
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Error in BTAfLargeSeek 3 %d", getLastError());
            return status;
        }
        inst->frameIndexAtFilePos--;
    }

    while (index > inst->frameIndexAtFilePos) {
        // seek forward one index
        if (inst->fileFormatVersion == 1) {
            // v1 has no header. Nothing to skip
            uint32_t bufferTempLen = 200000000;
            uint8_t *bufferTemp = (uint8_t *)malloc(bufferTempLen);
            if (!bufferTemp) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Stream: Can't allocate 1");
                return BTA_StatusOutOfMemory;
            }
            uint32_t bufferCount = 0;
            int32_t newPortionLen = 78000;
            while (1) {
                if (bufferCount + newPortionLen > bufferTempLen) {
                    free(bufferTemp);
                    bufferTemp = 0;
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Stream: Frame is huge %d", bufferTempLen);
                    return BTA_StatusOutOfMemory;
                }
                status = BTAfLargeRead(inst->file, bufferTemp + bufferCount, newPortionLen, 0);
                if (status != BTA_StatusOk) {
                    free(bufferTemp);
                    bufferTemp = 0;
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Error in BTAfLargeRead 2 %d", getLastError());
                    return status;
                }
                bufferCount += newPortionLen;
                BTA_Frame *frameTemp;
                uint32_t frameLen = bufferCount;
                status = BTAdeserializeFrame(&frameTemp, bufferTemp, &frameLen);
                if (status == BTA_StatusOutOfMemory) {
                    // Need more data from file
                    newPortionLen *= 2;
                    continue;
                }
                free(bufferTemp);
                bufferTemp = 0;
                if (status != BTA_StatusOk) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Unexpected error in BTAdeserializeFrame 1 %d", getLastError());
                    return status;
                }
                status = BTAfreeFrame(&frameTemp);
                if (status != BTA_StatusOk) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Error in BTAfreeFrame");
                    return status;
                }
                status = BTAfLargeSeek(inst->file, inst->filePos + frameLen + frameFooterLen, BTA_SeekOriginBeginning, &inst->filePos);
                if (status != BTA_StatusOk) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Error in BTAfLargeSeek 4 %d", getLastError());
                    return status;
                }
                break;
            }
        }
        if (inst->fileFormatVersion == 2) {
            uint32_t frameLen;
            status = BTAfLargeRead(inst->file, &frameLen, frameFooterLen, 0);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Error in BTAfLargeRead 3 %d", getLastError());
                return status;
            }
            status = BTAfLargeSeek(inst->file, frameLen + frameFooterLen, BTA_SeekOriginCurrent, &inst->filePos);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Error in BTAfLargeSeek 5 %d", getLastError());
                return status;
            }
        }
        inst->frameIndexAtFilePos++;
    }

    // The desired frame is ready to be parsed from filePos
    if (inst->fileFormatVersion == 1) {
        // v1 has no header. Nothing to skip
        uint32_t bufferTempLen = 200000000;
        uint8_t *bufferTemp = (uint8_t *)malloc(bufferTempLen);
        if (!bufferTemp) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Stream: Can't allocate 2");
            return BTA_StatusOutOfMemory;
        }
        uint32_t bufferCount = 0;
        uint32_t newPortionLen = 78000;
        while (1) {
            if (bufferCount + newPortionLen > bufferTempLen) {
                free(bufferTemp);
                bufferTemp = 0;
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Stream: Frame is huge %d", bufferTempLen);
                return BTA_StatusOutOfMemory;
            }
            status = BTAfLargeRead(inst->file, bufferTemp + bufferCount, newPortionLen, 0);
            if (status != BTA_StatusOk) {
                free(bufferTemp);
                bufferTemp = 0;
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Error in BTAfLargeRead 4 %d", getLastError());
                return status;
            }
            bufferCount += newPortionLen;
            uint32_t frameLen = bufferCount;
            status = BTAdeserializeFrame(frame, bufferTemp, &frameLen);
            if (status == BTA_StatusOutOfMemory) {
                // Need more data from file
                newPortionLen *= 2;
                continue;
            }
            free(bufferTemp);
            bufferTemp = 0;
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Unexpected error in BTAdeserializeFrame 2");
                return status;
            }
            status = BTAfLargeSeek(inst->file, inst->filePos + frameLen + frameFooterLen, BTA_SeekOriginBeginning, &inst->filePos);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Error in BTAfLargeSeek 6 %d", getLastError());
                return status;
            }
            break;
        }
    }
    if (inst->fileFormatVersion == 2) {
        uint32_t frameLen;
        status = BTAfLargeRead(inst->file, &frameLen, frameFooterLen, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Error in BTAfLargeRead 5 %d", getLastError());
            return status;
        }
        uint8_t *bufferTemp = (uint8_t *)malloc(frameLen);
        if (!bufferTemp) {
            free(bufferTemp);
            bufferTemp = 0;
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Stream: Can't allocate 3");
            return BTA_StatusOutOfMemory;
        }
        status = BTAfLargeRead(inst->file, bufferTemp, frameLen, 0);
        if (status != BTA_StatusOk) {
            free(bufferTemp);
            bufferTemp = 0;
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Error in BTAfLargeRead 6 %d", getLastError());
            return status;
        }
        status = BTAdeserializeFrame(frame, bufferTemp, &frameLen);
        free(bufferTemp);
        bufferTemp = 0;
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Error in BTAdeserializeFrame 3");
            return status;
        }
        status = BTAfLargeSeek(inst->file, frameFooterLen, BTA_SeekOriginCurrent, &inst->filePos);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Error in BTAfLargeSeek 7 %d", getLastError());
            return status;
        }
    }
    inst->frameIndexAtFilePos++;
    return BTA_StatusOk;
}


static void *bufferRunFunction(void *handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return 0;
    }
    BTA_StreamLibInst *inst = (BTA_StreamLibInst *)winst->inst;
    if (!inst) {
        return 0;
    }

    BTA_Status status;
    const uint32_t bufferLenMax = 100000000;
    uint32_t bufferLen = 0;
    uint8_t *buffer = (uint8_t *)malloc(bufferLenMax);
    if (!buffer) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Stream: BufferRunFunction could not allocate 4");
        return 0;
    }
    uint32_t bufferPos = 0;
    uint64_t filePos;
    status = BTAfLargeSeek(inst->file, inst->filePos, BTA_SeekOriginBeginning, &filePos);
    if (status != BTA_StatusOk) {
        free(buffer);
        buffer = 0;
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: BufferRunFunction could not fseek 1 %d", getLastError());
        return 0;
    }

    uint32_t frameHeaderLen = 0;
    uint32_t frameFooterLen = 4;
    if (inst->fileFormatVersion == 2) {
        frameHeaderLen = 4;
    }
    BTA_Frame *frame = 0;
    while (!inst->abortBufferThread) {
        float autoPlaybackSpeed = inst->autoPlaybackSpeed;
        if (autoPlaybackSpeed > 0) {
            uint32_t frameSerializedLen = 0;
            if (bufferPos >= bufferLen) {
                status = BTA_StatusOutOfMemory;
            }
            else {
                frameSerializedLen = bufferLen - bufferPos - frameHeaderLen;
                status = BTAdeserializeFrame(&frame, &buffer[bufferPos + frameHeaderLen], &frameSerializedLen);
            }

            if (status == BTA_StatusOk) {
                FrameAndIndex *frameAndIndex = (FrameAndIndex *)malloc(sizeof(FrameAndIndex));
                if (!frameAndIndex) {
                    BTAfreeFrame(&frame);
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: BufferRunFunction could not allocate 5 %d", getLastError());
                    break;
                }
                frameAndIndex->index = inst->frameIndexAtFilePos;
                frameAndIndex->frame = frame;
                // Frame was deserialized, set filePos to next frame
                bufferPos += frameHeaderLen + frameSerializedLen + frameFooterLen;
                // adjust filePos and frameIndexAtFilePos to current frame, seek to this position when thread ends
                inst->filePos += frameHeaderLen + frameSerializedLen + frameFooterLen;
                inst->frameIndexAtFilePos++;
                while (1) {
                    if (inst->abortBufferThread) {
                        freeFrameAndIndex(&frameAndIndex);
                        break;
                    }
                    status = BVQenqueue(inst->frameAndIndexQueueInst, frameAndIndex);
                    if (status == BTA_StatusOk) {
                        break;
                    }
                    if (status == BTA_StatusOutOfMemory) {
                        // Try again
                        BTAmsleep(20);
                        continue;
                    }
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: BufferRunFunction error, can't enqueue internally");
                    freeFrameAndIndex(&frameAndIndex);                    
                    break;
                }
            }
            else if (status == BTA_StatusOutOfMemory) {
                // We want more bytes in buffer (reading from file)
                if (filePos == inst->filePosMax) {
                    inst->endReached = 1;
                    break;
                }
                // Go back # of bytes that are still in the buffer (gonna read them again)
                status = BTAfLargeSeek(inst->file, -((int64_t)bufferLen - (int64_t)bufferPos), BTA_SeekOriginCurrent, &filePos);
                if (status != BTA_StatusOk) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: BufferRunFunction could not fseek 2 %d", getLastError());
                    break;
                }
                uint32_t temp;
                status = BTAfLargeRead(inst->file, buffer, bufferLenMax, &temp);
                if (status != BTA_StatusOk && status != BTA_StatusOutOfMemory) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: BufferRunFunction error, can't BTAfLargeRead 7 %d", getLastError());
                    break;
                }
                bufferPos = 0;
                bufferLen = temp;
                filePos += temp;
                continue;
            }
            else {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: BufferRunFunction: Error deserializing frame");
                //// Frame was NOT deserialized, set filePos to next frame and enqueue it
                //uint32_t frameLen = *((uint32_t *)(buffer + bufferPos));
                //bufferPos += frameHeaderLen + frameLen + frameFooterLen;
                //// adjust filePos and frameIndexAtFilePos to current frame, seek to this position when thread ends
                //inst->filePos += frameHeaderLen + frameLen + frameFooterLen;
                //inst->frameIndexAtFilePos++;
                break;
            }
        }
        else if (autoPlaybackSpeed < 0) {
            if (bufferPos < (int32_t)frameFooterLen) {
                status = BTA_StatusOutOfMemory;
            }
            else {
                uint32_t frameLen = *((uint32_t *)(buffer + bufferPos - frameFooterLen));
                if (bufferPos < (int32_t)(frameLen + frameHeaderLen + frameFooterLen)) {
                    status = BTA_StatusOutOfMemory;
                }
                else {
                    // Set filePos to previous frame
                    bufferPos -= frameLen + frameHeaderLen + frameFooterLen;
                    // adjust filePos and frameIndexAtFilePos to current frame, seek to this position when thread ends
                    inst->filePos -= frameLen + frameHeaderLen + frameFooterLen;
                    inst->frameIndexAtFilePos--;
                    uint32_t frameSerializedLen = bufferLen - bufferPos - frameHeaderLen;
                    status = BTAdeserializeFrame(&frame, &buffer[bufferPos + frameHeaderLen], &frameSerializedLen);
                }
            }
            if (status == BTA_StatusOk) {
                FrameAndIndex *frameAndIndex = (FrameAndIndex *)malloc(sizeof(FrameAndIndex));
                if (!frameAndIndex) {
                    BTAfreeFrame(&frame);
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: BufferRunFunction could not allocate 5 %d", getLastError());
                    break;
                }
                frameAndIndex->index = inst->frameIndexAtFilePos;
                frameAndIndex->frame = frame;
                while (1) {
                    if (inst->abortBufferThread) {
                        BTAfreeFrame(&frame);
                        break;
                    }
                    status = BVQenqueue(inst->frameAndIndexQueueInst, frameAndIndex);
                    if (status == BTA_StatusOutOfMemory) {
                        // Try again
                        BTAmsleep(20);
                        continue;
                    }
                    if (status != BTA_StatusOk) {
                        freeFrameAndIndex(&frameAndIndex);
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: BufferRunFunction error, can't enqueue internally");
                    }
                    break;
                }
            }
            else if (status == BTA_StatusOutOfMemory) {
                // (filePos is the index where buffer ends
                // We want more bytes in buffer (reading from file)
                if (filePos - bufferLen <= inst->filePosMin) {
                    // last time we read from filePosMin
                    inst->endReached = 1;
                    break;
                }

                // Go back bufferLenMax minus bufferPos so that  (gonna read them again)
                uint64_t filePosToReadFrom;
                if ((int64_t)filePos - bufferLen - bufferLenMax + bufferPos < (int64_t)inst->filePosMin) {
                    filePosToReadFrom = inst->filePosMin;
                }
                else {
                    filePosToReadFrom = filePos - bufferLen - bufferLenMax + bufferPos;
                }
                uint32_t bytesToRead = (uint32_t)(filePos - bufferLen + bufferPos - filePosToReadFrom);
                status = BTAfLargeSeek(inst->file, filePosToReadFrom, BTA_SeekOriginBeginning, &filePos);
                if (status != BTA_StatusOk) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: BufferRunFunction could not fseek 2 %d", getLastError());
                    break;
                }
                uint32_t temp2;
                status = BTAfLargeRead(inst->file, buffer, bytesToRead, &temp2);
                if (status != BTA_StatusOk && status != BTA_StatusOutOfMemory) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: BufferRunFunction error, can't BTAfLargeRead 8 %d", getLastError());
                    break;
                }
                bufferPos = temp2;
                bufferLen = temp2;
                filePos += temp2;
                continue;
            }
            else {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: BufferRunFunction: Error deserializing frame");
                break;
            }
        }
        else {
            break;
        }
    }
    free(buffer);
    buffer = 0;
    // Reset filePos to last known position
    status = BTAfLargeSeek(inst->file, inst->filePos, BTA_SeekOriginBeginning, 0);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: BufferRunFunction could not fseek 3");
        return 0;
    }
    return 0;
}


static void *streamRunFunction(void *handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return 0;
    }
    BTA_StreamLibInst *inst = (BTA_StreamLibInst *)winst->inst;
    if (!inst) {
        return 0;
    }

    BTA_Status status;
    int8_t direction = 0;
    while (!inst->closing) {

        if (inst->autoPlaybackSpeed != 0) {
            if ((direction < 0 && inst->autoPlaybackSpeed > 0) || (direction > 0 && inst->autoPlaybackSpeed < 0)) {
                // change of playback direction, clear buffer
                BVQclear(inst->frameAndIndexQueueInst);
                // get a frame (this function sets filePos and frameIndexAtFilePos to the desired position)
                BTA_Frame *frame;
                status = getFrameFromFile(winst, inst->frameIndex, &frame);
                BTAfreeFrame(&frame);
                if (status != BTA_StatusOk) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: RunFunction can't change direction");
                    return 0;
                }
            }
            if (inst->autoPlaybackSpeed > 0) {
                direction = 1;
            }
            else {
                direction = -1;
            }
            inst->endReached = 0;
            uint32_t frameAndIndexQueueCount = BVQgetCount(inst->frameAndIndexQueueInst);
            //if (!frameAndIndexQueueCount) {
            //    // only buffer if the queue has to be filled from scratch
            //    BTAinfoEventHelper(winst->infoEventInst, IMPORTANCE_INFO, BTA_StatusInformation, "Stream: Buffering...");
            //}
            // start bufferThread
            inst->abortBufferThread = 0;
            status = BTAcreateThread(&inst->bufferThread, &bufferRunFunction, (void *)winst);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: RunFunction could not start bufferThread");
                return 0;
            }
            if (!frameAndIndexQueueCount) {
                // allow buffer to fill a bit if the queue has to be filled from scratch
                BTAmsleep(250);
            }

            //BTAinfoEventHelper(winst->infoEventInst, IMPORTANCE_INFO, BTA_EventIdInformation, "StreamThread: Stream starts");
            uint64_t timeTriggerPrev = BTAgetTickCount64();
            uint32_t timestampPrev = 0;
            while (inst->autoPlaybackSpeed != 0 && !inst->closing) {
                FrameAndIndex *frameAndIndex;
                status = BVQpeek(inst->frameAndIndexQueueInst, (void **)&frameAndIndex, 200);
                if (status != BTA_StatusOk) {
                    if (inst->endReached) {
                        inst->autoPlaybackSpeed = 0.0f;
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Stream: End of stream reached");
                    }
                    if (status == BTA_StatusTimeOut) {
                        BTAmsleep(5);
                        continue;
                    }
                    // error
                    inst->autoPlaybackSpeed = 0.0f;
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: Error dequeueing frameAndIndex");
                    continue;
                }
                uint32_t timeStamp = frameAndIndex->frame->timeStamp / 1000;
                if (!timestampPrev) {
                    // Suppose it is uninitialized
                    timestampPrev = timeStamp;
                }
                uint32_t timestampDiff;
                if (direction > 0) {
                    timestampDiff = timeStamp - timestampPrev;
                }
                else {
                    timestampDiff = timestampPrev - timeStamp;
                }
                // The case of timestamp overrun: no way to surely know if the device overruns at 0xffffffff or earlier
                // Workaround: a difference higher than 30 minutes is an overrun, assume 1ms delay between the two frames
                if (timestampDiff > 30*60*1000) {
                    timestampDiff = 1;
                }
                // The frames timestamp implies the framerate. calc waiting time and wait
                while (inst->autoPlaybackSpeed != 0 && !inst->closing) {
                    BTAmsleep(1);
                    float autoPlaybackSpeed = inst->autoPlaybackSpeed > 0 ? inst->autoPlaybackSpeed : -inst->autoPlaybackSpeed;
                    if (autoPlaybackSpeed == 0) {
                        break;
                    }
                    uint64_t timeTrigger = timeTriggerPrev + (uint64_t)(timestampDiff / autoPlaybackSpeed);
                    if (BTAgetTickCount64() >= timeTrigger) {
                        BVQdequeue(inst->frameAndIndexQueueInst, 0, 0);
                        BTApostprocessGrabCallbackEnqueue(winst, frameAndIndex->frame);
                        inst->frameIndex = frameAndIndex->index;
                        // Frame is still in use, only free struct:
                        free(frameAndIndex);
                        timeTriggerPrev = timeTrigger;
                        timestampPrev = timeStamp;
                        break;
                    }
                }
            }
            // stop the bufferThread
            inst->abortBufferThread = 1;
            status = BTAjoinThread(inst->bufferThread);
            inst->bufferThread = 0;
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: RunFunction failed to join bufferThread");
            }
        }

        while (inst->autoPlaybackSpeed == 0 && !inst->closing) {
            if (inst->frameIndexToSeek < 0) {
                BTAmsleep(22);
                continue;
            }
            BVQclear(inst->frameAndIndexQueueInst);
            BTA_Frame *frame;
            status = getFrameFromFile(winst, inst->frameIndexToSeek, &frame);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "Stream: getFrameFromFile failed %d", inst->frameIndexToSeek);
                inst->frameIndexToSeek = -1;
                continue;
            }
            BTApostprocessGrabCallbackEnqueue(winst, frame);
            inst->frameIndex = inst->frameIndexToSeek;
            inst->frameIndexToSeek = -1;
        }
    }
    return 0;
}


static BTA_Status freeFrameAndIndex(FrameAndIndex **frameAndIndex) {
    if (!frameAndIndex) {
        return BTA_StatusInvalidParameter;
    }
    if (!*frameAndIndex) {
        return BTA_StatusOk;
    }
    BTA_Status status = BTAfreeFrame(&((*frameAndIndex)->frame));
    free(*frameAndIndex);
    *frameAndIndex = 0;
    return status;
}


BTA_Status BTASTREAMsetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float value) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_StreamLibInst *inst = (BTA_StreamLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    switch (libParam) {
    case BTA_LibParamStreamTotalFrameCount:
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusIllegalOperation, "BTAsetLibParam: read only");
        return BTA_StatusIllegalOperation;
    case BTA_LibParamStreamAutoPlaybackSpeed:
        if ((inst->autoPlaybackSpeed >= 0 && value >= 0) || (inst->autoPlaybackSpeed <= 0 && value <= 0)) {
            // no change of playback direction
            inst->autoPlaybackSpeed = value;
        }
        else {
            // change of playback direction
            inst->autoPlaybackSpeed = 0;
            while (inst->bufferThread) {
                BTAmsleep(5);
            }
            inst->autoPlaybackSpeed = value;
        }
        return BTA_StatusOk;
    case BTA_LibParamStreamPos:
        if (inst->frameIndexToSeek >= 0) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusIllegalOperation, "BTAsetLibParam: Seek still in progress");
            return BTA_StatusIllegalOperation;
        }
        inst->autoPlaybackSpeed = 0;
        inst->frameIndexToSeek = (int32_t)value;
        while (inst->frameIndexToSeek >= 0) {
            BTAmsleep(5);
        }
        return BTA_StatusOk;
    case BTA_LibParamStreamPosIncrement: {
        if (inst->frameIndexToSeek >= 0) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusIllegalOperation, "BTAsetLibParam: Seek still in progress");
            return BTA_StatusIllegalOperation;
        }
        inst->autoPlaybackSpeed = 0;
        int32_t frameIndexToSeek = (int32_t)inst->frameIndex + (int32_t)value;
        if (frameIndexToSeek < 0) frameIndexToSeek = 0;
        inst->frameIndexToSeek = frameIndexToSeek;
        while (inst->frameIndexToSeek >= 0) {
            BTAmsleep(5);
        }
        return BTA_StatusOk;
    }
    default:
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAsetLibParam: LibParam %s not supported", BTAlibParamToString(libParam));
        return BTA_StatusNotSupported;
    }
}


BTA_Status BTASTREAMgetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float *value) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_StreamLibInst *inst = (BTA_StreamLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    switch (libParam) {
    case BTA_LibParamStreamTotalFrameCount:
        *value = (float)inst->totalFrameCount;
        return BTA_StatusOk;
    case BTA_LibParamStreamAutoPlaybackSpeed:
        *value = (float)inst->autoPlaybackSpeed;
        return BTA_StatusOk;
    case BTA_LibParamStreamPos:
        if (inst->frameIndexToSeek >= 0) {
            *value = -1;
            return BTA_StatusOk;
        }
        *value = (float)inst->frameIndex;
        return BTA_StatusOk;
    case BTA_LibParamStreamPosIncrement:
        return BTA_StatusIllegalOperation;
    default:
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAgetLibParam: LibParam %s not supported", BTAlibParamToString(libParam));
        return BTA_StatusNotSupported;
    }
}


BTA_Status BTASTREAMsetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode frameMode) {
    return BTA_StatusNotSupported;
}
BTA_Status BTASTREAMgetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode *frameMode) {
    *frameMode = BTA_FrameModeCurrentConfig;
    return BTA_StatusNotSupported;
}
BTA_Status BTASTREAMsendReset(BTA_WrapperInst *winst) {
    return BTA_StatusNotSupported;
}
BTA_Status BTASTREAMgetIntegrationTime(BTA_WrapperInst *winst, uint32_t *integrationTime) {
    return BTA_StatusNotSupported;
}
BTA_Status BTASTREAMsetIntegrationTime(BTA_WrapperInst *winst, uint32_t integrationTime) {
    return BTA_StatusNotSupported;
}
BTA_Status BTASTREAMgetFrameRate(BTA_WrapperInst *winst, float *frameRate) {
    return BTA_StatusNotSupported;
}
BTA_Status BTASTREAMsetFrameRate(BTA_WrapperInst *winst, float frameRate) {
    return BTA_StatusNotSupported;
}
BTA_Status BTASTREAMsetModulationFrequency(BTA_WrapperInst *winst, uint32_t modulationFrequency) {
    return BTA_StatusNotSupported;
}
BTA_Status BTASTREAMgetModulationFrequency(BTA_WrapperInst *winst, uint32_t *modulationFrequency) {
    return BTA_StatusNotSupported;
}
BTA_Status BTASTREAMgetGlobalOffset(BTA_WrapperInst *winst, float *offset) {
    return BTA_StatusNotSupported;
}
BTA_Status BTASTREAMsetGlobalOffset(BTA_WrapperInst *winst, float offset) {
    return BTA_StatusNotSupported;
}
BTA_Status BTASTREAMwriteCurrentConfigToNvm(BTA_WrapperInst *winst) {
    return BTA_StatusNotSupported;
}
BTA_Status BTASTREAMrestoreDefaultConfig(BTA_WrapperInst *winst) {
    return BTA_StatusNotSupported;
}
BTA_Status BTASTREAMreadRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount) {
    return BTA_StatusNotSupported;
}
BTA_Status BTASTREAMwriteRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount) {
    return BTA_StatusNotSupported;
}
BTA_Status BTASTREAMflashUpdate(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport) {
    return BTA_StatusNotSupported;
}
BTA_Status BTASTREAMflashRead(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport, uint8_t quiet) {
    return BTA_StatusNotSupported;
}

#endif