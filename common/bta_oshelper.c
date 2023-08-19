#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "bta_oshelper.h"
#include "timing_helper.h"

#include <stdio.h>
#ifdef PLAT_WINDOWS
#   include <Windows.h>
#   include <io.h>
#   include <fcntl.h>
#   include <share.h>
#   include <sys\stat.h>
#   include <direct.h>
#   define GetCurrentDir _getcwd
#   include <memory>
#   include <stdexcept>
#   include <iostream>
#else
#   include <sys/ioctl.h>
#   include <unistd.h>
#   include <time.h>
#   include <unistd.h>
#   define GetCurrentDir (void)!getcwd
#   include <stdio.h>
#   define _popen popen
#   define _pclose pclose
#endif


BTA_Status BTAfLargeOpen(const char* filename, const char *mode, void **file) {
    if (!filename || !mode || !file) {
        return BTA_StatusInvalidParameter;
    }
    #ifdef PLAT_WINDOWS
        int openFlag;
        if (!strcmp(mode, "r")) {
            openFlag = _O_RDONLY;
        }
        else if (!strcmp(mode, "w")) {
            openFlag = _O_WRONLY | _O_CREAT | _O_TRUNC;
        }
        else if (!strcmp(mode, "rb")) {
            openFlag = _O_RDONLY | _O_BINARY;
        }
        else if (!strcmp(mode, "r+")) {
            openFlag = _O_RDWR;
        }
        else if (!strcmp(mode, "ab")) {
            openFlag = _O_WRONLY | O_CREAT | _O_APPEND | _O_BINARY;
        }
        else {
            return BTA_StatusInvalidParameter;
        }

        errno_t err = _sopen_s((int *)file, filename, openFlag, _SH_DENYNO, _S_IREAD | _S_IWRITE);
        if (err) {
            return BTA_StatusRuntimeError;
        }
        return BTA_StatusOk;
    #else
        return BTAfopen(filename, mode, file);
    #endif
}


BTA_Status BTAfLargeClose(void *file) {
    if (!file) {
        return BTA_StatusInvalidParameter;
    }
    #ifdef PLAT_WINDOWS
        int result = _close((int)(intptr_t)file);
        if (result < 0) {
            return BTA_StatusRuntimeError;
        }
        return BTA_StatusOk;
    #else
        return BTAfclose(file);
    #endif
}


BTA_Status BTAfLargeTell(void *file, uint64_t *position) {
    if (!file || !position) {
        return BTA_StatusInvalidParameter;
    }
    #ifdef PLAT_WINDOWS
        int64_t pos = _telli64((int)(intptr_t)file);
        if (pos < 0) {
            *position = 0;
            return BTA_StatusRuntimeError;
        }
        *position = pos;
    #else
        *position = ftell((FILE *)file);
        if (*position < 0) {
            *position = 0;
            return BTA_StatusRuntimeError;
        }
    #endif
        return BTA_StatusOk;
}


BTA_Status BTAfLargeSeek(void *file, int64_t offset, BTA_SeekOrigin origin, uint64_t *position) {
    if (!file) {
        return BTA_StatusInvalidParameter;
    }
    #ifdef PLAT_WINDOWS
        if (offset == 0 && origin == BTA_SeekOriginCurrent) {
            return BTA_StatusOk;
        }
        int64_t pos = _lseeki64((int)(intptr_t)file, offset, origin);
        if (pos < 0) {
            return BTA_StatusRuntimeError;
        }
        if (position) {
            *position = pos;
        }
    #else
        int err = fseek((FILE *)file, offset, origin);
        if (err < 0) {
            return BTA_StatusRuntimeError;
        }
        if (position) {
            return BTAfLargeTell(file, position);
        }
    #endif
    return BTA_StatusOk;
}


BTA_Status BTAfLargeRead(void *file, void *buffer, uint32_t bytesToReadCount, uint32_t *bytesReadCount) {
    if (!file || !buffer) {
        return BTA_StatusInvalidParameter;
    }
    #ifdef PLAT_WINDOWS
        int32_t count = _read((int)(intptr_t)file, buffer, bytesToReadCount);
        if (count < 0) {
            if (bytesReadCount) {
                *bytesReadCount = 0;
            }
            //int err = GetLastError();
            return BTA_StatusRuntimeError;
        }
        if (bytesReadCount) {
            *bytesReadCount = count;
        }
        if ((uint32_t)count < bytesToReadCount) {
            return BTA_StatusOutOfMemory;
        }
        return BTA_StatusOk;
    #else
        return BTAfread(file, buffer, bytesToReadCount, bytesReadCount);
    #endif
}


