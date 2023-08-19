#ifndef BTA_WO_UART

#include <bta.h>
#include "bta_helper.h"
#include <bta_flash_update.h>
#include <bta_frame.h>
#include <bta_status.h>

#include "bta_uart.h"
#include "bta_uart_helper.h"
#include <bta_oshelper.h>
#include <timing_helper.h>
#include <pthread_helper.h>
#include "configuration.h"
#include <uart_helper.h>

#if defined PLAT_LINUX || defined PLAT_APPLE
    #include <netdb.h>
    #include <errno.h>
#endif

#include <string.h>
//#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <crc7.h>
#include <crc16.h>


struct BTA_InfoEventInst;

/////////// Local prototypes
static void *readFramesRunFunction(void *handle);
//static BTA_Status toBltTofApiStatus(uint8_t status);
//static BTA_Status flashStatusToBltTofApiStatus(uint8_t status);
static BTA_Status toByteStream(BTA_WrapperInst *winst, BTA_UartCommand cmd, BTA_UartFlashCommand subCmd, uint32_t addr, uint8_t *data, uint32_t length, uint8_t **resultPtr, uint32_t *resultLen);
static BTA_Status readFrame(BTA_WrapperInst *winst, uint8_t **dataPtr, uint32_t *dataLen, int timeout);
static BTA_Status parseFrame(BTA_WrapperInst *winst, uint8_t *data, uint32_t dataLen, BTA_Frame **framePtr);
//static BTA_Status sendKeepAliveMsg(BTA_WrapperInst *winst);
static int getLastError(void);
//////////////////////////////////////////////////////////////////////////////////




BTA_Status BTAUARTopen(BTA_Config *config, BTA_WrapperInst *winst) {

    if (!config || !winst) {
        return BTA_StatusInvalidParameter;
    }

    if (!config->uartPortName) {
        return BTA_StatusInvalidParameter;
    }

    winst->inst = calloc(1, sizeof(BTA_UartLibInst));
    BTA_UartLibInst *inst = (BTA_UartLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusOutOfMemory;
    }
    inst->serialHandle = (void *)-1;

    if (!config->uartPortName) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen uart: No port name provided");
        BTAUARTclose(winst);
        return BTA_StatusInvalidParameter;
    }

    inst->uartTransmitterAddress = config->uartTransmitterAddress;
    inst->uartReceiverAddress = config->uartReceiverAddress;

    BTA_Status status = BTAinitMutex(&inst->handleMutex);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen uart: Cannot create handleMutex");
        BTAUARTclose(winst);
        return status;
    }

    if (config->uartReceiverAddress == config->uartTransmitterAddress) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen uart: Receiver and transmitter adresses must not equal");
        return BTA_StatusInvalidParameter;
    }

    status = UARTHLPserialOpenByName((const char *)config->uartPortName, config->uartBaudRate, config->uartDataBits, config->uartStopBits, config->uartParity, &inst->serialHandle);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen uart: Could not open serial port");
        BTAUARTclose(winst);
        return status;
    }

    status = BTAcreateThread(&(inst->readFramesThread), &readFramesRunFunction, (void *)inst);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen uart: Could not start readFramesThread");
        BTAUARTclose(winst);
        return status;
    }

    return BTA_StatusOk;
}


BTA_Status BTAUARTclose(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UartLibInst *inst = (BTA_UartLibInst *)winst->inst;
    if (!inst) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAclose UART: inst missing!");
        return BTA_StatusInvalidParameter;
    }
    inst->closing = 1;
    // TODO
    BTAlockMutex(inst->handleMutex);
    BTAunlockMutex(inst->handleMutex);

    BTA_Status status = BTAjoinThread(inst->readFramesThread);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose UART: Failed to join readFramesThread");
    }

    status = BGRBclose(&(winst->grabInst));
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose UART: Failed to close grabber");
    }

    status = UARTHLPserialClose(inst->serialHandle);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose UART: Failed to close serial handle");
    }
    status = BTAcloseMutex(inst->handleMutex);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose UART: Failed to close handleMutex");
    }
    free(inst);
    inst = 0;
    return BTA_StatusOk;
}


