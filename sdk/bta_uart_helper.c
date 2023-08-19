#ifndef BTA_WO_UART

#include "bta_uart_helper.h"



BTA_ChannelId BTAUARTgetChannelId(BTA_UartImgMode imgMode, uint8_t channelIndex) {
    switch (imgMode) {
    case BTA_UartImgModeDistAmp:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdDistance;
        case 1:
            return BTA_ChannelIdAmplitude;
        }
    case BTA_UartImgModeDist:
        switch (channelIndex) {
        case 0:
            return BTA_ChannelIdDistance;
        }
    default:
        return BTA_ChannelIdUnknown;
    }
}


BTA_DataFormat BTAUARTgetDataFormat(BTA_UartImgMode imgMode, uint8_t channelIndex) {
    switch (imgMode) {
    case BTA_UartImgModeDistAmp:
        switch (channelIndex) {
        case 0:
            return BTA_DataFormatUInt16;
        case 1:
            return BTA_DataFormatUInt16;
        }
    case BTA_UartImgModeDist:
        switch (channelIndex) {
        case 0:
            return BTA_DataFormatUInt16;
        }
    default:
        return BTA_DataFormatUnknown;
    }
}


BTA_Unit BTAUARTgetUnit(BTA_UartImgMode imgMode, uint8_t channelIndex) {
    switch (imgMode) {
    case BTA_UartImgModeDistAmp:
        switch (channelIndex) {
        case 0:
            return BTA_UnitMillimeter;
        case 1:
            return BTA_UnitUnitLess;
        }
    case BTA_UartImgModeDist:
        switch (channelIndex) {
        case 0:
            return BTA_UnitMillimeter;
        }
    default:
        return BTA_UnitUnitLess;
    }
}

#endif