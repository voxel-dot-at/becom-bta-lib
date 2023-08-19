#ifndef BTA_WO_UART

#ifndef BTA_UART_H_INCLUDED
#define BTA_UART_H_INCLUDED

#ifdef PLAT_WINDOWS
    #include <Windows.h>
#else
    #include <time.h>
#endif


#include <bta.h>
#include <bta_helper.h>
#include <bta_oshelper.h>
#include "bta_grabbing.h"

#if !defined PLAT_WINDOWS && !defined PLAT_LINUX && !defined PLAT_APPLE
#   error "Please define PLAT_WINDOWS, PLAT_LINUX or PLAT_APPLE in your makefile/project"
#endif

#define BTA_UART_PREAMBLE_0                 0xef
#define BTA_UART_PREAMBLE_1                 0xa1
#define BTA_UART_PROTOCOL_VERSION           1


#if defined PLAT_LINUX || defined PLAT_APPLE
    #define DWORD               unsigned long
    #define byte                unsigned char

    #define NO_ERROR            0
#endif


typedef struct BTA_UartLibInst  {
    void *handleMutex;
    uint8_t closing;

    void *readFramesThread;

    void *serialHandle;

    uint8_t uartTransmitterAddress;
    uint8_t uartReceiverAddress;
} BTA_UartLibInst;


//void *BTAUARTdiscoveryRunFunction(BTA_DiscoveryInst *inst);
BTA_Status BTAUARTopen(BTA_Config *config, BTA_WrapperInst *winst);
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

#endif