BTA_Status BTAUARTgetDeviceType(BTA_WrapperInst *winst, BTA_DeviceType *deviceType) {
    if (!winst || !deviceType) {
        return BTA_StatusInvalidParameter;
    }
    BTA_DeviceInfo *deviceInfo;
    BTA_Status status = BTAUARTgetDeviceInfo(winst, &deviceInfo);
    if (status != BTA_StatusOk) {
        return status;
    }
    *deviceType = deviceInfo->deviceType;
    BTAfreeDeviceInfo(deviceInfo);
    return BTA_StatusOk;
}


BTA_Status BTAUARTgetDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo) {
    BTA_Status status;
    BTA_DeviceInfo *info;
    uint32_t reg;
    if (!winst || !deviceInfo) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UartLibInst *inst = (BTA_UartLibInst *)winst->inst;
    *deviceInfo = 0;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }

    info = (BTA_DeviceInfo *)calloc(1, sizeof(BTA_DeviceInfo));
    if (!info) {
        return BTA_StatusOutOfMemory;
    }
    status = BTAUARTreadRegister(winst, BTA_UartRegAddrDeviceType, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->deviceType = (BTA_DeviceType)reg;
    if (info->deviceType == 0) {
        info->deviceType = BTA_DeviceTypeUart;
    }

    status = BTAUARTreadRegister(winst, BTA_UartRegAddrSerialNrLowWord, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->serialNumber |= reg;
    status = BTAUARTreadRegister(winst, BTA_UartRegAddrSerialNrHighWord, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->serialNumber |= reg << 16;

    status = BTAUARTreadRegister(winst, BTA_UartRegAddrFirmwareInfo, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->firmwareVersionMajor = (reg & 0xf800) >> 11;
    info->firmwareVersionMinor = (reg & 0x07c0) >> 6;
    info->firmwareVersionNonFunc = (reg & 0x003f);

    *deviceInfo = info;
    return BTA_StatusOk;
}


uint8_t BTAUARTisRunning(BTA_WrapperInst *winst) {
    return 1;
}


uint8_t BTAUARTisConnected(BTA_WrapperInst *winst) {
    return 1;
}


BTA_Status BTAUARTsetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode frameMode) {
    return BTA_StatusNotSupported;
}


BTA_Status BTAUARTgetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode *frameMode) {
    *frameMode = BTA_FrameModeCurrentConfig;
    return BTA_StatusNotSupported;
}


BTA_Status BTAUARTsendReset(BTA_WrapperInst *winst) {
    return BTA_StatusNotSupported;
}


static void *readFramesRunFunction(void *handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return 0;
    }
    BTA_UartLibInst *inst = (BTA_UartLibInst *)handle;
    if (!inst) {
        return 0;
    }
    while (!inst->closing) {
        BTAlockMutex(inst->handleMutex);
        uint8_t *buffer;
        uint32_t bufferLen;
        BTA_Status status = readFrame(winst, &buffer, &bufferLen, 200);
        if (status == BTA_StatusOk) {
            if (bufferLen >= 2 && buffer[2] == 1) {
                // ProtocolVersion is 1
                if (bufferLen >= 5 && buffer[5] == 3) {
                    // We have stream (image) data
                    BTA_Frame *frame;
                    status = parseFrame(winst, &buffer[9], bufferLen - 9, &frame);
                    if (status != BTA_StatusOk) {
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "readFramesRunFunction: Error parsing frame");
                    }
                    else {
                        BTApostprocessGrabCallbackEnqueue(winst, frame);
                    }
                }
                else {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "readFramesRunFunction: not an image %d", buffer[5]);
                }
            }
            free(buffer);
            buffer = 0;
        }
        BTAunlockMutex(inst->handleMutex);
        BTAmsleep(1);
    }
    return 0;
}


