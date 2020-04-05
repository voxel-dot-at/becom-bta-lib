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
#include <bta_frame_queueing.h>
#include <bta_oshelper.h>
#include "bta_grabbing.h"

#define BTA_UART_PREAMBLE_0                 0xef
#define BTA_UART_PREAMBLE_1                 0xa1
#define BTA_UART_PROTOCOL_VERSION           1


#ifdef PLAT_LINUX
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

    BTA_GrabInst *grabInst;
} BTA_UartLibInst;


typedef enum BTA_UartCommand {
    BTA_UartCommandRead = 1,
    BTA_UartCommandWrite = 2,
    BTA_UartCommandStream = 3,
    BTA_UartCommandResponse = 4,
    BTA_UartCommandFlashUpdate = 5,
} BTA_UartCommand;

typedef enum BTA_UartFlashCommand {
    BTA_UartFlashCommandInit = 0,
    BTA_UartFlashCommandSetCrc = 1,
    BTA_UartFlashCommandSetPacket = 2,
    BTA_UartFlashCommandGetMaxPacketSize = 3,
    BTA_UartFlashCommandFinalize = 4,
} BTA_UartFlashCommand;


typedef enum BTA_UartImgMode {
    BTA_UartImgModeDistAmp = 0,
    BTA_UartImgModeDist = 12,
} BTA_UartImgMode;


typedef enum BTA_UartRegAddr {
    BTA_UartRegAddrIntegrationTime = 0x0005,
    BTA_UartRegAddrDeviceType = 0x0006,
    BTA_UartRegAddrFirmwareInfo = 0x0008,
    BTA_UartRegAddrModulationFrequency = 0x0009,
    BTA_UartRegAddrFramerate = 0x000A,
    BTA_UartRegAddrSerialNrLowWord = 0x000C,
    BTA_UartRegAddrSerialNrHighWord = 0x000D,
    BTA_UartRegAddrCmdEnablePasswd = 0x0022,
    BTA_UartRegAddrCmdExec = 0x0033,
    BTA_UartRegAddrCmdExecResult = 0x0034
} BTA_UartRegAddr;


BTA_Status BTAUARTstartDiscovery(BTA_DiscoveryConfig *discoveryConfig, FN_BTA_DeviceFound deviceFound, FN_BTA_InfoEvent infoEvent, BTA_Handle *handle);
BTA_Status BTAUARTstopDiscovery(BTA_Handle *handle);
BTA_Status BTAUARTopen(BTA_Config *config, BTA_WrapperInst *winst);
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

#endif