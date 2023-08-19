#include "uart_helper.h"
#include <timing_helper.h>
#include <stdio.h>
#include <string.h>
#include <bta.h>

#ifdef PLAT_WINDOWS
#   include <Windows.h>
#else
#   include <errno.h>
#   include <fcntl.h>
#   include <sys/ioctl.h>
#   include <termios.h>
#   include <unistd.h>
#endif



BTA_Status UARTHLPserialOpenByNr(uint8_t portNr, uint32_t baudRate, uint8_t dataBits, uint8_t stopBits, uint8_t parity, void **handle) {
    char portName[120];
#   ifdef PLAT_WINDOWS
    sprintf_s(portName, 120, "\\\\.\\COM%d", portNr);
#   else
    snprintf(portName, 120, "/dev/ttyUSB%d", portNr);
#   endif
    return UARTHLPserialOpenByName(portName, baudRate, dataBits, stopBits, parity, handle);
}


BTA_Status UARTHLPserialOpenByName(const char *portName, uint32_t baudRate, uint8_t dataBits, uint8_t stopBits, uint8_t parity, void **handle) {
    if (!handle /* || !portNr*/) {
        return BTA_StatusInvalidParameter;
    }
#   ifdef PLAT_WINDOWS
    if (!baudRate) {
        baudRate = CBR_115200;
    }
    if (baudRate != CBR_110 && baudRate != CBR_300 && baudRate != CBR_600 && baudRate != CBR_1200 &&
        baudRate != CBR_2400 && baudRate != CBR_4800 && baudRate != CBR_9600 && baudRate != CBR_14400 &&
        baudRate != CBR_19200 && baudRate != CBR_38400 && baudRate != CBR_56000 && baudRate != CBR_57600 &&
        baudRate != CBR_115200 && baudRate != CBR_128000 && baudRate != CBR_256000) {
        //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, intInvalidParameter, "BTAserialOpen: invalid baud rate %d", baudRate);
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
            //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, intInvalidParameter, "BTAserialOpen: invalid stop bits %d", stopBits);
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
            //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, intInvalidParameter, "BTAserialOpen: invalid parity %d", parity);
            return BTA_StatusInvalidParameter;
        }
    }

    wchar_t wUartPortName[120];
    mbstowcs(wUartPortName, portName, 120);
    *handle = CreateFile(wUartPortName, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if (*handle == INVALID_HANDLE_VALUE) {
        *handle = (void*)-1;
        printf("UARTHLPserialOpen: CreateFile %s, error: %d", portName, GetLastError());
        //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, intInvalidParameter, "BTAserialOpen: error in CreateFile %d", GetLastError());
        return BTA_StatusRuntimeError;
    }

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(*handle, &dcbSerialParams)) {
        //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, intInvalidParameter, "BTAserialOpen: error in GetCommState %d", GetLastError());
        return BTA_StatusRuntimeError;
    }
    dcbSerialParams.BaudRate = baudRate;
    dcbSerialParams.ByteSize = dataBits;
    dcbSerialParams.StopBits = stopBits;
    dcbSerialParams.Parity = parity;
    if (!SetCommState(*handle, &dcbSerialParams)) {
        //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, intInvalidParameter, "BTAserialOpen: error in SetCommState %d", GetLastError());
        return BTA_StatusRuntimeError;
    }
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = 1000;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 1000;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 1000;
    if (!SetCommTimeouts(*handle, &timeouts)) {
        //BTAinfoEventHelper(infoEventInst, IMPORTANCE_ERROR, intInvalidParameter, "BTAserialOpen: error in SetCommTimeouts %d", GetLastError());
        return BTA_StatusRuntimeError;
    }
    return BTA_StatusOk;