static BTA_Status readFrame(BTA_WrapperInst *winst, uint8_t **dataPtr, uint32_t *dataLen, int timeout) {
    if (!winst || !dataPtr || !dataLen) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UartLibInst *inst = (BTA_UartLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_Status status;
    uint8_t preamble0 = 0;
    uint8_t preamble1 = 0;
    uint64_t timeEnd = BTAgetTickCount64() + timeout;
    while (!inst->closing && (preamble0 != BTA_UART_PREAMBLE_0 || preamble1 != BTA_UART_PREAMBLE_1)) {
        preamble0 = preamble1;
        UARTHLPserialRead(inst->serialHandle, &preamble1, 1);
        if (BTAgetTickCount64() > timeEnd) {
            //BTAinfoEventHelper(winst->infoEventInst, IMPORTANCE_ERROR, BTA_StatusTimeOut, "ReadFrame: (preamble) %d", getLastError());
            return BTA_StatusTimeOut;
        }
    }
    uint8_t protocolVersion;
    status = UARTHLPserialRead(inst->serialHandle, &protocolVersion, 1);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "ReadFrame: can't read (protocolVersion) %d", getLastError());
        return status;
    }
    *dataLen = 9;
    uint8_t *data = (uint8_t *)malloc(*dataLen);
    if (!data) {
        return BTA_StatusOutOfMemory;
    }
    data[0] = preamble0;
    data[1] = preamble1;
    data[2] = protocolVersion;
    switch (protocolVersion) {
        case 1: {
            status = UARTHLPserialRead(inst->serialHandle, &data[3], 6);
            if (status != BTA_StatusOk) {
                free(data);
                data = 0;
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "ReadFrame: can't read (data[3], 6) %d", getLastError());
                return status;
            }
            uint8_t headerCrc7 = crc7(&data[2], 6);
            if (headerCrc7 != data[8]) {
                free(data);
                data = 0;
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusCrcError, "ReadFrame: crc7 checksum wrong");
                return BTA_StatusCrcError;
            }
            if (data[3] != inst->uartTransmitterAddress) {
                // Not my concern, don't listen, do nothing
                free(data);
                data = 0;
                return BTA_StatusOk;
            }
            // TODO? check receiverAddress???
            uint16_t payloadLen = data[6] | (data[7] << 8);
            uint8_t *temp = data;
            *dataLen = 9 + payloadLen + 2;
            data = (uint8_t *)realloc(data, *dataLen);
            if (!data) {
                free(temp);
                temp = 0;
                return BTA_StatusOutOfMemory;
            }
            status = UARTHLPserialRead(inst->serialHandle, &data[9], payloadLen + 2);
            if (status != BTA_StatusOk) {
                free(data);
                data = 0;
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "ReadFrame: can't read (payload) %d", getLastError());
                return status;
            }
            uint16_t dataCrc16 = crc16_ccitt(&data[9], payloadLen);
            if (dataCrc16 != (data[9 + payloadLen] | (data[9 + payloadLen + 1] << 8))) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusCrcError, "ReadFrame: crc16 checksum wrong");
                free(data);
                data = 0;
                return BTA_StatusCrcError;
            }
            *dataPtr = data;
            return BTA_StatusOk;
        }
        default:
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidVersion, "ReadFrame: (unknown protocol version)");
            free(data);
            data = 0;
            return BTA_StatusInvalidVersion;
    }
}


