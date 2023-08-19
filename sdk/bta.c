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
#include <bta_discovery_helper.h>
#include <bta_oshelper.h>
#include <sockets_helper.h>
#include <timing_helper.h>
#include <pthread_helper.h>
#include <bitconverter.h>
#include <bta_serialization.h>
#include "configuration.h"

#include "lzma/LzmaLib.h"

//#include <bta_p100.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#ifndef BTA_WO_LIBJPEG
#include <bta_jpg.h>
#endif
#include <undistort.h>
#include <calcXYZ.h>
#include <bvq_queue.h>

#include <crc16.h>
#include <crc32.h>


static char check1[(2 * (int)(sizeof(uint32_t) == sizeof(float))) - 1] = { 0 };
static char check2[(2 * (int)(sizeof(uint32_t) == sizeof(BTA_ChannelId))) - 1] = { 0 };
static char check3[(2 * (int)(sizeof(uint32_t) == sizeof(BTA_DataFormat))) - 1] = { 0 };
static char check4[(2 * (int)(sizeof(uint32_t) == sizeof(BTA_Unit))) - 1] = { 0 };
static char check5[(2 * (int)(sizeof(uint32_t) == sizeof(BTA_MetadataId))) - 1] = { 0 };
static char check6[(2 * (int)(sizeof(BTA_Config) == CONFIG_STRUCT_ORG_LEN * BTA_CONFIG_STRUCT_STRIDE)) - 1] = { 0 };
static char check7[(2 * (int)(sizeof(size_t) == sizeof(void *))) - 1] = { 0 };

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
void *BTAETHdiscoveryRunFunction(BTA_DiscoveryInst *inst);
BTA_Status BTAETHopen(BTA_Config *config, BTA_WrapperInst *wrapperInst);
BTA_Status BTAETHclose(BTA_WrapperInst *winst);
BTA_Status BTAETHgetDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo);
BTA_Status BTAETHgetDeviceType(BTA_WrapperInst *winst, BTA_DeviceType *deviceType);
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

#ifndef BTA_WO_P100
BTA_Status BTAP100open(BTA_Config *config, BTA_WrapperInst *wrapperInst);
BTA_Status BTAP100close(BTA_WrapperInst *winst);
BTA_Status BTAP100getDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo);
BTA_Status BTAP100getDeviceType(BTA_WrapperInst *winst, BTA_DeviceType *deviceType);
uint8_t BTAP100isRunning(BTA_WrapperInst *winst);
uint8_t BTAP100isConnected(BTA_WrapperInst *winst);
BTA_Status BTAP100setFrameMode(BTA_WrapperInst *winst, BTA_FrameMode frameMode);
BTA_Status BTAP100getFrameMode(BTA_WrapperInst *winst, BTA_FrameMode *frameMode);
BTA_Status BTAP100setIntegrationTime(BTA_WrapperInst *winst, uint32_t integrationTime);
BTA_Status BTAP100getIntegrationTime(BTA_WrapperInst *winst, uint32_t *integrationTime);
BTA_Status BTAP100setFrameRate(BTA_WrapperInst *winst, float frameRate);
BTA_Status BTAP100getFrameRate(BTA_WrapperInst *winst, float *frameRate);
BTA_Status BTAP100setModulationFrequency(BTA_WrapperInst *winst, uint32_t modulationFrequency);
BTA_Status BTAP100getModulationFrequency(BTA_WrapperInst *winst, uint32_t *modulationFrequency);
BTA_Status BTAP100setGlobalOffset(BTA_WrapperInst *winst, float globalOffset);
BTA_Status BTAP100getGlobalOffset(BTA_WrapperInst *winst, float *globalOffset);
BTA_Status BTAP100readRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTAP100writeRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTAP100setLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float value);
BTA_Status BTAP100getLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float *value);
BTA_Status BTAP100sendReset(BTA_WrapperInst *winst);
BTA_Status BTAP100flashUpdate(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport);
BTA_Status BTAP100flashRead(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport, uint8_t quiet);
BTA_Status BTAP100writeCurrentConfigToNvm(BTA_WrapperInst *winst);
BTA_Status BTAP100restoreDefaultConfig(BTA_WrapperInst *winst);
#endif

#ifndef BTA_WO_USB
void *BTAUSBdiscoveryRunFunction(BTA_DiscoveryInst *inst);
BTA_Status BTAUSBopen(BTA_Config *config, BTA_WrapperInst *wrapperInst);
BTA_Status BTAUSBclose(BTA_WrapperInst *winst);
BTA_Status BTAUSBgetDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo);
BTA_Status BTAUSBgetDeviceType(BTA_WrapperInst *winst, BTA_DeviceType *deviceType);
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
BTA_Status BTASTREAMopen(BTA_Config *config, BTA_WrapperInst *wrapperInst);
BTA_Status BTASTREAMclose(BTA_WrapperInst *winst);
BTA_Status BTASTREAMgetDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo);
BTA_Status BTASTREAMgetDeviceType(BTA_WrapperInst *winst, BTA_DeviceType *deviceType);
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

#ifndef BTA_WO_UART
//void *BTAUARTdiscoveryRunFunction(BTA_DiscoveryInst *inst);
BTA_Status BTAUARTopen(BTA_Config *config, BTA_WrapperInst *wrapperInst);
BTA_Status BTAUARTclose(BTA_WrapperInst *winst);
BTA_Status BTAUARTgetDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo);
BTA_Status BTAUARTgetDeviceType(BTA_WrapperInst *winst, BTA_DeviceType *deviceType);
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


BTA_Status BTA_CALLCONV BTAstartDiscoveryEx(BTA_DiscoveryConfig *config, BTA_Handle *handle) {
    return BTAstartDiscovery(config, 0, 0, handle);
}


BTA_Status BTA_CALLCONV BTAstartDiscovery(BTA_DiscoveryConfig *config, FN_BTA_DeviceFound deviceFound, FN_BTA_InfoEvent infoEvent, BTA_Handle *handle) {
    if (!config || !handle) {
        return BTA_StatusInvalidParameter;
    }
    if (deviceFound) {
        config->deviceFound = deviceFound;
    }

    if (config->deviceType == BTA_DeviceTypeAny) {
        // Otherwise syntax error if WO_ETH defined
    }
#   ifndef BTA_WO_ETH
    else if (BTAisEthDevice((uint16_t)config->deviceType)) {
        config->deviceType = BTA_DeviceTypeEthernet;
    }
#   endif
#   ifndef BTA_WO_P100
    else if (BTAisP100Device((uint16_t)config->deviceType)) {
        config->deviceType = BTA_DeviceTypeUsb;
    }
#   endif
#   ifndef BTA_WO_USB
    else if (BTAisUsbDevice((uint16_t)config->deviceType)) {
        config->deviceType = BTA_DeviceTypeUsb;
    }
#   endif
    else {
        return BTA_StatusInvalidParameter;
    }

    BTA_DiscoveryInst *inst = (BTA_DiscoveryInst *)calloc(1, sizeof(BTA_DiscoveryInst));
    if (!inst) {
        return BTA_StatusOutOfMemory;
    }
    inst->deviceType = config->deviceType;

    if (config->broadcastIpAddr) {
        if (config->broadcastIpAddrLen != 4) {
            free(inst);
            return BTA_StatusInvalidParameter;
        }
        inst->broadcastIpAddrLen = config->broadcastIpAddrLen;
        inst->broadcastIpAddr = (uint8_t *)malloc(inst->broadcastIpAddrLen);
        if (!inst->broadcastIpAddr) {
            free(inst);
            return BTA_StatusOutOfMemory;
        }
        memcpy(inst->broadcastIpAddr, config->broadcastIpAddr, inst->broadcastIpAddrLen);
    }
    inst->broadcastPort = config->broadcastPort;

    if (config->callbackIpAddr) {
        if (config->callbackIpAddrLen != 4) {
            free(inst);
            return BTA_StatusInvalidParameter;
        }
        inst->callbackIpAddrLen = config->callbackIpAddrLen;
        inst->callbackIpAddr = (uint8_t *)malloc(inst->callbackIpAddrLen);
        if (!inst->callbackIpAddr) {
            free(inst->broadcastIpAddr);
            free(inst);
            return BTA_StatusOutOfMemory;
        }
        memcpy(inst->callbackIpAddr, config->callbackIpAddr, inst->callbackIpAddrLen);
    }
    inst->callbackPort = config->callbackPort;

    inst->deviceFound = config->deviceFound;
    inst->deviceFoundEx = config->deviceFoundEx;
    inst->userArg = config->userArg;

    if (config->millisInterval) {
        inst->millisInterval = config->millisInterval < 1000 ? 1000 : config->millisInterval;
    }
    inst->periodicReports = config->periodicReports;

    BTA_InfoEventInst *infoEventInst = (BTA_InfoEventInst *)calloc(1, sizeof(BTA_InfoEventInst));
    if (!infoEventInst) {
        free(inst->callbackIpAddr);
        free(inst->broadcastIpAddr);
        free(inst);
        return BTA_StatusOutOfMemory;
    }
    infoEventInst->handle = inst;
    infoEventInst->infoEvent = infoEvent;
    infoEventInst->infoEventEx2 = config->infoEventEx2;
    infoEventInst->verbosity = UINT8_MAX;

    inst->infoEventInst = infoEventInst;

    BTA_Status status = BTAinitMutex(&inst->deviceListMutex);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_CRITICAL, status, "BTAopen Eth: Cannot init controlMutex");
        free(inst->callbackIpAddr);
        free(inst->broadcastIpAddr);
        free(infoEventInst);
        free(inst);
        return status;
    }
    inst->deviceList = (BTA_DeviceInfo **)calloc(250, sizeof(BTA_DeviceInfo *));
    inst->deviceListCountMax = 250;
    inst->deviceListCount = 0;

#   ifndef BTA_WO_ETH
    if (inst->deviceType != BTA_DeviceTypeUsb && inst->deviceType != BTA_DeviceTypeUart && inst->deviceType != BTA_DeviceTypeBltstream) {
        status = BTAcreateThread(&inst->discoveryThreadEth, (void *(*)(void *)) & BTAETHdiscoveryRunFunction, inst);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, status, "Discovery: Could not start Eth discoveryThread");
        }
    }
#   endif
#   ifndef BTA_WO_USB
    if (inst->deviceType != BTA_DeviceTypeEthernet && inst->deviceType != BTA_DeviceTypeUart && inst->deviceType != BTA_DeviceTypeBltstream) {
        status = BTAcreateThread(&inst->discoveryThreadUsb, (void *(*)(void *)) & BTAUSBdiscoveryRunFunction, inst);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "Discovery: Could not start USB discoveryThread");
        }
    }
#   endif

    *handle = inst;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAstopDiscovery(BTA_Handle *handle) {
    return BTAstopDiscoveryEx(handle, 0, 0);
}


