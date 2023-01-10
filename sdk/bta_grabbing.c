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



static const int32_t grabbingQueueLength = 250000000;
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
/////////////////////////////////////////////////////////////////////////////////



BTA_Status BGRBinit(BTA_GrabbingConfig *config, uint8_t *libNameVer, BTA_DeviceInfo *deviceInfo, BTA_GrabInst **instPtr, BTA_InfoEventInst *infoEventInst) {
    int result;
    char msg[1000];
    time_t now;
    void *file;
    BTA_GrabInst *inst;
    if (!instPtr || !config) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BRGBinit: Parameter instPtr or config missing");
        return BTA_StatusInvalidParameter;
    }
    if (*instPtr && (*instPtr)->grabbingEnabled) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusIllegalOperation, "BTAstartGrabbing: Grabbing is already in progress");
        return BTA_StatusIllegalOperation;
    }
    if (!config->filename) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BRGBinit: Parameter filename missing");
        return BTA_StatusInvalidParameter;
    }
    BTA_Status status = BTAfLargeOpen((char *)config->filename, "w", &file);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAstartGrabbing: Not able to open file");
        return BTA_StatusInvalidParameter;
    }
    msg[0] = 0;
    now = time(0);
    strftime(msg + strlen(msg), 100, "%Y-%m-%d %H:%M:%S\n", localtime(&now));
    sprintf(msg + strlen(msg), "%s%d\n", btaGrabVersionKey, btaGrabVersion);
    sprintf(msg + strlen(msg), "Grabbed with %s\n", libNameVer);
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

    inst = (BTA_GrabInst *)malloc(sizeof(BTA_GrabInst));
    if (!inst) {
        return BTA_StatusOutOfMemory;
    }

    inst->totalFrameCount = 0;
    inst->infoEventInst = infoEventInst;

    inst->grabbingFilename = (uint8_t *)malloc(strlen((char *)config->filename) + 1);
    if (!inst->grabbingFilename) {
        free(inst);
        inst = 0;
        return BTA_StatusOutOfMemory;
    }

    strcpy((char *)inst->grabbingFilename, (char *)config->filename);
    result = BTAcreateThread(&(inst->grabbingThread), &grabRunFunction, (void *)inst, 0);
    if (result != 0) {
        BTAcloseMutex(inst->grabbingQueueMutex);
        free(inst->grabbingQueue);
        inst->grabbingQueue = 0;
        free(inst->grabbingFilename);
        inst->grabbingFilename = 0;
        free(inst);
        inst = 0;
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAstartGrabbing: Could not start grabbingThread, error: %d", result);
        return BTA_StatusRuntimeError;
    }

    BTAinfoEventHelper(infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Grabbing: Grabbing started");
    *instPtr = inst;
    return BTA_StatusOk;
}


BTA_Status BGRBgrab(BTA_GrabInst *inst, BTA_Frame *frame) {
    uint8_t *frameSerialized;
    //if (!inst || !frame) {
    //    return BTA_StatusInvalidParameter;
    //}
    assert(inst);
    assert(frame);
    if (!inst->grabbingEnabled) {
        // Grabbing instance is closing
        return BTA_StatusOk;
    }
    int32_t frameserializedLen;
    BTA_Status status = BTAgetSerializedLength(frame, (uint32_t *)&frameserializedLen);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "grabbing: Error in BTAgetSerializedLength");
        return status;
    }
    if (sizeof(frameserializedLen) != 4) {
        assert(0);
    }
    // We want to store the whole serialized frame and two times its length
    frameSerialized = (uint8_t *)malloc(frameserializedLen + 2 * sizeof(frameserializedLen));
    if (!frameSerialized) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Grabbing: Cannot allocate");
        return BTA_StatusOutOfMemory;
    }
    frameSerialized[0] = (uint8_t)(frameserializedLen >> 0);
    frameSerialized[1] = (uint8_t)(frameserializedLen >> 8);
    frameSerialized[2] = (uint8_t)(frameserializedLen >> 16);
    frameSerialized[3] = (uint8_t)(frameserializedLen >> 24);
    int32_t frameserializedLenTemp = frameserializedLen;
    status = BTAserializeFrame(frame, frameSerialized + sizeof(frameserializedLen), (uint32_t *)&frameserializedLenTemp);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "Grabbing: Error in BTAserializeFrame");
        return status;
    }
    if (frameserializedLenTemp != frameserializedLen) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Grabbing: Error, BTAserializeFrame result %d differs from BTAgetSerializedLength %d", frameserializedLenTemp, frameserializedLen);
        return BTA_StatusRuntimeError;
    }
    frameSerialized[frameserializedLen + sizeof(frameserializedLen) + 0] = (uint8_t)(frameserializedLen >> 0);
    frameSerialized[frameserializedLen + sizeof(frameserializedLen) + 1] = (uint8_t)(frameserializedLen >> 8);
    frameSerialized[frameserializedLen + sizeof(frameserializedLen) + 2] = (uint8_t)(frameserializedLen >> 16);
    frameSerialized[frameserializedLen + sizeof(frameserializedLen) + 3] = (uint8_t)(frameserializedLen >> 24);
    // The frameserialized is now complete with header, frame, footer. Its real lengthmust be set:
    frameserializedLen += 2 * sizeof(frameserializedLen);
    // memcpy frameSerialized into queue, if we can
    if (inst->grabbingEnabled) {
        BTAlockMutex(inst->grabbingQueueMutex);
        if (inst->grabbingQueueCount + frameserializedLen <= grabbingQueueLength) {
            // there is enough space
            int32_t portion1 = MTHmin((grabbingQueueLength - inst->grabbingQueuePos), frameserializedLen);
            int32_t portion2 = frameserializedLen - portion1;
            memcpy(inst->grabbingQueue + inst->grabbingQueuePos, frameSerialized, portion1);
            inst->grabbingQueuePos += portion1;
            inst->grabbingQueueCount += portion1;
            if (inst->grabbingQueuePos == grabbingQueueLength) {
                inst->grabbingQueuePos = 0;
            }
            if (portion2) {
                memcpy(inst->grabbingQueue + inst->grabbingQueuePos, frameSerialized + portion1, portion2);
                inst->grabbingQueuePos += portion2;
                inst->grabbingQueueCount += portion2;
            }
            inst->totalFrameCount++;
        }
        else {
            BTAinfoEventHelper(inst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "Grabbing: buffer full, dropping a frame");
        }
        BTAunlockMutex(inst->grabbingQueueMutex);
    }
    free(frameSerialized);
    frameSerialized = 0;
    return BTA_StatusOk;
}