BTA_Status BTAfLargeWrite(void *file, void *buffer, uint32_t bufferLen, uint32_t *bytesWrittenCount) {
    if (!file || !buffer || !bufferLen) {
        return BTA_StatusInvalidParameter;
    }
    #ifdef PLAT_WINDOWS
        int32_t count = _write((int)(intptr_t)file, buffer, bufferLen);
        if (count < 0) {
            if (bytesWrittenCount) {
                *bytesWrittenCount = 0;
            }
            return BTA_StatusRuntimeError;
        }
        if (bytesWrittenCount) {
            *bytesWrittenCount = count;
        }
        if (bufferLen != (uint32_t)count) {
            return BTA_StatusOutOfMemory;
        }
        return BTA_StatusOk;
    #else
        return BTAfwrite(file, buffer, bufferLen, bytesWrittenCount);
    #endif
}


BTA_Status BTAfLargeReadLine(void *file, char **line) {
    // TODO: This method is horribly slow. Read bigger chunks!
    if (!file || !line) {
        return BTA_StatusInvalidParameter;
    }
    int count = 0;
    char c;
    int lineBufferLen = 256;
    char *lineBuffer;
    BTA_Status status = BTAfLargeRead(file, &c, 1, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    lineBuffer = (char *)malloc(sizeof(char) * lineBufferLen);
    if (!lineBuffer) {
        return BTA_StatusOutOfMemory;
    }
    while ((c != '\n') && (status != BTA_StatusOutOfMemory)) {
        if (c != '\r') {
            lineBuffer[count++] = c;
        }
        status = BTAfLargeRead(file, &c, 1, 0);
        if (status != BTA_StatusOk && status != BTA_StatusOutOfMemory) {
            free(lineBuffer);
            return status;
        }
        if (count >= lineBufferLen) {
            lineBufferLen += 128;
            char *lineBufferNew = (char *)realloc(lineBuffer, lineBufferLen);
            if (!lineBufferNew) {
                free(lineBuffer);
                return BTA_StatusOutOfMemory;
            }
            lineBuffer = lineBufferNew;
        }
    }
    lineBuffer[count] = 0;
    *line = lineBuffer;
    return BTA_StatusOk;
}


BTA_Status BTAfopen(const char* filename, const char *mode, void **file) {
    if (!filename || !mode || !file) {
        return BTA_StatusInvalidParameter;
    }
    #ifdef PLAT_WINDOWS
        errno_t err = fopen_s((FILE **)file, filename, mode);
        if (err || !*file) {
            return BTA_StatusRuntimeError;
        }
        return BTA_StatusOk;
    #else
        *file = fopen(filename, mode);
        if (!*file) {
            return BTA_StatusUnknown;
        }
        return BTA_StatusOk;
    #endif
}


BTA_Status BTAfclose(void *file) {
    if (!file) {
        return BTA_StatusInvalidParameter;
    }
    int err = fclose((FILE *)file);
    if (err) {
        return BTA_StatusRuntimeError;
    }
    return BTA_StatusOk;
}


BTA_Status BTAfflush(void *file) {
    if (!file) {
        return BTA_StatusInvalidParameter;
    }
    int err = fflush((FILE *)file);
    if (err) {
        return BTA_StatusRuntimeError;
    }
    return BTA_StatusOk;
}


BTA_Status BTAftell(void *file, uint32_t *position) {
    if (!file || !position) {
        return BTA_StatusInvalidParameter;
    }
    *position = ftell((FILE *)file);
    if (*position < 0) {
        *position = 0;
        return BTA_StatusRuntimeError;
    }
    return BTA_StatusOk;
}


BTA_Status BTAfseek(void *file, int32_t offset, BTA_SeekOrigin origin, uint32_t *position) {
    if (!file) {
        return BTA_StatusInvalidParameter;
    }
    if (offset == 0 && origin == BTA_SeekOriginCurrent) {
        return BTA_StatusOk;
    }
    int err = fseek((FILE *)file, offset, origin);
    if (err < 0) {
        return BTA_StatusRuntimeError;
    }
    if (position) {
        return BTAftell(file, position);
    }
    return BTA_StatusOk;
}


BTA_Status BTAfread(void *file, void *buffer, uint32_t bytesToReadCount, uint32_t *bytesReadCount) {
    if (!file || !buffer) {
        return BTA_StatusInvalidParameter;
    }
    size_t count = fread(buffer, 1, bytesToReadCount, (FILE *)file);
    if (count < 0) {
        if (bytesReadCount) {
            *bytesReadCount = 0;
        }
        return BTA_StatusRuntimeError;
    }
    if (bytesReadCount) {
        *bytesReadCount = (uint32_t)count;
    }
    if ((uint32_t)count < bytesToReadCount) {
        return BTA_StatusOutOfMemory;
    }
    return BTA_StatusOk;
}


BTA_Status BTAfwrite(void *file, void *buffer, uint32_t bufferLen, uint32_t *bytesWrittenCount) {
    if (!file || !buffer || !bufferLen) {
        return BTA_StatusInvalidParameter;
    }
    size_t count = fwrite(buffer, 1, bufferLen, (FILE *)file);
    if (count < 0) {
        if (bytesWrittenCount) {
            *bytesWrittenCount = 0;
        }
        return BTA_StatusRuntimeError;
    }
    if (bytesWrittenCount) {
        *bytesWrittenCount = (uint32_t)count;
    }
    if (bufferLen != count) {
        return BTA_StatusOutOfMemory;
    }
    return BTA_StatusOk;
}


BTA_Status BTAfreadLine(void *file, char *line, uint32_t lineLen) {
    if (!file || !line) {
        return BTA_StatusInvalidParameter;
    }
    char *c = fgets(line, lineLen, (FILE *)file);
    if (!c) {
        return BTA_StatusRuntimeError;
    }
    while (strlen(line) > 0 && (line[strlen(line) - 1] == '\n' || line[strlen(line) - 1] == '\r')) {
        line[strlen(line) - 1] = 0;
    }
    return BTA_StatusOk;
}


BTA_Status BTAgetCwd(uint8_t *cwd, int cwdLen) {
    GetCurrentDir((char *)cwd, cwdLen);
    return BTA_StatusOk;
}


BTA_Status BTAfwriteCsv(const char* filename, const char **headersX, const char **headersY, int *data, int xRes, int yRes) {
    void *file;
    BTA_Status status = BTAfopen(filename, "w+", &file);
    if (status != BTA_StatusOk) {
        return status;
    }

    if (headersX) {
        // todo
    }
    for (int y = 0; y < yRes; y++) {
        if (headersY) {
            // todo
        }
        for (int x = 0; x < xRes; x++) {
            int err = fprintf((FILE *)file, "%d;", data[x + y * xRes]);
            if (err <= 0) {
                BTAfclose(file);
                return BTA_StatusRuntimeError;
            }
        }
        int err = fprintf((FILE *)file, "\n");
        if (err <= 0) {
            BTAfclose(file);
            return BTA_StatusRuntimeError;
        }
    }
    BTAfclose(file);
    return BTA_StatusOk;
}


void BTAexec(const char *cmd, char *result) {
    const int bufferSize = 128;
    char buffer[bufferSize];
    *result = 0;
    FILE *pipe = _popen(cmd, "r");
    if (!pipe) {
        return;
    }
    while (fgets(buffer, sizeof buffer, pipe)) {
        sprintf(result + strlen(result), "%s", buffer);
    }
    _pclose(pipe);
}


uint8_t BTAisUsbDevicePresent(uint16_t vendorId, uint16_t productId, int requiredSuccesses, int intervalMillis, int timeoutMillis) {
    // This does not update in real time. Not trustworthy!
    //libusb_context *context;
    //int err = libusb_init(&context);
    //if (err < 0) {
    //    //BTAinfoEventHelper(infoEventInst, VERBOSE_CRITICAL, BTA_StatusRuntimeError, "BTAopen USB: cannot init usb library (%s)", libusb_error_name(err));
    //    return 0;
    //}
    //libusb_device **devs;
    //ssize_t cnt = libusb_get_device_list(context, &devs);
    //if (cnt > 0) {
    //    libusb_device *dev;
    //    int i = 0;
    //    while ((dev = devs[i++])) {
    //        struct libusb_device_descriptor desc;
    //        err = libusb_get_device_descriptor(dev, &desc);
    //        if (err < 0 || desc.idVendor != vendorId || desc.idProduct != productId) {
    //            libusb_free_device_list(devs, 1);
    //            return 1;
    //        }
    //    }
    //}
    //else if (cnt >= 0) {
    //    libusb_free_device_list(devs, 1);
    //}
    //libusb_exit(context);
    //return 0;


    char cmdBuf[500];
    char resultBuf[500];
#   if defined PLAT_WINDOWS
    sprintf(cmdBuf, "pnputil /enum-devices /connected /class USB | findstr \"VID_%04x&PID_%04x\"", vendorId, productId);
    sprintf(resultBuf, "VID_%04x&PID_%04x", vendorId, productId);
#   else
    sprintf(cmdBuf, "lsusb | grep 'ID %04x:%04x'", vendorId, productId);
    sprintf(resultBuf, "ID %04x:%04x", vendorId, productId);
#   endif

    uint64_t timeEnd = BTAgetTickCount64() + timeoutMillis;
    uint64_t timeToProbe = BTAgetTickCount64();
    int successes = 0;
    char output[1024];
    while (1) {
        uint64_t ticks = BTAgetTickCount64();
        if (ticks < timeToProbe) {
            BTAmsleep((uint32_t)(timeToProbe - ticks));
        }
        timeToProbe += intervalMillis;
        BTAexec(cmdBuf, output);
        if (strlen(output) && strstr(output, resultBuf)) {
            successes++;
        }
        else {
            successes = 0;
        }
        if (successes == requiredSuccesses) {
            return 1;
        }
        if (BTAgetTickCount64() > timeEnd) {
            return 0;
        }
    }
    return 0;
}