static BTA_Status parseFrame(BTA_WrapperInst *winst, uint8_t *data, uint32_t dataLen, BTA_Frame **framePtr) {
    BTA_Frame *frame = (BTA_Frame *)malloc(sizeof(BTA_Frame));
    if (!frame) {
        return BTA_StatusOutOfMemory;
    }
    if (dataLen < 17) {
        return BTA_StatusOutOfMemory;
    }
    uint32_t i = 0;
    int32_t lineIndex;
    uint32_t lineDataLength;
    uint8_t version = data[i++];
    switch (version) {
        case 1: {
            uint8_t xRes = data[i++];;
            uint8_t yRes = data[i++];
            uint16_t imgFormat = data[i++];
            imgFormat |= data[i++] << 8;
            frame->timeStamp = data[i++];
            frame->timeStamp |= data[i++] << 8;
            frame->timeStamp |= data[i++] << 16;
            frame->timeStamp |= data[i++] << 24;
            frame->frameCounter = data[i++];
            frame->frameCounter |= data[i++] << 8;
            frame->mainTemp = ((float)data[i++]);
            if (frame->mainTemp != 0xff)
            {
                frame->mainTemp -= 50;
            }
            frame->ledTemp = ((float)data[i++]);
            if (frame->ledTemp != 0xff)
            {
                frame->ledTemp -= 50;
            }
            uint16_t firmwareVersion = data[i++];
            firmwareVersion |= data[i++] << 8;
            frame->firmwareVersionMajor = firmwareVersion >> 11;
            frame->firmwareVersionMinor = (firmwareVersion >> 6) & 0x1f;
            frame->firmwareVersionNonFunc = firmwareVersion & 0x1f;
            frame->sequenceCounter = 0;
            BTA_UartImgMode imgMode = (BTA_UartImgMode)(imgFormat >> 3);
            switch (imgMode) {
                case BTA_UartImgModeDistAmp:
                    frame->channelsLen = 2;
                    break;
                case BTA_UartImgModeDist:
                    frame->channelsLen = 1;
                    break;
                default:
                    frame->channelsLen = 0;
                    break;
            }
            frame->channels = (BTA_Channel **)malloc(frame->channelsLen * sizeof(BTA_Channel *));
            if (!frame->channels) {
                free(frame);
                frame = 0;
                return BTA_StatusOutOfMemory;
            }
            int channelIndex;
            for (channelIndex = 0; channelIndex < frame->channelsLen; channelIndex++) {
                frame->channels[channelIndex] = (BTA_Channel *)malloc(sizeof(BTA_Channel));
                if (!frame->channels[channelIndex]) {
                    int j;
                    for (j = 0; j < channelIndex; j++) {
                        free(frame->channels[j]->data);
                        frame->channels[j]->data = 0;
                        free(frame->channels[j]);
                        frame->channels[j] = 0;
                    }
                    free(frame->channels);
                    frame->channels = 0;
                    free(frame);
                    frame = 0;
                    return BTA_StatusOutOfMemory;
                }
                frame->channels[channelIndex]->id = BTAUARTgetChannelId(imgMode, channelIndex);
                frame->channels[channelIndex]->xRes = xRes;
                frame->channels[channelIndex]->yRes = yRes;
                frame->channels[channelIndex]->integrationTime = 0;
                frame->channels[channelIndex]->modulationFrequency = 0;
                frame->channels[channelIndex]->dataFormat = BTAUARTgetDataFormat(imgMode, channelIndex);
                frame->channels[channelIndex]->unit = BTAUARTgetUnit(imgMode, channelIndex);
                frame->channels[channelIndex]->dataLen = frame->channels[channelIndex]->xRes * frame->channels[channelIndex]->yRes * (frame->channels[channelIndex]->dataFormat & 0xf);
                frame->channels[channelIndex]->data = (uint8_t *)malloc(frame->channels[channelIndex]->dataLen);
                if (!frame->channels[channelIndex]->data) {
                    free(frame->channels[channelIndex]);
                    frame->channels[channelIndex] = 0;
                    int j;
                    for (j = 0; j < channelIndex; j++) {
                        free(frame->channels[j]->data);
                        frame->channels[j]->data = 0;
                        free(frame->channels[j]);
                        frame->channels[j] = 0;
                    }
                    free(frame->channels);
                    frame->channels = 0;
                    free(frame);
                    frame = 0;
                    return BTA_StatusOutOfMemory;
                }
                lineDataLength = frame->channels[channelIndex]->xRes * (frame->channels[channelIndex]->dataFormat & 0xf);
                for (lineIndex = yRes - 1; lineIndex >= 0; lineIndex--) {
                    memcpy(frame->channels[channelIndex]->data + lineIndex * lineDataLength, data + i, lineDataLength);
                    i += lineDataLength;
                }
                frame->channels[channelIndex]->metadata = 0;
                frame->channels[channelIndex]->metadataLen = 0;
                //---------------------------------------------------------------------
            }
            *framePtr = frame;
            return BTA_StatusOk;
        }
        default: {
            free(frame);
            frame = 0;
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidVersion, "Parse frame: invalid version: %d", version);
            return BTA_StatusInvalidVersion;
        }
    }
}




BTA_Status BTAUARTgetIntegrationTime(BTA_WrapperInst *winst, uint32_t *integrationTime) {
    return BTAUARTreadRegister(winst, BTA_UartRegAddrIntegrationTime, integrationTime, 0);
}


BTA_Status BTAUARTsetIntegrationTime(BTA_WrapperInst *winst, uint32_t integrationTime) {
    if (integrationTime < 1 || integrationTime > 0xffff) {
        return BTA_StatusInvalidParameter;
    }
    return BTAUARTwriteRegister(winst, BTA_UartRegAddrIntegrationTime, &integrationTime, 0);
}


BTA_Status BTAUARTgetFrameRate(BTA_WrapperInst *winst, float *frameRate) {
    if (!frameRate) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t frameRate32;
    BTA_Status status = BTAUARTreadRegister(winst, BTA_UartRegAddrFramerate, &frameRate32, 0);
    if (status != BTA_StatusOk) {
        *frameRate = 0;
        return status;
    }
    *frameRate = (float)frameRate32;
    return BTA_StatusOk;
}


