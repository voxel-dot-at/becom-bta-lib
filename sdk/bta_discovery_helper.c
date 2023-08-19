
#include "bta_discovery_helper.h"
#include <pthread_helper.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <crc16.h>
#include <crc32.h>




BTA_DeviceInfo *BTAparseDiscoveryResponse(uint8_t *responsePayload, uint32_t responseLen, BTA_InfoEventInst *infoEventInst) {
    if (responseLen < 48) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_INFO, BTA_StatusInvalidData, "BTAparseDiscoveryResponse: The discovery response is too short!");
        return 0;
    }
    BTA_DeviceInfo *deviceInfo = (BTA_DeviceInfo *)calloc(1, sizeof(BTA_DeviceInfo));
    if (!deviceInfo) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_INFO, BTA_StatusOutOfMemory, "BTAparseDiscoveryResponse: Cannot allocate device info!");
        return 0;
    }
    deviceInfo->deviceMacAddr = (uint8_t*)malloc(6);
    if (deviceInfo->deviceMacAddr) {
        memcpy(deviceInfo->deviceMacAddr, &responsePayload[0x40 - 64], 6);
        deviceInfo->deviceMacAddrLen = 6;
    }
    deviceInfo->deviceIpAddr = (uint8_t*)malloc(4);
    if (deviceInfo->deviceIpAddr) {
        memcpy(deviceInfo->deviceIpAddr, &responsePayload[0x47 - 64], 4);
        deviceInfo->deviceIpAddrLen = 4;
    }
    deviceInfo->subnetMask = (uint8_t*)malloc(4);
    if (deviceInfo->subnetMask) {
        memcpy(deviceInfo->subnetMask, &responsePayload[0x4b - 64], 4);
        deviceInfo->subnetMaskLen = 4;
    }
    deviceInfo->gatewayIpAddr = (uint8_t*)malloc(4);
    if (deviceInfo->gatewayIpAddr) {
        memcpy(deviceInfo->gatewayIpAddr, &responsePayload[0x4f - 64], 4);
        deviceInfo->gatewayIpAddrLen = 4;
    }
    deviceInfo->udpDataIpAddr = (uint8_t*)malloc(4);
    if (deviceInfo->udpDataIpAddr) {
        memcpy(deviceInfo->udpDataIpAddr, &responsePayload[0x54 - 64], 4);
        deviceInfo->udpDataIpAddrLen = 4;
    }
    deviceInfo->udpDataPort = (responsePayload[0x58 - 64] << 8) | responsePayload[0x59 - 64];
    deviceInfo->udpControlPort = (responsePayload[0x5a - 64] << 8) | responsePayload[0x5b - 64];
    deviceInfo->tcpDataPort = (responsePayload[0x5c - 64] << 8) | responsePayload[0x5d - 64];
    deviceInfo->tcpControlPort = (responsePayload[0x5e - 64] << 8) | responsePayload[0x5f - 64];
    deviceInfo->deviceType = (BTA_DeviceType)((responsePayload[0x60 - 64] << 8) | responsePayload[0x61 - 64]);
    deviceInfo->serialNumber = (responsePayload[0x62 - 64] << 24) | (responsePayload[0x63 - 64] << 16) | (responsePayload[0x64 - 64] << 8) | responsePayload[0x65 - 64];
    deviceInfo->uptime = (responsePayload[0x66 - 64] << 24) | (responsePayload[0x67 - 64] << 16) | (responsePayload[0x68 - 64] << 8) | responsePayload[0x69 - 64];
    deviceInfo->mode0 = (responsePayload[0x6a - 64] << 8) | responsePayload[0x6b - 64];
    deviceInfo->status = (responsePayload[0x6c - 64] << 8) | responsePayload[0x6d - 64];
    uint16_t fwv = (responsePayload[0x6e - 64] << 8) | responsePayload[0x6f - 64];
    deviceInfo->firmwareVersionMajor = (fwv & 0xf800) >> 11;
    deviceInfo->firmwareVersionMinor = (fwv & 0x07c0) >> 6;
    deviceInfo->firmwareVersionNonFunc = (fwv & 0x003f);
    if (responseLen >= 54) {
        uint16_t artNum1 = (responsePayload[0x70 - 64] << 8) | responsePayload[0x71 - 64];
        uint16_t artNum2 = (responsePayload[0x72 - 64] << 8) | responsePayload[0x73 - 64];
        uint16_t devRevMaj = (responsePayload[0x74 - 64] << 8) | responsePayload[0x75 - 64];
        deviceInfo->productOrderNumber = (uint8_t *)calloc(1, 20);
        if (deviceInfo->productOrderNumber) {
            sprintf((char*)deviceInfo->productOrderNumber, "%03d-%04d-%1d", artNum1, artNum2, devRevMaj);
        }
    }
    return deviceInfo;
}


 BTA_Status BTAfillPon(BTA_DeviceInfo* deviceInfo, BTA_InfoEventInst* infoEventInst) {
    // Establish a connection and read PON directly
    BTA_Config config;
    BTA_Status status = BTAinitConfig(&config);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, status, "Discovery: Read PON: Error in BTAinitConfig");
        return status;
    }

    config.deviceType = BTA_DeviceTypeEthernet;
    if (deviceInfo->tcpControlPort) {
        config.tcpDeviceIpAddr = deviceInfo->deviceIpAddr;
        config.tcpDeviceIpAddrLen = (uint8_t)deviceInfo->deviceIpAddrLen;
        config.tcpControlPort = deviceInfo->tcpControlPort;
    }
    if (deviceInfo->udpControlPort != 0) {
        config.udpControlOutIpAddr = deviceInfo->deviceIpAddr;
        config.udpControlOutIpAddrLen = (uint8_t)deviceInfo->deviceIpAddrLen;
        config.udpControlPort = deviceInfo->udpControlPort;
    }
    BTA_Handle btaHandle;
    status = BTAopen(&config, &btaHandle);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, status, "Discovery: Read PON: Error in BTAopen");
        return status;
    }

    //ArticleNrPart1, ArticleNrPart2, DeviceRevisionMajor (no multi read in regard of TimEth which does not support it)
    uint32_t ponPart1;
    uint32_t ponPart2;
    uint32_t deviceRevisionMajor;
    status = BTAreadRegister(btaHandle, 0x0570, &ponPart1, 0);
    if (status != BTA_StatusOk) {
        BTAclose(&btaHandle);
        BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, status, "Discovery: Read PON: Error in BTAreadRegister");
        return status;
    }
    status = BTAreadRegister(btaHandle, 0x0571, &ponPart2, 0);
    if (status != BTA_StatusOk) {
        BTAclose(&btaHandle);
        BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, status, "Discovery: Read PON: Error in BTAreadRegister");
        return status;
    }
    status = BTAreadRegister(btaHandle, 0x0572, &deviceRevisionMajor, 0);
    if (status != BTA_StatusOk) {
        BTAclose(&btaHandle);
        BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, status, "Discovery: Read PON: Error in BTAreadRegister");
        return status;
    }
    BTAclose(&btaHandle);
    if (!ponPart1 || !ponPart2 || !deviceRevisionMajor) {
        //BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, BTA_StatusRuntimeError, "Discovery: Read PON: PON related registers are empty!");
        return  BTA_StatusRuntimeError;
    }
    deviceInfo->productOrderNumber = (uint8_t *)calloc(1, 20);
    if (deviceInfo->productOrderNumber) {
        sprintf((char*)deviceInfo->productOrderNumber, "%03d-%04d-%d", ponPart1, ponPart2, deviceRevisionMajor);
    }
    return BTA_StatusOk;
}