BTA_Status BTA_CALLCONV BTAstopDiscoveryEx(BTA_Handle *handle, BTA_DeviceInfo **deviceList, uint16_t *deviceListLen) {
    BTA_Status status;
    if (!handle) {
        return BTA_StatusInvalidParameter;
    }

    BTA_DiscoveryInst *inst = (BTA_DiscoveryInst *)*handle;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    inst->abortDiscovery = 1;
    status = BTAjoinThread(inst->discoveryThreadStream);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "Discovery: unable to join bltstream thread");
        return status;
    }
    status = BTAjoinThread(inst->discoveryThreadUsb);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "Discovery: unable to join USB thread");
        return status;
    }
    status = BTAjoinThread(inst->discoveryThreadP100);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "Discovery: unable to join P100 thread");
        return status;
    }
    status = BTAjoinThread(inst->discoveryThreadEth);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "Discovery: unable to join Eth thread");
        return status;
    }

    status = BTAcloseMutex(inst->deviceListMutex);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "Discovery: Failed to close mutex");
        return status;
    }

    // If user wants the result in a list, copy the list and let user free device infos. Otherwise free deviceInfos!
    if (deviceList && deviceListLen) {
        uint16_t lenMin = inst->deviceListCount < *deviceListLen ? inst->deviceListCount : *deviceListLen;
        for (int i = 0; i < lenMin; i++) {
            deviceList[i] = inst->deviceList[i];
        }
        // This loop frees remaining deviceInfos
        for (int i = lenMin; i < inst->deviceListCount; i++) {
            BTAfreeDeviceInfo(inst->deviceList[i]);
        }
        *deviceListLen = lenMin;
    }
    else {
        for (int i = 0; i < inst->deviceListCount; i++) {
            BTAfreeDeviceInfo(inst->deviceList[i]);
        }
    }

    free(inst->callbackIpAddr);
    free(inst->broadcastIpAddr);
    free(inst->deviceList);
    free(inst->infoEventInst);
    free(inst);
    *handle = 0;
    return BTA_StatusOk;
}


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

    { "udpDataAutoConfig", 0 },
    { "shmDataEnabled", 0 },
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
    MARK_USED(check6);
    MARK_USED(check7);
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
        sprintf(str + strlen(str), " %s %s", __DATE__, __TIME__);
        if (config->deviceType) sprintf(str + strlen(str), "  deviceType %s (0x%x)", BTAdeviceTypeToString(config->deviceType), config->deviceType);
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
        if (config->tcpControlPort) sprintf(str + strlen(str), "  tcpControlPort %d", config->tcpControlPort);
        if (config->tcpDataPort) sprintf(str + strlen(str), "  tcpDataPort %d", config->tcpDataPort);
        if (config->udpDataAutoConfig) sprintf(str + strlen(str), "  udpDataAutoConfig enabled");
        if (config->udpDataIpAddr && config->udpDataIpAddrLen) {
            sprintf(str + strlen(str), "  udpDataIpAddr %d", config->udpDataIpAddr[0]);
            for (uint8_t i = 1; i < config->udpDataIpAddrLen; i++) sprintf(str + strlen(str), ".%d", config->udpDataIpAddr[i]);
        }
        if (config->udpDataPort) sprintf(str + strlen(str), "  udpDataPort %d", config->udpDataPort);
        if (config->shmDataEnabled) sprintf(str + strlen(str), "  shmDataEnabled %d", config->shmDataEnabled);
        if (config->uartPortName) sprintf(str + strlen(str), "  uartPortName %s", config->uartPortName);
        if (config->uartBaudRate) sprintf(str + strlen(str), "  uartBaudRate %d", config->uartBaudRate);
        if (config->uartDataBits) sprintf(str + strlen(str), "  uartDataBits %d", config->uartDataBits);
        if (config->uartStopBits) sprintf(str + strlen(str), "  uartStopBits %d", config->uartStopBits);
        if (config->uartParity) sprintf(str + strlen(str), "  uartParity %d", config->uartParity);
        if (config->uartTransmitterAddress) sprintf(str + strlen(str), "  uartTransmitterAddress %d", config->uartTransmitterAddress);
        if (config->uartReceiverAddress) sprintf(str + strlen(str), "  uartReceiverAddress %d", config->uartReceiverAddress);
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

    // Initialize LibParams
    winst->lpDataStreamReadFailedCount = 0;
    winst->lpDataStreamBytesReceivedCount = 0;
    winst->lpDataStreamPacketsReceivedCount = 0;
    winst->lpDataStreamPacketsMissedCount = 0;
    winst->lpDataStreamPacketsToParse = 0;
    winst->lpDataStreamParseFrameDuration = 0;
    winst->lpDataStreamFrameCounterGap = 1;
    winst->lpDataStreamFrameCounterGapsCount = 0;
    winst->lpDataStreamFramesParsedCount = 0;
    winst->lpAllowIncompleteFrames = 1;
    winst->timeStampLast = 0;
    winst->frameCounterLast = 0;
    winst->lpDataStreamFramesParsedPerSecFrametimes = 0;
    winst->lpDataStreamFramesParsedPerSecUpdated = 0;
    winst->lpPauseCaptureThread = 0;
    winst->lpBilateralFilterWindow = 0;
    winst->lpCalcXyzEnabled = 0;
    winst->lpCalcXyzOffset = 0;
    winst->lpColorFromTofEnabled = 0;
#   ifndef BTA_WO_LIBJPEG
    winst->lpJpgDecodeEnabled = 1;
#   else
    winst->lpJpgDecodeEnabled = 0;
#   endif
    winst->lpUndistortRgbEnabled = 0;
    winst->lpDebugFlags01 = 0;
    winst->lpDebugValue01 = 0;
    winst->lpDebugValue02 = 0;
    winst->lpDebugValue03 = 0;
    winst->lpDebugValue04 = 0;
    winst->lpDebugValue05 = 0;
    winst->lpDebugValue06 = 0;
    winst->lpDebugValue07 = 0;
    winst->lpDebugValue08 = 0;
    winst->lpDebugValue09 = 0;
    winst->lpDebugValue10 = 0;

    winst->infoEventInst = (BTA_InfoEventInst *)calloc(1, sizeof(BTA_InfoEventInst));
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
        winst->infoEventInst->infoEventFilename = (uint8_t *)calloc(1, strlen((char *)config->infoEventFilename) + 1);
        if (!winst->infoEventInst->infoEventFilename) {
            BTAclose((BTA_Handle *)&winst);
            return BTA_StatusOutOfMemory;
        }
        strcpy((char *)winst->infoEventInst->infoEventFilename, (char *)config->infoEventFilename);
    }

    if (str) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, str);
        free(str);
        str = 0;
    }

    // generic config plausibility checks
    int configTest = !!config->frameQueueMode + !!config->frameQueueLength;
    if (configTest != 0 && configTest != 2) {
        if (!config->frameQueueMode) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen: frameQueueMode is missing");
        if (!config->frameQueueLength) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen: frameQueueLength is missing");
        BTAclose((BTA_Handle *)&winst);
        return BTA_StatusInvalidParameter;
    }
    if (config->frameQueueMode != BTA_QueueModeDoNotQueue && config->frameQueueMode != BTA_QueueModeDropCurrent && config->frameQueueMode != BTA_QueueModeDropOldest) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen: Only queue modes DropCurrent and DropOldest are allowed");
        BTAclose((BTA_Handle *)&winst);
        return BTA_StatusInvalidParameter;
    }
    if (config->frameArrivedEx2 && config->frameQueueMode) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen: FrameArrivedEx2 cannot be used in conjunction with frame queueing");
        BTAclose((BTA_Handle *)&winst);
        return BTA_StatusInvalidParameter;
    }

    if (config->deviceType == BTA_DeviceTypeAny)
    {
        if (config->bltstreamFilename) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen: Bltstream filename given, but device type not set. Use BTA_DeviceTypeBltstream (15)");
            BTAclose((BTA_Handle *)&winst);
            return BTA_StatusInvalidParameter;
        }
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "BTAopen: Trying all interfaces (device type not set)!");
    }
#   ifndef BTA_WO_ETH
    else if (BTAisEthDevice((uint16_t)config->deviceType)) {
        config->deviceType = BTA_DeviceTypeEthernet;
    }
#   endif
#   ifndef BTA_WO_P100
    else if (BTAisP100Device((uint16_t)config->deviceType)) {
        config->deviceType = BTA_DeviceTypeUsb;
    }
#   endif
#   ifndef BTA_WO_USB
    else if (BTAisUsbDevice((uint16_t)config->deviceType)) {
        config->deviceType = BTA_DeviceTypeUsb;
    }
#   endif
#   ifndef BTA_WO_STREAM
    else if (config->deviceType == BTA_DeviceTypeBltstream) {
        // deviceType is good
    }
#   endif
#   ifndef BTA_WO_UART
    else if (BTAisUartDevice((uint16_t)config->deviceType)) {
        config->deviceType = BTA_DeviceTypeUart;
    }
#   endif
    else {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen: Invalid or unsupported device type: %d", config->deviceType);
        BTAclose((BTA_Handle *)&winst);
        return BTA_StatusInvalidParameter;
    }

    if (config->calibFileName && config->deviceType != BTA_DeviceTypeUsb) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusConfigParamError, "BTAopen: Parameter calibFileName ignored, please use BTAflashUpdate()");
    }
    if (config->zFactorsFileName && config->deviceType != BTA_DeviceTypeUsb) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusConfigParamError, "BTAopen: Parameter zFactorsFileName ignored, not supported");
    }
    if (config->wigglingFileName && config->deviceType != BTA_DeviceTypeUsb) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusConfigParamError, "BTAopen: Parameter wigglingFileName ignored, please use BTAflashUpdate()");
    }
    if (config->averageWindowLength && config->deviceType != BTA_DeviceTypeUsb) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusConfigParamError, "BTAopen: Parameter averageWindowLength ignored, not supported");
    }

    if (config->frameQueueMode != BTA_QueueModeDoNotQueue && config->frameQueueMode != BTA_QueueModeDropOldest && config->frameQueueMode != BTA_QueueModeDropCurrent) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen: Invalid frameQueueMode, use BTA_QueueModeDoNotQueue, BTA_QueueModeDropOldest or BTA_QueueModeDropCurrent");
        BTAclose((BTA_Handle *)&winst);
        return BTA_StatusInvalidParameter;
    }

    if ((!config->frameQueueLength && config->frameQueueMode != BTA_QueueModeDoNotQueue) || (config->frameQueueLength && config->frameQueueMode == BTA_QueueModeDoNotQueue)) {
        // queueing on and queue size == 0 or queueing off and queue size > 0. Contradiction
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen: Invalid frameQueueLength - frameQueueMode combination");
        BTAclose((BTA_Handle *)&winst);
        return BTA_StatusInvalidParameter;
    }

    BTA_Status status;

    if (config->frameQueueMode) {
        status = BFQinit(config->frameQueueLength, config->frameQueueMode, &(winst->frameQueue));
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen: Error initializing frameQueue");
            BTAclose((BTA_Handle *)&winst);
            return status;
        }
    }

    status = BGRBinit(&winst->grabInst, winst->infoEventInst);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen: Error initializing grabber");
        BTAETHclose(winst);
        return status;
    }

    status = BTAundistortInit(&(winst->undistortInst), winst->infoEventInst);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen: Error initializing undistort");
        BTAETHclose(winst);
        return status;
    }

    status = BTAcalcXYZInit(&(winst->calcXYZInst), winst->infoEventInst);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen: Error initializing calcXYZ");
        BTAETHclose(winst);
        return status;
    }

    winst->frameArrivedInst = (BTA_FrameArrivedInst *)calloc(1, sizeof(BTA_FrameArrivedInst));
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

    BVQinit(8, BTA_QueueModeDropOldest, 0, &winst->lpDataStreamFramesParsedPerSecFrametimes);

#   ifndef BTA_WO_ETH
    if (config->deviceType == BTA_DeviceTypeAny || config->deviceType == BTA_DeviceTypeEthernet) {
        status = BTAETHopen(config, winst);
        if (status == BTA_StatusOk) {
            // connected to an Ethernet device
            winst->close = &BTAETHclose;
            winst->getDeviceInfo = &BTAETHgetDeviceInfo;
            winst->getDeviceType = &BTAETHgetDeviceType;
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

        if (config->deviceType) {
            // The user wanted to connect to a specific device, so do not continue trying others)
            // infoEvent was sent within BTAETHopen
            BTAclose((BTA_Handle *)&winst);
            return status;
        }
    }
#   endif


    if (config->deviceType == BTA_DeviceTypeAny || config->deviceType == BTA_DeviceTypeUsb) {
#       ifndef BTA_WO_P100
        status = BTAP100open(config, winst);
        if (status == BTA_StatusOk) {
            // connected to a P100 device
            winst->close = &BTAP100close;
            winst->getDeviceInfo = &BTAP100getDeviceInfo;
            winst->getDeviceType = &BTAP100getDeviceType;
            winst->isRunning = &BTAP100isRunning;
            winst->isConnected = &BTAP100isConnected;
            winst->setFrameMode = &BTAP100setFrameMode;
            winst->getFrameMode = &BTAP100getFrameMode;
            winst->setIntegrationTime = &BTAP100setIntegrationTime;
            winst->getIntegrationTime = &BTAP100getIntegrationTime;
            winst->setFrameRate = &BTAP100setFrameRate;
            winst->getFrameRate = &BTAP100getFrameRate;
            winst->setModulationFrequency = &BTAP100setModulationFrequency;
            winst->getModulationFrequency = &BTAP100getModulationFrequency;
            winst->setGlobalOffset = &BTAP100setGlobalOffset;
            winst->getGlobalOffset = &BTAP100getGlobalOffset;
            winst->readRegister = &BTAP100readRegister;
            winst->writeRegister = &BTAP100writeRegister;
            winst->setLibParam = &BTAP100setLibParam;
            winst->getLibParam = &BTAP100getLibParam;
            winst->sendReset = &BTAP100sendReset;
            winst->flashUpdate = &BTAP100flashUpdate;
            winst->flashRead = &BTAP100flashRead;
            winst->writeCurrentConfigToNvm = &BTAP100writeCurrentConfigToNvm;
            winst->restoreDefaultConfig = &BTAP100restoreDefaultConfig;
            *handle = winst;
            return BTA_StatusOk;
        }
#       endif

#       ifndef BTA_WO_USB
        status = BTAUSBopen(config, winst);
        if (status == BTA_StatusOk) {
            // connected to a USB device
            winst->close = &BTAUSBclose;
            winst->getDeviceInfo = &BTAUSBgetDeviceInfo;
            winst->getDeviceType = &BTAUSBgetDeviceType;
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
#       endif

        if (config->deviceType) {
            // The user wanted to connect to a specific device, so do not continue trying others)
            // infoEvent was sent within BTAP100open/BTAUSBopen
            BTAclose((BTA_Handle *)&winst);
            return status;
        }
    }


#   ifndef BTA_WO_UART
    if (!config->deviceType || config->deviceType == BTA_DeviceTypeUart) {
        status = BTAUARTopen(config, winst);
        if (status == BTA_StatusOk) {
            // connected to a UART device
            winst->close = &BTAUARTclose;
            winst->getDeviceInfo = &BTAUARTgetDeviceInfo;
            winst->getDeviceType = &BTAUARTgetDeviceType;
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

        if (config->deviceType) {
            // The user wanted to connect to a specific device, so do not continue trying others)
            // infoEvent was sent within BTAUARTopen
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
            winst->getDeviceType = &BTASTREAMgetDeviceType;
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

        if (config->deviceType) {
            // The user wanted to connect to a specific device, so do not continue trying others)
            // infoEvent was sent within BTASTREAMopen
            BTAclose((BTA_Handle *)&winst);
            return status;
        }
    }
#   endif

    assert(!config->deviceType);
    BTAclose((BTA_Handle *)&winst);
    // infoEvent was sent within BTA***open
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
    BTA_Status status;
    if (winst->close) {
        status = winst->close(winst);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose: Failed to close inner handle!");
        }
    }

    status = BTAcalcXYZClose(&(winst->calcXYZInst));
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose: Failed to close calcXYZ!");
    }

    status = BTAundistortClose(&(winst->undistortInst));
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose: Failed to close undistort!");
    }

    //BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAclose: Closing grabber");
    status = BGRBclose(&(winst->grabInst));
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose: Failed to close grabber!");
    }

    if (winst->frameQueue) {
        //BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAclose: Closing frameQueue");
        status = BFQclose(&(winst->frameQueue));
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose: Failed to close frameQueue!");
        }
    }

    //BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAclose: Freeing up the rest");
    status = BVQclose(&(winst->lpDataStreamFramesParsedPerSecFrametimes));
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose: Failed to close internal queue!");
    }

    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAclose done");

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
    free(*handle);
    *handle = 0;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAgetDeviceInfo(BTA_Handle handle, BTA_DeviceInfo **deviceInfo) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetDeviceInfo call");
    BTA_Status status = winst->getDeviceInfo(winst, deviceInfo);
    if (status == BTA_StatusOk) {
        if ((*deviceInfo)->productOrderNumber) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetDeviceInfo response:  device type 0x%04x  PON %s-%06d  fwVer %d.%d.%d  uptime %ds", 
                               (*deviceInfo)->deviceType, (*deviceInfo)->productOrderNumber, (*deviceInfo)->serialNumber, 
                               (*deviceInfo)->firmwareVersionMajor, (*deviceInfo)->firmwareVersionMinor, (*deviceInfo)->firmwareVersionNonFunc, (*deviceInfo)->uptime);
        }
        else {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetDeviceInfo response:  device type 0x%04x  PON %s-%06d  fwVer %d.%d.%d  uptime %ds",
                               (*deviceInfo)->deviceType, "\?\?\?-\?\?\?\?-\?", (*deviceInfo)->serialNumber,
                               (*deviceInfo)->firmwareVersionMajor, (*deviceInfo)->firmwareVersionMinor, (*deviceInfo)->firmwareVersionNonFunc, (*deviceInfo)->uptime);
        }
    }
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, status, "BTAgetDeviceInfo failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAgetDeviceType(BTA_Handle handle, BTA_DeviceType *deviceType) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetDeviceType call");
    BTA_Status status = winst->getDeviceType(winst, deviceType);
    if (status == BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetDeviceType response:  device type 0x%04x", deviceType);
    }
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, status, "BTAgetDeviceType failed!");
    return status;
}