BTA_Status BTAUARTsetFrameRate(BTA_WrapperInst *winst, float frameRate) {
    uint32_t frameRate32 = (uint32_t)frameRate;
    if (frameRate < 1 || frameRate > 0xffff) {
        return BTA_StatusInvalidParameter;
    }
    return BTAUARTwriteRegister(winst, BTA_UartRegAddrFramerate, &frameRate32, 0);
}


BTA_Status BTAUARTgetModulationFrequency(BTA_WrapperInst *winst, uint32_t *modulationFrequency) {
    if (!modulationFrequency) {
        return BTA_StatusInvalidParameter;
    }
    BTA_Status status;
    status = BTAUARTreadRegister(winst, BTA_UartRegAddrModulationFrequency, modulationFrequency, 0);
    if (status != BTA_StatusOk) {
        *modulationFrequency = 0;
        return status;
    }
    *modulationFrequency *= 10000;
    return BTA_StatusOk;
}


BTA_Status BTAUARTsetModulationFrequency(BTA_WrapperInst *winst, uint32_t modulationFrequency) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_Status status;
    BTA_DeviceType deviceType;
    status = BTAUARTgetDeviceType(winst, &deviceType);
    if (status != BTA_StatusOk) {
        return status;
    }
    uint32_t modFreq;
    status = BTAgetNextBestModulationFrequency(winst, modulationFrequency, &modFreq, 0);
    if (status != BTA_StatusOk) {
        if (status == BTA_StatusNotSupported) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAsetModulationFrequency: Not supported for this deviceType: %d", deviceType);
        }
        return status;
    }
    modFreq /= 10000;
    return BTAUARTwriteRegister(winst, BTA_UartRegAddrModulationFrequency, &modFreq, 0);
}


BTA_Status BTAUARTgetGlobalOffset(BTA_WrapperInst *winst, float *offset) {
    return BTA_StatusNotSupported;
}


BTA_Status BTAUARTsetGlobalOffset(BTA_WrapperInst *winst, float offset) {
    return BTA_StatusNotSupported;
}


