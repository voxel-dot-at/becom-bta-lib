
#include "bta_discovery_helper.h"
#include <pthread_helper.h>
#include <string.h>


// this methon is obsolete once discovery supports pon
uint8_t BTAisInDiscoveredListIgnorePon(void* deviceListMutex, BTA_DeviceInfo **deviceList, uint16_t deviceListCount, BTA_DeviceInfo *deviceInfo) {
    if (!deviceList || !deviceListMutex || !deviceInfo) {
        return 0;
    }
    BTAlockMutex(deviceListMutex);
    for (int i = 0; i < deviceListCount; i++) {
        BTA_DeviceInfo *deviceInfoTemp = deviceList[i];
        int ipsEqual = deviceInfoTemp->deviceIpAddrLen == deviceInfo->deviceIpAddrLen && (!deviceInfo->deviceIpAddrLen || !memcmp(deviceInfoTemp->deviceIpAddr, deviceInfo->deviceIpAddr, deviceInfo->deviceIpAddrLen));
        int macsEqual = deviceInfoTemp->deviceMacAddrLen == deviceInfo->deviceMacAddrLen && (!deviceInfo->deviceMacAddrLen || !memcmp(deviceInfoTemp->deviceMacAddr, deviceInfo->deviceMacAddr, deviceInfo->deviceMacAddrLen));
        int deviceTypesEqual = deviceInfoTemp->deviceType == deviceInfo->deviceType;
        int serialNumbersEqual = deviceInfoTemp->serialNumber == deviceInfo->serialNumber;
        if (ipsEqual && macsEqual && deviceTypesEqual && serialNumbersEqual) {
            BTAunlockMutex(deviceListMutex);
            return 1;
        }
    }
    BTAunlockMutex(deviceListMutex);
    return 0;
}


uint8_t BTAaddToDiscoveredList(void* deviceListMutex, BTA_DeviceInfo **deviceList, uint16_t *deviceListCount, uint16_t deviceListCountMax, BTA_DeviceInfo *deviceInfo) {
    if (!deviceList || !deviceListMutex || !deviceInfo) {
        return 0;
    }
    BTAlockMutex(deviceListMutex);
    // make sure we even have space
    if (*deviceListCount >= deviceListCountMax) {
        BTAunlockMutex(deviceListMutex);
        return 0;
    }
    // make sure to not add a duplicate
    for (int i = 0; i < *deviceListCount; i++) {
        BTA_DeviceInfo *deviceInfoTemp = deviceList[i];
        int ipsEqual = deviceInfoTemp->deviceIpAddrLen == deviceInfo->deviceIpAddrLen && (!deviceInfo->deviceIpAddrLen || !memcmp(deviceInfoTemp->deviceIpAddr, deviceInfo->deviceIpAddr, deviceInfo->deviceIpAddrLen));
        int macsEqual = deviceInfoTemp->deviceMacAddrLen == deviceInfo->deviceMacAddrLen && (!deviceInfo->deviceMacAddrLen || !memcmp(deviceInfoTemp->deviceMacAddr, deviceInfo->deviceMacAddr, deviceInfo->deviceMacAddrLen));
        int deviceTypesEqual = deviceInfoTemp->deviceType == deviceInfo->deviceType;
        int productOrderNumbersEqual;
        if (!deviceInfoTemp->productOrderNumber && !deviceInfo->productOrderNumber) {
            productOrderNumbersEqual = 1;
        }
        else if (!deviceInfoTemp->productOrderNumber || !deviceInfo->productOrderNumber) {
            // PON is not reliable because it must be read seperately -> treat as equal if one is missing
            productOrderNumbersEqual = 1;
        }
        else {
            productOrderNumbersEqual = !strcmp((char *)deviceInfoTemp->productOrderNumber, (char *)deviceInfo->productOrderNumber);
        }
        int serialNumbersEqual = deviceInfoTemp->serialNumber == deviceInfo->serialNumber;
        if (ipsEqual && macsEqual && deviceTypesEqual && productOrderNumbersEqual && serialNumbersEqual) {
            BTAunlockMutex(deviceListMutex);
            return 0;
        }
    }
    // add and return 1
    deviceList[*deviceListCount] = deviceInfo;
    *deviceListCount = *deviceListCount + 1;
    BTAunlockMutex(deviceListMutex);
    return 1;
}