uint8_t BTA_CALLCONV BTAisRunning(BTA_Handle handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return 0;
    }
    return winst->isRunning(winst);
}


uint8_t BTA_CALLCONV BTAisConnected(BTA_Handle handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return 0;
    }
    return winst->isConnected(winst);
}


BTA_Status BTA_CALLCONV BTAsetFrameMode(BTA_Handle handle, BTA_FrameMode frameMode) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }

    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetFrameMode call:  frameMode %s (%d)", BTAframeModeToString(frameMode), frameMode);
    BTA_Status status = winst->setFrameMode(winst, frameMode);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetFrameMode done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, status, "BTAsetFrameMode failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAgetFrameMode(BTA_Handle handle, BTA_FrameMode *frameMode) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetFrameMode call");

    BTA_Status status = winst->getFrameMode(winst, frameMode);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetFrameMode response:  frameMode %s (%d)", BTAframeModeToString(*frameMode), *frameMode);
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, status, "BTAgetFrameMode failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAgetFrame(BTA_Handle handle, BTA_Frame **frame, uint32_t millisecondsTimeout) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }

    if (!winst->frameQueue) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusIllegalOperation, "BTAgetFrame: Frame queueing must be enabled in BTAopen");
        return BTA_StatusIllegalOperation;
    }
    BTA_Status status = BFQdequeue(winst->frameQueue, frame, millisecondsTimeout);
    return status;
}


BTA_Status BTA_CALLCONV BTAsetChannelSelection(BTA_Handle handle, BTA_ChannelSelection *channelSelection, int channelSelectionCount) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (winst->infoEventInst && winst->infoEventInst->verbosity >= VERBOSE_WRITE_OP) {
        char *str = (char *)malloc(1024);
        if (!str) return BTA_StatusOutOfMemory;
        sprintf(str, "BTAsetChannelSelection call:  channelSelection ");
        if (channelSelection) {
            for (int i = 0; i < channelSelectionCount; i++) {
                if (i == 0) sprintf(str + strlen(str), "%s", BTAchannelSelectionToString(channelSelection[i]));
                else sprintf(str + strlen(str), ", %s", BTAchannelSelectionToString(channelSelection[i]));
            }
            sprintf(str + strlen(str), "  channelSelectionCount %d", channelSelectionCount);
        }
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, str);
        free(str);
        str = 0;
    }
    if (channelSelectionCount < 1 || channelSelectionCount > 8) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAsetChannelSelection: Channel selection is limited to 1-8 channels");
        return BTA_StatusInvalidParameter;
    }
    uint32_t dataLen = 8;
    uint32_t data[8] = { 0 };
    for (int i = 0; i < channelSelectionCount; i++) {
        data[i] = channelSelection[i];
    }
    BTA_Status status = BTAwriteRegister(handle, 0x620, data, &dataLen);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAsetChannelSelection: Channel selection probably not supported by this device");
        return status;
    }
    data[0] = BTA_EthImgModeChannelSelection << 3;
    status = BTAwriteRegister(handle, 4, data, 0);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetChannelSelection done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, status, "BTAsetChannelSelection failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAgetChannelSelection(BTA_Handle handle, BTA_ChannelSelection *channelSelection, int *channelSelectionCount) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetChannelSelection call");
    memset(channelSelection, 0, sizeof(BTA_ChannelSelection) * *channelSelectionCount);
    uint32_t imgDataFormat;
    BTA_Status status = BTAreadRegister(handle, 4, &imgDataFormat, 0);
    if (status != BTA_StatusOk) {
        *channelSelectionCount = 0;
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, status, "BTAgetChannelSelection failed!");
        return status;
    }
    uint8_t imageMode = (imgDataFormat >> 3) & 0xff;
    if (imageMode != BTA_EthImgModeChannelSelection) {
        // The image mode is not set to 'user defined'
        *channelSelectionCount = 0;
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAgetChannelSelection response:  image mode not set to 'user defined'");
        return BTA_StatusOk;
    }
    uint32_t dataLen = 8;
    uint32_t data[8] = { 0 };
    status = BTAreadRegister(handle, 0x620, data, &dataLen);
    if (status != BTA_StatusOk) {
        *channelSelectionCount = 0;
        return status;
    }
    for (int i = 0; i < *channelSelectionCount; i++) {
        if (!data[i]) {
            *channelSelectionCount = i;
            break;
        }
        channelSelection[i] = (BTA_ChannelSelection)data[i];
    }
    if (winst->infoEventInst && winst->infoEventInst->verbosity >= VERBOSE_READ_OP) {
        char *str = (char *)malloc(1024);
        if (!str) return BTA_StatusOutOfMemory;
        sprintf(str, "BTAgetChannelSelection response:  channelSelection ");
        if (channelSelection) {
            for (int i = 0; i < *channelSelectionCount; i++) {
                if (i == 0) sprintf(str + strlen(str), "%s", BTAchannelSelectionToString(channelSelection[i]));
                else sprintf(str + strlen(str), ", %s", BTAchannelSelectionToString(channelSelection[i]));
            }
            sprintf(str + strlen(str), "  channelSelectionCount %d", *channelSelectionCount);
        }
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, str);
        free(str);
        str = 0;
    }
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAgetFrameCount(BTA_Handle handle, uint32_t *frameCount) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (!winst->frameQueue) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusIllegalOperation, "BTAgetFrameCount: Frame queueing must be enabled in BTAopen");
        return BTA_StatusIllegalOperation;
    }
    return BFQgetCount(winst->frameQueue, frameCount);
}


BTA_Status BTA_CALLCONV BTAflushFrameQueue(BTA_Handle handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (!winst->frameQueue) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusIllegalOperation, "BTAflushFrameQueue: Frame queueing must be enabled in BTAopen");
        return BTA_StatusIllegalOperation;
    }
    return BFQclear(winst->frameQueue);
}