BTA_Status BTAUARTwriteCurrentConfigToNvm(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t password = 0x4877;
    BTA_Status status = BTAUARTwriteRegister(winst, BTA_UartRegAddrCmdEnablePasswd, &password, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    BTAmsleep(200);
    uint32_t command = 0xdd9e;
    status = BTAUARTwriteRegister(winst, BTA_UartRegAddrCmdExec, &command, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    // wait until the device is ready again
    uint32_t result;
    uint64_t endTime = BTAgetTickCount64() + 20000;
    do {
        BTAmsleep(50);
        status = BTAUARTreadRegister(winst, BTA_UartRegAddrDeviceType, &result, 0);
    } while (status != BTA_StatusOk && BTAgetTickCount64() < endTime);
    if (status != BTA_StatusOk) {
        return status;
    }
    BTAmsleep(100);
    endTime = BTAgetTickCount64() + 7000;
    do {
        BTAmsleep(150);
        status = BTAUARTreadRegister(winst, BTA_UartRegAddrCmdExecResult, &result, 0);
        if (status != BTA_StatusOk) {
            return status;
        }
    } while (result == 0 && BTAgetTickCount64() < endTime);
    if (result != 1) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAwriteCurrentConfigToNvm() failed %d", result);
        return BTA_StatusRuntimeError;
    }
    return BTA_StatusOk;
}


BTA_Status BTAUARTrestoreDefaultConfig(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t password = 0x4877;
    BTA_Status status = BTAUARTwriteRegister(winst, BTA_UartRegAddrCmdEnablePasswd, &password, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    BTAmsleep(200);
    uint32_t command = 0xc2ae;
    status = BTAUARTwriteRegister(winst, BTA_UartRegAddrCmdExec, &command, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    // wait until the device is ready again
    uint32_t result;
    uint64_t endTime = BTAgetTickCount64() + 20000;
    do {
        BTAmsleep(500);
        status = BTAUARTreadRegister(winst, BTA_UartRegAddrDeviceType, &result, 0);
    } while (status != BTA_StatusOk && BTAgetTickCount64() < endTime);
    if (status != BTA_StatusOk) {
        return status;
    }
    BTAmsleep(100);
    endTime = BTAgetTickCount64() + 20000;
    do {
        BTAmsleep(550);
        status = BTAUARTreadRegister(winst, BTA_UartRegAddrCmdExecResult, &result, 0);
        if (status != BTA_StatusOk) {
            return status;
        }
    } while (result == 0 && BTAgetTickCount64() < endTime);
    if (result != 1) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTArestoreDefaultConfig() failed %d", result);
        return BTA_StatusRuntimeError;
    }
    return BTA_StatusOk;
}


BTA_Status BTAUARTreadRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount) {
    BTA_Status status;
    uint32_t lenToRead = 2;
    if (!winst || !data) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UartLibInst *inst = (BTA_UartLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    if (address > 0xffff) {
        if (registerCount) {
            *registerCount = 0;
        }
        return BTA_StatusInvalidParameter;
    }
    if (registerCount) {
        if (address + *registerCount > 0x10000) {
            *registerCount = 0;
            return BTA_StatusInvalidParameter;
        }
        lenToRead = *registerCount * 2;
        *registerCount = 0;
    }
    if (lenToRead != 2) {
        return BTA_StatusInvalidParameter;
    }

    uint32_t sendBufferLen;
    uint8_t *sendBuffer;
    status = toByteStream(winst, BTA_UartCommandRead, (BTA_UartFlashCommand)0, address, 0, 0, &sendBuffer, &sendBufferLen);
    if (status != BTA_StatusOk) {
        if (registerCount) {
            *registerCount = 0;
        }
        return status;
    }
    if (inst->closing) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusIllegalOperation, "BTAreadRegister: Instance is closing");
        return BTA_StatusIllegalOperation;
    }
    BTAlockMutex(inst->handleMutex);
    status = UARTHLPserialWrite(inst->serialHandle, sendBuffer, sendBufferLen);
    free(sendBuffer);
    sendBuffer = 0;
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAreadRegister: Failed to write serial data %d", getLastError());
        BTAunlockMutex(inst->handleMutex);
        return status;
    }
    uint64_t endTime = BTAgetTickCount64() + 3000;
    do {
        uint8_t *buffer;
        uint32_t bufferLen;
        status = readFrame(winst, &buffer, &bufferLen, 250);
        if (status == BTA_StatusOk) {
            if (bufferLen >= 2 && buffer[2] == 1) {
                // ProtocolVersion is 1
                if (bufferLen >= 5 && buffer[5] == BTA_UartCommandStream) {
                    // We have stream (image) data -> process it
                    BTA_Frame *frame;
                    status = parseFrame(winst, &buffer[9], bufferLen - 9, &frame);
                    if (status != BTA_StatusOk) {
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAreadRegister: Error parsing frame");
                    }
                    else {
                        BTApostprocessGrabCallbackEnqueue(winst, frame);
                    }
                }
                else if (bufferLen >= 5 && buffer[5] == BTA_UartCommandResponse) {
                    // We have a response
                    if (bufferLen >= 9 && buffer[9] == BTA_UartCommandRead) {
                        // We have a response to a read
                        if (bufferLen >= 11 && (uint32_t)(buffer[10] | (buffer[11] << 8)) == address) {
                            // The register address matches
                            if (bufferLen >= 13) {
                                *data = buffer[12] | (buffer[13] << 8);
                                if (registerCount) {
                                    *registerCount = 1;
                                }
                                BTAunlockMutex(inst->handleMutex);
                                free(buffer);
                                buffer = 0;
                                return BTA_StatusOk;
                            }
                        }
                    }
                }
            }
            free(buffer);
            buffer = 0;
        }
    } while (BTAgetTickCount64() < endTime);
    BTAunlockMutex(inst->handleMutex);
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAreadRegister: timeout reading readRegister response");
    return BTA_StatusTimeOut;
}


