/**  @file bta_wrapper.c
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

#include <undistort.h>
#ifndef WO_LIBJPEG
#   include <bta_jpg.h>
#endif
#include <bvq_queue.h>

#include <crc16.h>
#include <crc32.h>


static char check1[(2 * (int)(sizeof(uint32_t) == sizeof(float))) - 1] = { 0 };
static char check2[(2 * (int)(sizeof(uint32_t) == sizeof(BTA_ChannelId))) - 1] = { 0 };
static char check3[(2 * (int)(sizeof(uint32_t) == sizeof(BTA_DataFormat))) - 1] = { 0 };
static char check4[(2 * (int)(sizeof(uint32_t) == sizeof(BTA_Unit))) - 1] = { 0 };
static char check5[(2 * (int)(sizeof(uint32_t) == sizeof(BTA_MetadataId))) - 1] = { 0 };

// char must be one byte long
extern char __CHECK0__[1 / (uint8_t)!(sizeof(char) - sizeof(uint8_t))];
extern char __CHECK1__[1 / (uint8_t)!(4 - sizeof(float))];
// We want BTA_Handle to be as big as a pointer on the current platform
extern char __CHECK2__[1 / (uint8_t)!(sizeof(void *) - sizeof(BTA_Handle))];


static char checkDiscoveryHmaj[(2 * (int)(BTA_VER_MAJ == BTA_DISCOVERY_H_VER_MAJ)) - 1] = { 0 };
static char checkDiscoveryHmin[(2 * (int)(BTA_VER_MIN == BTA_DISCOVERY_H_VER_MIN)) - 1] = { 0 };
static char checkDiscoveryHnfu[(2 * (int)(BTA_VER_NON_FUNC == BTA_DISCOVERY_H_VER_NON_FUNC)) - 1] = { 0 };
static char checkBtaExtHmaj[(2 * (int)(BTA_VER_MAJ == BTA_EXT_H_VER_MAJ)) - 1] = { 0 };
static char checkBtaExtHmin[(2 * (int)(BTA_VER_MIN == BTA_EXT_H_VER_MIN)) - 1] = { 0 };
static char checkBtaExtHnfu[(2 * (int)(BTA_VER_NON_FUNC == BTA_EXT_H_VER_NON_FUNC)) - 1] = { 0 };
static char checkFlashUpdateHmaj[(2 * (int)(BTA_VER_MAJ == BTA_FLASH_UPDATE_H_VER_MAJ)) - 1] = { 0 };
static char checkFlashUpdateHmin[(2 * (int)(BTA_VER_MIN == BTA_FLASH_UPDATE_H_VER_MIN)) - 1] = { 0 };
static char checkFlashUpdateHnfu[(2 * (int)(BTA_VER_NON_FUNC == BTA_FLASH_UPDATE_H_VER_NON_FUNC)) - 1] = { 0 };
static char checkFrameHmaj[(2 * (int)(BTA_VER_MAJ == BTA_FRAME_H_VER_MAJ)) - 1] = { 0 };
static char checkFrameHmin[(2 * (int)(BTA_VER_MIN == BTA_FRAME_H_VER_MIN)) - 1] = { 0 };
static char checkFrameHnfu[(2 * (int)(BTA_VER_NON_FUNC == BTA_FRAME_H_VER_NON_FUNC)) - 1] = { 0 };
static char checkStatusHmaj[(2 * (int)(BTA_VER_MAJ == BTA_STATUS_H_VER_MAJ)) - 1] = { 0 };
static char checkStatusHmin[(2 * (int)(BTA_VER_MIN == BTA_STATUS_H_VER_MIN)) - 1] = { 0 };
static char checkStatusHnfu[(2 * (int)(BTA_VER_NON_FUNC == BTA_STATUS_H_VER_NON_FUNC)) - 1] = { 0 };

#define MARK_USED(x) ((void)(x))


#ifndef BTA_WO_ETH
BTA_Status BTAETHstartDiscovery(BTA_DiscoveryConfig *discoveryConfig, FN_BTA_DeviceFound deviceFound, FN_BTA_InfoEvent infoEvent, BTA_Handle *handle);
BTA_Status BTAETHstopDiscovery(BTA_Handle *handle);
BTA_Status BTAETHopen(BTA_Config *config, BTA_WrapperInst *wrapperInst);
BTA_Status BTAETHclose(BTA_WrapperInst *winst);
BTA_Status BTAETHgetDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo);
uint8_t BTAETHisRunning(BTA_WrapperInst *winst);
uint8_t BTAETHisConnected(BTA_WrapperInst *winst);
BTA_Status BTAETHsetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode frameMode);
BTA_Status BTAETHgetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode *frameMode);
BTA_Status BTAETHsetIntegrationTime(BTA_WrapperInst *winst, uint32_t integrationTime);
BTA_Status BTAETHgetIntegrationTime(BTA_WrapperInst *winst, uint32_t *integrationTime);
BTA_Status BTAETHsetFrameRate(BTA_WrapperInst *winst, float frameRate);
BTA_Status BTAETHgetFrameRate(BTA_WrapperInst *winst, float *frameRate);
BTA_Status BTAETHsetModulationFrequency(BTA_WrapperInst *winst, uint32_t modulationFrequency);
BTA_Status BTAETHgetModulationFrequency(BTA_WrapperInst *winst, uint32_t *modulationFrequency);
BTA_Status BTAETHsetGlobalOffset(BTA_WrapperInst *winst, float globalOffset);
BTA_Status BTAETHgetGlobalOffset(BTA_WrapperInst *winst, float *globalOffset);
BTA_Status BTAETHreadRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTAETHwriteRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTAETHsetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float value);
BTA_Status BTAETHgetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float *value);
BTA_Status BTAETHsendReset(BTA_WrapperInst *winst);
BTA_Status BTAETHflashUpdate(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport);
BTA_Status BTAETHflashRead(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport, uint8_t quiet);
BTA_Status BTAETHwriteCurrentConfigToNvm(BTA_WrapperInst *winst);
BTA_Status BTAETHrestoreDefaultConfig(BTA_WrapperInst *winst);
#endif

#ifndef BTA_WO_UART
BTA_Status BTAUARTstartDiscovery(BTA_DiscoveryConfig *discoveryConfig, FN_BTA_DeviceFound deviceFound, FN_BTA_InfoEvent infoEvent, BTA_Handle *handle);
BTA_Status BTAUARTstopDiscovery(BTA_Handle *handle);
BTA_Status BTAUARTopen(BTA_Config *config, BTA_WrapperInst *wrapperInst);
BTA_Status BTAUARTclose(BTA_WrapperInst *winst);
BTA_Status BTAUARTgetDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo);
uint8_t BTAUARTisRunning(BTA_WrapperInst *winst);
uint8_t BTAUARTisConnected(BTA_WrapperInst *winst);
BTA_Status BTAUARTsetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode frameMode);
BTA_Status BTAUARTgetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode *frameMode);
BTA_Status BTAUARTsetIntegrationTime(BTA_WrapperInst *winst, uint32_t integrationTime);
BTA_Status BTAUARTgetIntegrationTime(BTA_WrapperInst *winst, uint32_t *integrationTime);
BTA_Status BTAUARTsetFrameRate(BTA_WrapperInst *winst, float frameRate);
BTA_Status BTAUARTgetFrameRate(BTA_WrapperInst *winst, float *frameRate);
BTA_Status BTAUARTsetModulationFrequency(BTA_WrapperInst *winst, uint32_t modulationFrequency);
BTA_Status BTAUARTgetModulationFrequency(BTA_WrapperInst *winst, uint32_t *modulationFrequency);
BTA_Status BTAUARTsetGlobalOffset(BTA_WrapperInst *winst, float globalOffset);
BTA_Status BTAUARTgetGlobalOffset(BTA_WrapperInst *winst, float *globalOffset);
BTA_Status BTAUARTreadRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTAUARTwriteRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTAUARTsetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float value);
BTA_Status BTAUARTgetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float *value);
BTA_Status BTAUARTsendReset(BTA_WrapperInst *winst);
BTA_Status BTAUARTflashUpdate(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport);
BTA_Status BTAUARTflashRead(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport, uint8_t quiet);
BTA_Status BTAUARTwriteCurrentConfigToNvm(BTA_WrapperInst *winst);
BTA_Status BTAUARTrestoreDefaultConfig(BTA_WrapperInst *winst);
#endif

#ifndef BTA_WO_USB
BTA_Status BTAUSBstartDiscovery(BTA_DiscoveryConfig *discoveryConfig, FN_BTA_DeviceFound deviceFound, FN_BTA_InfoEvent infoEvent, BTA_Handle *handle);
BTA_Status BTAUSBstopDiscovery(BTA_Handle *handle);
BTA_Status BTAUSBopen(BTA_Config *config, BTA_WrapperInst *wrapperInst);
BTA_Status BTAUSBclose(BTA_WrapperInst *winst);
BTA_Status BTAUSBgetDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo);
uint8_t BTAUSBisRunning(BTA_WrapperInst *winst);
uint8_t BTAUSBisConnected(BTA_WrapperInst *winst);
BTA_Status BTAUSBsetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode frameMode);
BTA_Status BTAUSBgetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode *frameMode);
BTA_Status BTAUSBsetIntegrationTime(BTA_WrapperInst *winst, uint32_t integrationTime);
BTA_Status BTAUSBgetIntegrationTime(BTA_WrapperInst *winst, uint32_t *integrationTime);
BTA_Status BTAUSBsetFrameRate(BTA_WrapperInst *winst, float frameRate);
BTA_Status BTAUSBgetFrameRate(BTA_WrapperInst *winst, float *frameRate);
BTA_Status BTAUSBsetModulationFrequency(BTA_WrapperInst *winst, uint32_t modulationFrequency);
BTA_Status BTAUSBgetModulationFrequency(BTA_WrapperInst *winst, uint32_t *modulationFrequency);
BTA_Status BTAUSBsetGlobalOffset(BTA_WrapperInst *winst, float globalOffset);
BTA_Status BTAUSBgetGlobalOffset(BTA_WrapperInst *winst, float *globalOffset);
BTA_Status BTAUSBreadRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTAUSBwriteRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTAUSBsetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float value);
BTA_Status BTAUSBgetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float *value);
BTA_Status BTAUSBsendReset(BTA_WrapperInst *winst);
BTA_Status BTAUSBflashUpdate(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport);
BTA_Status BTAUSBflashRead(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport, uint8_t quiet);
BTA_Status BTAUSBwriteCurrentConfigToNvm(BTA_WrapperInst *winst);
BTA_Status BTAUSBrestoreDefaultConfig(BTA_WrapperInst *winst);
#endif

#ifndef BTA_WO_STREAM
BTA_Status BTASTREAMstartDiscovery(BTA_DiscoveryConfig *discoveryConfig, FN_BTA_DeviceFound deviceFound, FN_BTA_InfoEvent infoEvent, BTA_Handle *handle);
BTA_Status BTASTREAMstopDiscovery(BTA_Handle *handle);
BTA_Status BTASTREAMopen(BTA_Config *config, BTA_WrapperInst *wrapperInst);
BTA_Status BTASTREAMclose(BTA_WrapperInst *winst);
BTA_Status BTASTREAMgetDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo);
uint8_t BTASTREAMisRunning(BTA_WrapperInst *winst);
uint8_t BTASTREAMisConnected(BTA_WrapperInst *winst);
BTA_Status BTASTREAMsetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode frameMode);
BTA_Status BTASTREAMgetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode *frameMode);
BTA_Status BTASTREAMsetIntegrationTime(BTA_WrapperInst *winst, uint32_t integrationTime);
BTA_Status BTASTREAMgetIntegrationTime(BTA_WrapperInst *winst, uint32_t *integrationTime);
BTA_Status BTASTREAMsetFrameRate(BTA_WrapperInst *winst, float frameRate);
BTA_Status BTASTREAMgetFrameRate(BTA_WrapperInst *winst, float *frameRate);
BTA_Status BTASTREAMsetModulationFrequency(BTA_WrapperInst *winst, uint32_t modulationFrequency);
BTA_Status BTASTREAMgetModulationFrequency(BTA_WrapperInst *winst, uint32_t *modulationFrequency);
BTA_Status BTASTREAMsetGlobalOffset(BTA_WrapperInst *winst, float globalOffset);
BTA_Status BTASTREAMgetGlobalOffset(BTA_WrapperInst *winst, float *globalOffset);
BTA_Status BTASTREAMreadRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTASTREAMwriteRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTASTREAMsetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float value);
BTA_Status BTASTREAMgetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float *value);
BTA_Status BTASTREAMsendReset(BTA_WrapperInst *winst);
BTA_Status BTASTREAMflashUpdate(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport);
BTA_Status BTASTREAMflashRead(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport, uint8_t quiet);
BTA_Status BTASTREAMwriteCurrentConfigToNvm(BTA_WrapperInst *winst);
BTA_Status BTASTREAMrestoreDefaultConfig(BTA_WrapperInst *winst);
#endif




BTA_Status BTA_CALLCONV BTAgetVersion(uint32_t *verMaj, uint32_t *verMin, uint32_t *verNonFun, uint8_t *buildDateTime, uint32_t buildDateTimeLen, uint16_t *supportedDeviceTypes, uint32_t *supportedDeviceTypesLen) {
    if (verMaj && verMin && verNonFun) {
        *verMaj = BTA_VER_MAJ;
        *verMin = BTA_VER_MIN;
        *verNonFun = BTA_VER_NON_FUNC;
    }

    if (buildDateTime && buildDateTimeLen) {
        char buildDateTimeTemp[200];
        int result = sprintf((char *)buildDateTimeTemp, "%s - %s", __DATE__, __TIME__);
        if (result >= 0) {
            if (buildDateTimeLen < strlen(buildDateTimeTemp) + 1) {
                return BTA_StatusOutOfMemory;
            }
            // it's safe to copy
            strcpy((char *)buildDateTime, buildDateTimeTemp);
        }
    }

    if (supportedDeviceTypes && supportedDeviceTypesLen) {
        uint32_t countTotal = 0;
#       ifndef BTA_WO_ETH
            countTotal++;
#       endif
#       ifndef BTA_WO_USB
            countTotal++;
#       endif
#       ifndef BTA_WO_UART
            countTotal++;
#       endif
#       ifndef BTA_WO_STREAM
            countTotal++;
#       endif
        if (*supportedDeviceTypesLen >= countTotal) {
            int count = 0;
#           ifndef BTA_WO_ETH
                supportedDeviceTypes[count++] = BTA_DeviceTypeEthernet;
#           endif
#           ifndef BTA_WO_USB
                supportedDeviceTypes[count++] = BTA_DeviceTypeUsb;
#           endif
#           ifndef BTA_WO_UART
                supportedDeviceTypes[count++] = BTA_DeviceTypeUart;
#           endif
#           ifndef BTA_WO_STREAM
                supportedDeviceTypes[count++] = BTA_DeviceTypeBltstream;
#           endif
            *supportedDeviceTypesLen = count;
        }
    }
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAinitDiscoveryConfig(BTA_DiscoveryConfig *config) {
    if (!config) {
        return BTA_StatusInvalidParameter;
    }
    memset(config, 0, sizeof(BTA_DiscoveryConfig));
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAstartDiscovery(BTA_DiscoveryConfig *config, FN_BTA_DeviceFound deviceFound, FN_BTA_InfoEvent infoEvent, BTA_Handle *handle) {
    if (!handle) {
        return BTA_StatusInvalidParameter;
    }

    if (0) {
        if (infoEvent) {
            char *str = (char *)malloc(1024);
            if (!str) return BTA_StatusOutOfMemory;
            sprintf(str, "BTAstartDiscovery call:  ");
            if (config->broadcastIpAddr && config->broadcastIpAddrLen) {
                sprintf(str + strlen(str), "  broadcastIpAddr %d", config->broadcastIpAddr[0]);
                for (int i = 1; i < config->broadcastIpAddrLen; i++) sprintf(str + strlen(str), ".%d", config->broadcastIpAddr[i]);
            }
            sprintf(str + strlen(str), "  broadcastPort %d", config->broadcastPort);
            if (config->callbackIpAddr && config->callbackIpAddrLen) {
                sprintf(str + strlen(str), "  callbackIpAddr %d", config->callbackIpAddr[0]);
                for (int i = 1; i < config->callbackIpAddrLen; i++) sprintf(str + strlen(str), ".%d", config->callbackIpAddr[i]);
            }
            sprintf(str + strlen(str), "  callbackPort %d", config->callbackPort);
            sprintf(str + strlen(str), "  uartPortName %s", config->uartPortName);
            sprintf(str + strlen(str), "  uartBaudRate %d", config->uartBaudRate);
            sprintf(str + strlen(str), "  uartDataBits %d", config->uartDataBits);
            sprintf(str + strlen(str), "  uartStopBits %d", config->uartStopBits);
            sprintf(str + strlen(str), "  uartParity %d", config->uartParity);
            sprintf(str + strlen(str), "  uartTransmitterAddress %d", config->uartTransmitterAddress);
            sprintf(str + strlen(str), "  deviceType 0x%x", config-> deviceType);
            (*infoEvent)(BTA_StatusInformation, (int8_t *)str);
            free(str);
            str = 0;
        }
    }

    BTA_Handle *discoveryHandles = (BTA_Handle *)calloc(2, sizeof(BTA_Handle));
    *handle = discoveryHandles;

    int countCheck = 0;
#   ifndef BTA_WO_ETH
    if (!config->deviceType || config->deviceType == BTA_DeviceTypeGenericEth || BTAisEthDevice(config->deviceType)) {
        countCheck++;
        BTA_Status status = BTAETHstartDiscovery(config, deviceFound, infoEvent, &discoveryHandles[0]);
        if (status != BTA_StatusOk) {
            return status;
        }
    }
#   endif
#   ifndef BTA_WO_USB
    if (!config->deviceType || config->deviceType == BTA_DeviceTypeGenericUsb || BTAisUsbDevice(config->deviceType)) {
        countCheck++;
        BTA_Status status = BTAUSBstartDiscovery(config, deviceFound, infoEvent, &discoveryHandles[1]);
        if (status != BTA_StatusOk) {
            return status;
        }
    }
#   endif
    if (!countCheck) {
        if (infoEvent) {
            (*infoEvent)(BTA_StatusNotSupported, (int8_t *)"The given device type is not supported");
        }
        return BTA_StatusNotSupported;
    }
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAstopDiscovery(BTA_Handle *handle) {
    if (!handle) {
        return BTA_StatusInvalidParameter;
    }
    BTA_Handle *discoveryHandles = (BTA_Handle *)*handle;
#   ifndef BTA_WO_ETH
    if (discoveryHandles[0]) {
        BTA_Status status = BTAETHstopDiscovery(&discoveryHandles[0]);
        if (status != BTA_StatusOk) {
            return status;
        }
    }
#   endif
#   ifndef BTA_WO_USB
    if (discoveryHandles[1]) {
        BTA_Status status = BTAUSBstopDiscovery(&discoveryHandles[1]);
        if (status != BTA_StatusOk) {
            return status;
        }
    }
#   endif
    free(discoveryHandles);
    discoveryHandles = 0;
    *handle = 0;
    return BTA_StatusOk;
}

#define CONFIG_STRUCT_ORG_LEN 40
const uint32_t btaConfigStructOrgLen = CONFIG_STRUCT_ORG_LEN;
BTA_ConfigStructOrg btaConfigStructOrg[CONFIG_STRUCT_ORG_LEN] = {
    { "udpDataIpAddr", 1 },
    { "udpDataIpAddrLen", 0 },
    { "udpDataPort", 0 },
    { "udpControlOutIpAddr", 1 },
    { "udpControlOutIpAddrLen", 0 },
    { "udpControlOutPort", 0 },
    { "udpControlInIpAddr", 1 },
    { "udpControlInIpAddrLen", 0 },
    { "udpControlInPort", 0 },
    { "tcpDeviceIpAddr", 1 },
    { "tcpDeviceIpAddrLen", 0 },
    { "tcpDataPort", 0 },
    { "tcpControlPort", 0 },

    { "uartPortName", 1 },
    { "uartBaudRate", 0 },
    { "uartDataBits", 0 },
    { "uartStopBits", 0 },
    { "uartParity", 0 },
    { "uartTransmitterAddress", 0 },
    { "uartReceiverAddress", 0 },

    { "deviceType", 0 },
    { "pon", 1 },
    { "serialNumber", 0 },

    { "calibFileName", 1 },
    { "zFactorsFileName", 1 },
    { "wigglingFileName", 1 },

    { "frameMode", 0 },

    { "infoEvent", 0 },
    { "infoEventEx", 0 },
    { "infoEventEx2", 0 },
    { "verbosity", 0 },
    { "frameArrived", 0 },
    { "frameArrivedEx", 0 },
    { "frameArrivedEx2", 0 },
    { "userArg", 0 },

    { "frameQueueLength", 0 },
    { "frameQueueMode", 0 },

    { "averageWindowLength", 0 },

    { "bltstreamFilename", 1 },
    { "infoEventFilename", 1 },
};


BTA_ConfigStructOrg *BTA_CALLCONV BTAgetConfigStructOrg(uint32_t *fieldCount, uint8_t *bytesPerField) {
    if (fieldCount) {
        *fieldCount = btaConfigStructOrgLen;
    }
    if (bytesPerField) {
        *bytesPerField = BTA_CONFIG_STRUCT_STRIDE;
    }
    return btaConfigStructOrg;
}


BTA_Status BTA_CALLCONV BTAinitConfig(BTA_Config *config) {
    if (!config) {
        return BTA_StatusInvalidParameter;
    }
    memset(config, 0, sizeof(BTA_Config));

    MARK_USED(check1);
    MARK_USED(check2);
    MARK_USED(check3);
    MARK_USED(check4);
    MARK_USED(check5);
    MARK_USED(checkDiscoveryHmaj);
    MARK_USED(checkDiscoveryHmin);
    MARK_USED(checkDiscoveryHnfu);
    MARK_USED(checkBtaExtHmaj);
    MARK_USED(checkBtaExtHmin);
    MARK_USED(checkBtaExtHnfu);
    MARK_USED(checkFlashUpdateHmaj);
    MARK_USED(checkFlashUpdateHmin);
    MARK_USED(checkFlashUpdateHnfu);
    MARK_USED(checkFrameHmaj);
    MARK_USED(checkFrameHmin);
    MARK_USED(checkFrameHnfu);
    MARK_USED(checkStatusHmaj);
    MARK_USED(checkStatusHmin);
    MARK_USED(checkStatusHnfu);

    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAopen(BTA_Config *config, BTA_Handle *handle) {

    if (!config || !handle) {
        return BTA_StatusInvalidParameter;
    }
    *handle = 0;

    char *str = 0;
    if (config->verbosity >= VERBOSE_WRITE_OP) {
        str = (char *)malloc(2000);
        if (!str) return BTA_StatusOutOfMemory;
        sprintf(str, "BTAopen call:  ");
        sprintf(str + strlen(str), "  BltTofApi v%d.%d.%d", BTA_VER_MAJ, BTA_VER_MIN, BTA_VER_NON_FUNC);
        if (config->udpDataIpAddr && config->udpDataIpAddrLen) {
            sprintf(str + strlen(str), "  udpDataIpAddr %d", config->udpDataIpAddr[0]);
            for (uint8_t i = 1; i < config->udpDataIpAddrLen; i++) sprintf(str + strlen(str), ".%d", config->udpDataIpAddr[i]);
        }
        if (config->udpDataPort) sprintf(str + strlen(str), "  udpDataPort %d", config->udpDataPort);
        if (config->udpControlOutIpAddr && config->udpControlOutIpAddrLen) {
            sprintf(str + strlen(str), "  udpControlOutIpAddr %d", config->udpControlOutIpAddr[0]);
            for (uint8_t i = 1; i < config->udpControlOutIpAddrLen; i++) sprintf(str + strlen(str), ".%d", config->udpControlOutIpAddr[i]);
        }
        if (config->udpControlPort) sprintf(str + strlen(str), "  udpControlOutPort %d", config->udpControlPort);
        if (config->udpControlInIpAddr && config->udpControlInIpAddrLen) {
            sprintf(str + strlen(str), "  udpControlInIpAddr %d", config->udpControlInIpAddr[0]);
            for (uint8_t i = 1; i < config->udpControlInIpAddrLen; i++) sprintf(str + strlen(str), ".%d", config->udpControlInIpAddr[i]);
        }
        if (config->udpControlCallbackPort) sprintf(str + strlen(str), "  udpControlInPort %d", config->udpControlCallbackPort);
        if (config->tcpDeviceIpAddr && config->tcpDeviceIpAddrLen) {
            sprintf(str + strlen(str), "  tcpDeviceIpAddr %d", config->tcpDeviceIpAddr[0]);
            for (uint8_t i = 1; i < config->tcpDeviceIpAddrLen; i++) sprintf(str + strlen(str), ".%d", config->tcpDeviceIpAddr[i]);
        }
        if (config->tcpDataPort) sprintf(str + strlen(str), "  tcpDataPort %d", config->tcpDataPort);
        if (config->tcpControlPort) sprintf(str + strlen(str), "  tcpControlPort %d", config->tcpControlPort);
        if (config->uartPortName) sprintf(str + strlen(str), "  uartPortName %s", config->uartPortName);
        if (config->uartBaudRate) sprintf(str + strlen(str), "  uartBaudRate %d", config->uartBaudRate);
        if (config->uartDataBits) sprintf(str + strlen(str), "  uartDataBits %d", config->uartDataBits);
        if (config->uartStopBits) sprintf(str + strlen(str), "  uartStopBits %d", config->uartStopBits);
        if (config->uartParity) sprintf(str + strlen(str), "  uartParity %d", config->uartParity);
        if (config->uartTransmitterAddress) sprintf(str + strlen(str), "  uartTransmitterAddress %d", config->uartTransmitterAddress);
        if (config->uartReceiverAddress) sprintf(str + strlen(str), "  uartReceiverAddress %d", config->uartReceiverAddress);
        if (config->deviceType) sprintf(str + strlen(str), "  deviceType 0x%x", config->deviceType);
        if (config->pon) sprintf(str + strlen(str), "  pon %s", config->pon);
        if (config->serialNumber) sprintf(str + strlen(str), "  serialNumber %d", config->serialNumber);
        //obsolete if (config->calibFileName) sprintf(str + strlen(str), "  calibFileName %s", config->calibFileName);
        //obsolete if (config->zFactorsFileName) sprintf(str + strlen(str), "  zFactorsFileName %s", config->zFactorsFileName);
        //obsolete if (config->wigglingFileName) sprintf(str + strlen(str), "  wigglingFileName %s", config->wigglingFileName);
        if (config->frameMode) sprintf(str + strlen(str), "  frameMode %d", config->frameMode);
        if (config->infoEvent) sprintf(str + strlen(str), "  infoEvent 0x%p", config->infoEvent);
        if (config->infoEventEx) sprintf(str + strlen(str), "  infoEventEx 0x%p", config->infoEventEx);
        if (config->infoEventFilename) sprintf(str + strlen(str), "  infoEventFilename %s", config->infoEventFilename);
        if (config->verbosity) sprintf(str + strlen(str), "  verbosity %d", config->verbosity);
        if (config->frameArrived) sprintf(str + strlen(str), "  frameArrived 0x%p", config->frameArrived);
        if (config->frameArrivedEx) sprintf(str + strlen(str), "  frameArrivedEx 0x%p", config->frameArrivedEx);
        if (config->frameArrivedEx2) sprintf(str + strlen(str), "  frameArrivedEx2 0x%p", config->frameArrivedEx2);
        if (config->userArg) sprintf(str + strlen(str), "  userArg 0x%p", config->userArg);
        if (config->frameQueueLength) sprintf(str + strlen(str), "  frameQueueLength %d", config->frameQueueLength);
        if (config->frameQueueMode) sprintf(str + strlen(str), "  frameQueueMode %d", config->frameQueueMode);
        //obsolete if (config->averageWindowLength) sprintf(str + strlen(str), "  averageWindowLength %d", config->averageWindowLength);
        if (config->bltstreamFilename) sprintf(str + strlen(str), "  bltstreamFilename %s", config->bltstreamFilename);
    }

    BTA_WrapperInst *winst = (BTA_WrapperInst *)calloc(1, sizeof(BTA_WrapperInst));
    if (!winst) {
        return BTA_StatusOutOfMemory;
    }

    // Initialize LibParams that are not defaultly 0

    winst->infoEventInst = (BTA_InfoEventInst *)malloc(sizeof(BTA_InfoEventInst));
    if (!winst->infoEventInst) {
        BTAclose((BTA_Handle *)&winst);
        return BTA_StatusOutOfMemory;
    }
    winst->infoEventInst->handle = (BTA_Handle)winst;
    winst->infoEventInst->infoEvent = config->infoEvent;
    winst->infoEventInst->infoEventEx = config->infoEventEx;
    winst->infoEventInst->infoEventEx2 = config->infoEventEx2;
    winst->infoEventInst->verbosity = config->verbosity;
    winst->infoEventInst->infoEventFilename = 0;
    winst->infoEventInst->userArg = config->userArg;
    if (config->infoEventFilename) {
        winst->infoEventInst->infoEventFilename = (uint8_t *)malloc(strlen((char *)config->infoEventFilename) + 1);
        if (!winst->infoEventInst->infoEventFilename) {
            BTAclose((BTA_Handle *)&winst);
            return BTA_StatusOutOfMemory;
        }
        strcpy((char *)winst->infoEventInst->infoEventFilename, (char *)config->infoEventFilename);
    }

    if (str) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInformation, str);
        free(str);
        str = 0;
    }

    // generic config plausibility checks

    int configTest = !!config->udpDataIpAddr + !!config->udpDataIpAddrLen + !!config->udpDataPort;
    if (configTest != 0 && configTest != 3) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInvalidParameter, "BTAopen: One or more of udpDataIpAddr, udpDataIpAddrLen and udpDataPort is missing");
        return BTA_StatusInvalidParameter;
    }
    configTest = !!config->tcpDeviceIpAddr + !!config->tcpDeviceIpAddrLen + !!config->tcpControlPort;
    if (configTest != 0 && configTest != 3) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInvalidParameter, "BTAopen: One or more of tcpDeviceIpAddr, tcpDeviceIpAddrLen and tcpControlPort is missing");
        return BTA_StatusInvalidParameter;
    }
    configTest = !!config->udpControlOutIpAddr + !!config->udpControlOutIpAddrLen + !!config->udpControlPort;
    if (configTest != 0 && configTest != 3) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInvalidParameter, "BTAopen: One or more of udpControlOutIpAddr, udpControlOutIpAddrLen and udpControlPort is missing");
        return BTA_StatusInvalidParameter;
    }
    configTest = !!config->frameQueueMode + !!config->frameQueueLength;
    if (configTest != 0 && configTest != 2) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInvalidParameter, "BTAopen: One or more of frameQueueMode and frameQueueLength is missing");
        return BTA_StatusInvalidParameter;
    }
    if (config->frameQueueMode != BTA_QueueModeDoNotQueue && config->frameQueueMode != BTA_QueueModeDropCurrent && config->frameQueueMode != BTA_QueueModeDropOldest) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInvalidParameter, "BTAopen: Only queue modes DropCurrent and DropOldest are allowed");
        return BTA_StatusInvalidParameter;
    }
    if (config->frameArrivedEx2 && config->frameQueueMode) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInvalidParameter, "BTAopen: FrameArrivedEx2 cannot be used in conjunction with frame queueing");
        return BTA_StatusInvalidParameter;
    }

    if (0) { } // Otherwise syntax error if WO_ETH defined
#   ifndef BTA_WO_ETH
    else if (BTAisEthDevice(config->deviceType)) {
        config->deviceType = BTA_DeviceTypeGenericEth;
    }
#   endif
#   ifndef BTA_WO_USB
    else if (BTAisUsbDevice(config->deviceType)) {
        config->deviceType = BTA_DeviceTypeGenericUsb;
    }
#   endif
#   ifndef BTA_WO_UART
    else if (BTAisUartDevice(config->deviceType)) {
        config->deviceType = BTA_DeviceTypeGenericUart;
    }
#   endif
#   ifndef BTA_WO_STREAM
    else if (config->deviceType == BTA_DeviceTypeAny || config->deviceType == BTA_DeviceTypeBltstream) {
        // deviceType is good
    }
#   endif
    else
    {
        BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInvalidParameter, "Invalid or unsupported device type: %d", config->deviceType);
        BTAclose((BTA_Handle *)&winst);
        return BTA_StatusInvalidParameter;
    }

    if (config->calibFileName) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "BTAopen Eth: Parameter calibFileName ignored, please use BTAflashUpdate()");
    }
    if (config->zFactorsFileName) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "BTAopen Eth: Parameter zFactorsFileName ignored, not supported");
    }
    if (config->wigglingFileName) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "BTAopen Eth: Parameter wigglingFileName ignored, not supported");
    }
    if (config->averageWindowLength) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "BTAopen: Parameter averageWindowLength ignored, not supported");
    }

    if (config->frameQueueMode != BTA_QueueModeDoNotQueue && config->frameQueueMode != BTA_QueueModeDropOldest && config->frameQueueMode != BTA_QueueModeDropCurrent) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInvalidParameter, "Invalid frameQueueMode use BTA_QueueModeDoNotQueue, BTA_QueueModeDropOldest or BTA_QueueModeDropCurrent");
        BTAclose((BTA_Handle *)&winst);
        return BTA_StatusInvalidParameter;
    }

    if ((!config->frameQueueLength && config->frameQueueMode != BTA_QueueModeDoNotQueue) || (config->frameQueueLength && config->frameQueueMode == BTA_QueueModeDoNotQueue)) {
        // queueing on and queue size == 0 or queueing off and queue size > 0. Contradiction
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInvalidParameter, "Invalid frameQueueLength - frameQueueMode combination");
        BTAclose((BTA_Handle *)&winst);
        return BTA_StatusInvalidParameter;
    }

    BTA_Status status;

    if (config->frameQueueLength && (config->frameQueueMode == BTA_QueueModeDropCurrent || config->frameQueueMode == BTA_QueueModeDropOldest)) {
        status = BFQinit(config->frameQueueLength, config->frameQueueMode, &(winst->frameQueue));
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen: Error initializing frameQueue");
            BTAclose((BTA_Handle *)&winst);
            return status;
        }
    }

    winst->frameArrivedInst = (BTA_FrameArrivedInst *)malloc(sizeof(BTA_FrameArrivedInst));
    if (!winst->frameArrivedInst) {
        BTAclose((BTA_Handle *)&winst);
        return BTA_StatusOutOfMemory;
    }
    winst->frameArrivedInst->handle = (BTA_Handle)winst;
    winst->frameArrivedInst->frameArrived = config->frameArrived;
    winst->frameArrivedInst->frameArrivedEx = config->frameArrivedEx;
    winst->frameArrivedInst->frameArrivedEx2 = config->frameArrivedEx2;
    winst->frameArrivedInst->userArg = config->userArg;
    winst->frameArrivedInst->frameArrivedReturnOptions = (BTA_FrameArrivedReturnOptions *)calloc(1, sizeof(BTA_FrameArrivedReturnOptions));
    if (!winst->frameArrivedInst->frameArrivedReturnOptions) {
        BTAclose((BTA_Handle *)&winst);
        return BTA_StatusOutOfMemory;
    }

    if (!config->deviceType) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusInformation, "Trying all interfaces (device type not set)!");
    }

    BVQinit(8, BTA_QueueModeDropOldest, &winst->lpDataStreamFramesParsedPerSecFrametimes);

#   ifndef BTA_WO_ETH
    if (config->deviceType == BTA_DeviceTypeAny || config->deviceType == BTA_DeviceTypeGenericEth) {
        status = BTAETHopen(config, winst);
        if (status == BTA_StatusOk) {
            // connected to an Ethernet device
            winst->close = &BTAETHclose;
            winst->getDeviceInfo = &BTAETHgetDeviceInfo;
            winst->isRunning = &BTAETHisRunning;
            winst->isConnected = &BTAETHisConnected;
            winst->setFrameMode = &BTAETHsetFrameMode;
            winst->getFrameMode = &BTAETHgetFrameMode;
            winst->setIntegrationTime = &BTAETHsetIntegrationTime;
            winst->getIntegrationTime = &BTAETHgetIntegrationTime;
            winst->setFrameRate = &BTAETHsetFrameRate;
            winst->getFrameRate = &BTAETHgetFrameRate;
            winst->setModulationFrequency = &BTAETHsetModulationFrequency;
            winst->getModulationFrequency = &BTAETHgetModulationFrequency;
            winst->setGlobalOffset = &BTAETHsetGlobalOffset;
            winst->getGlobalOffset = &BTAETHgetGlobalOffset;
            winst->readRegister = &BTAETHreadRegister;
            winst->writeRegister = &BTAETHwriteRegister;
            winst->setLibParam = &BTAETHsetLibParam;
            winst->getLibParam = &BTAETHgetLibParam;
            winst->sendReset = &BTAETHsendReset;
            winst->flashUpdate = &BTAETHflashUpdate;
            winst->flashRead = &BTAETHflashRead;
            winst->writeCurrentConfigToNvm = &BTAETHwriteCurrentConfigToNvm;
            winst->restoreDefaultConfig = &BTAETHrestoreDefaultConfig;
            *handle = winst;
            return BTA_StatusOk;
        }

        if (config->deviceType != BTA_DeviceTypeAny) {
            // The user wanted to connect to a specific device, so do not continue trying others)
            BTAclose((BTA_Handle *)&winst);
            return status;
        }
    }
#   endif


#   ifndef BTA_WO_USB
    if (config->deviceType == BTA_DeviceTypeAny || config->deviceType == BTA_DeviceTypeGenericUsb) {
        status = BTAUSBopen(config, winst);
        if (status == BTA_StatusOk) {
            // connected to a USB device
            winst->close = &BTAUSBclose;
            winst->getDeviceInfo = &BTAUSBgetDeviceInfo;
            winst->isRunning = &BTAUSBisRunning;
            winst->isConnected = &BTAUSBisConnected;
            winst->setFrameMode = &BTAUSBsetFrameMode;
            winst->getFrameMode = &BTAUSBgetFrameMode;
            winst->setIntegrationTime = &BTAUSBsetIntegrationTime;
            winst->getIntegrationTime = &BTAUSBgetIntegrationTime;
            winst->setFrameRate = &BTAUSBsetFrameRate;
            winst->getFrameRate = &BTAUSBgetFrameRate;
            winst->setModulationFrequency = &BTAUSBsetModulationFrequency;
            winst->getModulationFrequency = &BTAUSBgetModulationFrequency;
            winst->setGlobalOffset = &BTAUSBsetGlobalOffset;
            winst->getGlobalOffset = &BTAUSBgetGlobalOffset;
            winst->readRegister = &BTAUSBreadRegister;
            winst->writeRegister = &BTAUSBwriteRegister;
            winst->setLibParam = &BTAUSBsetLibParam;
            winst->getLibParam = &BTAUSBgetLibParam;
            winst->sendReset = &BTAUSBsendReset;
            winst->flashUpdate = &BTAUSBflashUpdate;
            winst->flashRead = &BTAUSBflashRead;
            winst->writeCurrentConfigToNvm = &BTAUSBwriteCurrentConfigToNvm;
            winst->restoreDefaultConfig = &BTAUSBrestoreDefaultConfig;
            *handle = winst;
            return BTA_StatusOk;
        }

        if (config->deviceType != BTA_DeviceTypeAny) {
            // The user wanted to connect to a specific device, so do not continue trying others)
            BTAclose((BTA_Handle *)&winst);
            return status;
        }
    }
#   endif


#   ifndef BTA_WO_UART
    if (!config->deviceType || config->deviceType == BTA_DeviceTypeGenericUart) {
        status = BTAUARTopen(config, winst);
        if (status == BTA_StatusOk) {
            // connected to a UART device
            winst->close = &BTAUARTclose;
            winst->getDeviceInfo = &BTAUARTgetDeviceInfo;
            winst->isRunning = &BTAUARTisRunning;
            winst->isConnected = &BTAUARTisConnected;
            winst->setFrameMode = &BTAUARTsetFrameMode;
            winst->getFrameMode = &BTAUARTgetFrameMode;
            winst->setIntegrationTime = &BTAUARTsetIntegrationTime;
            winst->getIntegrationTime = &BTAUARTgetIntegrationTime;
            winst->setFrameRate = &BTAUARTsetFrameRate;
            winst->getFrameRate = &BTAUARTgetFrameRate;
            winst->setModulationFrequency = &BTAUARTsetModulationFrequency;
            winst->getModulationFrequency = &BTAUARTgetModulationFrequency;
            winst->setGlobalOffset = &BTAUARTsetGlobalOffset;
            winst->getGlobalOffset = &BTAUARTgetGlobalOffset;
            winst->readRegister = &BTAUARTreadRegister;
            winst->writeRegister = &BTAUARTwriteRegister;
            winst->setLibParam = &BTAUARTsetLibParam;
            winst->getLibParam = &BTAUARTgetLibParam;
            winst->sendReset = &BTAUARTsendReset;
            winst->flashUpdate = &BTAUARTflashUpdate;
            winst->flashRead = &BTAUARTflashRead;
            winst->writeCurrentConfigToNvm = &BTAUARTwriteCurrentConfigToNvm;
            winst->restoreDefaultConfig = &BTAUARTrestoreDefaultConfig;
            *handle = winst;
            return BTA_StatusOk;
        }

        if (config->deviceType != BTA_DeviceTypeAny) {
            // The user wanted to connect to a specific device, so do not continue trying others)
            BTAclose((BTA_Handle *)&winst);
            return status;
        }
    }
#   endif


#   ifndef BTA_WO_STREAM
    if (!config->deviceType || config->deviceType == BTA_DeviceTypeBltstream) {
        status = BTASTREAMopen(config, winst);
        if (status == BTA_StatusOk) {
            // connected to a Bltstream file
            winst->close = &BTASTREAMclose;
            winst->getDeviceInfo = &BTASTREAMgetDeviceInfo;
            winst->isRunning = &BTASTREAMisRunning;
            winst->isConnected = &BTASTREAMisConnected;
            winst->setFrameMode = &BTASTREAMsetFrameMode;
            winst->getFrameMode = &BTASTREAMgetFrameMode;
            winst->setIntegrationTime = &BTASTREAMsetIntegrationTime;
            winst->getIntegrationTime = &BTASTREAMgetIntegrationTime;
            winst->setFrameRate = &BTASTREAMsetFrameRate;
            winst->getFrameRate = &BTASTREAMgetFrameRate;
            winst->setModulationFrequency = &BTASTREAMsetModulationFrequency;
            winst->getModulationFrequency = &BTASTREAMgetModulationFrequency;
            winst->setGlobalOffset = &BTASTREAMsetGlobalOffset;
            winst->getGlobalOffset = &BTASTREAMgetGlobalOffset;
            winst->readRegister = &BTASTREAMreadRegister;
            winst->writeRegister = &BTASTREAMwriteRegister;
            winst->setLibParam = &BTASTREAMsetLibParam;
            winst->getLibParam = &BTASTREAMgetLibParam;
            winst->sendReset = &BTASTREAMsendReset;
            winst->flashUpdate = &BTASTREAMflashUpdate;
            winst->flashRead = &BTASTREAMflashRead;
            winst->writeCurrentConfigToNvm = &BTASTREAMwriteCurrentConfigToNvm;
            winst->restoreDefaultConfig = &BTASTREAMrestoreDefaultConfig;
            *handle = winst;
            return BTA_StatusOk;
        }

        if (config->deviceType != BTA_DeviceTypeAny) {
            // The user wanted to connect to a specific device, so do not continue trying others)
            BTAclose((BTA_Handle *)&winst);
            return status;
        }
    }
#   endif


    BTAclose((BTA_Handle *)&winst);
    return BTA_StatusDeviceUnreachable;
}


BTA_Status BTA_CALLCONV BTAclose(BTA_Handle *handle) {
    if (!handle) {
        return BTA_StatusInvalidParameter;
    }
    BTA_WrapperInst *winst = (BTA_WrapperInst *)*handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAclose call");
    if (winst->close) {
        winst->close(winst);  // close always returns ok. infoevents are called within
    }

    BTA_Status status = BFQclose(&(winst->frameQueue));
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose: Failed to close frameQueue");
    }

    status = BGRBclose(&(winst->grabInst));
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose: Failed to close grabber");
    }

    if (winst->infoEventInst) {
        free(winst->infoEventInst->infoEventFilename);
        winst->infoEventInst->infoEventFilename = 0;
    }
    free(winst->infoEventInst);
    winst->infoEventInst = 0;
    if (winst->frameArrivedInst) {
        free(winst->frameArrivedInst->frameArrivedReturnOptions);
        winst->frameArrivedInst->frameArrivedReturnOptions = 0;
    }
    free(winst->frameArrivedInst);
    winst->frameArrivedInst = 0;
    free(winst);
    winst = 0;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAgetDeviceInfo(BTA_Handle handle, BTA_DeviceInfo **deviceInfo) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetDeviceInfo call");
    BTA_Status status = winst->getDeviceInfo(winst, deviceInfo);
    if (status == BTA_StatusOk) BTAinfoEventHelperISIIIII(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetDeviceInfo response: device type 0x%04x  PON %s-%06d  fwVer %d.%d.%d  uptime %ds",
                                                          (*deviceInfo)->deviceType, (*deviceInfo)->productOrderNumber, (*deviceInfo)->serialNumber, (*deviceInfo)->firmwareVersionMajor, (*deviceInfo)->firmwareVersionMinor, (*deviceInfo)->firmwareVersionNonFunc, (*deviceInfo)->uptime);
    return status;
}


uint8_t BTA_CALLCONV BTAisRunning(BTA_Handle handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return 0;
    }
    return winst->isRunning(winst);
}


uint8_t BTA_CALLCONV BTAisConnected(BTA_Handle handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return 0;
    }
    return winst->isConnected(winst);
}


BTA_Status BTA_CALLCONV BTAsetFrameMode(BTA_Handle handle, BTA_FrameMode frameMode) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }

    BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetFrameMode call:  frameMode %d", frameMode);
    return winst->setFrameMode(winst, frameMode);
}


BTA_Status BTA_CALLCONV BTAgetFrameMode(BTA_Handle handle, BTA_FrameMode *frameMode) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetFrameMode call");

    BTA_Status status = winst->getFrameMode(winst, frameMode);
    if (status == BTA_StatusOk) BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetFrameMode response:  frameMode %d", *frameMode);
    return status;
}


BTA_Status BTA_CALLCONV BTAgetFrame(BTA_Handle handle, BTA_Frame **frame, uint32_t millisecondsTimeout) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }

    if (!winst->frameQueue) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusIllegalOperation, "BTAgetFrame: Frame queueing must be enabled in BTAopen");
        return BTA_StatusIllegalOperation;
    }
    BTA_Status status = BFQdequeue(winst->frameQueue, frame, millisecondsTimeout);
    return status;
}


BTA_Status BTA_CALLCONV BTAgetFrameCount(BTA_Handle handle, uint32_t *frameCount) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetFrameCount call");

    if (!winst->frameQueue) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusIllegalOperation, "BTAgetFrameCount: Frame queueing must be enabled in BTAopen");
        return BTA_StatusIllegalOperation;
    }

    BTA_Status status = BFQgetCount(winst->frameQueue, frameCount);
    return status;
}


BTA_Status BTA_CALLCONV BTAflushFrameQueue(BTA_Handle handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAflushFrameQueue call");

    if (!winst->frameQueue) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusIllegalOperation, "BTAflushFrameQueue: Frame queueing must be enabled in BTAopen");
        return BTA_StatusIllegalOperation;
    }

    BTA_Status status = BFQclear(winst->frameQueue);
    return status;
}


BTA_Status BTA_CALLCONV BTAsetIntegrationTime(BTA_Handle handle, uint32_t integrationTime) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetIntegrationTime call:  integrationTime %d", integrationTime);
    return winst->setIntegrationTime(winst, integrationTime);
}


BTA_Status BTA_CALLCONV BTAgetIntegrationTime(BTA_Handle handle, uint32_t *integrationTime) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetIntegrationTime call");
    BTA_Status status = winst->getIntegrationTime(winst, integrationTime);
    if (status == BTA_StatusOk) BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetIntegrationTime response:  integrationTime %d", *integrationTime);
    return status;
}


BTA_Status BTA_CALLCONV BTAsetFrameRate(BTA_Handle handle, float frameRate) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelperF(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetFrameRate call:  frameRate %f", frameRate);
    return winst->setFrameRate(winst, frameRate);
}


BTA_Status BTA_CALLCONV BTAgetFrameRate(BTA_Handle handle, float *frameRate) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetFrameRate call");
    BTA_Status status = winst->getFrameRate(winst, frameRate);
    if (status == BTA_StatusOk) BTAinfoEventHelperF(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetFrameRate response:  frameRate %f", *frameRate);
    return status;
}


BTA_Status BTA_CALLCONV BTAsetModulationFrequency(BTA_Handle handle, uint32_t modulationFrequency) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetModulationFrequency call:  modulationFrequency %d", modulationFrequency);
    return winst->setModulationFrequency(winst, modulationFrequency);
}


BTA_Status BTA_CALLCONV BTAgetModulationFrequency(BTA_Handle handle, uint32_t *modulationFrequency) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetModulationFrequency call");
    BTA_Status status = winst->getModulationFrequency(winst, modulationFrequency);
    if (status == BTA_StatusOk) BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetModulationFrequency response:  modulationFrequency %d", *modulationFrequency);
    return status;
}


BTA_Status BTA_CALLCONV BTAsetGlobalOffset(BTA_Handle handle, float globalOffset) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelperF(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetGlobalOffset call:  globalOffset %f", globalOffset);
    return winst->setGlobalOffset(winst, globalOffset);
}


BTA_Status BTA_CALLCONV BTAgetGlobalOffset(BTA_Handle handle, float *globalOffset) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetGlobalOffset call");
    BTA_Status status = winst->getGlobalOffset(winst, globalOffset);
    if (status == BTA_StatusOk) BTAinfoEventHelperF(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetGlobalOffset response:  globalOffset %f", *globalOffset);
    return status;
}


BTA_Status BTA_CALLCONV BTAreadRegister(BTA_Handle handle, uint32_t address, uint32_t *data, uint32_t *registerCount) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (registerCount) BTAinfoEventHelperII(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAreadRegister call:  address 0x%04x  registerCount %d", address, *registerCount);
    else BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAreadRegister call:  address 0x%04x", address);
    BTA_Status status = winst->readRegister(winst, address, data, registerCount);
    if (status == BTA_StatusOk && winst->infoEventInst->verbosity >= VERBOSE_READ_OP) {
        char *str = (char *)malloc(1024);
        if (!str) return BTA_StatusOutOfMemory;
        sprintf(str, "BTAreadRegister response:  address 0x%04x", address);
        if (data) {
            sprintf(str + strlen(str), "  data 0x%x", data[0]);
        }
        if (registerCount && data) {
            uint32_t i;
            for (i = 1; i < *registerCount; i++) {
                if (i > 10) {
                    sprintf(str + strlen(str), " ...");
                    break;
                }
                sprintf(str + strlen(str), ", 0x%x", data[i]);
            }
            sprintf(str + strlen(str), "  registerCount %d", *registerCount);
        }
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, str);
        free(str);
        str = 0;
    }
    return status;
}


BTA_Status BTA_CALLCONV BTAwriteRegister(BTA_Handle handle, uint32_t address, uint32_t *data, uint32_t *registerCount) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (winst->infoEventInst->verbosity >= VERBOSE_WRITE_OP) {
        char *str = (char *)malloc(1024);
        if (!str) return BTA_StatusOutOfMemory;
        sprintf(str, "BTAwriteRegister call:  address 0x%04x", address);
        if (data) {
            sprintf(str + strlen(str), "  data 0x%x", data[0]);
        }
        if (registerCount && data) {
            for (uint16_t i = 1; i < *registerCount; i++) {
                if (i > 10) {
                    sprintf(str + strlen(str), " ...");
                    break;
                }
                sprintf(str + strlen(str), ", 0x%x", data[i]);
            }
            sprintf(str + strlen(str), "  registerCount %d", *registerCount);
        }
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, str);
        free(str);
        str = 0;
    }
    return winst->writeRegister(winst, address, data, registerCount);
}


BTA_Status BTA_CALLCONV BTAsetLibParam(BTA_Handle handle, BTA_LibParam libParam, float value) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelperISF(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetLibParam call:  libParam %d (%s)  value %f", (int)libParam, (uint8_t *)BTAlibParamToString(libParam), value);
    switch (libParam) {
    case BTA_LibParamInfoEventVerbosity:
        if (!winst->infoEventInst) {
            return BTA_StatusRuntimeError;
        }
        winst->infoEventInst->verbosity = (int)value;
        return BTA_StatusOk;
    case BTA_LibParamTestPatternEnabled:
        winst->lpTestPatternEnabled = (uint16_t)value;
        return BTA_StatusOk;
    case BTA_LibParamUndistortRgb:
        if (!winst->undistortInst) {
            return BTA_StatusRuntimeError;
        }
        winst->undistortInst->enabled = (uint8_t)(value != 0);
        return BTA_StatusOk;
#   ifndef WO_LIBJPEG
    case BTA_LibParamEnableJpgDecoding:
        return BTAjpgEnable(winst, (uint8_t)(value != 0));
#   endif

    case BTA_LibParamPauseCaptureThread:
        winst->lpPauseCaptureThread = value > 0 ? 1 : 0;
        return BTA_StatusOk;


    case BTA_LibParamDataStreamReadFailedCount:
    case BTA_LibParamDataStreamBytesReceivedCount:
    case BTA_LibParamDataStreamPacketsReceivedCount:
    case BTA_LibParamDataStreamPacketsMissedCount:
    case BTA_LibParamDataStreamPacketsToParse:
    case BTA_LibParamDataStreamParseFrameDuration:
    case BTA_LibParamDataStreamFrameCounterGapsCount:
    case BTA_LibParamDataStreamFramesParsedCount:
    case BTA_LibParamDataStreamFramesParsedPerSec:
        return BTA_StatusIllegalOperation;

    case BTA_LibParamDebugFlags01:
        winst->lpDebugFlags01 = (uint32_t)value;
        return BTA_StatusOk;
    case BTA_LibParamDebugValue01:
        winst->lpDebugValue01 = value;
        return BTA_StatusOk;
    case BTA_LibParamDebugValue02:
        winst->lpDebugValue02 = value;
        return BTA_StatusOk;
    case BTA_LibParamDebugValue03:
        winst->lpDebugValue03 = value;
        return BTA_StatusOk;
    case BTA_LibParamDebugValue04:
        winst->lpDebugValue04 = value;
        return BTA_StatusOk;
    case BTA_LibParamDebugValue05:
        winst->lpDebugValue05 = value;
        return BTA_StatusOk;
    case BTA_LibParamDebugValue06:
        winst->lpDebugValue06 = value;
        return BTA_StatusOk;
    case BTA_LibParamDebugValue07:
        winst->lpDebugValue07 = value;
        return BTA_StatusOk;
    case BTA_LibParamDebugValue08:
        winst->lpDebugValue08 = value;
        return BTA_StatusOk;
    case BTA_LibParamDebugValue09:
        winst->lpDebugValue09 = value;
        return BTA_StatusOk;
    case BTA_LibParamDebugValue10:
        winst->lpDebugValue10 = value;
        return BTA_StatusOk;

    default:
        return winst->setLibParam(winst, libParam, value);
    }
}


BTA_Status BTA_CALLCONV BTAgetLibParam(BTA_Handle handle, BTA_LibParam libParam, float *value) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst || !value) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelperIS(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAgetLibParam call:  libParam %d (%s)", (int)libParam, (uint8_t *)BTAlibParamToString(libParam));
    BTA_Status status;
    switch (libParam) {
    case BTA_LibParamInfoEventVerbosity:
        if (!winst->infoEventInst) {
            *value = 0;
        }
        else {
            *value = (float)winst->infoEventInst->verbosity;
        }
        status = BTA_StatusOk;
        break;
    case BTA_LibParamTestPatternEnabled:
        *value = winst->lpTestPatternEnabled;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamUndistortRgb:
        if (!winst->undistortInst) {
            *value = 0;
        }
        else {
            *value = (float)winst->undistortInst->enabled;
        }
        status = BTA_StatusOk;
        break;
#   ifndef WO_LIBJPEG
    case BTA_LibParamEnableJpgDecoding: {
        uint8_t enabled;
        status = BTAjpgIsEnabled(winst, &enabled);
        if (status == BTA_StatusOk) {
            *value = enabled;
        }
        break;
    }
#   endif

    case BTA_LibParamPauseCaptureThread:
        *value = (float)winst->lpPauseCaptureThread;
        status = BTA_StatusOk;
        break;


    case BTA_LibParamDataStreamReadFailedCount:
        *value = winst->lpDataStreamReadFailedCount;
        winst->lpDataStreamReadFailedCount = 0;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDataStreamBytesReceivedCount:
        *value = winst->lpDataStreamBytesReceivedCount;
        winst->lpDataStreamBytesReceivedCount = 0;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDataStreamPacketsReceivedCount:
        *value = winst->lpDataStreamPacketsReceivedCount;
        winst->lpDataStreamPacketsReceivedCount = 0;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDataStreamPacketsMissedCount:
        *value = winst->lpDataStreamPacketsMissedCount;
        winst->lpDataStreamPacketsMissedCount = 0;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDataStreamPacketsToParse:
        *value = winst->lpDataStreamPacketsToParse;
        winst->lpDataStreamPacketsToParse = 0;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDataStreamParseFrameDuration:
        *value = winst->lpDataStreamParseFrameDuration;
        winst->lpDataStreamParseFrameDuration = 0;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDataStreamFrameCounterGapsCount:
        *value = winst->lpDataStreamFrameCounterGapsCount;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDataStreamFramesParsedCount:
        *value = winst->lpDataStreamFramesParsedCount;
        winst->lpDataStreamFramesParsedCount = 0;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDataStreamFramesParsedPerSec: {
        void **list;
        uint32_t listLen;
        uint32_t microsSum = 0;
        uint32_t count = 0;
        BVQgetList(winst->lpDataStreamFramesParsedPerSecFrametimes, &list, &listLen);
        for (uint32_t i = 0; i < listLen; i++) {
            microsSum += (uint32_t)(uint64_t)(list[i]);
            count++;
            if (microsSum > 1500) {
                break;
            }
        }
        free(list);
        list = 0;
        if (microsSum == 0 || count == 0) {
            *value = 0;
            status = BTA_StatusOk;
            break;
        }
        float microsAverage = (float)microsSum / count;
        uint32_t millisPassed = (uint32_t)(BTAgetTickCount() - winst->lpDataStreamFramesParsedPerSecUpdated);
        if (millisPassed > 5000) {
            // not updated in 5 seconds
            *value = 0;
        }
        else if (millisPassed > 5 * microsAverage / 1000) {
            // not updated in 5 times the time we expect it to have been updated
            *value = (int)(5000.0f / millisPassed + 5000000.0f / microsAverage) / 10.0f;
        }
        else {
            *value = (int)(10000000.0f / microsAverage) / 10.0f;
        }
        status = BTA_StatusOk;
        break;
    }

    case BTA_LibParamDebugFlags01:
        *value = (float)winst->lpDebugFlags01;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDebugValue01:
        *value = (float)winst->lpDebugValue01;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDebugValue02:
        *value = (float)winst->lpDebugValue02;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDebugValue03:
        *value = (float)winst->lpDebugValue03;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDebugValue04:
        *value = (float)winst->lpDebugValue04;
        winst->lpDebugValue04 = 0;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDebugValue05:
        *value = (float)winst->lpDebugValue05;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDebugValue06:
        *value = (float)winst->lpDebugValue06;
        winst->lpDebugValue06 = 0;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDebugValue07:
        *value = (float)winst->lpDebugValue07;
        winst->lpDebugValue07 = 0;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDebugValue08:
        *value = (float)winst->lpDebugValue08;
        winst->lpDebugValue08 = 0;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDebugValue09:
        *value = (float)winst->lpDebugValue09;
        winst->lpDebugValue09 = 0;
        status = BTA_StatusOk;
        break;
    case BTA_LibParamDebugValue10:
        *value = (float)winst->lpDebugValue10;
        winst->lpDebugValue10 = 0;
        status = BTA_StatusOk;
        break;

    default:
        status = winst->getLibParam(winst, libParam, value);
        break;
    }
    if (status == BTA_StatusOk) BTAinfoEventHelperF(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetLibParam response:  value %f", *value);
    return status;
}


BTA_Status BTA_CALLCONV BTAsendReset(BTA_Handle handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (winst->infoEventInst->verbosity >= VERBOSE_WRITE_OP) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsendReset call");
    }
    return winst->sendReset(winst);
}


BTA_Status BTA_CALLCONV BTAflashUpdate(BTA_Handle handle, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst || !flashUpdateConfig) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelperIIIVI(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAflashUpdate call:  target %d  flashId %d  address 0x%x  data 0x%p  dataLen %d",
                            flashUpdateConfig->target, flashUpdateConfig->flashId, flashUpdateConfig->address, flashUpdateConfig->data, flashUpdateConfig->dataLen);
    return winst->flashUpdate(winst, flashUpdateConfig, progressReport);
}


BTA_Status BTA_CALLCONV BTAgetLensParameters(BTA_Handle handle, BTA_IntrinsicData *intData, uint32_t *intDataLen, BTA_ExtrinsicData *extData, uint32_t *extDataLen) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (intData && intDataLen) {
        memset(intData, 0, *intDataLen * sizeof(BTA_IntrinsicData));
    }
    if (extData && extDataLen) {
        memset(extData, 0, *extDataLen * sizeof(BTA_ExtrinsicData));
    }

    BTA_IntrinsicData **intDataTemp = 0;
    uint16_t intDataLenTemp = 0;
    BTA_ExtrinsicData **extDataTemp = 0;
    uint16_t extDataLenTemp = 0;
    BTA_Status status = BTAreadGeomModelFromFlash(winst, &intDataTemp, &intDataLenTemp, &extDataTemp, &extDataLenTemp, 0);
    if (status != BTA_StatusOk) {
        *intDataLen = 0;
        *extDataLen = 0;
        return status;
    }

    if (intData && intDataLen) {
        if (*intDataLen < intDataLenTemp) {
            BTAfreeIntrinsicData(&intDataTemp, intDataLenTemp);
            BTAfreeExtrinsicData(&extDataTemp, extDataLenTemp);
            return BTA_StatusOutOfMemory;
        }
        *intDataLen = intDataLenTemp;
        for (int i = 0; i < intDataLenTemp; i++) {
            if (intDataTemp[i]) {
                memcpy(&(intData[i]), intDataTemp[i], sizeof(BTA_IntrinsicData));
            }
            else
            {
                memset(&(intData[i]), 0, sizeof(BTA_IntrinsicData));
            }
        }
        BTAfreeIntrinsicData(&intDataTemp, intDataLenTemp);
    }
    if (extData && extDataLen) {
        if (*extDataLen < extDataLenTemp) {
            BTAfreeExtrinsicData(&extDataTemp, extDataLenTemp);
            return BTA_StatusOutOfMemory;
        }
        *extDataLen = extDataLenTemp;
        for (int i = 0; i < extDataLenTemp; i++) {
            if (extDataTemp[i]) {
                memcpy(&(extData[i]), extDataTemp[i], sizeof(BTA_ExtrinsicData));
            }
            else
            {
                memset(&(extData[i]), 0, sizeof(BTA_ExtrinsicData));
            }
        }
        BTAfreeExtrinsicData(&extDataTemp, extDataLenTemp);
    }
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAflashRead(BTA_Handle handle, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst || !flashUpdateConfig) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelperIIIVI(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAflashRead call:  target %d  flashId %d  address 0x%x  data 0x%p  dataLen %d",
                            flashUpdateConfig->target, flashUpdateConfig->flashId, flashUpdateConfig->address, flashUpdateConfig->data, flashUpdateConfig->dataLen);
    return winst->flashRead(winst, flashUpdateConfig, progressReport, 0);
}


BTA_Status BTA_CALLCONV BTAwriteCurrentConfigToNvm(BTA_Handle handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAwriteCurrentConfigToNvm call");
    return winst->writeCurrentConfigToNvm(winst);
}


BTA_Status BTA_CALLCONV BTArestoreDefaultConfig(BTA_Handle handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst*)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTArestoreDefaultConfig call");
    return winst->restoreDefaultConfig(winst);
}


BTA_Status BTA_CALLCONV BTAstartGrabbing(BTA_Handle handle, BTA_GrabbingConfig *config) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (config) BTAinfoEventHelperS(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAstartGrabbing call  filename %s", config->filename);
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAstartGrabbing call  grabbingConfig null");
    uint8_t libNameVer[70];
    sprintf((char *)libNameVer, "BltTofApiLib v%d.%d.%d", BTA_VER_MAJ, BTA_VER_MIN, BTA_VER_NON_FUNC);
    BTA_DeviceInfo *deviceInfo;
    BTA_Status status = winst->getDeviceInfo(winst, &deviceInfo);
    if (status != BTA_StatusOk) {
        // allow grabbing without control connection
        deviceInfo = (BTA_DeviceInfo *)calloc(1, sizeof(BTA_DeviceInfo));
        if (!deviceInfo) {
            return BTA_StatusOutOfMemory;
        }
    }
    if (!config) {
        BTA_GrabInst *instTemp = winst->grabInst;
        // had problems, so try to set it null immediately
        winst->grabInst = 0;
        return BGRBclose(&instTemp);
    }
    status = BGRBinit(config, libNameVer, deviceInfo, &(winst->grabInst), winst->infoEventInst);
    BTAfreeDeviceInfo(deviceInfo);
    return status;
}


BTA_Status BTA_CALLCONV BTAfirmwareUpdate(BTA_Handle handle, const uint8_t *filename, FN_BTA_ProgressReport progressReport) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelperSV(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAfirmwareUpdate call  filename %s  progressReport 0x%p", (uint8_t *)filename, (void *)progressReport);
    return flashUpdate(winst, filename, progressReport, BTA_FlashTargetApplication);
}

BTA_Status BTA_CALLCONV BTAfpnUpdate(BTA_Handle handle, const uint8_t *filename, FN_BTA_ProgressReport progressReport) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelperSV(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAfpnUpdate call  filename %s  progressReport 0x%p", (uint8_t *)filename, (void *)progressReport);
    return flashUpdate(winst, filename, progressReport, BTA_FlashTargetFpn);
}

BTA_Status BTA_CALLCONV BTAfppnUpdate(BTA_Handle handle, const uint8_t *filename, FN_BTA_ProgressReport progressReport) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelperSV(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAfppnUpdate call  filename %s  progressReport 0x%p", (uint8_t *)filename, (void *)progressReport);
    return flashUpdate(winst, filename, progressReport, BTA_FlashTargetFppn);
}

BTA_Status BTA_CALLCONV BTAwigglingUpdate(BTA_Handle handle, const uint8_t *filename, FN_BTA_ProgressReport progressReport) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelperSV(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAwigglingUpdate call  filename %s  progressReport 0x%p", (uint8_t *)filename, (void *)progressReport);
    return flashUpdate(winst, filename, progressReport, BTA_FlashTargetWigglingCalibration);
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle-less functions




BTA_Status BTA_CALLCONV BTAgetDistances(BTA_Frame *frame, void **distBuffer, BTA_DataFormat *dataFormat, BTA_Unit *unit, uint16_t *xRes, uint16_t *yRes) {
    int chInd;
    if (!frame || !distBuffer || !dataFormat || !unit || !xRes || !yRes) {
        return BTA_StatusInvalidParameter;
    }
    if (frame->channels) {
        for (chInd = 0; chInd < frame->channelsLen; chInd++) {
            if (frame->channels[chInd]->id == BTA_ChannelIdDistance) {
                *dataFormat = frame->channels[chInd]->dataFormat;
                *unit = frame->channels[chInd]->unit;
                *xRes = frame->channels[chInd]->xRes;
                *yRes = frame->channels[chInd]->yRes;
                *distBuffer = frame->channels[chInd]->data;
                return BTA_StatusOk;
            }
        }
    }
    return BTA_StatusInvalidParameter;
}


BTA_Status BTA_CALLCONV BTAgetAmplitudes(BTA_Frame *frame, void **ampBuffer, BTA_DataFormat *dataFormat, BTA_Unit *unit, uint16_t *xRes, uint16_t *yRes) {
    int chInd;
    if (!frame || !ampBuffer || !dataFormat || !unit || !xRes || !yRes) {
        return BTA_StatusInvalidParameter;
    }
    if (frame->channels) {
        for (chInd = 0; chInd < frame->channelsLen; chInd++) {
            if (frame->channels[chInd]->id == BTA_ChannelIdAmplitude) {
                *dataFormat = frame->channels[chInd]->dataFormat;
                *unit = frame->channels[chInd]->unit;
                *xRes = frame->channels[chInd]->xRes;
                *yRes = frame->channels[chInd]->yRes;
                *ampBuffer = frame->channels[chInd]->data;
                return BTA_StatusOk;
            }
        }
    }
    return BTA_StatusInvalidParameter;
}


BTA_Status BTA_CALLCONV BTAgetFlags(BTA_Frame *frame, void **flagBuffer, BTA_DataFormat *dataFormat, BTA_Unit *unit, uint16_t *xRes, uint16_t *yRes) {
    int i;
    if (!frame || !flagBuffer || !dataFormat || !unit || !xRes || !yRes) {
        return BTA_StatusInvalidParameter;
    }
    if (frame->channels) {
        for (i = 0; i < frame->channelsLen; i++) {
            if (frame->channels[i]->id == BTA_ChannelIdFlags) {
                *dataFormat = frame->channels[i]->dataFormat;
                *unit = frame->channels[i]->unit;
                *xRes = frame->channels[i]->xRes;
                *yRes = frame->channels[i]->yRes;
                *flagBuffer = frame->channels[i]->data;
                return BTA_StatusOk;
            }
        }
    }
    return BTA_StatusInvalidParameter;
}


BTA_Status BTA_CALLCONV BTAgetXYZcoordinates(BTA_Frame *frame, void **xBuffer, void **yBuffer, void **zBuffer, BTA_DataFormat *dataFormat, BTA_Unit *unit, uint16_t *xRes, uint16_t *yRes) {
    int chInd;
    int xChannel = -1;
    int yChannel = -1;
    int zChannel = -1;
    if (!frame || !xBuffer || !yBuffer || !zBuffer || !dataFormat || !unit || !xRes || !yRes) {
        return BTA_StatusInvalidParameter;
    }
    if (frame->channels) {
        for (chInd = 0; chInd < frame->channelsLen; chInd++) {
            if (frame->channels[chInd]->id == BTA_ChannelIdX) {
                xChannel = chInd;
            }
            if (frame->channels[chInd]->id == BTA_ChannelIdY) {
                yChannel = chInd;
            }
            if (frame->channels[chInd]->id == BTA_ChannelIdZ) {
                zChannel = chInd;
            }
        }
        if (xChannel >= 0 && yChannel >= 0 && zChannel >= 0) {
            *dataFormat = frame->channels[xChannel]->dataFormat;
            if (*dataFormat != frame->channels[yChannel]->dataFormat || *dataFormat != frame->channels[zChannel]->dataFormat) {
                return BTA_StatusInvalidParameter;
            }
            *unit = frame->channels[xChannel]->unit;
            if (*unit != frame->channels[yChannel]->unit || *unit != frame->channels[zChannel]->unit) {
                return BTA_StatusInvalidParameter;
            }
            *xRes = frame->channels[xChannel]->xRes;
            if (*xRes != frame->channels[yChannel]->xRes || *xRes != frame->channels[zChannel]->xRes) {
                return BTA_StatusInvalidParameter;
            }
            *yRes = frame->channels[xChannel]->yRes;
            if (*yRes != frame->channels[yChannel]->yRes || *yRes != frame->channels[zChannel]->yRes) {
                return BTA_StatusInvalidParameter;
            }
            *xBuffer = frame->channels[xChannel]->data;
            *yBuffer = frame->channels[yChannel]->data;
            *zBuffer = frame->channels[zChannel]->data;
            return BTA_StatusOk;
        }
    }
    return BTA_StatusInvalidParameter;
}


BTA_Status BTA_CALLCONV BTAgetColors(BTA_Frame *frame, void **colorBuffer, BTA_DataFormat *dataFormat, BTA_Unit *unit, uint16_t *xRes, uint16_t *yRes) {
    int chInd;
    if (!frame || !colorBuffer || !dataFormat || !unit || !xRes || !yRes) {
        return BTA_StatusInvalidParameter;
    }
    if (frame->channels) {
        for (chInd = 0; chInd < frame->channelsLen; chInd++) {
            if (frame->channels[chInd]->id == BTA_ChannelIdColor) {
                *dataFormat = frame->channels[chInd]->dataFormat;
                *unit = frame->channels[chInd]->unit;
                *xRes = frame->channels[chInd]->xRes;
                *yRes = frame->channels[chInd]->yRes;
                *colorBuffer = frame->channels[chInd]->data;
                return BTA_StatusOk;
            }
        }
    }
    return BTA_StatusInvalidParameter;
}


BTA_Status BTA_CALLCONV BTAgetMetadata(BTA_Channel *channel, uint32_t metadataId, void **metadata, uint32_t *metadataLen) {
    if (!channel || !metadata || !metadataLen) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t mdInd;
    for (mdInd = 0; mdInd < channel->metadataLen; mdInd++) {
        if (channel->metadata[mdInd]->id == metadataId) {
            *metadata = channel->metadata[mdInd]->data;
            *metadataLen = channel->metadata[mdInd]->dataLen;
            return BTA_StatusOk;
        }
    }
    return BTA_StatusInvalidParameter;
}


BTA_Status BTA_CALLCONV BTAcloneFrame(BTA_Frame *frameSrc, BTA_Frame **frameDst) {
    int chInd;
    BTA_Frame *frame;
    if (!frameSrc || !frameDst) {
        return BTA_StatusInvalidParameter;
    }
    *frameDst = 0;
    frame = (BTA_Frame *)malloc(sizeof(BTA_Frame));
    if (!frame) {
        return BTA_StatusOutOfMemory;
    }
    memcpy(frame, frameSrc, sizeof(BTA_Frame));
    frame->channels = (BTA_Channel **)calloc(frame->channelsLen, sizeof(BTA_Channel *));;
    for (chInd = 0; chInd < frameSrc->channelsLen; chInd++) {
        BTA_Status status = BTAcloneChannel(frameSrc->channels[chInd], &(frame->channels[chInd]));
        if (status != BTA_StatusOk) {
            for (chInd--; chInd >= 0; chInd--) {
                BTAfreeChannel(&(frame->channels[chInd]));
            }
            free(frame->channels);
            frame->channels = 0;
            free(frame);
            frame = 0;
            return status;
        }
    }
    *frameDst = frame;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAgetSerializedLength(BTA_Frame *frame, uint32_t *frameSerializedLen) {
    uint32_t length = 0;
    uint32_t chInd;
    uint32_t mdInd;
    length += 2;                            // preamble
    length++;                               // version of serialized frame
    length += sizeof(uint8_t);              // firmwareVersionNonFunc
    length += sizeof(uint8_t);              // firmwareVersionMinor
    length += sizeof(uint8_t);              // firmwareVersionMajor
    length += sizeof(float);                // mainTemp
    length += sizeof(float);                // ledTemp
    length += sizeof(float);                // genericTemp
    length += sizeof(uint32_t);             // frameCounter
    length += sizeof(uint32_t);             // timeStamp
    for (chInd = 0; chInd < frame->channelsLen; chInd++) {
        length += sizeof(BTA_ChannelId);            // id
        length += sizeof(uint16_t);                 // xRes
        length += sizeof(uint16_t);                 // yRes
        length += sizeof(BTA_DataFormat);           // dataFormat
        length += sizeof(BTA_Unit);                 // unit
        length += sizeof(uint32_t);                 // integrationTime
        length += sizeof(uint32_t);                 // modulationFrequency
        length += frame->channels[chInd]->dataLen;  // data
        length += sizeof(uint32_t);                 // dataLen
        for (mdInd = 0; mdInd < frame->channels[chInd]->metadataLen; mdInd++) {
            length += sizeof(BTA_MetadataId);                           // id
            length += frame->channels[chInd]->metadata[mdInd]->dataLen; // data
            length += sizeof(uint32_t);                                 // dataLen
        }
        length += sizeof(uint32_t);         // metadataLen
    }
    length += sizeof(uint8_t);              // channelsLen
    length += sizeof(uint8_t);              // sequenceCounter
    *frameSerializedLen = length;
    return BTA_StatusOk;
}



#define BTA_FRAME_SERIALIZED_PREAMBLE   0xb105
#define BTA_FRAME_SERIALIZED_VERSION    3

BTA_Status BTA_CALLCONV BTAserializeFrame(BTA_Frame *frame, uint8_t *frameSerialized, uint32_t *frameSerializedLen) {
    BTA_Status status;
    uint32_t length;
    uint32_t index = 0;
    uint32_t chInd, mdInd;
    if (!frame || !frameSerialized || !frameSerializedLen) {
        return BTA_StatusInvalidParameter;
    }
    status = BTAgetSerializedLength(frame, &length);
    if (status != BTA_StatusOk) {
        return status;
    }
    if (*frameSerializedLen < length) {
        return BTA_StatusOutOfMemory;
    }
    BTAbitConverterFromUInt16(frameSerialized, &index, BTA_FRAME_SERIALIZED_PREAMBLE);
    BTAbitConverterFromUInt08(frameSerialized, &index, BTA_FRAME_SERIALIZED_VERSION);
    BTAbitConverterFromUInt08(frameSerialized, &index, frame->firmwareVersionNonFunc);
    BTAbitConverterFromUInt08(frameSerialized, &index, frame->firmwareVersionMinor);
    BTAbitConverterFromUInt08(frameSerialized, &index, frame->firmwareVersionMajor);
    BTAbitConverterFromFloat4(frameSerialized, &index, frame->mainTemp);
    BTAbitConverterFromFloat4(frameSerialized, &index, frame->ledTemp);
    BTAbitConverterFromFloat4(frameSerialized, &index, frame->genericTemp);
    BTAbitConverterFromUInt32(frameSerialized, &index, frame->frameCounter);
    BTAbitConverterFromUInt32(frameSerialized, &index, frame->timeStamp);
    BTAbitConverterFromUInt08(frameSerialized, &index, frame->channelsLen);
    for (chInd = 0; chInd < frame->channelsLen; chInd++) {
        BTAbitConverterFromUInt32(frameSerialized, &index, frame->channels[chInd]->id);
        BTAbitConverterFromUInt16(frameSerialized, &index, frame->channels[chInd]->xRes);
        BTAbitConverterFromUInt16(frameSerialized, &index, frame->channels[chInd]->yRes);
        BTAbitConverterFromUInt32(frameSerialized, &index, frame->channels[chInd]->dataFormat);
        BTAbitConverterFromUInt32(frameSerialized, &index, frame->channels[chInd]->unit);
        BTAbitConverterFromUInt32(frameSerialized, &index, frame->channels[chInd]->integrationTime);
        BTAbitConverterFromUInt32(frameSerialized, &index, frame->channels[chInd]->modulationFrequency);
        BTAbitConverterFromUInt32(frameSerialized, &index, frame->channels[chInd]->dataLen);
        BTAbitConverterFromStream(frameSerialized, &index, frame->channels[chInd]->data, frame->channels[chInd]->dataLen);
        BTAbitConverterFromUInt32(frameSerialized, &index, frame->channels[chInd]->metadataLen);
        for (mdInd = 0; mdInd < frame->channels[chInd]->metadataLen; mdInd++) {
            BTAbitConverterFromUInt32(frameSerialized, &index, frame->channels[chInd]->metadata[mdInd]->id);
            BTAbitConverterFromUInt32(frameSerialized, &index, frame->channels[chInd]->metadata[mdInd]->dataLen);
            BTAbitConverterFromStream(frameSerialized, &index, (uint8_t *)frame->channels[chInd]->metadata[mdInd]->data, frame->channels[chInd]->metadata[mdInd]->dataLen);
        }
    }
    BTAbitConverterFromUInt08(frameSerialized, &index, frame->sequenceCounter);
    *frameSerializedLen = index;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAdeserializeFrame(BTA_Frame **framePtr, uint8_t *frameSerialized, uint32_t *frameSerializedLen) {
    uint32_t index = 0;
    uint32_t temp;
    uint32_t chInd;
    uint32_t mdInd;
    BTA_Frame *frame;
    if (!framePtr || !frameSerialized || !frameSerializedLen) {
        return BTA_StatusInvalidParameter;
    }
    *framePtr = 0;
    if (*frameSerializedLen < 3 + sizeof(BTA_Frame)) {
        // not long enough to contain preamble, version and BTA_Frame
        return BTA_StatusOutOfMemory;
    }
    uint16_t preamble;
    BTAbitConverterToUInt16(frameSerialized, &index, &preamble);
    if (preamble != BTA_FRAME_SERIALIZED_PREAMBLE) {
        // wrong preamble
        return BTA_StatusInvalidParameter;
    }

    frame = (BTA_Frame *)calloc(1, sizeof(BTA_Frame));
    if (!frame) {
        return BTA_StatusOutOfMemory;
    }

    uint8_t version;
    BTAbitConverterToUInt08(frameSerialized, &index, &version);
    switch (version) {

    case 1:
        if (*frameSerializedLen - index < sizeof(BTA_Frame)) {
            // not long enough to contain a BTA_Frame
            return BTA_StatusOutOfMemory;
        }
        frame->firmwareVersionNonFunc = frameSerialized[index++];
        frame->firmwareVersionMinor = frameSerialized[index++];
        frame->firmwareVersionMajor = frameSerialized[index++];
        temp = (uint32_t)(frameSerialized[index++]);
        temp |= ((uint32_t)frameSerialized[index++]) << 8;
        temp |= ((uint32_t)frameSerialized[index++]) << 16;
        temp |= ((uint32_t)frameSerialized[index++]) << 24;
        frame->mainTemp = (float)temp;
        temp = (uint32_t)(frameSerialized[index++]);
        temp |= ((uint32_t)frameSerialized[index++]) << 8;
        temp |= ((uint32_t)frameSerialized[index++]) << 16;
        temp |= ((uint32_t)frameSerialized[index++]) << 24;
        frame->ledTemp = (float)temp;
        temp = (uint32_t)(frameSerialized[index++]);
        temp |= ((uint32_t)frameSerialized[index++]) << 8;
        temp |= ((uint32_t)frameSerialized[index++]) << 16;
        temp |= ((uint32_t)frameSerialized[index++]) << 24;
        frame->genericTemp = (float)temp;
        frame->frameCounter = (uint32_t)(frameSerialized[index++]);
        frame->frameCounter |= ((uint32_t)frameSerialized[index++]) << 8;
        frame->frameCounter |= ((uint32_t)frameSerialized[index++]) << 16;
        frame->frameCounter |= ((uint32_t)frameSerialized[index++]) << 24;
        frame->sequenceCounter = 0;
        frame->timeStamp = (uint32_t)(frameSerialized[index++]);
        frame->timeStamp |= ((uint32_t)frameSerialized[index++]) << 8;
        frame->timeStamp |= ((uint32_t)frameSerialized[index++]) << 16;
        frame->timeStamp |= ((uint32_t)frameSerialized[index++]) << 24;
        frame->sequenceCounter = frameSerialized[index++];
        frame->channelsLen = frameSerialized[index++];
        frame->channels = (BTA_Channel **)calloc(frame->channelsLen, sizeof(BTA_Channel *));
        if (!frame->channels) {
            free(frame);
            frame = 0;
            return BTA_StatusOutOfMemory;
        }
        for (chInd = 0; chInd < frame->channelsLen; chInd++) {
            if (*frameSerializedLen - index < sizeof(BTA_Channel)) {
                // not long enough to contain BTA_Channel
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            frame->channels[chInd] = (BTA_Channel *)calloc(1, sizeof(BTA_Channel));
            if (!frame->channels[chInd]) {
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            temp = (uint32_t)(frameSerialized[index++]);
            temp |= ((uint32_t)frameSerialized[index++]) << 8;
            temp |= ((uint32_t)frameSerialized[index++]) << 16;
            temp |= ((uint32_t)frameSerialized[index++]) << 24;
            frame->channels[chInd]->id = (BTA_ChannelId)temp;
            frame->channels[chInd]->xRes = (uint32_t)(frameSerialized[index++]);
            frame->channels[chInd]->xRes |= ((uint32_t)frameSerialized[index++]) << 8;
            frame->channels[chInd]->yRes = (uint32_t)(frameSerialized[index++]);
            frame->channels[chInd]->yRes |= ((uint32_t)frameSerialized[index++]) << 8;
            temp = (uint32_t)(frameSerialized[index++]);
            temp |= ((uint32_t)frameSerialized[index++]) << 8;
            temp |= ((uint32_t)frameSerialized[index++]) << 16;
            temp |= ((uint32_t)frameSerialized[index++]) << 24;
            frame->channels[chInd]->dataFormat = (BTA_DataFormat)temp;
            temp = (uint32_t)(frameSerialized[index++]);
            temp |= ((uint32_t)frameSerialized[index++]) << 8;
            temp |= ((uint32_t)frameSerialized[index++]) << 16;
            temp |= ((uint32_t)frameSerialized[index++]) << 24;
            frame->channels[chInd]->unit = (BTA_Unit)temp;
            frame->channels[chInd]->integrationTime = (uint32_t)(frameSerialized[index++]);
            frame->channels[chInd]->integrationTime |= ((uint32_t)frameSerialized[index++]) << 8;
            frame->channels[chInd]->integrationTime |= ((uint32_t)frameSerialized[index++]) << 16;
            frame->channels[chInd]->integrationTime |= ((uint32_t)frameSerialized[index++]) << 24;
            frame->channels[chInd]->modulationFrequency = (uint32_t)(frameSerialized[index++]);
            frame->channels[chInd]->modulationFrequency |= ((uint32_t)frameSerialized[index++]) << 8;
            frame->channels[chInd]->modulationFrequency |= ((uint32_t)frameSerialized[index++]) << 16;
            frame->channels[chInd]->modulationFrequency |= ((uint32_t)frameSerialized[index++]) << 24;
            if (*frameSerializedLen - index < (uint32_t)(frame->channels[chInd]->xRes * frame->channels[chInd]->yRes * (frame->channels[chInd]->dataFormat & 0xf))) {
                // not long enough to contain channel data
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            frame->channels[chInd]->dataLen = frame->channels[chInd]->xRes * frame->channels[chInd]->yRes * (uint32_t)(frame->channels[chInd]->dataFormat & 0xf);
            frame->channels[chInd]->data = (uint8_t *)malloc(frame->channels[chInd]->dataLen);
            if (!frame->channels[chInd]->data) {
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            memcpy(frame->channels[chInd]->data, &frameSerialized[index], frame->channels[chInd]->dataLen);
            index += frame->channels[chInd]->dataLen;
            // TODO add metadata!!
        }
        break;

    case 2:
        if (*frameSerializedLen - index < sizeof(BTA_Frame)) {
            // not long enough to contain a BTA_Frame
            return BTA_StatusOutOfMemory;
        }
        frame->firmwareVersionNonFunc = frameSerialized[index++];
        frame->firmwareVersionMinor = frameSerialized[index++];
        frame->firmwareVersionMajor = frameSerialized[index++];
        temp = (uint32_t)(frameSerialized[index++]);
        temp |= ((uint32_t)frameSerialized[index++]) << 8;
        temp |= ((uint32_t)frameSerialized[index++]) << 16;
        temp |= ((uint32_t)frameSerialized[index++]) << 24;
        frame->mainTemp = (float)temp;
        temp = (uint32_t)(frameSerialized[index++]);
        temp |= ((uint32_t)frameSerialized[index++]) << 8;
        temp |= ((uint32_t)frameSerialized[index++]) << 16;
        temp |= ((uint32_t)frameSerialized[index++]) << 24;
        frame->ledTemp = (float)temp;
        temp = (uint32_t)(frameSerialized[index++]);
        temp |= ((uint32_t)frameSerialized[index++]) << 8;
        temp |= ((uint32_t)frameSerialized[index++]) << 16;
        temp |= ((uint32_t)frameSerialized[index++]) << 24;
        frame->genericTemp = (float)temp;
        frame->frameCounter = (uint32_t)(frameSerialized[index++]);
        frame->frameCounter |= ((uint32_t)frameSerialized[index++]) << 8;
        frame->frameCounter |= ((uint32_t)frameSerialized[index++]) << 16;
        frame->frameCounter |= ((uint32_t)frameSerialized[index++]) << 24;
        frame->sequenceCounter = 0;
        frame->timeStamp = (uint32_t)(frameSerialized[index++]);
        frame->timeStamp |= ((uint32_t)frameSerialized[index++]) << 8;
        frame->timeStamp |= ((uint32_t)frameSerialized[index++]) << 16;
        frame->timeStamp |= ((uint32_t)frameSerialized[index++]) << 24;
        frame->sequenceCounter = frameSerialized[index++];
        // channels
        frame->channelsLen = frameSerialized[index++];
        frame->channels = (BTA_Channel **)calloc(1, frame->channelsLen * sizeof(BTA_Channel *));
        if (!frame->channels) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        for (chInd = 0; chInd < frame->channelsLen; chInd++) {
            if (*frameSerializedLen - index < sizeof(BTA_Channel)) {
                // not long enough to contain BTA_Channel
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            frame->channels[chInd] = (BTA_Channel *)calloc(1, sizeof(BTA_Channel));
            if (!frame->channels[chInd]) {
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            temp = (uint32_t)(frameSerialized[index++]);
            temp |= ((uint32_t)frameSerialized[index++]) << 8;
            temp |= ((uint32_t)frameSerialized[index++]) << 16;
            temp |= ((uint32_t)frameSerialized[index++]) << 24;
            frame->channels[chInd]->id = (BTA_ChannelId)temp;
            frame->channels[chInd]->xRes = (uint32_t)(frameSerialized[index++]);
            frame->channels[chInd]->xRes |= ((uint32_t)frameSerialized[index++]) << 8;
            frame->channels[chInd]->yRes = (uint32_t)(frameSerialized[index++]);
            frame->channels[chInd]->yRes |= ((uint32_t)frameSerialized[index++]) << 8;
            temp = (uint32_t)(frameSerialized[index++]);
            temp |= ((uint32_t)frameSerialized[index++]) << 8;
            temp |= ((uint32_t)frameSerialized[index++]) << 16;
            temp |= ((uint32_t)frameSerialized[index++]) << 24;
            frame->channels[chInd]->dataFormat = (BTA_DataFormat)temp;
            temp = (uint32_t)(frameSerialized[index++]);
            temp |= ((uint32_t)frameSerialized[index++]) << 8;
            temp |= ((uint32_t)frameSerialized[index++]) << 16;
            temp |= ((uint32_t)frameSerialized[index++]) << 24;
            frame->channels[chInd]->unit = (BTA_Unit)temp;
            frame->channels[chInd]->integrationTime = (uint32_t)(frameSerialized[index++]);
            frame->channels[chInd]->integrationTime |= ((uint32_t)frameSerialized[index++]) << 8;
            frame->channels[chInd]->integrationTime |= ((uint32_t)frameSerialized[index++]) << 16;
            frame->channels[chInd]->integrationTime |= ((uint32_t)frameSerialized[index++]) << 24;
            frame->channels[chInd]->modulationFrequency = (uint32_t)(frameSerialized[index++]);
            frame->channels[chInd]->modulationFrequency |= ((uint32_t)frameSerialized[index++]) << 8;
            frame->channels[chInd]->modulationFrequency |= ((uint32_t)frameSerialized[index++]) << 16;
            frame->channels[chInd]->modulationFrequency |= ((uint32_t)frameSerialized[index++]) << 24;
            frame->channels[chInd]->dataLen = (uint32_t)(frameSerialized[index++]);
            frame->channels[chInd]->dataLen |= ((uint32_t)frameSerialized[index++]) << 8;
            frame->channels[chInd]->dataLen |= ((uint32_t)frameSerialized[index++]) << 16;
            frame->channels[chInd]->dataLen |= ((uint32_t)frameSerialized[index++]) << 24;
            if (*frameSerializedLen - index < (uint32_t)(frame->channels[chInd]->dataLen)) {
                // not long enough to contain channel data
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            frame->channels[chInd]->data = (uint8_t *)malloc(frame->channels[chInd]->dataLen);
            if (!frame->channels[chInd]->data) {
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            memcpy(frame->channels[chInd]->data, &frameSerialized[index], frame->channels[chInd]->dataLen);
            index += frame->channels[chInd]->dataLen;
        }
        break;

    case 3:
        if (*frameSerializedLen - index < sizeof(BTA_Frame)) {
            // not long enough to contain a BTA_Frame
            return BTA_StatusOutOfMemory;
        }
        BTAbitConverterToUInt08(frameSerialized, &index, &frame->firmwareVersionNonFunc);
        BTAbitConverterToUInt08(frameSerialized, &index, &frame->firmwareVersionMinor);
        BTAbitConverterToUInt08(frameSerialized, &index, &frame->firmwareVersionMajor);
        BTAbitConverterToFloat4(frameSerialized, &index, &frame->mainTemp);
        BTAbitConverterToFloat4(frameSerialized, &index, &frame->ledTemp);
        BTAbitConverterToFloat4(frameSerialized, &index, &frame->genericTemp);
        BTAbitConverterToUInt32(frameSerialized, &index, &frame->frameCounter);
        BTAbitConverterToUInt32(frameSerialized, &index, &frame->timeStamp);
        BTAbitConverterToUInt08(frameSerialized, &index, &frame->channelsLen);
        frame->channels = (BTA_Channel **)calloc(1, frame->channelsLen * sizeof(BTA_Channel *));
        if (!frame->channels) {
            BTAfreeFrame(&frame);
            return BTA_StatusOutOfMemory;
        }
        for (chInd = 0; chInd < frame->channelsLen; chInd++) {
            if (*frameSerializedLen - index < 28) {
                // not long enough to contain BTA_Channel
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            frame->channels[chInd] = (BTA_Channel *)calloc(1, sizeof(BTA_Channel));
            if (!frame->channels[chInd]) {
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            BTAbitConverterToUInt32(frameSerialized, &index, (uint32_t *)&frame->channels[chInd]->id);
            BTAbitConverterToUInt16(frameSerialized, &index, &frame->channels[chInd]->xRes);
            BTAbitConverterToUInt16(frameSerialized, &index, &frame->channels[chInd]->yRes);
            BTAbitConverterToUInt32(frameSerialized, &index, (uint32_t *)&frame->channels[chInd]->dataFormat);
            BTAbitConverterToUInt32(frameSerialized, &index, (uint32_t *)&frame->channels[chInd]->unit);
            BTAbitConverterToUInt32(frameSerialized, &index, &frame->channels[chInd]->integrationTime);
            BTAbitConverterToUInt32(frameSerialized, &index, &frame->channels[chInd]->modulationFrequency);
            BTAbitConverterToUInt32(frameSerialized, &index, &frame->channels[chInd]->dataLen);
            if (*frameSerializedLen - index < (uint32_t)(frame->channels[chInd]->dataLen)) {
                // not long enough to contain channel data
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            frame->channels[chInd]->data = (uint8_t *)malloc(frame->channels[chInd]->dataLen);
            if (!frame->channels[chInd]->data) {
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            BTAbitConverterToStream(frameSerialized, &index, frame->channels[chInd]->data, frame->channels[chInd]->dataLen);
            // metadata
            if (*frameSerializedLen - index < 4) {
                // not long enough to contain metadataLen
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            BTAbitConverterToUInt32(frameSerialized, &index, &frame->channels[chInd]->metadataLen);
            frame->channels[chInd]->metadata = (BTA_Metadata **)calloc(frame->channels[chInd]->metadataLen, sizeof(BTA_Metadata *));
            if (!frame->channels[chInd]->metadata) {
                BTAfreeFrame(&frame);
                return BTA_StatusOutOfMemory;
            }
            for (mdInd = 0; mdInd < frame->channels[chInd]->metadataLen; mdInd++) {
                if (*frameSerializedLen - index < 8) {
                    // not long enough to contain metadata
                    BTAfreeFrame(&frame);
                    return BTA_StatusOutOfMemory;
                }
                frame->channels[chInd]->metadata[mdInd] = (BTA_Metadata *)calloc(1, sizeof(BTA_Metadata));
                if (!frame->channels[chInd]->metadata[mdInd]) {
                    BTAfreeFrame(&frame);
                    return BTA_StatusOutOfMemory;
                }
                BTAbitConverterToUInt32(frameSerialized, &index, (uint32_t *)&frame->channels[chInd]->metadata[mdInd]->id);
                BTAbitConverterToUInt32(frameSerialized, &index, &frame->channels[chInd]->metadata[mdInd]->dataLen);
                if (*frameSerializedLen - index < (uint32_t)(frame->channels[chInd]->metadata[mdInd]->dataLen)) {
                    // not long enough to contain the data
                    BTAfreeFrame(&frame);
                    return BTA_StatusOutOfMemory;
                }
                frame->channels[chInd]->metadata[mdInd]->data = malloc(frame->channels[chInd]->metadata[mdInd]->dataLen);
                if (!frame->channels[chInd]->metadata[mdInd]->data) {
                    BTAfreeFrame(&frame);
                    return BTA_StatusOutOfMemory;
                }
                BTAbitConverterToStream(frameSerialized, &index, (uint8_t *)frame->channels[chInd]->metadata[mdInd]->data, frame->channels[chInd]->metadata[mdInd]->dataLen);
            }
        }
        BTAbitConverterToUInt08(frameSerialized, &index, &frame->sequenceCounter);
        break;

    default:
        return BTA_StatusInvalidVersion;
    }
    *framePtr = frame;
    *frameSerializedLen = index;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAfreeFrame(BTA_Frame **frame) {
    int i;
    if (!frame) {
        return BTA_StatusInvalidParameter;
    }
    if (!*frame) {
        return BTA_StatusInvalidParameter;
    }
    if ((*frame)->channels) {
        for (i = 0; i < (*frame)->channelsLen; i++) {
            BTAfreeChannel(&((*frame)->channels[i]));
        }
    }
    free((*frame)->channels);
    (*frame)->channels = 0;
    free(*frame);
    *frame = 0;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAfreeDeviceInfo(BTA_DeviceInfo *deviceInfo) {
    if (!deviceInfo) {
        return BTA_StatusInvalidParameter;
    }
    free(deviceInfo->productOrderNumber);
    deviceInfo->productOrderNumber = 0;
    free(deviceInfo->deviceMacAddr);
    deviceInfo->deviceMacAddr = 0;
    free(deviceInfo->deviceIpAddr);
    deviceInfo->deviceIpAddr = 0;
    free(deviceInfo->subnetMask);
    deviceInfo->subnetMask = 0;
    free(deviceInfo->gatewayIpAddr);
    deviceInfo->gatewayIpAddr = 0;
    free(deviceInfo->udpDataIpAddr);
    deviceInfo->udpDataIpAddr = 0;
    free(deviceInfo);
    deviceInfo = 0;
    return BTA_StatusOk;
}

const char* BTA_CALLCONV BTAlibParamToString(BTA_LibParam libParam) {
    switch ((int)libParam) {
    case BTA_LibParamKeepAliveMsgInterval: return "KeepAliveMsgInterval";
    case BTA_LibParamCrcControlEnabled: return "CrcControlEnabled";
    case BTA_LibParamBltStreamTotalFrameCount: return "BltStreamTotalFrameCount";
    case BTA_LibParamBltStreamAutoPlaybackSpeed: return "BltStreamAutoPlaybackSpeed";
    case BTA_LibParamBltStreamPos: return "BltStreamPos";
    case BTA_LibParamBltStreamPosIncrement: return "BltStreamPosIncrement";
    case BTA_LibParamTestPatternEnabled: return "TestPatternEnabled";
    case BTA_LibParamPauseCaptureThread: return "PauseCaptureThread";
    case BTA_LibParamDisableDataScaling: return "DisableDataScaling";
    case BTA_LibParamUndistortRgb: return "UndistortRgb";
    case BTA_LibParamInfoEventVerbosity: return "InfoEventVerbosity";
    case BTA_LibParamEnableJpgDecoding: return "EnableJpgDecoding";
    case BTA_LibParamDataStreamReadFailedCount: return "DataStreamReadFailedCount";
    case BTA_LibParamDataStreamBytesReceivedCount: return "DataStreamBytesReceivedCount";
    case BTA_LibParamDataStreamPacketsReceivedCount: return "DataStreamPacketsReceivedCount";
    case BTA_LibParamDataStreamPacketsMissedCount: return "DataStreamPacketsMissedCount";
    case BTA_LibParamDataStreamPacketsToParse: return "DataStreamPacketsToParse";
    case BTA_LibParamDataStreamParseFrameDuration: return "DataStreamParseFrameDuration";
    case BTA_LibParamDataStreamFrameCounterGapsCount: return "DataStreamFrameCounterGapsCount";
    case BTA_LibParamDataStreamFramesParsedCount: return "DataStreamFramesParsedCount";
    case BTA_LibParamDataStreamFramesParsedPerSec: return "DataStreamFramesParsedPerSec";
    case BTA_LibParamDataStreamRetrReqMode: return "DataStreamRetrReqMode";
    case BTA_LibParamDataStreamPacketWaitTimeout: return "DataStreamPacketWaitTimeout";
    case BTA_LibParamDataStreamRetrReqIntervalMin: return "DataStreamRetrReqIntervalMin";
    case BTA_LibParamDataStreamRetrReqMaxAttempts: return "DataStreamRetrReqMaxAttempts";
    case BTA_LibParamDataStreamRetrReqsCount: return "DataStreamRetrReqsCount";
    case BTA_LibParamDataStreamRetransPacketsCount: return "DataStreamRetransPacketsCount";
    case BTA_LibParamDataStreamNdasReceived: return "DataStreamNdasReceived";
    case BTA_LibParamDataStreamRedundantPacketCount: return "DataStreamRedundantPacketCount";
    case BTA_LibParamDataSockOptRcvtimeo: return "DataSockOptRcvtimeo";
    case BTA_LibParamDataSockOptRcvbuf: return "DataSockOptRcvbuf";
    default: return "LibParamUnnamed";
    }
}


const char* BTA_CALLCONV BTAstatusToString2(BTA_Status status) {
    switch (status) {
    case BTA_StatusOk: return "Ok";
    case BTA_StatusInvalidParameter: return "InvalidParameter";
    case BTA_StatusIllegalOperation: return "IllegalOperation";
    case BTA_StatusTimeOut: return "TimeOut";
    case BTA_StatusDeviceUnreachable: return "DeviceUnreachable";
    case BTA_StatusNotConnected: return "NotConnected";
    case BTA_StatusInvalidVersion: return "InvalidVersion";
    case BTA_StatusRuntimeError: return "RuntimeError";
    case BTA_StatusOutOfMemory: return "OutOfMemory";
    case BTA_StatusNotSupported: return "NotSupported";
    case BTA_StatusCrcError: return "CrcError";
    case BTA_StatusUnknown: return "Unknown";
    case BTA_StatusInvalidData: return "InvalidData";
    case BTA_StatusInformation: return "Information";
    case BTA_StatusWarning: return "Warning";
    case BTA_StatusDebug: return "Debug";
    default: return "<not a BTA_Status>";
    }
}


// obsolete, use BTAstatusToString2!!
BTA_Status BTA_CALLCONV BTAstatusToString(BTA_Status status, char *statusString, uint16_t statusStringLen) {
    if (!statusString) {
        return BTA_StatusInvalidParameter;
    }
    switch (status) {
    case BTA_StatusOk:
        if (statusStringLen <= strlen("Ok")) return BTA_StatusOutOfMemory;
        strcpy((char *)statusString, "Ok");
        break;
    case BTA_StatusInvalidParameter:
        if (statusStringLen <= strlen("InvalidParameter")) return BTA_StatusOutOfMemory;
        strcpy((char *)statusString, "InvalidParameter");
        break;
    case BTA_StatusIllegalOperation:
        if (statusStringLen <= strlen("IllegalOperation")) return BTA_StatusOutOfMemory;
        strcpy((char *)statusString, "IllegalOperation");
        break;
    case BTA_StatusTimeOut:
        if (statusStringLen <= strlen("TimeOut")) return BTA_StatusOutOfMemory;
        strcpy((char *)statusString, "TimeOut");
        break;
    case BTA_StatusDeviceUnreachable:
        if (statusStringLen <= strlen("DeviceUnreachable")) return BTA_StatusOutOfMemory;
        strcpy((char *)statusString, "DeviceUnreachable");
        break;
    case BTA_StatusNotConnected:
        if (statusStringLen <= strlen("NotConnected")) return BTA_StatusOutOfMemory;
        strcpy((char *)statusString, "NotConnected");
        break;
    case BTA_StatusInvalidVersion:
        if (statusStringLen <= strlen("InvalidVersion")) return BTA_StatusOutOfMemory;
        strcpy((char *)statusString, "InvalidVersion");
        break;
    case BTA_StatusRuntimeError:
        if (statusStringLen <= strlen("RuntimeError")) return BTA_StatusOutOfMemory;
        strcpy((char *)statusString, "RuntimeError");
        break;
    case BTA_StatusOutOfMemory:
        if (statusStringLen <= strlen("OutOfMemory")) return BTA_StatusOutOfMemory;
        strcpy((char *)statusString, "OutOfMemory");
        break;
    case BTA_StatusNotSupported:
        if (statusStringLen <= strlen("NotSupported")) return BTA_StatusOutOfMemory;
        strcpy((char *)statusString, "NotSupported");
        break;
    case BTA_StatusCrcError:
        if (statusStringLen <= strlen("CrcError")) return BTA_StatusOutOfMemory;
        strcpy((char *)statusString, "CrcError");
        break;
    case BTA_StatusUnknown:
        if (statusStringLen <= strlen("Unknown")) return BTA_StatusOutOfMemory;
        strcpy((char *)statusString, "Unknown");
        break;
    case BTA_StatusInvalidData:
        if (statusStringLen <= strlen("InvalidData")) return BTA_StatusOutOfMemory;
        strcpy((char *)statusString, "InvalidData");
        break;

    case BTA_StatusInformation:
        if (statusStringLen <= strlen("Information")) return BTA_StatusOutOfMemory;
        strcpy((char *)statusString, "Information");
        break;
    case BTA_StatusWarning:
        if (statusStringLen <= strlen("Warning")) return BTA_StatusOutOfMemory;
        strcpy((char *)statusString, "Warning");
        break;
    case BTA_StatusDebug:
        if (statusStringLen <= strlen("Debug")) return BTA_StatusOutOfMemory;
        strcpy((char *)statusString, "Debug");
        break;
    default:
        if (statusStringLen <= strlen("Status -32xxx")) return BTA_StatusOutOfMemory;
        sprintf((char *)statusString, "Status %d", status);
        break;
    }
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAeventIdToString(BTA_EventId status, char *statusString, uint16_t statusStringLen) {
    return BTAstatusToString(status, statusString, statusStringLen);
}



BTA_Status BTA_CALLCONV BTAunitToString(BTA_Unit unit, char *unitString, uint16_t unitStringLen) {
    char unitStringTemp[50];
    if (!unitString) {
        return BTA_StatusInvalidParameter;
    }
    switch (unit) {
    case BTA_UnitUnitLess:
        strcpy((char *)unitStringTemp, "");
        break;
    case BTA_UnitMeter:
        strcpy((char *)unitStringTemp, "m");
        break;
    case BTA_UnitMillimeter:
        strcpy((char *)unitStringTemp, "mm");
        break;
    default:
        sprintf((char *)unitStringTemp, "%d", unit);
        break;
    }
    if (strlen(unitStringTemp) + 1 > unitStringLen) {
        return BTA_StatusOutOfMemory;
    }
#ifdef PLAT_WINDOWS
    strcpy_s((char *)unitString, unitStringLen, unitStringTemp);
#elif defined PLAT_LINUX || defined PLAT_APPLE
    strcpy((char *)unitString, unitStringTemp);
#endif
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAsetKeepAliveMsgInterval(BTA_Handle handle, float interval) { return BTA_StatusNotSupported; }
BTA_Status BTA_CALLCONV BTAsetControlCrcEnabled(BTA_Handle handle, uint8_t enabled) { return BTA_StatusNotSupported; }


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





uint8_t BTA_CALLCONV BTAisEthDevice(uint16_t deviceType) {
    if (deviceType == BTA_DeviceTypeGenericEth) {
        return 1;
    }
    int i;
    uint16_t ethDeviceTypes[BTA_ETH_DEVICE_TYPES_LEN] = { BTA_ETH_DEVICE_TYPES };
    for (i = 0; i < BTA_ETH_DEVICE_TYPES_LEN; i++) {
        if (deviceType == ethDeviceTypes[i]) {
            return 1;
        }
    }
    return 0;
}


uint8_t BTA_CALLCONV BTAisUsbDevice(uint16_t deviceType) {
    if (deviceType == BTA_DeviceTypeGenericUsb) {
        return 1;
    }
    int i;
    uint16_t usbDeviceTypes[BTA_USB_DEVICE_TYPES_LEN] = { BTA_USB_DEVICE_TYPES };
    for (i = 0; i < BTA_USB_DEVICE_TYPES_LEN; i++) {
        if (deviceType == usbDeviceTypes[i]) {
            return 1;
        }
    }
    return 0;
}


uint8_t BTA_CALLCONV BTAisP100Device(uint16_t deviceType) {
    return 0;
}


uint8_t BTA_CALLCONV BTAisUartDevice(uint16_t deviceType) {
    if (deviceType == BTA_DeviceTypeGenericUart) {
        return 1;
    }
    int i;
    uint16_t uartDeviceTypes[BTA_UART_DEVICE_TYPES_LEN] = { BTA_UART_DEVICE_TYPES };
    for (i = 0; i < BTA_UART_DEVICE_TYPES_LEN; i++) {
        if (deviceType == uartDeviceTypes[i]) {
            return 1;
        }
    }
    return 0;
}



BTA_Status BTA_CALLCONV BTAinsertChannelIntoFrame(BTA_Frame *frame, BTA_Channel *channel) {
    if (!frame || !channel) {
        return BTA_StatusInvalidParameter;
    }
    if (!frame->channels) {
        frame->channels = (BTA_Channel **)malloc(sizeof(BTA_Channel *));
        if (!frame->channels) {
            return BTA_StatusOutOfMemory;
        }
        frame->channelsLen = 1;
    }
    else {
        frame->channelsLen++;
        BTA_Channel **temp = frame->channels;
        frame->channels = (BTA_Channel **)realloc(frame->channels, frame->channelsLen * sizeof(BTA_Channel *));
        if (!frame->channels) {
            frame->channelsLen--;
            frame->channels = temp;
            return BTA_StatusOutOfMemory;
        }
    }
    frame->channels[frame->channelsLen - 1] = channel;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAinsertChannelDataIntoFrame(BTA_Frame *frame, BTA_ChannelId id, uint16_t xRes, uint16_t yRes, BTA_DataFormat dataFormat, BTA_Unit unit, uint32_t integrationTime, uint32_t modulationFrequency, uint8_t *data, uint32_t dataLen) {
    BTA_Channel *channel;
    channel = (BTA_Channel *)malloc(sizeof(BTA_Channel));
    if (!channel) {
        return BTA_StatusOutOfMemory;
    }
    channel->id = id;
    channel->xRes = xRes;
    channel->yRes = yRes;
    channel->dataFormat = dataFormat;
    channel->unit = unit;
    channel->integrationTime = integrationTime;
    channel->modulationFrequency = modulationFrequency;
    channel->data = data;
    channel->dataLen = dataLen;
    channel->metadata = 0;
    channel->metadataLen = 0;
    return BTAinsertChannelIntoFrame(frame, channel);
}


BTA_Status BTA_CALLCONV BTAremoveChannelFromFrame(BTA_Frame *frame, BTA_Channel *channel) {
    if (!frame || !channel) {
        return BTA_StatusInvalidParameter;
    }
    if (!frame->channels) {
        return BTA_StatusInvalidParameter;
    }
    int channelsLenNew = 0;
    int chInd;
    for (chInd = 0; chInd < frame->channelsLen; chInd++) {
        if (frame->channels[chInd] != channel) {
            channelsLenNew++;
        }
    }
    int i = 0;
    BTA_Channel **channelsNew = (BTA_Channel **)malloc(channelsLenNew * sizeof(BTA_Channel *));
    for (chInd = 0; chInd < frame->channelsLen; chInd++) {
        if (frame->channels[chInd] != channel) {
            channelsNew[i++] = frame->channels[chInd];
        }
        else {
            BTAfreeChannel(&(frame->channels[chInd]));
        }
    }
    free(frame->channels);
    frame->channels = channelsNew;
    frame->channelsLen = channelsLenNew;
    return BTA_StatusOk;

}


BTA_Status BTA_CALLCONV BTAcloneChannel(BTA_Channel *channelSrc, BTA_Channel **channelDst) {
    BTA_Channel *channel;
    if (!channelSrc || !channelDst) {
        return BTA_StatusInvalidParameter;
    }
    *channelDst = 0;
    channel = (BTA_Channel *)malloc(sizeof(BTA_Channel));
    if (!channel) {
        return BTA_StatusOutOfMemory;
    }
    memcpy(channel, channelSrc, sizeof(BTA_Channel));
    channel->data = (uint8_t *)malloc(channel->dataLen * sizeof(uint8_t));
    if (!channel->data) {
        free(channel);
        channel = 0;
        return BTA_StatusOutOfMemory;
    }
    memcpy(channel->data, channelSrc->data, channel->dataLen * sizeof(uint8_t));
    channel->metadata = (BTA_Metadata **)calloc(channel->metadataLen, sizeof(BTA_Metadata *));;
    int i;
    for (i = 0; i < (int)channel->metadataLen; i++) {
        BTA_Status status = BTAcloneMetadata(channelSrc->metadata[i], &(channel->metadata[i]));
        if (status != BTA_StatusOk) {
            for (i--; i >= 0; i--) {
                BTAfreeMetadata(&(channel->metadata[i]));
            }
            free(channel->metadata);
            channel->metadata = 0;
            free(channel);
            channel = 0;
            return status;
        }
    }
    *channelDst = channel;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAcloneChannelNoMemcpy(BTA_Channel *channelSrc, BTA_Channel **channelDst, uint8_t *dataNew) {
    BTA_Channel *channel;
    if (!channelSrc || !channelDst) {
        return BTA_StatusInvalidParameter;
    }
    *channelDst = 0;
    channel = (BTA_Channel *)malloc(sizeof(BTA_Channel));
    if (!channel) {
        return BTA_StatusOutOfMemory;
    }
    memcpy(channel, channelSrc, sizeof(BTA_Channel));
    channel->data = (uint8_t *)malloc(channel->dataLen * sizeof(uint8_t));
    if (!channel->data) {
        free(channel);
        channel = 0;
        return BTA_StatusOutOfMemory;
    }
    channel->data = dataNew;
    channel->metadata = (BTA_Metadata **)calloc(channel->metadataLen, sizeof(BTA_Metadata *));;
    int i;
    for (i = 0; i < (int)channel->metadataLen; i++) {
        BTA_Status status = BTAcloneMetadata(channelSrc->metadata[i], &(channel->metadata[i]));
        if (status != BTA_StatusOk) {
            for (i--; i >= 0; i--) {
                BTAfreeMetadata(&(channel->metadata[i]));
            }
            free(channel->metadata);
            channel->metadata = 0;
            free(channel);
            channel = 0;
            return status;
        }
    }
    *channelDst = channel;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAinsertMetadataIntoChannel(BTA_Channel *channel, BTA_Metadata *metadata) {
    if (!channel || !metadata) {
        return BTA_StatusInvalidParameter;
    }
    if (!channel->metadata) {
        channel->metadata = (BTA_Metadata **)malloc(sizeof(BTA_Metadata *));
        if (!channel->metadata) {
            return BTA_StatusOutOfMemory;
        }
        channel->metadataLen = 1;
    }
    else {
        channel->metadataLen++;
        BTA_Metadata **temp = channel->metadata;
        channel->metadata = (BTA_Metadata **)realloc(channel->metadata, channel->metadataLen * sizeof(BTA_Metadata *));
        if (!channel->metadata) {
            channel->metadataLen--;
            channel->metadata = temp;
            return BTA_StatusOutOfMemory;
        }
    }
    channel->metadata[channel->metadataLen - 1] = metadata;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAinsertMetadataDataIntoChannel(BTA_Channel *channel, BTA_MetadataId id, void *data, uint32_t dataLen) {
    BTA_Metadata *metadata;
    metadata = (BTA_Metadata *)malloc(sizeof(BTA_Metadata));
    if (!metadata) {
        return BTA_StatusOutOfMemory;
    }
    metadata->id = id;
    metadata->data = data;
    metadata->dataLen = dataLen;
    return BTAinsertMetadataIntoChannel(channel, metadata);
}


BTA_Status BTA_CALLCONV BTAcloneMetadata(BTA_Metadata *metadataSrc, BTA_Metadata **metadataDst) {
    BTA_Metadata *channel;
    if (!metadataSrc || !metadataDst) {
        return BTA_StatusInvalidParameter;
    }
    *metadataDst = 0;
    channel = (BTA_Metadata *)malloc(sizeof(BTA_Metadata));
    if (!channel) {
        return BTA_StatusOutOfMemory;
    }
    memcpy(channel, metadataSrc, sizeof(BTA_Metadata));
    channel->data = (uint8_t *)malloc(channel->dataLen * sizeof(uint8_t));
    if (!channel->data) {
        free(channel);
        channel = 0;
        return BTA_StatusOutOfMemory;
    }
    memcpy(channel->data, metadataSrc->data, channel->dataLen * sizeof(uint8_t));
    *metadataDst = channel;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAcolorToBw(BTA_Channel *channelIn, BTA_Channel **channelOut) {
    if (!channelIn || !channelOut || (channelIn->dataFormat != BTA_DataFormatRgb24 && channelIn->dataFormat != BTA_DataFormatRgb565)) {
        return BTA_StatusInvalidParameter;
    }
    BTA_Channel *result = *channelOut;
    result = (BTA_Channel *)malloc(sizeof(BTA_Channel));
    result->id = BTA_ChannelIdGrayScale;
    result->unit = BTA_UnitUnitLess;
    result->integrationTime = channelIn->integrationTime;
    result->modulationFrequency = channelIn->modulationFrequency;
    result->dataFormat = BTA_DataFormatUInt16;
    result->xRes = channelIn->xRes;
    result->yRes = channelIn->yRes;
    result->dataLen = result->xRes * result->yRes * 2;
    result->data = (uint8_t *)malloc(result->dataLen);
    int xy;
    switch (channelIn->dataFormat) {

    case BTA_DataFormatRgb24:
        for (xy = 0; xy < result->yRes * result->xRes; xy++) {
            uint16_t rgb = 0;
            rgb += ((uint8_t *)channelIn->data)[3 * xy + 0];
            rgb += ((uint8_t *)channelIn->data)[3 * xy + 1];
            rgb += ((uint8_t *)channelIn->data)[3 * xy + 2];
            ((uint16_t *)result->data)[xy] = 65535 * rgb / (255 * 3);
        }
        return BTA_StatusOk;

    case BTA_DataFormatRgb565:
        for (xy = 0; xy < result->yRes * result->xRes; xy++) {
            ((uint16_t *)result->data)[xy] = (((uint16_t *)channelIn->data)[xy] >> 11) | ((((uint16_t *)channelIn->data)[xy] >> 5) & 0x3f) | (((uint16_t *)channelIn->data)[xy] & 0x1f);
        }
        return BTA_StatusOk;

    default:
        // unreachable
        assert(0);
        return BTA_StatusNotSupported;
    }
}


BTA_Status BTA_CALLCONV BTAdivideChannelByNumber(BTA_Channel *dividend, uint32_t divisor, BTA_Channel **quotient/*, BTA_InfoEventInst *infoEventInst*/) {
    int xy;
    BTA_Channel *result;
    if (!dividend || !divisor) {
        return BTA_StatusInvalidParameter;
    }
    BTAcloneChannel(dividend, &result);
    switch (dividend->dataFormat) {
    case BTA_DataFormatUInt16:
        for (xy = 0; xy < dividend->xRes * dividend->yRes; xy++) {
            ((uint16_t *)result->data)[xy] = ((uint16_t *)dividend->data)[xy] / divisor;
        }
        break;
    case BTA_DataFormatSInt32:
        for (xy = 0; xy < dividend->xRes * dividend->yRes; xy++) {
            ((int32_t *)result->data)[xy] = ((int32_t *)dividend->data)[xy] / divisor;
        }
        break;
    default:
        //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAdivideChannelByNumber unsupported channel format");
        return BTA_StatusNotSupported;
    }
    *quotient = result;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAaddChannelInPlace(BTA_Channel *augendSum, BTA_Channel *addend/*, BTA_InfoEventInst *infoEventInst*/) {
    int xy;
    if (!augendSum || !addend) {
        return BTA_StatusInvalidParameter;
    }
    if (augendSum->xRes != addend->xRes || augendSum->yRes != addend->yRes) {
        //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, BTA_StatusInvalidParameter, "BTAaddChannelInPlace wrong resolution");
        return BTA_StatusInvalidParameter;
    }
    switch (augendSum->dataFormat) {
    case BTA_DataFormatSInt32:
        switch (addend->dataFormat) {
        case BTA_DataFormatUInt16:
            for (xy = 0; xy < augendSum->xRes * augendSum->yRes; xy++) {
                ((int32_t *)augendSum->data)[xy] += ((uint16_t *)addend->data)[xy];
            }
            break;
        default:
            //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAaddChannelInPlace unsupported addend format");
            return BTA_StatusNotSupported;
        }
        break;
    default:
        //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAaddChannelInPlace unsupported addend format");
        return BTA_StatusNotSupported;
    }
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAsubtChannelInPlace(BTA_Channel *minuendDiff, BTA_Channel *subtrahend/*, BTA_InfoEventInst *infoEventInst*/) {
    int xy;
    if (!minuendDiff || !subtrahend) {
        return BTA_StatusInvalidParameter;
    }
    if (minuendDiff->xRes != subtrahend->xRes || minuendDiff->yRes != subtrahend->yRes) {
        //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, BTA_StatusInvalidParameter, "BTAsubtChannelInPlace wrong resolution");
        return BTA_StatusInvalidParameter;
    }
    switch (minuendDiff->dataFormat) {
    case BTA_DataFormatSInt32:
        switch (subtrahend->dataFormat) {
        case BTA_DataFormatUInt16:
            for (xy = 0; xy < minuendDiff->xRes * minuendDiff->yRes; xy++) {
                ((int32_t *)minuendDiff->data)[xy] -= ((uint16_t *)subtrahend->data)[xy];
            }
            break;
        default:
            //BTAinfoEventHelper1(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAsubtChannelInPlace unsupported subtrahend format", subtrahend->dataFormat);
            return BTA_StatusNotSupported;
        }
        break;
    default:
        //BTAinfoEventHelper1(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAsubtChannelInPlace unsupported minuend format", minuendDiff->dataFormat);
        return BTA_StatusNotSupported;
    }
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAsubtChannel(BTA_Channel *minuend, BTA_Channel *subtrahend, BTA_Channel **diff/*, BTA_InfoEventInst *infoEventInst*/) {
    int xy;
    BTA_Channel *result;
    if (!minuend || !subtrahend || !diff) {
        return BTA_StatusInvalidParameter;
    }
    if (minuend->xRes != subtrahend->xRes || minuend->yRes != subtrahend->yRes) {
        //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, BTA_StatusInvalidParameter, "BTAsubtChannel wrong resolution");
        return BTA_StatusInvalidParameter;
    }
    BTAcloneChannel(minuend, &result);
    switch (result->dataFormat) {
    case BTA_DataFormatSInt32:
        switch (subtrahend->dataFormat) {
        case BTA_DataFormatUInt16:
            for (xy = 0; xy < result->xRes * result->yRes; xy++) {
                ((int32_t *)result->data)[xy] -= ((uint16_t *)subtrahend->data)[xy];
            }
            break;
        default:
            //BTAinfoEventHelper1(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAsubtChannel unsupported subtrahend format", subtrahend->dataFormat);
            BTAfreeChannel(&result);
            return BTA_StatusNotSupported;
        }
        break;
    default:
        //BTAinfoEventHelper1(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAsubtChannel unsupported minuend format", minuend->dataFormat);
        BTAfreeChannel(&result);
        return BTA_StatusNotSupported;
    }
    *diff = result;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAthresholdInPlace(BTA_Channel *channel, uint32_t threshold, uint8_t alsoNegative/*, BTA_InfoEventInst *infoEventInst*/) {
    int xy;
    if (!channel) {
        return BTA_StatusInvalidParameter;
    }
    if (alsoNegative) {
        switch (channel->dataFormat) {
        case BTA_DataFormatUInt16:
            for (xy = 0; xy < channel->xRes * channel->yRes; xy++) {
                ((uint16_t *)channel->data)[xy] = ((uint16_t *)channel->data)[xy] > threshold;
            }
            break;
        case BTA_DataFormatSInt32:
            for (xy = 0; xy < channel->xRes * channel->yRes; xy++) {
                ((int32_t *)channel->data)[xy] = (((int32_t *)channel->data)[xy] >(int32_t)threshold) || (((int32_t *)channel->data)[xy] < -(int32_t)threshold);
            }
            break;
        default:
            //BTAinfoEventHelper1(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAthresholdInPlace unsupported format", channel->dataFormat);
            return BTA_StatusNotSupported;
        }
    }
    else {
        switch (channel->dataFormat) {
        case BTA_DataFormatUInt16:
            for (xy = 0; xy < channel->xRes * channel->yRes; xy++) {
                ((uint16_t *)channel->data)[xy] = ((uint16_t *)channel->data)[xy] > threshold;
            }
            break;
        case BTA_DataFormatSInt32:
            for (xy = 0; xy < channel->xRes * channel->yRes; xy++) {
                ((int32_t *)channel->data)[xy] = ((int32_t *)channel->data)[xy] >(int32_t)threshold;
            }
            break;
        default:
            //BTAinfoEventHelper1(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAthresholdInPlace unsupported format", channel->dataFormat);
            return BTA_StatusNotSupported;
        }
    }
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAchangeDataFormat(BTA_Channel *channel, BTA_DataFormat dataFormat/*, BTA_InfoEventInst *infoEventInst*/) {
    int xy;
    if (!channel) {
        return BTA_StatusInvalidParameter;
    }
    switch (channel->dataFormat) {
    case BTA_DataFormatSInt32:
        switch (dataFormat) {
        case BTA_DataFormatUInt16: {
            uint32_t dataLen = channel->xRes * channel->yRes * sizeof(uint16_t);
            uint16_t *data = (uint16_t *)malloc(dataLen);
            if (!data) {
                return BTA_StatusOutOfMemory;
            }
            for (xy = 0; xy < channel->xRes * channel->yRes; xy++) {
                data[xy] = ((int32_t *)channel->data)[xy];
            }
            free(channel->data);
            channel->dataLen = dataLen;
            channel->data = (uint8_t *)data;
            channel->dataFormat = dataFormat;
            break;
        }

        case BTA_DataFormatSInt32:
            return BTA_StatusOk;

        default:
            //BTAinfoEventHelper1(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAsubtChannel unsupported subtrahend format", dataFormat);
            return BTA_StatusNotSupported;
        }
        break;
    default:
        //BTAinfoEventHelper1(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAsubtChannel unsupported minuend format", channel->dataFormat);
        return BTA_StatusNotSupported;
    }
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAfreeChannel(BTA_Channel **channel) {
    if (!channel) {
        return BTA_StatusInvalidParameter;
    }
    if (*channel) {
        free((*channel)->data);
        (*channel)->data = 0;
        if ((*channel)->metadata) {
            int i;
            for (i = 0; i < (int)(*channel)->metadataLen; i++) {
                BTAfreeMetadata(&((*channel)->metadata[i]));
            }
        }
        free((*channel)->metadata);
        (*channel)->metadata = 0;
        free(*channel);
        *channel = 0;
    }
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAfreeMetadata(BTA_Metadata **metadata) {
    if (!metadata) {
        return BTA_StatusInvalidParameter;
    }
    if (*metadata) {
        free((*metadata)->data);
        (*metadata)->data = 0;
        free(*metadata);
        *metadata = 0;
    }
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAgetValidModulationFrequencies(BTA_DeviceType deviceType, const uint32_t **modulationFrequencies, int32_t *modulationFrequenciesCount) {
    if (!modulationFrequencies && !modulationFrequenciesCount) {
        return BTA_StatusInvalidParameter;
    }
    switch ((int)deviceType) {
        case BTA_DeviceTypeArgos3dP33x:                *modulationFrequencies = modFreqs_5; *modulationFrequenciesCount = sizeof(modFreqs_5); break;
        case BTA_DeviceTypeMlx75123ValidationPlatform: *modulationFrequencies = modFreqs_4; *modulationFrequenciesCount = sizeof(modFreqs_4); break;
        case BTA_DeviceTypeEvk7512x:                   *modulationFrequencies = modFreqs_4; *modulationFrequenciesCount = sizeof(modFreqs_4); break;
        case BTA_DeviceTypeEvk75027:                   *modulationFrequencies = 0;          *modulationFrequenciesCount = 0;                  break;
        case BTA_DeviceTypeEvk7512xTofCcBa:            *modulationFrequencies = modFreqs_4; *modulationFrequenciesCount = sizeof(modFreqs_4); break;
        case BTA_DeviceTypeP320S:                      *modulationFrequencies = modFreqs_1; *modulationFrequenciesCount = sizeof(modFreqs_1); break;
        case BTA_DeviceTypeGrabberBoard:               *modulationFrequencies = modFreqs_1; *modulationFrequenciesCount = sizeof(modFreqs_1); break;
        case BTA_DeviceTypeSentis3dP509:               *modulationFrequencies = modFreqs_1; *modulationFrequenciesCount = sizeof(modFreqs_1); break;
        case BTA_DeviceTypeLimTesterV2:                *modulationFrequencies = modFreqs_6; *modulationFrequenciesCount = sizeof(modFreqs_6); break;
        case BTA_DeviceTypeSentis3dM520:               *modulationFrequencies = modFreqs_1; *modulationFrequenciesCount = sizeof(modFreqs_1); break;
        case BTA_DeviceTypeSentis3dM530:               *modulationFrequencies = modFreqs_5; *modulationFrequenciesCount = sizeof(modFreqs_5); break;
        case BTA_DeviceTypeTimUpIrs1125:               *modulationFrequencies = modFreqs_8; *modulationFrequenciesCount = sizeof(modFreqs_8); break;
        case BTA_DeviceTypeMlx75023TofEval:            *modulationFrequencies = modFreqs_3; *modulationFrequenciesCount = sizeof(modFreqs_3); break;
        case BTA_DeviceTypeTimUp19kS3Eth:              *modulationFrequencies = modFreqs_2; *modulationFrequenciesCount = sizeof(modFreqs_2); break;
        case BTA_DeviceTypeEPC610TofModule:            *modulationFrequencies = modFreqs_6; *modulationFrequenciesCount = sizeof(modFreqs_6); break;
        case BTA_DeviceTypeArgos3dP310:                *modulationFrequencies = modFreqs_1; *modulationFrequenciesCount = sizeof(modFreqs_1); break;
        case BTA_DeviceTypeSentis3dM100:               *modulationFrequencies = modFreqs_1; *modulationFrequenciesCount = sizeof(modFreqs_1); break;
        case BTA_DeviceTypeArgos3dP32x:                *modulationFrequencies = modFreqs_1; *modulationFrequenciesCount = sizeof(modFreqs_1); break;
        case BTA_DeviceTypeArgos3dP321:                *modulationFrequencies = modFreqs_1; *modulationFrequenciesCount = sizeof(modFreqs_1); break;
        case BTA_DeviceTypeSentis3dP509Irs1020:        *modulationFrequencies = modFreqs_5; *modulationFrequenciesCount = sizeof(modFreqs_5); break;
        case BTA_DeviceTypeArgos3dP510SKT:             *modulationFrequencies = modFreqs_1; *modulationFrequenciesCount = sizeof(modFreqs_1); break;
        case BTA_DeviceTypeTimUp19kS3EthP:             *modulationFrequencies = modFreqs_2; *modulationFrequenciesCount = sizeof(modFreqs_2); break;
        case BTA_DeviceTypeMultiTofPlatformMlx:        *modulationFrequencies = modFreqs_4; *modulationFrequenciesCount = sizeof(modFreqs_4); break;
        case BTA_DeviceTypeMhsCamera:                  *modulationFrequencies = modFreqs_5; *modulationFrequenciesCount = sizeof(modFreqs_5); break;
    default:
        return BTA_StatusNotSupported;
    }
    *modulationFrequenciesCount = *modulationFrequenciesCount / sizeof(uint32_t);
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAgetNextBestModulationFrequency(BTA_DeviceType deviceType, uint32_t modFreq, uint32_t *validModFreq, int32_t *index) {

    // All devices that don't need no intelligence, just allow any frequency and use offset index 0
    switch (deviceType) {
    case BTA_DeviceTypeEvk75027:
        if (index) {
            *index = 0;
        }
        if (validModFreq) {
            *validModFreq = modFreq;
        }
        return BTA_StatusOk;
    }

    const uint32_t *modulationFrequencies;
    int32_t modulationFrequenciesCount;
    BTA_Status status = BTAgetValidModulationFrequencies(deviceType, &modulationFrequencies, &modulationFrequenciesCount);
    if (status != BTA_StatusOk) {
        return status;
    }


    uint32_t differenceBest = UINT32_MAX;
    for (uint8_t modFreqInd = 0; modFreqInd < modulationFrequenciesCount; modFreqInd++) {
        uint32_t difference = (uint32_t)abs((int32_t)(modulationFrequencies[modFreqInd] - modFreq));
        if (difference < differenceBest) {
            differenceBest = difference;
            if (index) {
                *index = modFreqInd;
            }
            if (validModFreq) {
                *validModFreq = modulationFrequencies[modFreqInd];
            }
        }
    }
    return BTA_StatusOk;
}


BTA_FrameMode BTA_CALLCONV BTAimageDataFormatToFrameMode(int deviceType, int imageMode) {
    // for now the device type is ignored. not needed
    switch (imageMode) {
    case BTA_EthImgModeDistAmp:             return BTA_FrameModeDistAmp;
    case BTA_EthImgModeXAmp:                return BTA_FrameModeZAmp;
    case BTA_EthImgModeDistAmpBalance:      return BTA_FrameModeDistAmpBalance;
    case BTA_EthImgModeXYZ:                 return BTA_FrameModeXYZ;
    case BTA_EthImgModeXYZAmp:              return BTA_FrameModeXYZAmp;
    case BTA_EthImgModeDistAmpColor:        return BTA_FrameModeDistAmpColor;
    case BTA_EthImgModePhase0_90_180_270:   return BTA_FrameModeRawPhases;
    case BTA_EthImgModeDistColor:           return BTA_FrameModeDistColor;
    case BTA_EthImgModeXYZColor:            return BTA_FrameModeXYZColor;
    case BTA_EthImgModeDist:                return BTA_FrameModeDist;
    case BTA_EthImgModeDistConfExt:         return BTA_FrameModeDistConfExt;
    case BTA_EthImgModeAmp:                 return BTA_FrameModeAmp;
    case BTA_EthImgModeRawdistAmp:          return BTA_FrameModeRawdistAmp;
    case BTA_EthImgModeRawPhases:           return BTA_FrameModeRawPhasesExt;
    case BTA_EthImgModeRawQI:               return BTA_FrameModeRawQI;
    case BTA_EthImgModeXYZConfColor:        return BTA_FrameModeXYZConfColor;
    case BTA_EthImgModeXYZAmpColorOverlay:  return BTA_FrameModeXYZAmpColorOverlay;
    default:                                return BTA_FrameModeCurrentConfig;
    }
}


int BTA_CALLCONV BTAframeModeToImageMode(int deviceType, BTA_FrameMode frameMode) {
    // for now the device type is ignored. not needed
    switch (frameMode) {
    case BTA_FrameModeDistAmp:              return BTA_EthImgModeDistAmp;
    case BTA_FrameModeZAmp:                 return BTA_EthImgModeXAmp;
    case BTA_FrameModeXYZ:                  return BTA_EthImgModeXYZ;
    case BTA_FrameModeXYZAmp:               return BTA_EthImgModeXYZAmp;
    case BTA_FrameModeDistColor:            return BTA_EthImgModeDistColor;
    case BTA_FrameModeDistAmpColor:         return BTA_EthImgModeDistAmpColor;
    case BTA_FrameModeRawPhases:            return BTA_EthImgModePhase0_90_180_270;
    case BTA_FrameModeDistAmpBalance:       return BTA_EthImgModeDistAmpBalance;
    case BTA_FrameModeXYZColor:             return BTA_EthImgModeXYZColor;
    case BTA_FrameModeDist:                 return BTA_EthImgModeDist;
    case BTA_FrameModeDistConfExt:          return BTA_EthImgModeDistConfExt;
    case BTA_FrameModeAmp:                  return BTA_EthImgModeAmp;
    case BTA_FrameModeRawdistAmp:           return BTA_EthImgModeRawdistAmp;
    case BTA_FrameModeRawPhasesExt:         return BTA_EthImgModeRawPhases;
    case BTA_FrameModeRawQI:                return BTA_EthImgModeRawQI;
    case BTA_FrameModeXYZConfColor:         return BTA_EthImgModeXYZConfColor;
    case BTA_FrameModeXYZAmpColorOverlay:   return BTA_EthImgModeXYZAmpColorOverlay;
    default:                                return BTA_EthImgModeNone;
    }
}




void BTA_CALLCONV BTAsleep(uint32_t milliseconds) {
    BTAmsleep(milliseconds);
}