BTA_Status BTA_CALLCONV BTAsetIntegrationTime(BTA_Handle handle, uint32_t integrationTime) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetIntegrationTime call:  integrationTime %d", integrationTime);
    BTA_Status status = winst->setIntegrationTime(winst, integrationTime);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetIntegrationTime done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, status, "BTAsetIntegrationTime failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAgetIntegrationTime(BTA_Handle handle, uint32_t *integrationTime) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetIntegrationTime call");
    BTA_Status status = winst->getIntegrationTime(winst, integrationTime);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetIntegrationTime response:  integrationTime %d", *integrationTime);
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, status, "BTAgetIntegrationTime failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAsetFrameRate(BTA_Handle handle, float frameRate) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetFrameRate call:  frameRate %f", frameRate);
    BTA_Status status = winst->setFrameRate(winst, frameRate);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetFrameRate done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, status, "BTAsetFrameRate failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAgetFrameRate(BTA_Handle handle, float *frameRate) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetFrameRate call");
    BTA_Status status = winst->getFrameRate(winst, frameRate);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetFrameRate response:  frameRate %f", *frameRate);
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, status, "BTAgetFrameRate failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAsetModulationFrequency(BTA_Handle handle, uint32_t modulationFrequency) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetModulationFrequency call:  modulationFrequency %d", modulationFrequency);
    BTA_Status status = winst->setModulationFrequency(winst, modulationFrequency);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetModulationFrequency done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, status, "BTAsetModulationFrequency failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAgetModulationFrequency(BTA_Handle handle, uint32_t *modulationFrequency) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetModulationFrequency call");
    BTA_Status status = winst->getModulationFrequency(winst, modulationFrequency);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetModulationFrequency response:  modulationFrequency %d", *modulationFrequency);
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, status, "BTAgetModulationFrequency failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAsetGlobalOffset(BTA_Handle handle, float globalOffset) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetGlobalOffset call:  globalOffset %f", globalOffset);
    BTA_Status status = winst->setGlobalOffset(winst, globalOffset);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetGlobalOffset done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, status, "BTAsetGlobalOffset failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAgetGlobalOffset(BTA_Handle handle, float *globalOffset) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetGlobalOffset call");
    BTA_Status status = winst->getGlobalOffset(winst, globalOffset);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetGlobalOffset response:  globalOffset %f", *globalOffset);
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, status, "BTAgetGlobalOffset failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAreadRegister(BTA_Handle handle, uint32_t address, uint32_t *data, uint32_t *registerCount) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (registerCount) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAreadRegister call:  address 0x%04x  registerCount %d", address, *registerCount);
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAreadRegister call:  address 0x%04x", address);
    BTA_Status status = winst->readRegister(winst, address, data, registerCount);
    if (status == BTA_StatusOk && winst->infoEventInst && winst->infoEventInst->verbosity >= VERBOSE_READ_OP) {
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
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, status, "BTAreadRegister failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAwriteRegister(BTA_Handle handle, uint32_t address, uint32_t *data, uint32_t *registerCount) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (winst->infoEventInst && winst->infoEventInst->verbosity >= VERBOSE_WRITE_OP) {
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
    BTA_Status status = winst->writeRegister(winst, address, data, registerCount);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAwriteRegister done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, status, "BTAwriteRegister failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAsetLibParam(BTA_Handle handle, BTA_LibParam libParam, float value) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (libParam == BTA_LibParamInfoEventVerbosity) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInformation, "BTAsetLibParam call:  libParam %s (%d)  value %f", BTAlibParamToString(libParam), libParam, value);
    }
    else {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetLibParam call:  libParam %s (%d)  value %f", BTAlibParamToString(libParam), libParam, value);
    }
    BTA_Status status = BTA_StatusOk;
    switch (libParam) {
    case BTA_LibParamInfoEventVerbosity:
        if (!winst->infoEventInst) {
            status = BTA_StatusRuntimeError;
            break;
        }
        winst->infoEventInst->verbosity = (uint8_t)value;
        break;
    case BTA_LibParamUndistortRgb:
        winst->lpUndistortRgbEnabled = (uint8_t)(value != 0);
        break;
    case BTA_LibParamCalcXYZ:
        winst->lpCalcXyzEnabled = (uint8_t)(value != 0);
        break;
    case BTA_LibParamOffsetForCalcXYZ:
        winst->lpCalcXyzOffset = value;
        break;
    case BTA_LibParamBilateralFilterWindow: {
        uint8_t windowSize = (uint8_t)value;
        if (windowSize == 0 || (windowSize >= 3 && (windowSize % 2) == 1)) {
            winst->lpBilateralFilterWindow = windowSize;
            break;
        }
        status = BTA_StatusInvalidParameter;
    }
    case BTA_LibParamGenerateColorFromTof:
        winst->lpColorFromTofEnabled = (uint8_t)(value != 0);
        break;
    case BTA_LibParamEnableJpgDecoding:
#       ifndef BTA_WO_LIBJPEG
        winst->lpJpgDecodeEnabled = (uint8_t)(value != 0);
#       else
        status = value ? BTA_StatusNotSupported : BTA_StatusOk;
#       endif
        break;
    case BTA_LibParamBltstreamCompressionMode:
        winst->grabInst->lpBltstreamCompressionMode = (BTA_CompressionMode)value;
        break;

    case BTA_LibParamPauseCaptureThread:
        winst->lpPauseCaptureThread = (uint8_t)(value != 0);
        break;


    case BTA_LibParamDataStreamReadFailedCount:
    case BTA_LibParamDataStreamBytesReceivedCount:
    case BTA_LibParamDataStreamPacketsReceivedCount:
    case BTA_LibParamDataStreamPacketsMissedCount:
    case BTA_LibParamDataStreamPacketsToParse:
    case BTA_LibParamDataStreamParseFrameDuration:
    case BTA_LibParamDataStreamFrameCounterGapsCount:
    case BTA_LibParamDataStreamFramesParsedCount:
    case BTA_LibParamDataStreamFramesParsedPerSec:
        status = BTA_StatusIllegalOperation;
        break;

    case BTA_LibParamDataStreamFrameCounterGap:
        winst->lpDataStreamFrameCounterGap = value;
        break;

    case BTA_LIBParamDataStreamAllowIncompleteFrames:
        winst->lpAllowIncompleteFrames = value < 1.0f ? 0.0f : 1.0f;
        break;

    case BTA_LibParamDebugFlags01:
        winst->lpDebugFlags01 = (uint32_t)value;
        break;
    case BTA_LibParamDebugValue01:
        winst->lpDebugValue01 = value;
        break;
    case BTA_LibParamDebugValue02:
        winst->lpDebugValue02 = value;
        break;
    case BTA_LibParamDebugValue03:
        winst->lpDebugValue03 = value;
        break;
    case BTA_LibParamDebugValue04:
        winst->lpDebugValue04 = value;
        break;
    case BTA_LibParamDebugValue05:
        winst->lpDebugValue05 = value;
        break;
    case BTA_LibParamDebugValue06:
        winst->lpDebugValue06 = value;
        break;
    case BTA_LibParamDebugValue07:
        winst->lpDebugValue07 = value;
        break;
    case BTA_LibParamDebugValue08:
        winst->lpDebugValue08 = value;
        break;
    case BTA_LibParamDebugValue09:
        winst->lpDebugValue09 = value;
        break;
    case BTA_LibParamDebugValue10:
        winst->lpDebugValue10 = value;
        break;

    default:
        status = winst->setLibParam(winst, libParam, value);
        break;
    }
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsetLibParam done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, status, "BTAsetLibParam failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAgetLibParam(BTA_Handle handle, BTA_LibParam libParam, float *value) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst || !value) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAgetLibParam call:  libParam %d (%s)", libParam, BTAlibParamToString(libParam));
    BTA_Status status = BTA_StatusOk;
    switch (libParam) {
    case BTA_LibParamInfoEventVerbosity:
        if (!winst->infoEventInst) {
            *value = 0;
        }
        else {
            *value = (float)winst->infoEventInst->verbosity;
        }
        break;
    case BTA_LibParamUndistortRgb:
        *value = (float)winst->lpUndistortRgbEnabled;
        break;
    case BTA_LibParamCalcXYZ:
        *value = (float)winst->lpCalcXyzEnabled;
        break;
    case BTA_LibParamOffsetForCalcXYZ:
        *value = winst->lpCalcXyzOffset;
        break;
    case BTA_LibParamBilateralFilterWindow:
        *value = (float)winst->lpBilateralFilterWindow;
        break;
    case BTA_LibParamGenerateColorFromTof:
        *value = (float)winst->lpColorFromTofEnabled;
        break;
    case BTA_LibParamEnableJpgDecoding:
        *value = (float)winst->lpJpgDecodeEnabled;
        break;
    case BTA_LibParamBltstreamCompressionMode:
        *value = (float)winst->grabInst->lpBltstreamCompressionMode;
        break;

    case BTA_LibParamPauseCaptureThread:
        *value = (float)winst->lpPauseCaptureThread;
        break;


    case BTA_LibParamDataStreamReadFailedCount:
        *value = winst->lpDataStreamReadFailedCount;
        winst->lpDataStreamReadFailedCount = 0;
        break;
    case BTA_LibParamDataStreamBytesReceivedCount:
        *value = winst->lpDataStreamBytesReceivedCount;
        winst->lpDataStreamBytesReceivedCount = 0;
        break;
    case BTA_LibParamDataStreamPacketsReceivedCount:
        *value = winst->lpDataStreamPacketsReceivedCount;
        winst->lpDataStreamPacketsReceivedCount = 0;
        break;
    case BTA_LibParamDataStreamPacketsMissedCount:
        *value = winst->lpDataStreamPacketsMissedCount;
        winst->lpDataStreamPacketsMissedCount = 0;
        break;
    case BTA_LibParamDataStreamPacketsToParse:
        *value = winst->lpDataStreamPacketsToParse;
        winst->lpDataStreamPacketsToParse = 0;
        break;
    case BTA_LibParamDataStreamParseFrameDuration:
        *value = winst->lpDataStreamParseFrameDuration;
        winst->lpDataStreamParseFrameDuration = 0;
        break;
    case BTA_LibParamDataStreamFrameCounterGapsCount:
        *value = winst->lpDataStreamFrameCounterGapsCount;
        break;
    case BTA_LibParamDataStreamFrameCounterGap:
        *value = winst->lpDataStreamFrameCounterGap;
        break;
    case BTA_LibParamDataStreamFramesParsedCount:
        *value = winst->lpDataStreamFramesParsedCount;
        winst->lpDataStreamFramesParsedCount = 0;
        break;
    case BTA_LibParamDataStreamFramesParsedPerSec: {
        void **list;
        uint32_t listLen;
        uint64_t microsSum = 0;
        uint32_t count = 0;
        BVQgetList(winst->lpDataStreamFramesParsedPerSecFrametimes, &list, &listLen);
        for (uint32_t i = 0; i < listLen; i++) {
            microsSum += (uint64_t)(size_t)list[i];
            count++;
            if (microsSum > 1500) {
                break;
            }
        }
        free(list);
        list = 0;
        if (microsSum == 0 || count == 0) {
            *value = 0;
            break;
        }
        float microsAverage = (float)microsSum / count;
        uint64_t millisPassed = (uint64_t)(BTAgetTickCount64() - winst->lpDataStreamFramesParsedPerSecUpdated);
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
        break;
    }

    case BTA_LIBParamDataStreamAllowIncompleteFrames:
        *value = winst->lpAllowIncompleteFrames;
        break;
    case BTA_LibParamDebugFlags01:
        *value = (float)winst->lpDebugFlags01;
        break;
    case BTA_LibParamDebugValue01:
        *value = (float)winst->lpDebugValue01;
        break;
    case BTA_LibParamDebugValue02:
        *value = (float)winst->lpDebugValue02;
        break;
    case BTA_LibParamDebugValue03:
        *value = (float)winst->lpDebugValue03;
        break;
    case BTA_LibParamDebugValue04:
        *value = (float)winst->lpDebugValue04;
        winst->lpDebugValue04 = 0;
        break;
    case BTA_LibParamDebugValue05:
        *value = (float)winst->lpDebugValue05;
        break;
    case BTA_LibParamDebugValue06:
        *value = (float)winst->lpDebugValue06;
        winst->lpDebugValue06 = 0;
        break;
    case BTA_LibParamDebugValue07:
        *value = (float)winst->lpDebugValue07;
        winst->lpDebugValue07 = 0;
        break;
    case BTA_LibParamDebugValue08:
        *value = (float)winst->lpDebugValue08;
        winst->lpDebugValue08 = 0;
        break;
    case BTA_LibParamDebugValue09:
        *value = (float)winst->lpDebugValue09;
        winst->lpDebugValue09 = 0;
        break;
    case BTA_LibParamDebugValue10:
        *value = (float)winst->lpDebugValue10;
        winst->lpDebugValue10 = 0;
        break;

    default:
        status = winst->getLibParam(winst, libParam, value);
        break;
    }
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetLibParam response:  value %f", *value);
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, status, "BTAgetLibParam failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAsendReset(BTA_Handle handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (winst->infoEventInst && winst->infoEventInst->verbosity >= VERBOSE_WRITE_OP) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsendReset call");
    }
    BTA_Status status = winst->sendReset(winst);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAsendReset done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, status, "BTAsendReset failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAinitFlashUpdateConfig(BTA_FlashUpdateConfig *config) {
    if (!config) {
        return BTA_StatusInvalidParameter;
    }
    memset(config, 0, sizeof(BTA_FlashUpdateConfig));
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAflashUpdate(BTA_Handle handle, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst || !flashUpdateConfig) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAflashUpdate call:  target %d  flashId %d  address 0x%x  data 0x%p  dataLen %d",
                       flashUpdateConfig->target, flashUpdateConfig->flashId, flashUpdateConfig->address, flashUpdateConfig->data, flashUpdateConfig->dataLen);
    BTA_Status status = winst->flashUpdate(winst, flashUpdateConfig, progressReport);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAflashUpdate done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, status, "BTAflashUpdate failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAgetLensParameters(BTA_Handle handle, BTA_IntrinsicData *intrinsicData, uint32_t *intrinsicDataLen, BTA_ExtrinsicData *extrinsicData, uint32_t *extrinsicDataLen) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (!intrinsicData == !!intrinsicDataLen) {  // checks if both or none are given
        return BTA_StatusInvalidParameter;
    }
    if (!extrinsicData == !!extrinsicDataLen) {  // checks if both or none are given
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetLensParameters call");
    if (intrinsicData && intrinsicDataLen) {
        memset(intrinsicData, 0, *intrinsicDataLen * sizeof(BTA_IntrinsicData));
    }
    if (extrinsicData && extrinsicDataLen) {
        memset(extrinsicData, 0, *extrinsicDataLen * sizeof(BTA_ExtrinsicData));
    }

    BTA_IntrinsicData **intDataTemp = 0;
    uint16_t intDataLenTemp = 0;
    BTA_ExtrinsicData **extDataTemp = 0;
    uint16_t extDataLenTemp = 0;
    BTA_Status status = BTAreadGeomModelFromFlash(winst, &intDataTemp, &intDataLenTemp, &extDataTemp, &extDataLenTemp, 0);
    if (status == BTA_StatusOk) {
        if (intrinsicData && intrinsicDataLen) {
            if (*intrinsicDataLen < intDataLenTemp) {
                BTAfreeIntrinsicData(&intDataTemp, intDataLenTemp);
                BTAfreeExtrinsicData(&extDataTemp, extDataLenTemp);
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetLensParameters failed:  Not enough spcae for %d intrinsic calibrations", intDataLenTemp);
                return BTA_StatusOutOfMemory;
            }
            *intrinsicDataLen = intDataLenTemp;
            for (int i = 0; i < intDataLenTemp; i++) {
                if (intDataTemp[i]) {
                    memcpy(&(intrinsicData[i]), intDataTemp[i], sizeof(BTA_IntrinsicData));
                }
                else
                {
                    memset(&(intrinsicData[i]), 0, sizeof(BTA_IntrinsicData));
                }
            }
        }
        if (extrinsicData && extrinsicDataLen) {
            if (*extrinsicDataLen < extDataLenTemp) {
                BTAfreeIntrinsicData(&intDataTemp, intDataLenTemp);
                BTAfreeExtrinsicData(&extDataTemp, extDataLenTemp);
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetLensParameters failed:  Not enough spcae for %d extrinsic calibrations", extDataLenTemp);
                return BTA_StatusOutOfMemory;
            }
            *extrinsicDataLen = extDataLenTemp;
            for (int i = 0; i < extDataLenTemp; i++) {
                if (extDataTemp[i]) {
                    memcpy(&(extrinsicData[i]), extDataTemp[i], sizeof(BTA_ExtrinsicData));
                }
                else
                {
                    memset(&(extrinsicData[i]), 0, sizeof(BTA_ExtrinsicData));
                }
            }
        }
        BTAfreeIntrinsicData(&intDataTemp, intDataLenTemp);
        BTAfreeExtrinsicData(&extDataTemp, extDataLenTemp);
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetLensParameters response:  %d intrinsic and %d extrinsic calibrations", intDataLenTemp, extDataLenTemp);
    }
    else {
        if (intrinsicDataLen) *intrinsicDataLen = 0;
        if (extrinsicDataLen) *extrinsicDataLen = 0;
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, status, "BTAgetLensParameters failed!");
    }
    return status;
}


