#ifndef BTA_WO_UART
#ifndef BTA_ETH_HELPER_H_INCLUDED
#define BTA_ETH_HELPER_H_INCLUDED

#include "bta.h"
#include "bta_uart.h"

BTA_Status BTAserialOpen(uint8_t *portName, uint32_t baudRate, uint8_t dataBits, uint8_t stopBits, uint8_t parity, void **handle/*, BTA_InfoEventInst *infoEventInst*/);
BTA_Status BTAserialClose(void *handle);
BTA_Status BTAserialRead(void *handle, void *buffer, uint32_t bytesToReadCount, uint32_t *bytesReadCount);
BTA_Status BTAserialWrite(void *handle, void *buffer, uint32_t bufferLen, uint32_t *bytesWrittenCount);

BTA_ChannelId BTAUARTgetChannelId(BTA_UartImgMode imgMode, uint8_t channelIndex);
BTA_DataFormat BTAUARTgetDataFormat(BTA_UartImgMode imgMode, uint8_t channelIndex);
BTA_Unit BTAUARTgetUnit(BTA_UartImgMode imgMode, uint8_t channelIndex);

#endif

#endif