static void *grabRunFunction(void *handle) {
    BTA_Status status;
    BTA_GrabInst *inst = (BTA_GrabInst *)handle;
    void *file;
    inst->grabbingQueue = (uint8_t *)malloc(grabbingQueueLength);
    if (!inst->grabbingQueue) {
        free(inst->grabbingFilename);
        inst->grabbingFilename = 0;
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Grabbing: RunFunction failed to allocate");
        return 0;
    }
    status = BTAinitMutex(&inst->grabbingQueueMutex);
    if (status != BTA_StatusOk) {
        free(inst->grabbingQueue);
        inst->grabbingQueue = 0;
        free(inst->grabbingFilename);
        inst->grabbingFilename = 0;
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "Grabbing: RunFunction failed to init grabbingQueueMutex");
        return 0;
    }
    inst->grabbingQueueCount = 0;
    inst->grabbingQueuePos = 0;
    inst->grabbingEnabled = 1;
    while (1) {
        uint8_t *buffer = 0;
        int32_t bufferLen = 0;
        BTAlockMutex(inst->grabbingQueueMutex);
        if (inst->grabbingQueueCount) {
            // Data to write to disk is available
            int32_t index = inst->grabbingQueuePos - inst->grabbingQueueCount;
            if (index < 0) {
                index += grabbingQueueLength;
            }
            // Length is QueueCount or only until end of ringbuffer only
            bufferLen = MTHmin(inst->grabbingQueueCount, grabbingQueueLength - index);
            buffer = (uint8_t *)malloc(bufferLen);
            memcpy(buffer, inst->grabbingQueue + index, bufferLen);
            inst->grabbingQueueCount -= bufferLen;
        }
        else if (!inst->grabbingEnabled) {
            // Nothing in queue and grabbing disabled -> We're done
            BTAunlockMutex(inst->grabbingQueueMutex);
            break;
        }
        BTAunlockMutex(inst->grabbingQueueMutex);

        if (buffer && inst->grabbingFilename) {
            while (1) {
                BTA_Status status = BTAfLargeOpen((char *)inst->grabbingFilename, "ab", &file);
                if (status == BTA_StatusOk) {
                    BTAfLargeWrite(file, buffer, bufferLen, 0);
                    BTAfLargeClose(file);
                    break;
                }
                else {
                    BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "Grabbing: RunFunction failed to open bltstream file for writing");
                    BTAmsleep(222);
                }
                if (!inst->grabbingEnabled) {
                    break;
                }
            }
            free(buffer);
            buffer = 0;
        }
        BTAmsleep(22);
    }

    status = BTAcloseMutex(inst->grabbingQueueMutex);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "Grabbing: RunFunction failed to close grabbingQueueMutex");
        return 0;
    }
    inst->grabbingQueueMutex = 0;
    free(inst->grabbingQueue);
    inst->grabbingQueue = 0;
    return 0;
}


BTA_Status BGRBclose(BTA_GrabInst **instPtr) {
    BTA_Status status;
    if (!instPtr) {
        return BTA_StatusInvalidParameter;
    }
    if (!(*instPtr)) {
        // No grabber open, nothing to close
        return BTA_StatusOk;
    }
    (*instPtr)->grabbingEnabled = 0;
    BTAinfoEventHelper((*instPtr)->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Stopping grabbing");
    status = BTAjoinThread((*instPtr)->grabbingThread);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper((*instPtr)->infoEventInst, VERBOSE_ERROR, status, "Grabbing: Failed to join grabbingThread");
        return status;
    }
    BTA_GrabInst *inst = *instPtr;
    *instPtr = 0;

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
                if (BTAstartsWith(btaGrabTotalFrameCountKey, line)) {
                    free(line);
                    char msg[100];
                    msg[0] = 0;
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
    BTAinfoEventHelper(inst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Grabbing stopped");
    free(inst->grabbingFilename);
    inst->grabbingFilename = 0;
    free(inst);
    inst = 0;
    return BTA_StatusOk;
}