BTA_Status BTA_CALLCONV BTAgetLensVectors(BTA_Handle handle, BTA_LensVectors **lensVectorsList, uint16_t *lensVectorsListLen) {
    if (!handle || !lensVectorsList || !lensVectorsListLen) {
        return BTA_StatusInvalidParameter;
    }
    memset(lensVectorsList, 0, *lensVectorsListLen * sizeof(BTA_LensVectors *));
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;

    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetLensVectors call");
    BTA_FlashUpdateConfig flashUpdateConfig;
    BTAinitFlashUpdateConfig(&flashUpdateConfig);
    flashUpdateConfig.target = BTA_FlashTargetLensCalibration;
    BTA_LensVectors *vectors = 0;
    BTA_Status status = BTAflashRead(winst, &flashUpdateConfig, 0);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, status, "BTAgetLensVectors failed!");
        return status;
    }
    status = BTAparseLenscalib(flashUpdateConfig.data, flashUpdateConfig.dataLen, &vectors, winst->infoEventInst);
    free(flashUpdateConfig.data);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, status, "BTAgetLensVectors failed!");
        return status;
    }
    lensVectorsList[0] = vectors;
    *lensVectorsListLen = 1;
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetLensVectors response:  Read %d lens vector sets", *lensVectorsListLen);
    return BTA_StatusOk;
}


void BTA_CALLCONV BTAfreeLensVectors(BTA_LensVectors *lensVectors) {
    free(lensVectors->vectorsX);
    lensVectors->vectorsX = 0;
    free(lensVectors->vectorsY);
    lensVectors->vectorsY = 0;
    free(lensVectors->vectorsZ);
    lensVectors->vectorsZ = 0;
    free(lensVectors);
    lensVectors = 0;
}


BTA_Status BTA_CALLCONV BTAflashRead(BTA_Handle handle, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst || !flashUpdateConfig) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAflashRead call:  target %d  flashId %d  address 0x%x  data 0x%p  dataLen %d",
                       100 + flashUpdateConfig->target, flashUpdateConfig->flashId, flashUpdateConfig->address, flashUpdateConfig->data, flashUpdateConfig->dataLen);
    BTA_Status status = winst->flashRead(winst, flashUpdateConfig, progressReport, 0);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAflashRead done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, status, "BTAflashRead failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAwriteCurrentConfigToNvm(BTA_Handle handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAwriteCurrentConfigToNvm call");
    BTA_Status status = winst->writeCurrentConfigToNvm(winst);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAwriteCurrentConfigToNvm done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, status, "BTAwriteCurrentConfigToNvm failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTArestoreDefaultConfig(BTA_Handle handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTArestoreDefaultConfig call");
    BTA_Status status = winst->restoreDefaultConfig(winst);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTArestoreDefaultConfig done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, status, "BTArestoreDefaultConfig failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAstartGrabbing(BTA_Handle handle, BTA_GrabbingConfig *config) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (config) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAstartGrabbing call:  filename %s", config->filename);
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAstartGrabbing call:  grabbingConfig null");

    if (!config) {
        // == stopGrabbing
        return BGRBstop(winst->grabInst);
    }

    BTA_DeviceInfo *deviceInfo;
    BTA_Status status = winst->getDeviceInfo(winst, &deviceInfo);
    if (status != BTA_StatusOk) {
        // allow grabbing without control connection
        deviceInfo = (BTA_DeviceInfo *)calloc(1, sizeof(BTA_DeviceInfo));
        if (!deviceInfo) {
            return BTA_StatusOutOfMemory;
        }
    }
    status = BGRBstart(winst->grabInst, config, deviceInfo);
    BTAfreeDeviceInfo(deviceInfo);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAstartGrabbing done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, status, "BTAstartGrabbing failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAfirmwareUpdate(BTA_Handle handle, const uint8_t *filename, FN_BTA_ProgressReport progressReport) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAfirmwareUpdate call:  filename %s  progressReport 0x%p", filename, (void *)progressReport);
    BTA_Status status = flashUpdate(winst, filename, progressReport, BTA_FlashTargetApplication);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAfirmwareUpdate done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, status, "BTAfirmwareUpdate failed!");
    return status;
}

BTA_Status BTA_CALLCONV BTAfpnUpdate(BTA_Handle handle, const uint8_t *filename, FN_BTA_ProgressReport progressReport) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAfpnUpdate call:  filename %s  progressReport 0x%p", filename, (void *)progressReport);
    BTA_Status status = flashUpdate(winst, filename, progressReport, BTA_FlashTargetFpn);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAfpnUpdate done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, status, "BTAfpnUpdate failed!");
    return status;
}

BTA_Status BTA_CALLCONV BTAfppnUpdate(BTA_Handle handle, const uint8_t *filename, FN_BTA_ProgressReport progressReport) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAfppnUpdate call:  filename %s  progressReport 0x%p", filename, (void *)progressReport);
    BTA_Status status = flashUpdate(winst, filename, progressReport, BTA_FlashTargetFppn);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAfppnUpdate done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, status, "BTAfppnUpdate failed!");
    return status;
}

BTA_Status BTA_CALLCONV BTAwigglingUpdate(BTA_Handle handle, const uint8_t *filename, FN_BTA_ProgressReport progressReport) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAwigglingUpdate call:  filename %s  progressReport 0x%p", filename, (void *)progressReport);
    BTA_Status status = flashUpdate(winst, filename, progressReport, BTA_FlashTargetWigglingCalibration);
    if (status == BTA_StatusOk) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusInformation, "BTAwigglingUpdate done");
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, status, "BTAwigglingUpdate failed!");
    return status;
}


BTA_Status BTA_CALLCONV BTAgetValidModulationFrequencies(BTA_Handle handle, const uint32_t **modulationFrequencies, int32_t *modulationFrequenciesCount) {
    BTA_Status status;
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (!modulationFrequencies || !modulationFrequenciesCount) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetValidModulationFrequencies call:  modulationFrequencies 0x%p  modulationFrequenciesCount %d", modulationFrequencies, modulationFrequenciesCount);

    if (winst->modFreqsReadFromDevice > -3 && winst->modFreqsReadFromDevice <= 0) {
        // try to read registers that specify supported modulation frequencies
        uint32_t dataLen = sizeof(winst->modFreqs) / sizeof(uint32_t);
        status = BTAreadRegister(handle, 0x700, (uint32_t *)winst->modFreqs, &dataLen);
        if (status == BTA_StatusOk) {
            // remove empty items from tail of list
            while (dataLen >= 0 && !winst->modFreqs[dataLen - 1]) {
                dataLen--;
            }
            for (uint32_t i = 0; i < dataLen; i++) {
                winst->modFreqs[i] *= 10000;
            }
            if (dataLen) {
                winst->modFreqsReadFromDevice = dataLen;
            }
            else {
                // We read the registers, but all of them are 0. no retry
                winst->modFreqsReadFromDevice = -3;
            }
        }
        else if (status == BTA_StatusInvalidParameter) {
            // We couldn't read the registers, unsupported
            winst->modFreqsReadFromDevice = -4;
        }
        else {
            // We couldn't read the registers, error, ..retry
            winst->modFreqsReadFromDevice--;
        }
    }
    if (winst->modFreqsReadFromDevice > 0) {
        *modulationFrequencies = winst->modFreqs;
        *modulationFrequenciesCount = winst->modFreqsReadFromDevice;
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetValidModulationFrequencies response:  modulationFrequenciesCount %d", *modulationFrequenciesCount);
        return BTA_StatusOk;
    }
    BTA_DeviceType deviceType;
    status = BTAgetDeviceType(handle, &deviceType);
    if (status != BTA_StatusOk) {
        *modulationFrequencies = 0;
        *modulationFrequenciesCount = 0;
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, status, "BTAgetValidModulationFrequencies failed! (when reading device type for fallback)");
        return status;
    }
    // fallback: use compiled values
    switch ((int)deviceType) {
    case Argos3dP33x:                *modulationFrequencies = modFreqs_05; *modulationFrequenciesCount = sizeof(modFreqs_05); break;
    case Mlx75123ValidationPlatform: *modulationFrequencies = modFreqs_04; *modulationFrequenciesCount = sizeof(modFreqs_04); break;
    case Evk7512x:                   *modulationFrequencies = modFreqs_04; *modulationFrequenciesCount = sizeof(modFreqs_04); break;
    case Evk75027:                   *modulationFrequencies = 0;           *modulationFrequenciesCount = 0;                   break;
    case Evk7512xTofCcBa:            *modulationFrequencies = modFreqs_04; *modulationFrequenciesCount = sizeof(modFreqs_04); break;
    case Argos3dP320S:               *modulationFrequencies = modFreqs_01; *modulationFrequenciesCount = sizeof(modFreqs_01); break;
    case GrabberBoard:           *modulationFrequencies = modFreqs_01; *modulationFrequenciesCount = sizeof(modFreqs_01); break;
    case Sentis3dP509:               *modulationFrequencies = modFreqs_01; *modulationFrequenciesCount = sizeof(modFreqs_01); break;
    case LimTesterV2:                *modulationFrequencies = modFreqs_06; *modulationFrequenciesCount = sizeof(modFreqs_06); break;
    case Sentis3dM520:               *modulationFrequencies = modFreqs_01; *modulationFrequenciesCount = sizeof(modFreqs_01); break;
    case Sentis3dM530:               *modulationFrequencies = modFreqs_05; *modulationFrequenciesCount = sizeof(modFreqs_05); break;
    case TimUpIrs1125:               *modulationFrequencies = modFreqs_08; *modulationFrequenciesCount = sizeof(modFreqs_08); break;
    case TimUpIrs1125Ffc:            *modulationFrequencies = modFreqs_10; *modulationFrequenciesCount = sizeof(modFreqs_10); break;
    case Mlx75023TofEval:            *modulationFrequencies = modFreqs_03; *modulationFrequenciesCount = sizeof(modFreqs_03); break;
    case TimUp19kS3Eth:              *modulationFrequencies = modFreqs_02; *modulationFrequenciesCount = sizeof(modFreqs_02); break;
    case Epc610TofModule:            *modulationFrequencies = modFreqs_06; *modulationFrequenciesCount = sizeof(modFreqs_06); break;
    case Argos3dP310:                *modulationFrequencies = modFreqs_01; *modulationFrequenciesCount = sizeof(modFreqs_01); break;
    case Sentis3dM100:               *modulationFrequencies = modFreqs_01; *modulationFrequenciesCount = sizeof(modFreqs_01); break;
    case Argos3dP32x:                *modulationFrequencies = modFreqs_01; *modulationFrequenciesCount = sizeof(modFreqs_01); break;
    case Argos3dP321:                *modulationFrequencies = modFreqs_01; *modulationFrequenciesCount = sizeof(modFreqs_01); break;
    case Sentis3dP509Irs1020:        *modulationFrequencies = modFreqs_05; *modulationFrequenciesCount = sizeof(modFreqs_05); break;
    case Argos3dP510Skt:             *modulationFrequencies = modFreqs_01; *modulationFrequenciesCount = sizeof(modFreqs_01); break;
    case TimUp19kS3EthP:             *modulationFrequencies = modFreqs_02; *modulationFrequenciesCount = sizeof(modFreqs_02); break;
    case MultiTofPlatformMlx:        *modulationFrequencies = modFreqs_04; *modulationFrequenciesCount = sizeof(modFreqs_04); break;
    case MhsCamera:                  *modulationFrequencies = modFreqs_05; *modulationFrequenciesCount = sizeof(modFreqs_05); break;
    case PuFCamera:                  *modulationFrequencies = modFreqs_09; *modulationFrequenciesCount = sizeof(modFreqs_09); break;
    default:
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WRITE_OP, BTA_StatusNotSupported, "BTAgetValidModulationFrequencies failed!");
        return BTA_StatusNotSupported;
    }
    *modulationFrequenciesCount = *modulationFrequenciesCount / sizeof(uint32_t);
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetValidModulationFrequencies response:  (fallback result) modulationFrequenciesCount %d", *modulationFrequenciesCount);
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAgetNextBestModulationFrequency(BTA_Handle handle, uint32_t modFreq, uint32_t *validModFreq, int32_t *index) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetNextBestModulationFrequency call:  modFreq %d", modFreq);
    const uint32_t *modulationFrequencies;
    int32_t modulationFrequenciesCount;
    BTA_Status status = BTAgetValidModulationFrequencies(handle, &modulationFrequencies, &modulationFrequenciesCount);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetNextBestModulationFrequency failed!");
        return status;
    }

    if (!modulationFrequencies) {
        // All devices that don't need no intelligence, just allow any frequency and use offset index 0
        if (index) {
            *index = 0;
        }
        if (validModFreq) {
            *validModFreq = modFreq;
        }
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetNextBestModulationFrequency response:  modFreq %d", modFreq);
        return BTA_StatusOk;
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
    if (validModFreq) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetNextBestModulationFrequency response:  modFreq %d", *validModFreq);
    else BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "BTAgetNextBestModulationFrequency done");
    return BTA_StatusOk;
}


// obsolete
BTA_Status BTA_CALLCONV BTAsetKeepAliveMsgInterval(BTA_Handle handle, float interval) { return BTA_StatusNotSupported; }
BTA_Status BTA_CALLCONV BTAsetControlCrcEnabled(BTA_Handle handle, uint8_t enabled) { return BTA_StatusNotSupported; }



///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle-less functions




BTA_Status BTA_CALLCONV BTAgetDataByChannelId(BTA_Frame *frame, BTA_ChannelId channelId, void **data, BTA_DataFormat *dataFormat, BTA_Unit *unit, uint16_t *xRes, uint16_t *yRes) {
    if (!frame || !frame->channels || !data || !dataFormat || !unit || !xRes || !yRes) {
        return BTA_StatusInvalidParameter;
    }
    for (int chInd = 0; chInd < frame->channelsLen; chInd++) {
        if (frame->channels[chInd]->id == channelId) {
            *dataFormat = frame->channels[chInd]->dataFormat;
            *unit = frame->channels[chInd]->unit;
            *xRes = frame->channels[chInd]->xRes;
            *yRes = frame->channels[chInd]->yRes;
            *data = frame->channels[chInd]->data;
            return BTA_StatusOk;
        }
    }
    return BTA_StatusInvalidParameter;
}