#   else
    int32_t handleTemp = open(portName, O_RDWR | O_NOCTTY | O_SYNC);
    if (handleTemp < 0) {
        printf("Could not open %s (%s)\n", portName, strerror(errno));
        return BTA_StatusDeviceUnreachable;
    }

    // Configure uart
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(handleTemp, &tty)) {
        printf("Could not get attrs on %s (%s)\n", portName, strerror(errno));
        close(handleTemp);
        return BTA_StatusRuntimeError;
    }

    if (parity) {
        tty.c_cflag |= PARENB;
    }
    else {
        tty.c_cflag &= ~(PARENB | PARODD);
    }
    if (stopBits == 2) {
        tty.c_cflag |= CSTOPB;
    }
    else if (stopBits == 1) {
        tty.c_cflag &= ~CSTOPB;
    }
    else {
        printf("uart_helper: Misconfiguration stopBits %d\n", stopBits);
        close(handleTemp);
        return BTA_StatusNotSupported;
    }
    tty.c_cflag &= ~CSIZE;
    if (dataBits == 8) {
        tty.c_cflag |= CS8;
    }
    else {
        printf("uart_helper: Misconfiguration dataBits %d\n", dataBits);
        close(handleTemp);
        return BTA_StatusNotSupported;
    }
    //tty.c_cflag &= ~CRTSCTS;        // Disable RTS/CTS hardware flow control (most common)
    //tty.c_cflag |= CREAD | CLOCAL;  // Turn on READ & ignore ctrl lines (CLOCAL = 1)
    tty.c_cflag = 0x8be; // extracted after having a working cfg via a gui tool
    tty.c_lflag = 0;
    //tty.c_lflag &= ~ICANON;         // In canonical mode, input is processed when a new line character is received
    //tty.c_lflag &= ~(ECHO | ECHOE | ECHONL | ISIG);  // Disable echo, erasure, new-line echo, interpretation of INTR, QUIT and SUSP
    tty.c_iflag = IGNPAR; // extracted after having a working cfg via a gui tool
    //tty.c_iflag &= ~IGNBRK;         // disable break processing
    //tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    //tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); // Disable any special handling of received bytes
    tty.c_oflag = 0;                // no remapping, no delays
    //tty.c_oflag &= ~OPOST;          // Prevent special interpretation of output bytes (e.g. newline chars)
    //tty.c_oflag &= ~ONLCR;          // Prevent conversion of newline to carriage return/line feed
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;           // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
    switch(baudRate) {
        case B9600:
            cfsetspeed(&tty, B9600);
            break;
        case B19200:
            cfsetspeed(&tty, B19200);
            break;
        case B38400:
            cfsetspeed(&tty, B38400);
            break;
        case B57600:
            cfsetspeed(&tty, B57600);
            break;
        case B115200:
            cfsetspeed(&tty, B115200);
            break;
    }
    if (tcsetattr(handleTemp, TCSANOW, &tty)) {
        printf("Could not set attrs on %s (%s)\n", portName, strerror(errno));
        close(handleTemp);
        return BTA_StatusRuntimeError;
    }
    *handle = (void *)(intptr_t)handleTemp;
    return BTA_StatusOk;
#   endif
}


BTA_Status UARTHLPserialClose(void *handle) {
#   ifdef PLAT_WINDOWS
    if ((int64_t)handle >= 0) {
        if (!CloseHandle(handle)) {
            return BTA_StatusRuntimeError;
        }
    }
#   else
    int fd = (int)(intptr_t)handle;
    int32_t result = close(fd);
    if (result < 0) {
        printf("Could not close UART (%s)\n", strerror(errno));
        return BTA_StatusRuntimeError;
    }
#   endif
    handle = 0;
    return BTA_StatusOk;
}


BTA_Status UARTHLPserialRead(void *handle, void *buffer, uint32_t bytesToReadCount) {
    if (!(size_t)handle || !buffer) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t bytesReadCount = 0;
#   ifdef PLAT_WINDOWS
    BOOL result = ReadFile(handle, buffer, bytesToReadCount, (LPDWORD)&bytesReadCount, 0);
    if (result && bytesReadCount != bytesToReadCount) {
        printf("uart_helper: read %d bytes, but %d bytes expected!\n", bytesReadCount, bytesToReadCount);
        return BTA_StatusRuntimeError;
    }
#   else
    int fd = (int)(intptr_t)handle;
    while (bytesReadCount < bytesToReadCount) {
        int byteCount = read(fd, &(((uint8_t *)buffer)[bytesReadCount]), bytesToReadCount - bytesReadCount);
        if (byteCount <= 0) {
            printf("uart_helper: Error reading (%s). Read %d bytes, but %d bytes expected!\n", strerror(errno), bytesReadCount, bytesToReadCount);
            return BTA_StatusRuntimeError;
        }
        bytesReadCount += byteCount;
    }
#   endif
    return BTA_StatusOk;
}


BTA_Status UARTHLPserialFlush(void *handle) {
#   ifdef PLAT_WINDOWS
    PurgeComm(handle, PURGE_RXCLEAR);
#   else
    int fd = (int)(intptr_t)handle;
    tcflush(fd, TCIOFLUSH);
#   endif
    return BTA_StatusOk;
}


BTA_Status UARTHLPserialWrite(void *handle, void *buffer, uint32_t bytesToWriteCount) {
    if (!(size_t)handle || !buffer) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t bytesWrittenCount = 0;
#   ifdef PLAT_WINDOWS
    BOOL result = WriteFile(handle, buffer, bytesToWriteCount, (LPDWORD)&bytesWrittenCount, 0);
    if (result && bytesWrittenCount != bytesToWriteCount) {
        printf("uart_helper: wrote %d bytes, but %d bytes needed!\n", bytesWrittenCount, bytesToWriteCount);
        return BTA_StatusRuntimeError;
    }
#   else
    int fd = (int)(intptr_t)handle;
    while (bytesWrittenCount < bytesToWriteCount) {
        int byteCount = write(fd, &(((uint8_t *)buffer)[bytesWrittenCount]), bytesToWriteCount - bytesWrittenCount);
        if (byteCount <= 0) {
            printf("uart_helper: Error writing (%s). Wrote %d bytes, but %d bytes needed!\n", strerror(errno), bytesWrittenCount, bytesToWriteCount);
            return BTA_StatusRuntimeError;
        }
        bytesWrittenCount += byteCount;
    }
#   endif
    return BTA_StatusOk;
}
