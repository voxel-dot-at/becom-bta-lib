#ifndef BTA_WO_UART
#ifndef BTA_ETH_HELPER_H_INCLUDED
#define BTA_ETH_HELPER_H_INCLUDED

#include <stdint.h>
#include <bta_status.h>
#include <bta_frame.h>


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


BTA_ChannelId BTAUARTgetChannelId(BTA_UartImgMode imgMode, uint8_t channelIndex);
BTA_DataFormat BTAUARTgetDataFormat(BTA_UartImgMode imgMode, uint8_t channelIndex);
BTA_Unit BTAUARTgetUnit(BTA_UartImgMode imgMode, uint8_t channelIndex);

#endif

#endif