BTA_Status BTA_CALLCONV BTAgetDistances(BTA_Frame *frame, void **distBuffer, BTA_DataFormat *dataFormat, BTA_Unit *unit, uint16_t *xRes, uint16_t *yRes) {
    int chInd;
    if (!frame || !frame->channels || !distBuffer || !dataFormat || !unit || !xRes || !yRes) {
        return BTA_StatusInvalidParameter;
    }
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
    return BTA_StatusInvalidParameter;
}


BTA_Status BTA_CALLCONV BTAgetAmplitudes(BTA_Frame *frame, void **ampBuffer, BTA_DataFormat *dataFormat, BTA_Unit *unit, uint16_t *xRes, uint16_t *yRes) {
    int chInd;
    if (!frame || !frame->channels || !ampBuffer || !dataFormat || !unit || !xRes || !yRes) {
        return BTA_StatusInvalidParameter;
    }
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
    return BTA_StatusInvalidParameter;
}


BTA_Status BTA_CALLCONV BTAgetFlags(BTA_Frame *frame, void **flagBuffer, BTA_DataFormat *dataFormat, BTA_Unit *unit, uint16_t *xRes, uint16_t *yRes) {
    int i;
    if (!frame || !frame->channels || !flagBuffer || !dataFormat || !unit || !xRes || !yRes) {
        return BTA_StatusInvalidParameter;
    }
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
    return BTA_StatusInvalidParameter;
}


BTA_Status BTA_CALLCONV BTAgetXYZcoordinates(BTA_Frame *frame, void **xBuffer, void **yBuffer, void **zBuffer, BTA_DataFormat *dataFormat, BTA_Unit *unit, uint16_t *xRes, uint16_t *yRes) {
    int chInd;
    int xChannel = -1;
    int yChannel = -1;
    int zChannel = -1;
    if (!frame || !frame->channels || !xBuffer || !yBuffer || !zBuffer || !dataFormat || !unit || !xRes || !yRes) {
        return BTA_StatusInvalidParameter;
    }
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
    return BTA_StatusInvalidParameter;
}


BTA_Status BTA_CALLCONV BTAgetColors(BTA_Frame *frame, void **colorBuffer, BTA_DataFormat *dataFormat, BTA_Unit *unit, uint16_t *xRes, uint16_t *yRes) {
    if (!frame || !frame->channels || !colorBuffer || !dataFormat || !unit || !xRes || !yRes) {
        return BTA_StatusInvalidParameter;
    }
    for (int chInd = 0; chInd < frame->channelsLen; chInd++) {
        if (frame->channels[chInd]->id == BTA_ChannelIdColor) {
            *dataFormat = frame->channels[chInd]->dataFormat;
            *unit = frame->channels[chInd]->unit;
            *xRes = frame->channels[chInd]->xRes;
            *yRes = frame->channels[chInd]->yRes;
            *colorBuffer = frame->channels[chInd]->data;
            return BTA_StatusOk;
        }
    }
    return BTA_StatusInvalidParameter;
}


BTA_Status BTA_CALLCONV BTAgetChannels(BTA_Frame *frame, BTA_ChannelFilter *filter, BTA_Channel **channels, int *channlesLen) {
    if (!frame || !frame->channels || !filter || !channels || !channlesLen || !*channlesLen) {
        return BTA_StatusInvalidParameter;
    }
    int count = 0;
    for (int chInd = 0; chInd < frame->channelsLen; chInd++) {
        BTA_Channel *channel = frame->channels[chInd];
        if (filter->filterByChannelId && channel->id != filter->id) continue;
        if (filter->filterByResolution && (channel->xRes != filter->xRes || channel->yRes != filter->yRes)) continue;
        if (filter->filterByDataFormat && channel->dataFormat != filter->dataFormat) continue;
        if (filter->filterByLensIndex && channel->lensIndex != filter->lensIndex) continue;
        if (filter->filterByFlagsMask & (channel->flags ^ filter->flags)) continue;
        if (filter->filterBySequenceCounter && channel->sequenceCounter != filter->sequenceCounter) continue;
        // It's a match!
        channels[count++] = channel;
        if (count >= *channlesLen) {
            return BTA_StatusOutOfMemory;
        }
    }
    *channlesLen = count;
    return count > 0 ? BTA_StatusOk : BTA_StatusInvalidData;
}


