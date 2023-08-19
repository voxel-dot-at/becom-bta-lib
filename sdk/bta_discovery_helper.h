#ifndef BTA_DISCOVERY_HELPER_H_INCLUDED
#define BTA_DISCOVERY_HELPER_H_INCLUDED

#include <bta.h>
#include <bta_helper.h>

typedef struct BTA_DiscoveryInst {
    BTA_DeviceType deviceType;

    uint8_t *broadcastIpAddr;
    uint8_t broadcastIpAddrLen;
    uint16_t broadcastPort;
    uint8_t *callbackIpAddr;
    uint8_t callbackIpAddrLen;
    uint16_t callbackPort;

    int32_t uartBaudRate;
    uint8_t uartDataBits;
    uint8_t uartStopBits;
    uint8_t uartParity;
    uint8_t uartTransmitterAddress;
    uint8_t uartReceiverAddressStart;
    uint8_t uartReceiverAddressEnd;

    FN_BTA_DeviceFound deviceFound;
    FN_BTA_DeviceFoundEx deviceFoundEx;
    void *userArg;

    void *deviceListMutex;
    BTA_DeviceInfo **deviceList;
    uint16_t deviceListCountMax;
    uint16_t deviceListCount;

    uint8_t abortDiscovery;
    void *discoveryThreadEth;
    void *discoveryThreadP100;
    void *discoveryThreadUsb;
    void *discoveryThreadStream;

    BTA_InfoEventInst *infoEventInst;

    uint32_t millisInterval;
    uint8_t periodicReports;
} BTA_DiscoveryInst;


BTA_DeviceInfo* BTAparseDiscoveryResponse(uint8_t* responsePayload, uint32_t responseLen, BTA_InfoEventInst* infoEventInst);
BTA_Status BTAfillPon(BTA_DeviceInfo* deviceInfo, BTA_InfoEventInst* infoEventInst);

uint8_t BTAisEqualDeviceSnr(BTA_DeviceInfo* deviceInfo1, BTA_DeviceInfo* deviceInfo2, uint8_t ignorePon, uint8_t ignoreIpAddr, uint8_t ignoreMacAddr);
BTA_DeviceInfo *BTAgetFromDiscoveredList(void* deviceListMutex, BTA_DeviceInfo** deviceList, uint16_t deviceListCount, BTA_DeviceInfo* deviceInfo, uint8_t ignorePon, uint8_t ignoreIpAddr, uint8_t ignoreMacAddr);
uint8_t BTAaddToDiscoveredList(void* deviceListMutex, BTA_DeviceInfo** deviceList, uint16_t* deviceListCount, uint16_t deviceListCountMax, BTA_DeviceInfo* deviceInfo);


#endif