BTA_Status BTAUARTwriteRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount) {
    if (!winst || !data) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UartLibInst *inst = (BTA_UartLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_Status status;
    uint32_t lenToRead = 2;
    if (address > 0xffff) {
        if (registerCount) {
            *registerCount = 0;
        }
        return BTA_StatusInvalidParameter;
    }
    if (registerCount) {
        if (address + *registerCount > 0x10000) {
            *registerCount = 0;
            return BTA_StatusInvalidParameter;
        }
        lenToRead = *registerCount * 2;
        *registerCount = 0;
    }
    if (!inst || lenToRead != 2/*== 0*/) {
        return BTA_StatusInvalidParameter;
    }

    uint32_t sendBufferLen;
    uint8_t *sendBuffer;
    status = toByteStream(winst, BTA_UartCommandWrite, (BTA_UartFlashCommand)0, address, (uint8_t *)data, 2, &sendBuffer, &sendBufferLen);
    if (status != BTA_StatusOk) {
        if (registerCount) {
            *registerCount = 0;
        }
        return status;
    }
    if (inst->closing) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusIllegalOperation, "BTAreadRegister: Instance is closing");
        return BTA_StatusIllegalOperation;
    }
    BTAlockMutex(inst->handleMutex);
    status = UARTHLPserialWrite(inst->serialHandle, sendBuffer, sendBufferLen);
    free(sendBuffer);
    sendBuffer = 0;
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAwriteRegister: Failed to write serial data %d", getLastError());
        BTAunlockMutex(inst->handleMutex);
        return status;
    }
    uint64_t endTime = BTAgetTickCount64() + 3000;
    do {
        uint8_t *buffer;
        uint32_t bufferLen;
        status = readFrame(winst, &buffer, &bufferLen, 250);
        if (status == BTA_StatusOk) {
            if (bufferLen >= 2 && buffer[2] == 1) {
                // ProtocolVersion is 1
                if (bufferLen >= 5 && buffer[5] == BTA_UartCommandStream) {
                    // We have stream (image) data -> process it
                    BTA_Frame *frame;
                    status = parseFrame(winst, &buffer[9], bufferLen - 9, &frame);
                    if (status != BTA_StatusOk) {
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "readFramesRunFunction: Error parsing frame");
                    }
                    else {
                        BTApostprocessGrabCallbackEnqueue(winst, frame);
                    }
                }
                else if (bufferLen >= 5 && buffer[5] == BTA_UartCommandResponse) {
                    // We have a response
                    if (bufferLen >= 9 && buffer[9] == BTA_UartCommandWrite) {
                        // We have a response to a write
                        if (bufferLen >= 11 && (uint32_t)(buffer[10] | (buffer[11] << 8)) == address) {
                            // The register address matches
                            if (bufferLen >= 12) {
                                BTA_Status status = BTA_StatusOk;
                                if (buffer[12] != 0) {
                                    switch (buffer[12]) {
                                    case 0x0F: //Illegal write: The Address is not valid or the register is not write-enabled
                                        status = BTA_StatusIllegalOperation;
                                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAwriteRegister: Illegal write %d", buffer[12]);
                                        break;
                                    case 0x11: //Register end reached
                                        status = BTA_StatusInvalidParameter;
                                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAwriteRegister: Outside register range %d", buffer[12]);
                                        break;
                                    case 0xFC: //DataCrc16 mismatch
                                        status = BTA_StatusCrcError;
                                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAwriteRegister: Cyclic redundancy check failed %d", buffer[12]);
                                        break;
                                    case 0xFF: //Unknown command
                                        status = BTA_StatusRuntimeError;
                                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAwriteRegister: Unknown command %d", buffer[12]);
                                        break;
                                    default:
                                        status = BTA_StatusUnknown;
                                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAwriteRegister: Unrecognized error %d", buffer[12]);
                                    }
                                }
                                if (status == BTA_StatusOk) {
                                    if (registerCount) {
                                        *registerCount = 1;
                                    }
                                }
                                BTAunlockMutex(inst->handleMutex);
                                free(buffer);
                                buffer = 0;
                                return status;
                            }
                        }
                    }
                }
            }
            free(buffer);
            buffer = 0;
        }
        else {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "readFrame error");
        }
    } while (BTAgetTickCount64() < endTime);
    BTAunlockMutex(inst->handleMutex);
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusTimeOut, "BTAwriteRegister: timeout");
    return BTA_StatusTimeOut;
}


BTA_Status BTAUARTsetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float value) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    //switch (libParam) {
    //default:
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAsetLibParam: LibParam is not supported %d", libParam);
        return BTA_StatusNotSupported;
    //}
}


BTA_Status BTAUARTgetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float *value) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    //switch (libParam) {
    //default:
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAgetLibParam: LibParam is not supported %d", libParam);
        return BTA_StatusNotSupported;
    //}
}


BTA_Status BTAUARTflashUpdate(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport) {
    return BTA_StatusNotSupported;
}


BTA_Status BTAUARTflashRead(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport, uint8_t quiet) {
    return BTA_StatusNotSupported;
}