BTA_Status BTA_CALLCONV BTAgetMetadata(BTA_Channel *channel, BTA_MetadataId metadataId, void **metadata, uint32_t *metadataLen) {
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
    frame->channels = (BTA_Channel **)calloc(frame->channelsLen, sizeof(BTA_Channel *));
    if (!frame->channels) {
        free(frame);
        frame = 0;
        return BTA_StatusOutOfMemory;
    }
    for (uint8_t chInd = 0; chInd < frameSrc->channelsLen; chInd++) {
        BTA_Status status = BTAcloneChannel(frameSrc->channels[chInd], &(frame->channels[chInd]));
        if (status != BTA_StatusOk) {
            frame->channelsLen = chInd;
            BTAfreeFrame(&frame);
            return status;
        }
    }
    frame->metadata = (BTA_Metadata **)calloc(frame->metadataLen, sizeof(BTA_Metadata *));
    if (!frame->metadata) {
        frame->metadataLen = 0;
        BTAfreeFrame(&frame);
        return BTA_StatusOutOfMemory;
    }
    for (uint32_t mdInd = 0; mdInd < frameSrc->metadataLen; mdInd++) {
        BTA_Status status = BTAcloneMetadata(frameSrc->metadata[mdInd], &(frame->metadata[mdInd]));
        if (status != BTA_StatusOk) {
            frame->metadataLen = mdInd;
            BTAfreeFrame(&frame);
            return status;
        }
    }
    *frameDst = frame;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAgetSerializedLength(BTA_Frame *frame, uint32_t *frameSerializedLen) {
    uint32_t length = 0;
    length += sizeof(uint16_t);                     // preamble
    length += sizeof(uint8_t);                      // version of serialized frame
    length += sizeof(uint8_t);                      // firmwareVersionNonFunc
    length += sizeof(uint8_t);                      // firmwareVersionMinor
    length += sizeof(uint8_t);                      // firmwareVersionMajor
    length += sizeof(float);                        // mainTemp
    length += sizeof(float);                        // ledTemp
    length += sizeof(float);                        // genericTemp
    length += sizeof(uint32_t);                     // frameCounter
    length += sizeof(uint32_t);                     // timeStamp
    length += sizeof(uint8_t);                      // channelsLen
    for (uint32_t chInd = 0; chInd < frame->channelsLen; chInd++) {
        length += sizeof(uint32_t);                     // id
        length += sizeof(uint16_t);                     // xRes
        length += sizeof(uint16_t);                     // yRes
        length += sizeof(uint32_t);                     // dataFormat
        length += sizeof(uint32_t);                     // unit
        length += sizeof(uint32_t);                     // integrationTime
        length += sizeof(uint32_t);                     // modulationFrequency
        length += frame->channels[chInd]->dataLen;      // data
        length += sizeof(uint32_t);                     // dataLen
        length += sizeof(uint32_t);                     // metadataLen
        for (uint32_t mdInd = 0; mdInd < frame->channels[chInd]->metadataLen; mdInd++) {
            length += sizeof(uint32_t);                                 // id
            length += sizeof(uint32_t);                                 // dataLen
            length += frame->channels[chInd]->metadata[mdInd]->dataLen; // data
        }
        length += sizeof(uint8_t);                      // lensIndex
        length += sizeof(uint32_t);                     // flags
        length += sizeof(uint8_t);                      // sequenceCounter
        length += sizeof(float);                        // gain
    }
    length += sizeof(uint32_t);                     // metadataLen
    for (uint32_t mdInd = 0; mdInd < frame->metadataLen; mdInd++) {
        length += sizeof(uint32_t);                     // id
        length += sizeof(uint32_t);                     // dataLen
        length += frame->metadata[mdInd]->dataLen;      // data
    }
    *frameSerializedLen = length;
    return BTA_StatusOk;
}


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
    BTAbitConverterFromUInt16(BTA_FRAME_SERIALIZED_PREAMBLE, frameSerialized, &index);
    BTAbitConverterFromUInt08(BTA_FRAME_SERIALIZED_VERSION, frameSerialized, &index);
    BTAbitConverterFromUInt08(frame->firmwareVersionNonFunc, frameSerialized, &index);
    BTAbitConverterFromUInt08(frame->firmwareVersionMinor, frameSerialized, &index);
    BTAbitConverterFromUInt08(frame->firmwareVersionMajor, frameSerialized, &index);
    BTAbitConverterFromFloat4(frame->mainTemp, frameSerialized, &index);
    BTAbitConverterFromFloat4(frame->ledTemp, frameSerialized, &index);
    BTAbitConverterFromFloat4(frame->genericTemp, frameSerialized, &index);
    BTAbitConverterFromUInt32(frame->frameCounter, frameSerialized, &index);
    BTAbitConverterFromUInt32(frame->timeStamp, frameSerialized, &index);
    BTAbitConverterFromUInt08(frame->channelsLen, frameSerialized, &index);
    for (chInd = 0; chInd < frame->channelsLen; chInd++) {
        BTA_Channel *channel = frame->channels[chInd];
        BTAbitConverterFromUInt32(channel->id, frameSerialized, &index);
        BTAbitConverterFromUInt16(channel->xRes, frameSerialized, &index);
        BTAbitConverterFromUInt16(channel->yRes, frameSerialized, &index);
        BTAbitConverterFromUInt32(channel->dataFormat, frameSerialized, &index);
        BTAbitConverterFromUInt32(channel->unit, frameSerialized, &index);
        BTAbitConverterFromUInt32(channel->integrationTime, frameSerialized, &index);
        BTAbitConverterFromUInt32(channel->modulationFrequency, frameSerialized, &index);
        BTAbitConverterFromUInt32(channel->dataLen, frameSerialized, &index);
        BTAbitConverterFromStream(channel->data, frame->channels[chInd]->dataLen, frameSerialized, &index);
        BTAbitConverterFromUInt32(channel->metadataLen, frameSerialized, &index);
        for (mdInd = 0; mdInd < frame->channels[chInd]->metadataLen; mdInd++) {
            BTA_Metadata *metadata = channel->metadata[mdInd];
            BTAbitConverterFromUInt32(metadata->id, frameSerialized, &index);
            BTAbitConverterFromUInt32(metadata->dataLen, frameSerialized, &index);
            BTAbitConverterFromStream((uint8_t *)metadata->data, metadata->dataLen, frameSerialized, &index);
        }
        BTAbitConverterFromUInt08(channel->lensIndex, frameSerialized, &index);
        BTAbitConverterFromUInt32(channel->flags, frameSerialized, &index);
        BTAbitConverterFromUInt08(channel->sequenceCounter, frameSerialized, &index);
        BTAbitConverterFromFloat4(channel->gain, frameSerialized, &index);
    }
    BTAbitConverterFromUInt32(frame->metadataLen, frameSerialized, &index);
    for (mdInd = 0; mdInd < frame->metadataLen; mdInd++) {
        BTA_Metadata *metadata = frame->metadata[mdInd];
        BTAbitConverterFromUInt32(metadata->id, frameSerialized, &index);
        BTAbitConverterFromUInt32(metadata->dataLen, frameSerialized, &index);
        BTAbitConverterFromStream((uint8_t *)metadata->data, metadata->dataLen, frameSerialized, &index);
    }
    *frameSerializedLen = index;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAcompressSerializedFrame(uint8_t *frameSerialized, uint32_t frameSerializedLen, BTA_CompressionMode compressionMode, uint8_t *frameSerializedCompressed, uint32_t *frameSerializedCompressedLen) {
    if (!frameSerialized || !frameSerializedCompressed || !frameSerializedCompressedLen) {
        return BTA_StatusInvalidParameter;
    }
    switch (compressionMode) {
    case BTA_CompressionModeLzmaV22:
        return BTAcompressSerializedFrameLzmaV22(frameSerialized, frameSerializedLen, frameSerializedCompressed, frameSerializedCompressedLen);
    default:
        return BTA_StatusInvalidParameter;
    }
}


BTA_Status BTA_CALLCONV BTAdeserializeFrame(BTA_Frame **framePtr, uint8_t *frameSerialized, uint32_t *frameSerializedLen) {
    if (!framePtr || !frameSerialized || !frameSerializedLen) {
        return BTA_StatusInvalidParameter;
    }
    *framePtr = 0;
    if (*frameSerializedLen < 3 + sizeof(BTA_Frame)) {
        // not long enough to contain preamble, version and BTA_Frame
        return BTA_StatusOutOfMemory;
    }
    uint32_t index = 0;
    uint16_t preamble = BTAbitConverterToUInt16(frameSerialized, &index);
    if (preamble == BTA_FRAME_SERIALIZED_PREAMBLE_LZMAV22) {
        // The frame is compressed
        uint32_t frameUncompressedLen = BTAbitConverterToUInt32(frameSerialized, &index);
        uint8_t *frameUncompressed = (uint8_t *)malloc(frameUncompressedLen);
        size_t srcLen = *frameSerializedLen - index - LZMA_PROPS_SIZE;
        size_t dstLen = frameUncompressedLen;
        int result = LzmaUncompress(frameUncompressed, &dstLen, frameSerialized + index + LZMA_PROPS_SIZE, &srcLen, frameSerialized + index, (size_t)LZMA_PROPS_SIZE);
        if (result != SZ_OK) {
            free(frameUncompressed);
            frameUncompressed = 0;
            return BTA_StatusRuntimeError;
        }
        if (dstLen != frameUncompressedLen) {
            return BTA_StatusRuntimeError;
        }
        BTA_Status status = BTAdeserializeFrame(framePtr, frameUncompressed, &frameUncompressedLen);
        free(frameUncompressed);
        frameUncompressed = 0;
        *frameSerializedLen = (uint32_t)srcLen + sizeof(uint16_t) + sizeof(uint32_t) + LZMA_PROPS_SIZE;
        return status;
    }
    else if (preamble != BTA_FRAME_SERIALIZED_PREAMBLE) {
        // wrong preamble
        return BTA_StatusInvalidParameter;
    }

    uint8_t version = BTAbitConverterToUInt08(frameSerialized, &index);
    switch (version) {
    case 1:
        return BTAdeserializeFrameV1(framePtr, frameSerialized, frameSerializedLen);
    case 2:
        return BTAdeserializeFrameV2(framePtr, frameSerialized, frameSerializedLen);
    case 3:
        return BTAdeserializeFrameV3(framePtr, frameSerialized, frameSerializedLen);
    case 4:
        return BTAdeserializeFrameV4(framePtr, frameSerialized, frameSerializedLen);
    case 5:
        return BTAdeserializeFrameV5(framePtr, frameSerialized, frameSerializedLen);
    default:
        return BTA_StatusInvalidVersion;
    }
}


BTA_Status BTA_CALLCONV BTAfreeFrame(BTA_Frame **frame) {
    if (!frame) {
        return BTA_StatusInvalidParameter;
    }
    if (!*frame) {
        return BTA_StatusInvalidParameter;
    }
    if ((*frame)->channels) {
        for (int i = 0; i < (*frame)->channelsLen; i++) {
            BTAfreeChannel(&((*frame)->channels[i]));
        }
    }
    free((*frame)->channels);
    (*frame)->channels = 0;
    if ((*frame)->metadata) {
        for (int i = 0; i < (int)(*frame)->metadataLen; i++) {
            BTAfreeMetadata(&((*frame)->metadata[i]));
        }
    }
    free((*frame)->metadata);
    (*frame)->metadata = 0;
    free(*frame);
    *frame = 0;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAfreeFrameFromShm(BTA_Frame **frame) {
    if (!frame) {
        return BTA_StatusInvalidParameter;
    }
    if (!*frame) {
        return BTA_StatusInvalidParameter;
    }
    if ((*frame)->channels) {
        for (int i = 0; i < (*frame)->channelsLen; i++) {
            free((*frame)->channels[i]);
            (*frame)->channels[i] = 0;
        }
    }
    free((*frame)->channels);
    (*frame)->channels = 0;
    if ((*frame)->metadata) {
        for (int i = 0; i < (int)(*frame)->metadataLen; i++) {
            free((*frame)->metadata[i]);
            (*frame)->metadata[i] = 0;
        }
    }
    free((*frame)->metadata);
    (*frame)->metadata = 0;
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
    free(deviceInfo->bltstreamFilename);
    deviceInfo->bltstreamFilename = 0;
    free(deviceInfo);
    deviceInfo = 0;
    return BTA_StatusOk;
}


const char *BTA_CALLCONV BTAdeviceTypeToString(BTA_DeviceType deviceType) {
    switch (deviceType) {
    case BTA_DeviceTypeEthernet:
        return "Ethernet";
    case BTA_DeviceTypeUsb:
        return "USB";
    case BTA_DeviceTypeUart:
        return "UART";
    case BTA_DeviceTypeBltstream:
        return "bltstream";
    default:
        return "<Not a device type>";
    }
}


const char *BTA_CALLCONV BTAlibParamToString(BTA_LibParam libParam) {
    switch ((int)libParam) {
    case BTA_LibParamKeepAliveMsgInterval: return "KeepAliveMsgInterval";
    case BTA_LibParamCrcControlEnabled: return "CrcControlEnabled";
    case BTA_LibParamBltStreamTotalFrameCount: return "BltStreamTotalFrameCount";
    case BTA_LibParamBltStreamAutoPlaybackSpeed: return "BltStreamAutoPlaybackSpeed";
    case BTA_LibParamBltStreamPos: return "BltStreamPos";
    case BTA_LibParamBltStreamPosIncrement: return "BltStreamPosIncrement";
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
    case BTA_LibParamCalcXYZ: return "CalcXYZ";
    case BTA_LibParamOffsetForCalcXYZ: return "OffsetForCalcXYZ";
    case BTA_LibParamBilateralFilterWindow: return "BilateralFilterWindow";
    case BTA_LibParamGenerateColorFromTof: return "GenerateColorFromTof";
    case BTA_LibParamBltstreamCompressionMode: return "BltstreamCompressionMode";
    case BTA_LIBParamDataStreamAllowIncompleteFrames: return "DataStreamAllowIncompleteFrames";
    case BTA_LibParamDebugFlags01: return "DebugFlags01";
    case BTA_LibParamDebugValue01: return "DebugValue01";
    case BTA_LibParamDebugValue02: return "DebugValue02";
    case BTA_LibParamDebugValue03: return "DebugValue03";
    case BTA_LibParamDebugValue04: return "DebugValue04";
    case BTA_LibParamDebugValue05: return "DebugValue05";
    case BTA_LibParamDebugValue06: return "DebugValue06";
    case BTA_LibParamDebugValue07: return "DebugValue07";
    case BTA_LibParamDebugValue08: return "DebugValue08";
    case BTA_LibParamDebugValue09: return "DebugValue09";
    case BTA_LibParamDebugValue10: return "DebugValue10";
    default:
        assert(0);
        return "<Not a BTA_LibParam>";
    }
}


const char *BTA_CALLCONV BTAstatusToString2(BTA_Status status) {
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
    case BTA_StatusAlive: return "Alive";
    case BTA_StatusConfigParamError: return "ConfigParamError";
    default:
        return "<Not a BTA_Status>";
    }
}


const char *BTA_CALLCONV BTAframeModeToString(BTA_FrameMode frameMode) {
    switch (frameMode) {
    case BTA_FrameModeCurrentConfig:        return "Current config";
    case BTA_FrameModeDistAmp:              return "Distance, Amplitude";
    case BTA_FrameModeZAmp:                 return "Z, Amplitude";
    case BTA_FrameModeDistAmpFlags:         return "Distance, Amplitude, Flags";
    case BTA_FrameModeXYZ:                  return "X, Y, Z";
    case BTA_FrameModeXYZAmp:               return "X, Y, Z, Amplitude";
    case BTA_FrameModeDistAmpColor:         return "Distance, Amplitude, Color";
    case BTA_FrameModeXYZAmpFlags:          return "X, Y, Z, Amplitude, Flags";
    case BTA_FrameModeRawPhases:            return "Raw phases";
    case BTA_FrameModeIntensities:          return "Intensities";
    case BTA_FrameModeDistColor:            return "Distance, Color";
    case BTA_FrameModeDistAmpBalance:       return "Distance, Amplitude, Balance";
    case BTA_FrameModeXYZColor:             return "X, Y, Z, Color";
    case BTA_FrameModeDist:                 return "Distance";
    case BTA_FrameModeDistConfExt:          return "Distance, Confidence";
    case BTA_FrameModeAmp:                  return "Amplitude";
    case BTA_FrameModeRawdistAmp:           return "Raw distance, Amplitude";
    case BTA_FrameModeRawPhasesExt:         return "Raw phases";
    case BTA_FrameModeRawQI:                return "Raw QI";
    case BTA_FrameModeXYZConfColor:         return "X, Y, Z, Amplitude, Confidence, Color";
    case BTA_FrameModeXYZAmpColorOverlay:   return "X, Y, Z, Amplitude, Color, Overlay";
    case BTA_FrameModeDistAmpConf:          return "Distance, Amplitude, Confidence";
    case BTA_FrameModeChannelSelection:     return "Channel selection mode";
    default:
        assert(0); 
        return "<Not a BTA_FrameMode>";
    }
}


const char *BTA_CALLCONV BTAdataFormatToString(BTA_DataFormat dataFormat) {
    switch (dataFormat) {
        case BTA_DataFormatUnknown: return "Unknown";
        case BTA_DataFormatUInt8: return "UInt8";
        case BTA_DataFormatUInt16: return "UInt16";
        case BTA_DataFormatSInt16Mlx1C11S: return "SInt16Mlx1C11S";
        case BTA_DataFormatSInt16Mlx12S: return "SInt16Mlx12S";
        case BTA_DataFormatUInt16Mlx1C11U: return "UInt16Mlx1C11U";
        case BTA_DataFormatUInt16Mlx12U: return "UInt16Mlx12U";
        //case BTA_DataFormatUInt24 return "UInt24";
        case BTA_DataFormatUInt32: return "UInt32";
        //case BTA_DataFormatSInt8  return "SInt8";
        case BTA_DataFormatSInt16: return "SInt16";
        //case BTA_DataFormatSInt24 return "SInt24";
        case BTA_DataFormatSInt32: return "SInt32";
        //case BTA_DataFormatFloat8 return "Float8";
        //case BTA_DataFormatFloat16return "Float16";
        //case BTA_DataFormatFloat24return "Float24";
        case BTA_DataFormatFloat32: return "Float32";
        case BTA_DataFormatFloat64: return "Float64";
        case BTA_DataFormatRgb565: return "Rgb565";
        case BTA_DataFormatRgb24: return "Rgb24";
        case BTA_DataFormatJpeg: return "Jpeg";
        case BTA_DataFormatYuv422: return "Yuv422";
        case BTA_DataFormatYuv444: return "Yuv444";
        case BTA_DataFormatYuv444UYV: return "Yuv444UYV";
        default:
            assert(0);
            return "<Not a BTA_DataFormat>";
    }
}


const char *BTA_CALLCONV BTAchannelSelectionToString(BTA_ChannelSelection channelSelection) {
    switch (channelSelection) {
    case BTA_ChannelSelectionInactive:      return "Inactive";
    case BTA_ChannelSelectionDistance:      return "Distance";
    case BTA_ChannelSelectionAmplitude:     return "Amplitude";
    case BTA_ChannelSelectionX:             return "X";
    case BTA_ChannelSelectionY:             return "Y";
    case BTA_ChannelSelectionZ:             return "Z";
    case BTA_ChannelSelectionConfidence:    return "Confidence";
    case BTA_ChannelSelectionHeightMap:     return "HeightMap";
    case BTA_ChannelSelectionStdev:         return "Stdev";
    case BTA_ChannelSelectionColor0:        return "Color0";
    case BTA_ChannelSelectionOverlay0:      return "Overlay0";
    case BTA_ChannelSelectionColor1:        return "Color1";
    case BTA_ChannelSelectionOverlay1:      return "Overlay1";
    case BTA_ChannelSelectionAmplitude8:    return "Amplitude8";
    default: return "<Not a BTA_ChannelSelection>";
    }
}


// obsolete, use BTAstatusToString2!!
BTA_Status BTA_CALLCONV BTAstatusToString(BTA_Status status, char *statusString, uint16_t statusStringLen) {
    if (!statusString) {
        return BTA_StatusInvalidParameter;
    }
    const char *str = BTAstatusToString2(status);
    if (strcmp(str, "<not a BTA_Status>")) {
        if (statusStringLen <= strlen(str)) return BTA_StatusOutOfMemory;
        strcpy((char *)statusString, str);
    }
    else {
        if (statusStringLen <= strlen("Status -32xxx")) return BTA_StatusOutOfMemory;
        sprintf((char *)statusString, "Status %d", status);
    }
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAeventIdToString(BTA_EventId status, char *statusString, uint16_t statusStringLen) {
    return BTAstatusToString(status, statusString, statusStringLen);
}


const char *BTA_CALLCONV BTAunitToString(BTA_Unit unit) {
    switch (unit) {
    case BTA_UnitMeter:
        return "m";
    case BTA_UnitMillimeter:
        return "mm";
    default:
        return "";
    }
}


const char *BTA_CALLCONV BTAchannelIdToString(BTA_ChannelId id) {
    switch (id)
    {
    case BTA_ChannelIdUnknown:
        return "Unknown";
    case BTA_ChannelIdDistance:
        return "Distance";
    case BTA_ChannelIdAmplitude:
        return "Amplitude";
    case BTA_ChannelIdX:
        return "X";
    case BTA_ChannelIdY:
        return "Y";
    case BTA_ChannelIdZ:
        return "Z";
    case BTA_ChannelIdHeightMap:
        return "HeightMap";
    case BTA_ChannelIdConfidence:
        return "Confidence";
    case BTA_ChannelIdFlags:
        return "Flags";
    case BTA_ChannelIdPhase0:
        return "Phase0";
    case BTA_ChannelIdPhase90:
        return "Phase90";
    case BTA_ChannelIdPhase180:
        return "Phase180";
    case BTA_ChannelIdPhase270:
        return "Phase270";
    case BTA_ChannelIdRawPhase:
        return "RawPhase";
    case BTA_ChannelIdRawQ:
        return "RawQ";
    case BTA_ChannelIdRawI:
        return "RawI";
    case BTA_ChannelIdTest:
        return "Test";
    case BTA_ChannelIdColor:
        return "Color";
    case BTA_ChannelIdRawDist:
        return "RawDist";
    case BTA_ChannelIdBalance:
        return "Balance";
    case BTA_ChannelIdStdDev:
        return "StdDev";
    case BTA_ChannelIdCustom01:
        return "Custom01";
    case BTA_ChannelIdCustom02:
        return "Custom02";
    case BTA_ChannelIdCustom03:
        return "Custom03";
    case BTA_ChannelIdCustom04:
        return "Custom04";
    case BTA_ChannelIdCustom05:
        return "Custom05";
    case BTA_ChannelIdCustom06:
        return "Custom06";
    case BTA_ChannelIdCustom07:
        return "Custom07";
    case BTA_ChannelIdCustom08:
        return "Custom08";
    case BTA_ChannelIdCustom09:
        return "Custom09";
    case BTA_ChannelIdCustom10:
        return "Custom10";
    default:
        assert(0);
        return "<Not a BTA_ChannelId>";
    }
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void BTA_CALLCONV BTAzeroLogTimestamp() {
    BHLPzeroLogTimestamp();
}


uint8_t BTA_CALLCONV BTAisEthDevice(uint16_t deviceType) {
    if (deviceType == BTA_DeviceTypeEthernet) {
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
    if (deviceType == BTA_DeviceTypeUsb) {
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
    if (deviceType == BTA_DeviceTypeUsb) {
        return 1;
    }
    int i;
    uint16_t p100DeviceTypes[BTA_P100_DEVICE_TYPES_LEN] = { BTA_P100_DEVICE_TYPES };
    for (i = 0; i < BTA_P100_DEVICE_TYPES_LEN; i++) {
        if (deviceType == p100DeviceTypes[i]) {
            return 1;
        }
    }
    return 0;
}


uint8_t BTA_CALLCONV BTAisUartDevice(uint16_t deviceType) {
    if (deviceType == BTA_DeviceTypeUart) {
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


BTA_Status BTA_CALLCONV BTAinsertChannelIntoFrame2(BTA_Frame *frame, BTA_ChannelId id, uint16_t xRes, uint16_t yRes, BTA_DataFormat dataFormat, BTA_Unit unit, uint32_t integrationTime, uint32_t modulationFrequency, uint8_t *data, uint32_t dataLen,
                                                   BTA_Metadata **metadata, uint32_t metadataLen, uint8_t lensIndex, uint32_t flags, uint8_t sequenceCounter, float gain) {
    BTA_Channel *channel;
    channel = (BTA_Channel *)calloc(1, sizeof(BTA_Channel));
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
    for (uint32_t mdInd = 0; mdInd < metadataLen; mdInd++) {
        BTAinsertMetadataIntoChannel(channel, metadata[mdInd]);
    }
    channel->lensIndex = lensIndex;
    channel->flags = flags;
    channel->sequenceCounter = sequenceCounter;
    channel->gain = gain;
    return BTAinsertChannelIntoFrame(frame, channel);
}


BTA_Status BTA_CALLCONV BTAremoveChannelFromFrame(BTA_Frame *frame, BTA_Channel *channel) {
    if (!frame || !channel) {
        return BTA_StatusInvalidParameter;
    }
    if (!frame->channels) {
        return BTA_StatusInvalidParameter;
    }
    uint8_t channelsLenNew = 0;
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
    channel->metadata = (BTA_Metadata **)calloc(channel->metadataLen, sizeof(BTA_Metadata *));
    if (!channel->metadata) {
        free(channel->data);
        channel->data = 0;
        free(channel);
        channel = 0;
        return BTA_StatusOutOfMemory;
    }
    for (uint32_t mdInd = 0; mdInd < channel->metadataLen; mdInd++) {
        BTA_Status status = BTAcloneMetadata(channelSrc->metadata[mdInd], &(channel->metadata[mdInd]));
        if (status != BTA_StatusOk) {
            channel->metadataLen = mdInd;
            BTAfreeChannel(&channel);
            return status;
        }
    }
    *channelDst = channel;
    return BTA_StatusOk;
}


BTA_Status BTA_CALLCONV BTAcloneChannelEmpty(BTA_Channel *channelSrc, BTA_Channel **channelDst) {
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
    channel->metadata = 0;
    channel->metadataLen = 0;
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
    BTA_Metadata *metadata;
    if (!metadataSrc || !metadataDst) {
        return BTA_StatusInvalidParameter;
    }
    *metadataDst = 0;
    metadata = (BTA_Metadata *)malloc(sizeof(BTA_Metadata));
    if (!metadata) {
        return BTA_StatusOutOfMemory;
    }
    memcpy(metadata, metadataSrc, sizeof(BTA_Metadata));
    metadata->data = (uint8_t *)malloc(metadata->dataLen * sizeof(uint8_t));
    if (!metadata->data) {
        free(metadata);
        metadata = 0;
        return BTA_StatusOutOfMemory;
    }
    memcpy(metadata->data, metadataSrc->data, metadata->dataLen * sizeof(uint8_t));
    *metadataDst = metadata;
    return BTA_StatusOk;
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
            ((uint16_t *)result->data)[xy] = (uint16_t)(((uint16_t *)dividend->data)[xy] / divisor);
        }
        break;
    case BTA_DataFormatSInt32:
        for (xy = 0; xy < dividend->xRes * dividend->yRes; xy++) {
            ((int32_t *)result->data)[xy] = (int32_t)(((int32_t *)dividend->data)[xy] / divisor);
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
            //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAsubtChannelInPlace unsupported subtrahend format", subtrahend->dataFormat);
            return BTA_StatusNotSupported;
        }
        break;
    default:
        //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAsubtChannelInPlace unsupported minuend format", minuendDiff->dataFormat);
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
            //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAsubtChannel unsupported subtrahend format", subtrahend->dataFormat);
            BTAfreeChannel(&result);
            return BTA_StatusNotSupported;
        }
        break;
    default:
        //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAsubtChannel unsupported minuend format", minuend->dataFormat);
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
                ((int32_t *)channel->data)[xy] = (((int32_t *)channel->data)[xy] > (int32_t)threshold) || (((int32_t *)channel->data)[xy] < -(int32_t)threshold);
            }
            break;
        default:
            //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAthresholdInPlace unsupported format", channel->dataFormat);
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
                ((int32_t *)channel->data)[xy] = ((int32_t *)channel->data)[xy] > (int32_t)threshold;
            }
            break;
        default:
            //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAthresholdInPlace unsupported format", channel->dataFormat);
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
                data[xy] = (uint16_t)((int32_t *)channel->data)[xy];
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
            //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAsubtChannel unsupported subtrahend format", dataFormat);
            return BTA_StatusNotSupported;
        }
        break;
    default:
        //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, BTA_StatusNotSupported, "BTAsubtChannel unsupported minuend format", channel->dataFormat);
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


BTA_Status BTA_CALLCONV BTAgetOptimalAmplitude(uint16_t deviceType, float *amplitude) {
    if (!amplitude) {
        return BTA_StatusInvalidParameter;
    }
    switch (deviceType)
    {
        // 19kS3
        case TimUp19kS3Spartan6:
        case Argos3dP320S:
        case TimUp19kS3Eth:
        case Argos3dP100:
        case Sentis3dM100:
        case Argos3dP32x:
            *amplitude = 7500;
            return BTA_StatusOk;
        // IRS1020
        case Argos3dP33x:
        case TimUpIrs1125:
        case TimUpIrs1125Ffc:
        case MhsCamera:
        case BTA_DeviceTypeSentis3dP509Irs1020:
            *amplitude = 1800;
            return BTA_StatusOk;
        // PuF TofWare
        case PuFCamera:
        case Argos3dPulse:
        case ToreoP65x:
            *amplitude = 10000;
            return BTA_StatusOk;
        // boh...
        case MultiTofPlatformMlx:
        case Argos3dP510Skt:
        case Argos3dP310:
        case Evk7512x:
        case Evk75027:
        case Evk7512xTofCcBa:
        case GrabberBoard:
        case Sentis3dP509:
        case Sentis3dM520:
        case Sentis3dM530:
        case Epc610TofModule:
            return BTA_StatusNotSupported;
        default:
            return BTA_StatusNotSupported;
    }
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
    case BTA_EthImgModeDistAmpConf:         return BTA_FrameModeDistAmpConf;
    case BTA_EthImgModeChannelSelection:    return BTA_FrameModeChannelSelection;
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
    case BTA_FrameModeDistAmpConf:          return BTA_EthImgModeDistAmpConf;
    case BTA_FrameModeChannelSelection:     return BTA_EthImgModeChannelSelection;
    default:                                return BTA_EthImgModeNone;
    }
}




void BTA_CALLCONV BTAsleep(uint32_t milliseconds) {
    BTAmsleep(milliseconds);
}


void BTA_CALLCONV BTAfreeNetworkBroadcastAddrs(uint8_t ***localIpAddrs, uint8_t ***networkBroadcastAddrs, uint32_t networkBroadcastAddrsLen) {
    for (uint32_t i = 0; i < networkBroadcastAddrsLen; i++) {
        free((*localIpAddrs)[i]);
        (*localIpAddrs)[i] = 0;
        free((*networkBroadcastAddrs)[i]);
        (*networkBroadcastAddrs)[i] = 0;
    }
    free(*localIpAddrs);
    *localIpAddrs = 0;
    free(*networkBroadcastAddrs);
    *networkBroadcastAddrs = 0;
}


BTA_Status BTA_CALLCONV BTAgetNetworkBroadcastAddrs(uint8_t ***localIpAddrs, uint8_t ***networkBroadcastAddrs, uint32_t *networkBroadcastAddrsLen) {
    if (!localIpAddrs || !networkBroadcastAddrs || !networkBroadcastAddrsLen) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t *ipAddrs, *subnetMasks;
    BTA_Status status = BTAlistLocalIpAddrs(&ipAddrs, &subnetMasks, networkBroadcastAddrsLen);
    if (status != BTA_StatusOk) {
        return status;
    }
    uint32_t len = *networkBroadcastAddrsLen;
    if (len) {
        uint8_t **resultA1 = *localIpAddrs = (uint8_t **)calloc(len, sizeof(uint8_t *));
        uint8_t **resultB1 = *networkBroadcastAddrs = (uint8_t **)calloc(len, sizeof(uint8_t *));
        if (!resultA1 || !resultB1) {
            BTAfreeNetworkBroadcastAddrs(localIpAddrs, networkBroadcastAddrs, len);
            free(ipAddrs);
            free(subnetMasks);
            return BTA_StatusOutOfMemory;
        }
        for (uint32_t i = 0; i < len; i++) {
            uint8_t *resultA2 = resultA1[i] = (uint8_t *)malloc(4 * sizeof(uint8_t));
            uint8_t *resultB2 = resultB1[i] = (uint8_t *)malloc(4 * sizeof(uint8_t));
            if (!resultA2 || !resultB2) {
                BTAfreeNetworkBroadcastAddrs(localIpAddrs, networkBroadcastAddrs, len);
                free(ipAddrs);
                free(subnetMasks);
                return BTA_StatusOutOfMemory;
            }
            uint32_t ipAddr = ipAddrs[i];
            resultA2[0] = ipAddr & 0xff;
            resultA2[1] = (ipAddr >> 8) & 0xff;
            resultA2[2] = (ipAddr >> 16) & 0xff;
            resultA2[3] = (ipAddr >> 24) & 0xff;
            uint32_t broadcast = ipAddrs[i] | ~subnetMasks[i];
            resultB2[0] = broadcast & 0xff;
            resultB2[1] = (broadcast >> 8) & 0xff;
            resultB2[2] = (broadcast >> 16) & 0xff;
            resultB2[3] = (broadcast >> 24) & 0xff;
        }
        free(ipAddrs);
        free(subnetMasks);
    }
    return BTA_StatusOk;
}


void BTA_CALLCONV BTAgeneratePlanarView(int16_t *chX, int16_t *chY, int16_t *chZ, uint16_t *chAmp, int resX, int resY, int planarViewResX, int planarViewResY, float planarViewScale, int16_t *planarViewZ, uint16_t *planarViewAmp) {
    planarViewScale *= 10.0;
    int planarViewResXhalf = planarViewResX / 2;
    int planarViewResYhalf = planarViewResY / 2;
    int x, y, index;
    int count = resX * resY;
    for (int i = 0; i < count; i++) {
        x = (int)(chX[i] / planarViewScale + planarViewResXhalf);
        y = (int)(chY[i] / planarViewScale + planarViewResYhalf);
        index = x + y * planarViewResX;
        //#print('x: y: z:', x, y, frame.channels[z_ch_index].data[i])
        planarViewZ[index] = chZ[i];
        //# create top view amp image
        planarViewAmp[index] = chAmp[i];
    }
}
