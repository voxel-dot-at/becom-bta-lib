#pragma once

#include <stdint.h>
#include <bta_status.h>

#if !defined PLAT_WINDOWS && !defined PLAT_LINUX && !defined PLAT_APPLE
#   error "Please define PLAT_WINDOWS, PLAT_LINUX or PLAT_APPLE in your makefile/project"
#endif


BTA_Status UARTHLPserialOpenByNr(uint8_t portNr, uint32_t baudRate, uint8_t dataBits, uint8_t stopBits, uint8_t parity, void** handle/*, BTA_InfoEventInst *infoEventInst*/);
BTA_Status UARTHLPserialOpenByName(const char *portName, uint32_t baudRate, uint8_t dataBits, uint8_t stopBits, uint8_t parity, void **handle);
BTA_Status UARTHLPserialClose(void* handle);
BTA_Status UARTHLPserialRead(void* handle, void* buffer, uint32_t bytesToReadCount);
BTA_Status UARTHLPserialFlush(void *handle);
BTA_Status UARTHLPserialWrite(void* handle, void* buffer, uint32_t bytesToWriteCount);