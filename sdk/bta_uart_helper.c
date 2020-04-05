#ifndef BTA_WO_UART

#include "bta_uart_helper.h"



BTA_Status BTAserialOpen(uint8_t *portName, uint32_t baudRate, uint8_t dataBits, uint8_t stopBits, uint8_t parity, void **handle/*, BTA_InfoEventInst *infoEventInst*/) {
    if (!handle || !portName) {
        return BTA_StatusInvalidParameter;
    }
#ifdef PLAT_WINDOWS
    if (!baudRate) {
        baudRate = CBR_115200;
    }
    if (baudRate != CBR_110 && baudRate != CBR_300 && baudRate != CBR_600 && baudRate != CBR_1200 &&
        baudRate != CBR_2400 && baudRate != CBR_4800 && baudRate != CBR_9600 && baudRate != CBR_14400 &&
        baudRate != CBR_19200 && baudRate != CBR_38400 && baudRate != CBR_56000 && baudRate != CBR_57600 &&
        baudRate != CBR_115200 && baudRate != CBR_128000 && baudRate != CBR_256000) {
        //BTAinfoEventHelper1(infoEventInst, IMPORTANCE_ERROR, intInvalidParameter, "BTAserialOpen: invalid baud rate %d", baudRate);
        return BTA_StatusInvalidParameter;
    }
    if (!dataBits) {
        dataBits = 8;
    }
    if (stopBits) {
        switch (stopBits) {
        case 0:
        case 1:
            stopBits = ONESTOPBIT;
            break;
        case 3:
            stopBits = ONE5STOPBITS;
            break;
        case 2:
            stopBits = TWOSTOPBITS;
            break;
        default:
            //BTAinfoEventHelper1(infoEventInst, IMPORTANCE_ERROR, intInvalidParameter, "BTAserialOpen: invalid stop bits %d", stopBits);
            return BTA_StatusInvalidParameter;
        }
    }
    if (parity) {
        switch (parity) {
        case 0:
            parity = NOPARITY;
            break;
        case 1:
            parity = ODDPARITY;
            break;
        case 2:
            parity = EVENPARITY;
            break;
        case 3:
            parity = MARKPARITY;
            break;
        case 4:
            parity = SPACEPARITY;
            break;
        default:
            //BTAinfoEventHelper1(infoEventInst, IMPORTANCE_ERROR, intInvalidParameter, "BTAserialOpen: invalid parity %d", parity);
            return BTA_StatusInvalidParameter;
        }
    }

    char temp[50];
    sprintf_s(temp, 50, "\\\\.\\%s", portName);
    wchar_t wUartPortName[50];
    mbstowcs(wUartPortName, (const char *)temp, strlen((const char *)temp) + 1);
    *handle = CreateFile(wUartPortName, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if (*handle == INVALID_HANDLE_VALUE) {
        *handle = (void *)-1;
        //BTAinfoEventHelper1(infoEventInst, IMPORTANCE_ERROR, intInvalidParameter, "BTAserialOpen: error in CreateFile %d", GetLastError());
        return BTA_StatusRuntimeError;
    }

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(*handle, &dcbSerialParams)) {
        //BTAinfoEventHelper1(infoEventInst, IMPORTANCE_ERROR, intInvalidParameter, "BTAserialOpen: error in GetCommState %d", GetLastError());
        return BTA_StatusRuntimeError;
    }
    dcbSerialParams.BaudRate = baudRate;
    dcbSerialParams.ByteSize = dataBits;
    dcbSerialParams.StopBits = stopBits;
    dcbSerialParams.Parity = parity;
    if (!SetCommState(*handle, &dcbSerialParams)) {
        //BTAinfoEventHelper1(infoEventInst, IMPORTANCE_ERROR, intInvalidParameter, "BTAserialOpen: error in SetCommState %d", GetLastError());
        return BTA_StatusRuntimeError;
    }
    COMMTIMEOUTS timeouts;
    timeouts.ReadIntervalTimeout = 250;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.ReadTotalTimeoutConstant = 250;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 250;
    if (!SetCommTimeouts(*handle, &timeouts)) {
        //BTAinfoEventHelper1(infoEventInst, IMPORTANCE_ERROR, intInvalidParameter, "BTAserialOpen: error in SetCommTimeouts %d", GetLastError());
        return BTA_StatusRuntimeError;
    }
    return BTA_StatusOk;
#elif defined PLAT_LINUX
    return BTA_StatusNotSupported;
#endif
}


BTA_Status BTAserialClose(void *handle) {
#ifdef PLAT_WINDOWS
    if ((int64_t)handle >= 0) {
        if (!CloseHandle(handle)) {
            return BTA_StatusRuntimeError;
        }
        handle = (void *)-1;
    }
    return BTA_StatusOk;
#elif defined PLAT_LINUX
    return BTA_StatusNotSupported;
#endif
}


BTA_Status BTAserialRead(void *handle, void *buffer, uint32_t bytesToReadCount, uint32_t *bytesReadCount) {
    if ((int64_t)handle < 0 || !buffer) {
        return BTA_StatusInvalidParameter;
    }
#ifdef PLAT_WINDOWS
    uint8_t result;
    uint32_t temp;
    if (!bytesReadCount) {
        bytesReadCount = &temp;
    }
    result = ReadFile(handle, buffer, bytesToReadCount, (LPDWORD)bytesReadCount, 0);
    if (result && bytesToReadCount == *bytesReadCount) {
        return BTA_StatusOk;
    }
    memset(buffer, 0, bytesToReadCount);
    return BTA_StatusRuntimeError;
#elif defined PLAT_LINUX
    return BTA_StatusNotSupported;
#endif
}


BTA_Status BTAserialWrite(void *handle, void *buffer, uint32_t bufferLen, uint32_t *bytesWrittenCount) {
    if ((int64_t)handle < 0 || !buffer) {
        return BTA_StatusInvalidParameter;
    }
#ifdef PLAT_WINDOWS
    uint8_t result;
    uint32_t temp;
    if (!bytesWrittenCount) {
        bytesWrittenCount = &temp;
    }
    result = WriteFile(handle, buffer, bufferLen, (LPDWORD)bytesWrittenCount, 0);
    if (result && bufferLen == *bytesWrittenCount) {
        return BTA_StatusOk;
    }
    return BTA_StatusRuntimeError;
#elif defined PLAT_LINUX
    return BTA_StatusNotSupported;
#endif
}


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