static int getLastError() {
#ifdef PLAT_WINDOWS
    return GetLastError();
#else
    return errno;
#endif
}


/*static BTA_Status flashStatusToBltTofApiStatus(uint8_t status) {
    switch (status) {
    case 0:
        return BTA_StatusOk;
    case 0x01: //Unknown target
    case 0x02: //Invalid flash address
    case 0x03: //CRC32 error
    case 0x04: //Wrong packet number
    case 0x05: //Wrong packet size
    case 0x06: //Unknown command
    case 0x07: //Unsupported version
    case 0x08: //Not initialized
    case 0x09: //Protocol violation
    case 0x0A: //In progress error
    case 0x0B: //Flashing failed
    case 0x0C: //Max size exceeded
        return BTA_StatusRuntimeError;
    default:
        return BTA_StatusUnknown;
    }
}*/


static BTA_Status toByteStream(BTA_WrapperInst *winst, BTA_UartCommand cmd, BTA_UartFlashCommand flashCmd, uint32_t addr, uint8_t *data, uint32_t dataLen, uint8_t **resultPtr, uint32_t *resultLen) {
    if (!winst || !resultLen) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UartLibInst *inst = (BTA_UartLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    switch (BTA_UART_PROTOCOL_VERSION) {
        case 1: {
            if (cmd == BTA_UartCommandRead) {
                uint8_t *result = (uint8_t *)calloc(1, 9 + 2 + 2);
                if (!result) {
                    return BTA_StatusOutOfMemory;
                }
                uint32_t resultIndex = 0;
                result[resultIndex++] = BTA_UART_PREAMBLE_0;
                result[resultIndex++] = BTA_UART_PREAMBLE_1;
                result[resultIndex++] = (uint8_t)BTA_UART_PROTOCOL_VERSION;
                result[resultIndex++] = inst->uartReceiverAddress;
                result[resultIndex++] = inst->uartTransmitterAddress;
                result[resultIndex++] = (uint8_t)cmd;
                result[resultIndex++] = 2;  //length
                result[resultIndex++] = 0;  //length
                result[resultIndex++] = crc7(&result[2], 6);
                result[resultIndex++] = (uint8_t)(addr);
                result[resultIndex++] = (uint8_t)(addr >> 8);
                uint16_t crc16 = crc16_ccitt(&addr, 2);
                result[resultIndex++] = (uint8_t)(crc16);
                result[resultIndex++] = (uint8_t)(crc16 >> 8);
                *resultLen = resultIndex;
                *resultPtr = result;
                return BTA_StatusOk;
            }
            else if (cmd == BTA_UartCommandWrite) {
                if (!data || dataLen != 2) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "toByteStream: Parameters data or dataLen missing");
                    return BTA_StatusInvalidParameter;
                }
                uint8_t *result = (uint8_t *)calloc(1, 9 + 4 + 2);
                if (!result) {
                    return BTA_StatusOutOfMemory;
                }
                uint32_t resultIndex = 0;
                result[resultIndex++] = BTA_UART_PREAMBLE_0;
                result[resultIndex++] = BTA_UART_PREAMBLE_1;
                result[resultIndex++] = (uint8_t)BTA_UART_PROTOCOL_VERSION;
                result[resultIndex++] = inst->uartReceiverAddress;
                result[resultIndex++] = inst->uartTransmitterAddress;
                result[resultIndex++] = (uint8_t)cmd;
                result[resultIndex++] = 4;  //length
                result[resultIndex++] = 0;  //length
                result[resultIndex++] = crc7(&result[2], 6);
                result[resultIndex++] = (uint8_t)(addr);
                result[resultIndex++] = (uint8_t)(addr >> 8);
                result[resultIndex++] = data[0];
                result[resultIndex++] = data[1];
                uint16_t crc16 = crc16_ccitt(&result[9], 4);
                result[resultIndex++] = (uint8_t)(crc16);
                result[resultIndex++] = (uint8_t)(crc16 >> 8);
                *resultLen = resultIndex;
                *resultPtr = result;
                return BTA_StatusOk;
            }
            else if (cmd == BTA_UartCommandFlashUpdate) {
                // TODO!
            }
        }

        default:
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidVersion, "toByteStream: Version not supported %d", BTA_UART_PROTOCOL_VERSION);
            return BTA_StatusInvalidVersion;
    }
}

#endif