uint8_t BTAisEqualDeviceSnr(BTA_DeviceInfo* deviceInfo1, BTA_DeviceInfo* deviceInfo2, uint8_t ignorePon, uint8_t ignoreIpAddr, uint8_t ignoreMacAddr) {
    int deviceTypesEqual = deviceInfo1->deviceType == deviceInfo2->deviceType;
    int ponsEqual = 1;
    if (!ignorePon) {
        if (!deviceInfo1->productOrderNumber && !deviceInfo2->productOrderNumber) {
            ponsEqual = 1;
        }
        else if (!deviceInfo1->productOrderNumber || !deviceInfo2->productOrderNumber) {
            // One PON is unknown -> treat as not equal
            ponsEqual = 0;
        }
        else {
            ponsEqual = strcmp((char*)deviceInfo1->productOrderNumber, (char*)deviceInfo2->productOrderNumber) == 0;
        }
    }
    int serialNumbersEqual = deviceInfo1->serialNumber == deviceInfo2->serialNumber;
    int ipsEqual = 1;
    if (!ignoreIpAddr) {
        ipsEqual = deviceInfo1->deviceIpAddrLen == deviceInfo2->deviceIpAddrLen && (!deviceInfo2->deviceIpAddrLen || !memcmp(deviceInfo1->deviceIpAddr, deviceInfo2->deviceIpAddr, deviceInfo2->deviceIpAddrLen));
    }
    int macsEqual = 1;
    if (!ignoreMacAddr) {
        macsEqual = deviceInfo1->deviceMacAddrLen == deviceInfo2->deviceMacAddrLen && (!deviceInfo2->deviceMacAddrLen || !memcmp(deviceInfo1->deviceMacAddr, deviceInfo2->deviceMacAddr, deviceInfo2->deviceMacAddrLen));
    }
    return (ipsEqual && macsEqual && deviceTypesEqual && ponsEqual && serialNumbersEqual);
}


BTA_DeviceInfo *BTAgetFromDiscoveredList(void* deviceListMutex, BTA_DeviceInfo** deviceList, uint16_t deviceListCount, BTA_DeviceInfo* deviceInfo, uint8_t ignorePon, uint8_t ignoreIpAddr, uint8_t ignoreMacAddr) {
    if (!deviceList || !deviceListMutex || !deviceInfo) {
        return 0;
    }
    BTAlockMutex(deviceListMutex);
    for (int i = 0; i < deviceListCount; i++) {
        uint8_t present = BTAisEqualDeviceSnr(deviceInfo, deviceList[i], ignorePon, ignoreIpAddr, ignoreMacAddr);
        if (present) {
            BTA_DeviceInfo *result = deviceList[i];
            BTAunlockMutex(deviceListMutex);
            return result;
        }
    }
    // Device is not in list
    BTAunlockMutex(deviceListMutex);
    return 0;
}


uint8_t BTAaddToDiscoveredList(void* deviceListMutex, BTA_DeviceInfo** deviceList, uint16_t* deviceListCount, uint16_t deviceListCountMax, BTA_DeviceInfo* deviceInfo) {
    if (!deviceList || !deviceListMutex || !deviceInfo) {
        return 0;
    }
    BTAlockMutex(deviceListMutex);
    if (*deviceListCount < deviceListCountMax) {
        deviceList[*deviceListCount] = deviceInfo;
        *deviceListCount = *deviceListCount + 1;
        BTAunlockMutex(deviceListMutex);
        return 1;
    }
    BTAunlockMutex(deviceListMutex);
    return 0;
}
