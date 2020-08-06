#ifndef BTA_WO_USB

#include <bta.h>
#include <bta_helper.h>
#include <bta_flash_update.h>
#include <bta_frame.h>
#include <bta_status.h>

#include "bta_usb.h"
#include <bta_oshelper.h>
#include <timing_helper.h>
#include <pthread_helper.h>
#include "configuration.h"
#include <utils.h>


#ifdef PLAT_WINDOWS
#elif defined PLAT_LINUX || defined PLAT_APPLE
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <crc16.h>
#include <crc32.h>


//////////////////////////////////////////////////////////////////////////////////
// Local prototypes
static void *discoveryRunFunction(BTA_DiscoveryInst *inst);

static BTA_Status openUsbDevice(libusb_device **devs, BTA_UsbLibInst *inst, BTA_InfoEventInst *infoEventInst);

static void *readFramesRunFunction(void *handle);
//static void *connectionMonitorRunFunction(void *handle);

static BTA_Status BTAUSBgetDeviceType(BTA_WrapperInst *winst, BTA_DeviceType *deviceType);

static BTA_Status readRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount, uint32_t timeout);

static BTA_Status receiveControlResponse(BTA_WrapperInst *inst, uint8_t *request, uint8_t **data, uint32_t *dataLen, uint32_t timeout, FN_BTA_ProgressReport progressReport, BTA_InfoEventInst *infoEventInst);

static BTA_Status transmit(BTA_WrapperInst *inst, uint8_t *data, uint32_t length, uint32_t timeout);

static BTA_Status receive(uint8_t *data, uint32_t *length, struct libusb_device_handle *usbHandle, struct libusb_device *usbDevice, uint32_t timeout, BTA_InfoEventInst *infoEventInst);




static const uint32_t timeoutDefault = 4000;
static const uint32_t timeoutHuge = 120000;
static const uint32_t timeoutBigger = 30000;
static const uint32_t timeoutBig = 15000;
static const uint32_t timeoutSmall = 1000;
static const uint32_t timeoutTiny = 50;


#ifdef PLAT_WINDOWS
#elif defined PLAT_LINUX
#endif



BTA_Status BTAUSBopen(BTA_Config *config, BTA_WrapperInst *winst) {
    if (!config || !winst) {
        return BTA_StatusInvalidParameter;
    }

    winst->inst = calloc(1, sizeof(BTA_UsbLibInst));
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusOutOfMemory;
    }

    if (!config->frameArrived && !config->frameArrivedEx && !config->frameArrivedEx2 && config->frameQueueMode == BTA_QueueModeDoNotQueue) {
        // No way to get frames without queueing or callback
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInvalidParameter, "BTAopen usb: A data interface connection is given, but queueing and frameArrived callback are disabled");
        BTAUSBclose(winst);
        return BTA_StatusInvalidParameter;
    }

    BTA_Status status;

    status = BTAinitMutex(&inst->controlMutex);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen USB: cannot init controlMutex");
        BTAUSBclose(winst);
        return status;
    }

    int err = libusb_init(NULL);
    if (err < 0) {
        BTAinfoEventHelperS(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusRuntimeError, "BTAopen USB: cannot init usb library (%s)", (uint8_t *)libusb_error_name(err));
        BTAUSBclose(winst);
        return BTA_StatusRuntimeError;
    }

    libusb_device **devs;
    ssize_t cnt = libusb_get_device_list(NULL, &devs);
    if (cnt < 0) {
        BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusRuntimeError, "BTAopen USB: cannot get usb devices (%d)", (int)cnt);
        BTAUSBclose(winst);
        return BTA_StatusRuntimeError;
    }

    while (1) {
        status = openUsbDevice(devs, inst, winst->infoEventInst);
        if (status != BTA_StatusOk) {
            BTAUSBclose(winst);
            return status;
        }
        // TODO: check serial number
        break;
    }


#ifndef PLAT_WINDOWS
    //### DOES NOT WORK ON WINDOWS!!! ###
    err = libusb_reset_device(inst->usbHandle);
    if (err < 0) {
        BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusRuntimeError, "BTAopen usb: libusb_reset failed %d", err);
        BTAUSBclose(winst);
        return BTA_StatusRuntimeError;
    }
#endif



    // for future jpg support....
    //status = BTAjpgInit(winst);
    //if (status != BTA_StatusOk) {
    //	BTAinfoEventHelper(winst->infoEventInst, IMPORTANCE_MOST, status, "BTAopen USB: Error initializing jpg");
    //	BTAUSBclose(winst);
    //	return status;
    //}

    // TODO: handle connection events
    //status = BTAcreateThread(&(inst->connectionMonitorThread), &connectionMonitorRunFunction, (void *)winst, 0);
    //if (status != BTA_StatusOk) {
    //    BTAinfoEventHelper(winst->infoEventInst, IMPORTANCE_MOST, status, "BTAopen usb: Could not start connectionMonitorThread");
    //    BTAUSBclose(winst);
    //    return status;
    //}

    status = BTAcreateThread(&(inst->readFramesThread), &readFramesRunFunction, (void *)winst, 0);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen usb: Could not start readFramesThread");
        BTAUSBclose(winst);
        return status;
    }

    return BTA_StatusOk;
}


static BTA_Status openUsbDevice(libusb_device **devs, BTA_UsbLibInst *inst, BTA_InfoEventInst *infoEventInst) {
    int i = 0, err;
    libusb_device *dev;
    while ((dev = devs[i++]) != NULL) {
        struct libusb_device_descriptor desc;
        err = libusb_get_device_descriptor(dev, &desc);
        if (err < 0) {
            //BTAinfoEventHelper1(winst->infoEventInst, IMPORTANCE_ERROR, BTA_StatusRuntimeError, "BTAopen USB: cannot get device descriptor", err);
            continue;
        }
        if (desc.idVendor != BTA_USB_VID || desc.idProduct != BTA_USB_PID) {
            continue;
        }
        BTAinfoEventHelperI(infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAopen USB: found a device (%d)", desc.iSerialNumber);
        // TODO: filter on serial number
        err = libusb_open(dev, &inst->usbHandle);
        if (err < 0) {
            BTAinfoEventHelperS(infoEventInst, VERBOSE_WARNING, BTA_StatusRuntimeError, "BTAopen USB: cannot open usb device, error: (%s)", (uint8_t *)libusb_error_name(err));
            return BTA_StatusRuntimeError;
        }
        if (desc.bNumConfigurations == 0) {
            BTAinfoEventHelper(infoEventInst, VERBOSE_CRITICAL, BTA_StatusRuntimeError, "BTAopen USB: no configurations for this device");
            return BTA_StatusRuntimeError;
        }

        struct libusb_config_descriptor *conf_desc;
        err = libusb_get_config_descriptor(dev, 0, &conf_desc);
        if (err < 0) {
            BTAinfoEventHelperS(infoEventInst, VERBOSE_CRITICAL, BTA_StatusRuntimeError, "BTAopen USB: cannot get config descriptor, error: %s", (uint8_t *)libusb_error_name(err));
            return BTA_StatusRuntimeError;
        }

        // this acts like a light-weight reset
        err = libusb_set_configuration(inst->usbHandle, conf_desc->bConfigurationValue);
        if (err < 0) {
            BTAinfoEventHelperS(infoEventInst, VERBOSE_CRITICAL, BTA_StatusRuntimeError, "BTAopen USB: cannot set configuration, error: %s", (uint8_t *)libusb_error_name(err));
            return BTA_StatusRuntimeError;
        }
        inst->interfaceNumber = conf_desc->interface->altsetting[0].bInterfaceNumber;
        err = libusb_claim_interface(inst->usbHandle, inst->interfaceNumber);
        if (err < 0) {
            BTAinfoEventHelperS(infoEventInst, VERBOSE_CRITICAL, BTA_StatusRuntimeError, "BTAopen USB: cannot get claim interface, error: %s", (uint8_t *)libusb_error_name(err));
            return BTA_StatusRuntimeError;
        }

        inst->usbDevice = dev;
        if (inst->usbHandle) {
            break;
        }
    }
    libusb_free_device_list(devs, 1);

    if (!inst->usbHandle) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_CRITICAL, BTA_StatusDeviceUnreachable, "BTAopen USB: No valid usb device found");
        return BTA_StatusDeviceUnreachable;
    }
    return BTA_StatusOk;
}


BTA_Status BTAUSBclose(BTA_WrapperInst *winst) {
    if (!winst) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAclose USB: winst missing!");
        return BTA_StatusInvalidParameter;
    }
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst->inst;
    if (!inst) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAclose USB: inst missing!");
        return BTA_StatusInvalidParameter;
    }
    inst->closing = 1;

    BTA_Status status;
    if (inst->readFramesThread) {
        status = BTAjoinThread(inst->readFramesThread);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose USB: Failed to join readFramesThread");
        }
    }

    //if (inst->connectionMonitorThread) {
    //    status = BTAjoinThread(inst->connectionMonitorThread);
    //    if (status != BTA_StatusOk) {
    //        BTAinfoEventHelper(winst->infoEventInst, IMPORTANCE_ERROR, status, "BTAclose USB: Failed to join connectionMonitorThread");
    //    }
    //}

    if (inst->usbHandle && inst->usbDevice) {
        int err = libusb_release_interface(inst->usbHandle, inst->interfaceNumber);
        if (err < 0) {
            BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAclose USB: Failed to release usb handle (%d)", err);
        }
    }

    if (inst->usbHandle) {
        libusb_close(inst->usbHandle);
    }

    status = BGRBclose(&inst->grabInst);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose USB: Failed to close grabber");
    }

    status = BTAcloseMutex(inst->controlMutex);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose USB: Failed to close controlMutex");
    }
    free(inst);
    winst->inst = 0;
    return BTA_StatusOk;
}


static BTA_Status BTAUSBgetDeviceType(BTA_WrapperInst *winst, BTA_DeviceType *deviceType) {
    if (!winst || !deviceType) {
        return BTA_StatusInvalidParameter;
    }
    BTA_DeviceInfo *deviceInfo;
    BTA_Status status = BTAUSBgetDeviceInfo(winst, &deviceInfo);
    if (status != BTA_StatusOk) {
        return status;
    }
    *deviceType = deviceInfo->deviceType;
    BTAfreeDeviceInfo(deviceInfo);
    return BTA_StatusOk;
}


BTA_Status BTAUSBgetDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo) {
    if (!winst || !deviceInfo) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst;
    *deviceInfo = 0;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }

    BTA_DeviceInfo *info = (BTA_DeviceInfo *)calloc(1, sizeof(BTA_DeviceInfo));
    if (!info) {
        return BTA_StatusOutOfMemory;
    }

    uint32_t dataAddress = 0x0001;
    uint32_t data[8];
    uint32_t dataCount = 8;
    BTA_Status status = BTAUSBreadRegister(winst, dataAddress, data, &dataCount);
    if (status == BTA_StatusOk) {
        info->mode0 = data[0x0001 - dataAddress];
        info->status = data[0x0003 - dataAddress];

        info->deviceType = (BTA_DeviceType)data[0x0006 - dataAddress];
        if (info->deviceType == 0) {
            info->deviceType = BTA_DeviceTypeGenericUsb;
        }

        info->firmwareVersionMajor = (data[0x0008 - dataAddress] & 0xf800) >> 11;
        info->firmwareVersionMinor = (data[0x0008 - dataAddress] & 0x07c0) >> 6;
        info->firmwareVersionNonFunc = (data[0x0008 - dataAddress] & 0x003f);
    }

    dataAddress = 0x000c;
    dataCount = 2;
    status = BTAUSBreadRegister(winst, dataAddress, data, &dataCount);
    if (status == BTA_StatusOk) {
        info->serialNumber = data[0x000c - dataAddress];
        info->serialNumber |= data[0x000d - dataAddress] << 16;
    }

    dataAddress = 0x0040;
    dataCount = 2;
    status = BTAUSBreadRegister(winst, dataAddress, data, &dataCount);
    if (status == BTA_StatusOk) {
        info->uptime = data[0x0040 - dataAddress];
        info->uptime |= data[0x0041 - dataAddress] << 16;
    }

    dataAddress = 0x0570;
    dataCount = 3;
    status = BTAUSBreadRegister(winst, dataAddress, data, &dataCount);
    if (status == BTA_StatusOk) {
        uint32_t ponPart1 = data[0x0570 - dataAddress];
        uint32_t ponPart2 = data[0x0571 - dataAddress];
        uint32_t deviceRevisionMajor = data[0x0572 - dataAddress];
        if (ponPart1 && ponPart2) {
            info->productOrderNumber = (uint8_t *)calloc(1, 20);
            if (!info->productOrderNumber) {
                BTAfreeDeviceInfo(info);
                return BTA_StatusOutOfMemory;
            }
            sprintf((char *)info->productOrderNumber, "%03d-%04d-%d", ponPart1, ponPart2, deviceRevisionMajor);
        }
    }

    *deviceInfo = info;
    return BTA_StatusOk;
}


uint8_t BTAUSBisRunning(BTA_WrapperInst *winst) {
    if (!winst) {
        return 0;
    }
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst->inst;
    if (!inst) {
        return 0;
    }

    if (!inst->readFramesThread) {
        return 0;
    }
    return 1;
}


uint8_t BTAUSBisConnected(BTA_WrapperInst *winst) {
    // TODO: implement connectionMonitor
    return BTAUSBisRunning(winst);
}



BTA_Status BTAUSBsetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode frameMode) {

    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (frameMode == BTA_FrameModeCurrentConfig) {
        return BTA_StatusOk;
    }

    uint32_t imgDataFormat;
    BTA_Status status = BTAUSBreadRegister(winst, 4, &imgDataFormat, 0);
    if (status != BTA_StatusOk) {
        return status;
    }

    BTA_EthImgMode imageMode = (BTA_EthImgMode)BTAframeModeToImageMode(0, frameMode);
    if (imageMode == BTA_EthImgModeNone) {
        BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAsetFrameMode: frameMode not supported: %d", frameMode);
        return BTA_StatusNotSupported;
    }
    imgDataFormat &= ~(0xff << 3);
    imgDataFormat |= (int)imageMode << 3;

    status = BTAUSBwriteRegister(winst, 0x0004, &imgDataFormat, 0);
    if (status != BTA_StatusOk) {
        return status;
    }

    // To be sure the register content is up to date, let's wait the time the camera might need to apply new modus
    BTAmsleep(2000);

    // read back the imgDataFormat
    uint32_t imgDataFormatReadBack;
    status = BTAUSBreadRegister(winst, 4, &imgDataFormatReadBack, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    // If the imgDataFormat was not set means it is not supported
    if ((imgDataFormat & (0xff << 3)) != (imgDataFormatReadBack & (0xff << 3))) {
        BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAsetFrameMode: frameMode not supported: %d, device silently refused", frameMode);
        return BTA_StatusNotSupported;
    }
    return BTA_StatusOk;
}


BTA_Status BTAUSBgetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode *frameMode) {
    if (!winst || !frameMode) {
        return BTA_StatusInvalidParameter;
    }

    uint32_t imgDataFormat;
    BTA_Status status = BTAUSBreadRegister(winst, 4, &imgDataFormat, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    BTA_EthImgMode imageMode = (BTA_EthImgMode)((imgDataFormat >> 3) & 0xff);
    *frameMode = BTAimageDataFormatToFrameMode(0, imageMode);
    return BTA_StatusOk;
}


BTA_Status BTAUSBsendReset(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }

    uint8_t *sendBuffer;
    uint32_t sendBufferLen;
    BTA_Status status = BTAtoByteStream(BTA_EthCommandReset, BTA_EthSubCommandNone, (uint32_t)0, (uint8_t *)0, (uint32_t)0, inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen, 0, 0, 0, 0, 0, 0, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    BTAlockMutex(inst->controlMutex);
    status = transmit(winst, sendBuffer, sendBufferLen, timeoutDefault);
    if (status != BTA_StatusOk) {
        free(sendBuffer);
        sendBuffer = 0;
        BTAunlockMutex(inst->controlMutex);
        return status;
    }
    status = receiveControlResponse(winst, sendBuffer, 0, 0, timeoutDefault, 0, winst->infoEventInst);
    free(sendBuffer);
    sendBuffer = 0;
    BTAunlockMutex(inst->controlMutex);
    return status;
}


static void *readFramesRunFunction(void *handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return 0;
    }
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst->inst;
    if (!inst) {
        return 0;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "readFramesRunFunction: thread started");

    const uint8_t preamble3[4] = { 0xff, 0xff, 0x0, 0x3 };
    const uint8_t preamble4[4] = { 0xff, 0xff, 0x0, 0x4 };
    const int maxHeaderSize = 500;
    uint8_t bufHeader[maxHeaderSize];
    memset(bufHeader, 0, maxHeaderSize);
    uint64_t timeLastSuccess = BTAgetTickCount64();
    while (!inst->abortReadFramesThread && !inst->closing) {
        int bytesRead;
        int err = libusb_bulk_transfer(inst->usbHandle, BTA_USB_EP_DATA_STREAM, (unsigned char *)&(bufHeader[3]), 1, &bytesRead, timeoutSmall);
        if (err < 0 || bytesRead != 1) {
            if (BTAgetTickCount64() > timeLastSuccess + 7000) {
                timeLastSuccess = BTAgetTickCount64();
                BTAinfoEventHelperSI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "readFramesRunFunction: read header (1) error %s, bytes read %d", (uint8_t *)libusb_error_name(err), bytesRead);
            }
            if (err != LIBUSB_ERROR_TIMEOUT) {
                // dataReceiveTimeout does not always apply, so wait manually before retrying
                BTAmsleep(2000);
            }
            continue;
        }
        timeLastSuccess = BTAgetTickCount64();
        if (memcmp(bufHeader, preamble3, 4) == 0) {
            // found v3
            err = libusb_bulk_transfer(inst->usbHandle, BTA_USB_EP_DATA_STREAM, (unsigned char *)&(bufHeader[4]), BTA_ETH_FRAME_DATA_HEADER_SIZE - 4, &bytesRead, timeoutDefault);
            if (err < 0 || bytesRead != BTA_ETH_FRAME_DATA_HEADER_SIZE - 4) {
                BTAinfoEventHelperSI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "readFramesRunFunction: v3 read header (2) error %s, bytes read %d", (uint8_t *)libusb_error_name(err), bytesRead);
                continue;
            }
            // header is completely read, now parse
            int xRes = bufHeader[4] << 8 | bufHeader[5];
            int yRes = bufHeader[6] << 8 | bufHeader[7];
            int nofChannels = bufHeader[8];
            int bytesPerPixel = bufHeader[9];
            int dataLen = xRes * yRes * nofChannels * bytesPerPixel;
            uint16_t frameCounter = bufHeader[16] << 8 | bufHeader[17];
            uint16_t crc16 = bufHeader[62] << 8 | bufHeader[63];
            uint16_t crc16calc = crc16_ccitt(bufHeader + 2, BTA_ETH_FRAME_DATA_HEADER_SIZE - 4);
            if (crc16 != crc16calc) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "readFramesRunFunction: v3 CRC16 of header mismatch");
                continue;
            }
            // prepare frameToParse, as it is what parseFrame expects
            BTA_FrameToParse *frameToParse;
            BTA_Status status = BTAcreateFrameToParse(&frameToParse);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "readFramesRunFunction: v3 BTAcreateFrameToParse: Could not create FrameToParse");
                continue;
            }
            frameToParse->timestamp = BTAgetTickCount64();
            frameToParse->frameCounter = frameCounter;
            frameToParse->frameLen = BTA_ETH_FRAME_DATA_HEADER_SIZE + dataLen;
            frameToParse->frame = (uint8_t *)malloc(frameToParse->frameLen);
            memcpy(frameToParse->frame, bufHeader, BTA_ETH_FRAME_DATA_HEADER_SIZE);
            // now read data of frame
            err = libusb_bulk_transfer(inst->usbHandle, BTA_USB_EP_DATA_STREAM, (unsigned char *)&frameToParse->frame[BTA_ETH_FRAME_DATA_HEADER_SIZE], dataLen, &bytesRead, timeoutDefault);
            if (err < 0 || bytesRead != dataLen) {
                BTAinfoEventHelperS(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "readFramesRunFunction: v3 read data error %s", (uint8_t *)libusb_error_name(err));
                BTAfreeFrameToParse(&frameToParse);
                continue;
            }
            // got full frame
            BTAparseGrabCallbackEnqueue(winst, frameToParse);
            BTAfreeFrameToParse(&frameToParse);
        }
        if (memcmp(bufHeader, preamble4, 4) == 0) {
            // found v4
            err = libusb_bulk_transfer(inst->usbHandle, BTA_USB_EP_DATA_STREAM, (unsigned char *)&(bufHeader[4]), 2, &bytesRead, timeoutDefault);
            if (err < 0 || bytesRead != 2) {
                BTAinfoEventHelperSI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "readFramesRunFunction: v4 read header (3) error %s, bytes read %d", (uint8_t *)libusb_error_name(err), bytesRead);
                continue;
            }
            uint16_t headerLength = bufHeader[4] | (bufHeader[5] << 8);
            err = libusb_bulk_transfer(inst->usbHandle, BTA_USB_EP_DATA_STREAM, (unsigned char *)&(bufHeader[6]), headerLength - 6, &bytesRead, timeoutDefault);
            if (err < 0 || bytesRead != headerLength - 6) {
                BTAinfoEventHelperSI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "readFramesRunFunction: v4 read header (4) error %s, bytes read %d", (uint8_t *)libusb_error_name(err), bytesRead);
                continue;
            }
            // header is completely read

            uint16_t crc16 = bufHeader[headerLength - 2] | bufHeader[headerLength - 1] << 8;
            uint16_t crc16calc = crc16_ccitt(bufHeader + 2, headerLength - 4);
            if (crc16 != crc16calc) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "readFramesRunFunction: v4 CRC16 of header mismatch");
                continue;
            }

            // go through descriptors and calculate dataLen
            int dataLen = 0;
            int index = 6;
            uint16_t frameCounter = 0;
            uint32_t timestamp = 0;
            while (1) {
                BTA_Data4DescBase *descBase = (BTA_Data4DescBase *)&(bufHeader[index]);
                if (descBase->descriptorType == btaData4DescriptorTypeEof) {
                    if (!dataLen || !frameCounter || !timestamp) {
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "readFramesRunFunction: v4 No FrameInfo descriptor found!");
                        break;
                    }

                    // prepare frameToParse, as it is what parseFrame expects
                    BTA_FrameToParse *frameToParse; //TODO: reuse and only call BTAinitFrameToParse!
                    BTA_Status status = BTAcreateFrameToParse(&frameToParse);
                    if (status != BTA_StatusOk) {
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "readFramesRunFunction: v4 BTAcreateFrameToParse: Could not create FrameToParse");
                        break;
                    }
                    BTAinitFrameToParse(&frameToParse, timestamp, frameCounter, headerLength + dataLen, 1);
                    frameToParse->packetSize[0] = 1; // TODO: don't do this like this
                    memcpy(frameToParse->frame, bufHeader, headerLength);
                    // now read data of frame
                    err = libusb_bulk_transfer(inst->usbHandle, BTA_USB_EP_DATA_STREAM, (unsigned char *)&frameToParse->frame[headerLength], dataLen, &bytesRead, timeoutDefault);
                    if (err < 0 || bytesRead != dataLen) {
                        BTAinfoEventHelperS(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "readFramesRunFunction: v4 read data error %s", (uint8_t *)libusb_error_name(err));
                        BTAfreeFrameToParse(&frameToParse);
                        break;
                    }
                    // got full frame
                    BTAparseGrabCallbackEnqueue(winst, frameToParse);
                    BTAfreeFrameToParse(&frameToParse);
                    break;
                }

                if (descBase->descriptorType == btaData4DescriptorTypeFrameInfoV1) {
                    BTA_Data4DescFrameInfoV1 *frameInfo = (BTA_Data4DescFrameInfoV1 *)&(bufHeader[index]);
                    frameCounter = frameInfo->frameCounter;
                    timestamp = frameInfo->timestamp;
                }
                dataLen += descBase->dataLen;
                index += descBase->descriptorLen;
            }
        }
        bufHeader[0] = bufHeader[1];
        bufHeader[1] = bufHeader[2];
        bufHeader[2] = bufHeader[3];
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "readFramesRunFunction: terminated");
    return 0;
}


BTA_Status BTAUSBgetIntegrationTime(BTA_WrapperInst *winst, uint32_t *integrationTime) {
    return BTAUSBreadRegister(winst, 0x0005, integrationTime, 0);
}


BTA_Status BTAUSBsetIntegrationTime(BTA_WrapperInst *winst, uint32_t integrationTime) {
    if (integrationTime < 1 || integrationTime > 0xffff) {
        return BTA_StatusInvalidParameter;
    }
    return BTAUSBwriteRegister(winst, 0x0005, &integrationTime, 0);
}


BTA_Status BTAUSBgetFrameRate(BTA_WrapperInst *winst, float *frameRate) {
    if (!frameRate) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t frameRate32;
    BTA_Status status = BTAUSBreadRegister(winst, 0x000a, &frameRate32, 0);
    if (status == BTA_StatusOk) {
        *frameRate = (float)frameRate32;
    }
    return status;
}


BTA_Status BTAUSBsetFrameRate(BTA_WrapperInst *winst, float frameRate) {
    uint32_t frameRate32 = (uint32_t)frameRate;
    if (frameRate < 1 || frameRate > 0xffff) {
        return BTA_StatusInvalidParameter;
    }
    return BTAUSBwriteRegister(winst, 0x000a, &frameRate32, 0);
}


BTA_Status BTAUSBgetModulationFrequency(BTA_WrapperInst *winst, uint32_t *modulationFrequency) {
    if (!modulationFrequency) {
        return BTA_StatusInvalidParameter;
    }
    BTA_Status status;
    status = BTAUSBreadRegister(winst, 0x0009, modulationFrequency, 0);
    if (status != BTA_StatusOk) {
        *modulationFrequency = 0;
        return status;
    }
    *modulationFrequency *= 10000;
    return BTA_StatusOk;
}


BTA_Status BTAUSBsetModulationFrequency(BTA_WrapperInst *winst, uint32_t modulationFrequency) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_Status status;
    BTA_DeviceType deviceType;
    status = BTAUSBgetDeviceType(winst, &deviceType);
    if (status != BTA_StatusOk) {
        return status;
    }
    uint32_t modFreq;
    status = BTAgetNextBestModulationFrequency(deviceType, modulationFrequency, &modFreq, 0);
    if (status != BTA_StatusOk) {
        if (status == BTA_StatusNotSupported) {
            BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAsetModulationFrequency: Not supported for this deviceType: %d", deviceType);
        }
        return status;
    }
    modFreq /= 10000;
    return BTAUSBwriteRegister(winst, 0x0009, &modFreq, 0);
}


BTA_Status BTAUSBgetGlobalOffset(BTA_WrapperInst *winst, float *offset) {
    if (!winst || !offset) {
        return BTA_StatusInvalidParameter;
    }
    *offset = 0;
    uint32_t modFreq;
    BTA_Status status;
    status = BTAUSBreadRegister(winst, 9, &modFreq, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    modFreq = modFreq * 10000;
    BTA_DeviceType deviceType;
    status = BTAUSBgetDeviceType(winst, &deviceType);
    if (status != BTA_StatusOk) {
        return status;
    }
    int32_t modFreqIndex;
    status = BTAgetNextBestModulationFrequency(deviceType, modFreq, 0, &modFreqIndex);
    if (status != BTA_StatusOk) {
        if (status == BTA_StatusNotSupported) {
            BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAgetGlobalOffset: Not supported for this deviceType: %d", deviceType);
        }
        return status;
    }
    uint32_t offset32;
    status = BTAUSBreadRegister(winst, 0x00c1 + modFreqIndex, &offset32, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    *offset = (int16_t)offset32;
    return BTA_StatusOk;
}


BTA_Status BTAUSBsetGlobalOffset(BTA_WrapperInst *winst, float offset) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_Status status;
    if (offset < -32768 || offset > 32767) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t modFreq;
    status = BTAUSBreadRegister(winst, 9, &modFreq, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    modFreq = modFreq * 10000;
    BTA_DeviceType deviceType;
    status = BTAUSBgetDeviceType(winst, &deviceType);
    if (status != BTA_StatusOk) {
        return status;
    }
    int32_t modFreqIndex;
    status = BTAgetNextBestModulationFrequency(deviceType, modFreq, 0, &modFreqIndex);
    if (status != BTA_StatusOk) {
        if (status == BTA_StatusNotSupported) {
            BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAsetGlobalOffset: Not supported for this deviceType: %d", deviceType);
        }
        return status;
    }
    int32_t offset32 = (int32_t)offset;
    status = BTAUSBwriteRegister(winst, 0x00c1 + modFreqIndex, (uint32_t *)&offset32, 0);
    return status;
}


BTA_Status BTAUSBwriteCurrentConfigToNvm(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t password = 0x4877;
    BTA_Status status = BTAUSBwriteRegister(winst, 0x0022, &password, 0);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAwriteCurrentConfigToNvm() failed (a)");
        return status;
    }
    BTAmsleep(200);
    uint32_t command = 0xdd9e;
    status = BTAUSBwriteRegister(winst, 0x0033, &command, 0);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAwriteCurrentConfigToNvm() failed (b)");
        return status;
    }
    // wait until the device is ready again and poll result
    BTAmsleep(1000);
    uint32_t endTime = BTAgetTickCount() + timeoutBigger;
    do {
        BTAmsleep(500);
        uint32_t result;
        status = readRegister(winst, 0x0034, &result, 0, timeoutBigger);
        if (status == BTA_StatusOk) {
            if (result != 1) {
                BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAwriteCurrentConfigToNvm() failed, the device said %d", result);
                return BTA_StatusRuntimeError;
            }
            return BTA_StatusOk;
        }
    } while (BTAgetTickCount() < endTime);
    return BTA_StatusTimeOut;
}


BTA_Status BTAUSBrestoreDefaultConfig(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t password = 0x4877;
    BTA_Status status = BTAUSBwriteRegister(winst, 0x0022, &password, 0);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTArestoreDefaultConfig() failed (a)");
        return status;
    }
    BTAmsleep(200);
    uint32_t command = 0xc2ae;
    status = BTAUSBwriteRegister(winst, 0x0033, &command, 0);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTArestoreDefaultConfig() failed (b)");
        return status;
    }
    // wait until the device is ready again and poll result
    BTAmsleep(1000);
    uint32_t endTime = BTAgetTickCount() + timeoutBigger;
    do {
        BTAmsleep(500);
        uint32_t result;
        status = readRegister(winst, 0x0034, &result, 0, timeoutBigger);
        if (status == BTA_StatusOk) {
            if (result != 1) {
                BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTArestoreDefaultConfig() failed, the device said %d", result);
                return BTA_StatusRuntimeError;
            }
            return BTA_StatusOk;
        }
    } while (BTAgetTickCount() < endTime);
    return BTA_StatusTimeOut;
}


BTA_Status BTAUSBreadRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount) {
    return readRegister(winst, address, data, registerCount, timeoutDefault);
}


static BTA_Status readRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount, uint32_t timeout) {
    if (!winst || !data) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    if (address > 0xffff) {
        if (registerCount) {
            *registerCount = 0;
        }
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAreadRegister: Address overflow");
        return BTA_StatusInvalidParameter;
    }
    uint32_t lenToRead = 2;
    if (registerCount) {
        if (address + *registerCount > 0x10000) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAreadRegister: Address overflow");
            *registerCount = 0;
            return BTA_StatusInvalidParameter;
        }
        lenToRead = *registerCount * 2;
        *registerCount = 0;
    }
    if (lenToRead == 0) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAreadRegister: Length is 0");
        return BTA_StatusInvalidParameter;
    }

    uint8_t *sendBuffer;
    uint32_t sendBufferLen;
    BTA_Status status = BTAtoByteStream(BTA_EthCommandRead, BTA_EthSubCommandNone, address, 0, lenToRead, inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen, 0, 0, 0, 0, 0, 0, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    int attempts = 1;
    uint8_t *dataBuffer;
    uint32_t dataBufferLen;
    BTAlockMutex(inst->controlMutex);
    status = transmit(winst, sendBuffer, sendBufferLen, timeout);
    if (status != BTA_StatusOk) {
        free(sendBuffer);
        sendBuffer = 0;
        BTAunlockMutex(inst->controlMutex);
        return status;
    }
    status = receiveControlResponse(winst, sendBuffer, &dataBuffer, &dataBufferLen, timeout, 0, winst->infoEventInst);
    BTAunlockMutex(inst->controlMutex);
    free(sendBuffer);
    sendBuffer = 0;
    if (status != BTA_StatusOk) {
        return status;
    }
    if (dataBufferLen > lenToRead) {
        // the rare case that more data received than wanted
        dataBufferLen = lenToRead;
    }
    // copy data into output buffer
    for (uint32_t i = 0; i < dataBufferLen / 2; i++) {
        data[i] = (dataBuffer[2*i] << 8) | dataBuffer[2*i + 1];
    }
    free(dataBuffer);
    dataBuffer = 0;
    if (registerCount) {
        *registerCount = dataBufferLen / 2;
    }
    return BTA_StatusOk;
}


BTA_Status BTAUSBwriteRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount) {
    if (!winst || !data) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    if (address > 0xffff) {
        if (registerCount) {
            *registerCount = 0;
        }
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAwriteRegister: Address overflow");
        return BTA_StatusInvalidParameter;
    }
    uint32_t lenToWrite = 2;
    if (registerCount) {
        if (address + *registerCount > 0x10000) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAwriteRegister: Address overflow");
            *registerCount = 0;
            return BTA_StatusInvalidParameter;
        }
        lenToWrite = *registerCount * 2;
        *registerCount = 0;
    }
    if (lenToWrite == 0) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAwriteRegister: Length is 0");
        return BTA_StatusInvalidParameter;
    }

    uint8_t *sendBuffer;
    uint32_t sendBufferLen;
    BTA_Status status = BTAtoByteStream(BTA_EthCommandWrite, BTA_EthSubCommandNone, address, (void *)data, lenToWrite, inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen, 0, 0, 0, 0, 0, 0, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    BTAlockMutex(inst->controlMutex);
    // must send header and data seperately!
    status = transmit(winst, sendBuffer, BTA_ETH_HEADER_SIZE, timeoutDefault);
    if (status != BTA_StatusOk) {
        free(sendBuffer);
        sendBuffer = 0;
        BTAunlockMutex(inst->controlMutex);
        return status;
    }
    status = transmit(winst, sendBuffer + BTA_ETH_HEADER_SIZE, sendBufferLen - BTA_ETH_HEADER_SIZE, timeoutDefault);
    if (status != BTA_StatusOk) {
        free(sendBuffer);
        sendBuffer = 0;
        BTAunlockMutex(inst->controlMutex);
        return status;
    }
    uint32_t dataLen = 0;
    status = receiveControlResponse(winst, sendBuffer, 0, &dataLen, timeoutDefault, 0, winst->infoEventInst);
    BTAunlockMutex(inst->controlMutex);
    free(sendBuffer);
    sendBuffer = 0;
    if (status != BTA_StatusOk) {
        return status;
    }
    if (registerCount) {
        *registerCount = lenToWrite / 2;
    }
    return status;
}


BTA_Status BTAUSBsetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float value) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    switch (libParam) {
    case BTA_LibParamCrcControlEnabled:
        inst->lpControlCrcEnabled = (uint8_t)(value != 0);
        return BTA_StatusOk;
    default:
        BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAsetLibParam: LibParam not supported %d", libParam);
        return BTA_StatusNotSupported;
    }
}


BTA_Status BTAUSBgetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float *value) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    switch (libParam) {
    case BTA_LibParamCrcControlEnabled:
        *value = (float)inst->lpControlCrcEnabled;
        return BTA_StatusOk;
    default:
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAgetLibParam: LibParam not supported");
        return BTA_StatusNotSupported;
    }
}




BTA_Status BTAUSBflashUpdate(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *config, FN_BTA_ProgressReport progressReport) {
    if (!winst) {
        if (progressReport) (*progressReport)(BTA_StatusInvalidParameter, 0);
        return BTA_StatusInvalidParameter;
    }
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst->inst;
    if (!inst) {
        if (progressReport) (*progressReport)(BTA_StatusInvalidParameter, 0);
        return BTA_StatusInvalidParameter;
    }

    BTA_EthCommand cmd = BTA_EthCommandNone;
    BTA_EthSubCommand subCmd = BTA_EthSubCommandNone;
    BTAgetFlashCommand(config->target, config->flashId, &cmd, &subCmd);
    if (cmd == BTA_EthCommandNone || cmd == BTA_EthCommandNone) {
        if (progressReport) (*progressReport)(BTA_StatusInvalidParameter, 0);
        BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAflashUpdate: FlashTarget or FlashId not supported %d", (int)config->target);
        return BTA_StatusInvalidParameter;
    }

    BTA_Status status;
    uint8_t *sendBuffer;
    uint32_t sendBufferLen;
    status = BTAtoByteStream(cmd, subCmd, config->address, config->data, config->dataLen, inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen, 0, 0, 0, 0, 0, 0, 0);
    if (status != BTA_StatusOk) {
        if (progressReport) (*progressReport)(status, 0);
        return status;
    }
    // first send the header alone (must with usb)
    if (progressReport) (*progressReport)(BTA_StatusOk, 0);
    BTAlockMutex(inst->controlMutex);
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: Transmitting header (64 bytes)");
    status = transmit(winst, sendBuffer, BTA_ETH_HEADER_SIZE, timeoutHuge);
    // calculate size of data without header and if data large, divide into 10 parts
    uint8_t *sendBufferPart = sendBuffer + BTA_ETH_HEADER_SIZE;
    uint32_t sendBufferPartLen = sendBufferLen > 1000000 ? (sendBufferLen - BTA_ETH_HEADER_SIZE) / 10 : (sendBufferLen - BTA_ETH_HEADER_SIZE);
    uint32_t sendBufferLenSent = BTA_ETH_HEADER_SIZE;
    do {
        BTAinfoEventHelperII(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: Transmitting %d bytes of data (already sent %d bytes)", sendBufferLen - BTA_ETH_HEADER_SIZE, sendBufferLenSent - BTA_ETH_HEADER_SIZE);
        status = transmit(winst, sendBufferPart, sendBufferPartLen, timeoutHuge);
        if (status != BTA_StatusOk) {
            BTAunlockMutex(inst->controlMutex);
            free(sendBuffer);
            sendBuffer = 0;
            if (progressReport) (*progressReport)(status, 0);
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAflashUpdate: Failed!");
            return status;
        }
        sendBufferPart += sendBufferPartLen;
        sendBufferLenSent += sendBufferPartLen;
        if (sendBufferLenSent + sendBufferPartLen > sendBufferLen) {
            sendBufferPartLen = sendBufferLen - sendBufferLenSent;
        }
        // report progress (use 50 of the 100% for the transmission
        if (progressReport) (*progressReport)(status, (uint8_t)(50.0 * sendBufferLenSent / sendBufferLen));
        BTAmsleep(22);
    } while (sendBufferLenSent < sendBufferLen);
    BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: %d bytes sent", sendBufferLenSent);
    status = receiveControlResponse(winst, sendBuffer, 0, 0, timeoutHuge, 0, winst->infoEventInst);
    free(sendBuffer);
    sendBuffer = 0;
    BTAunlockMutex(inst->controlMutex);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, status, "BTAflashUpdate: Erroneous response! %d", status);
        if (progressReport) (*progressReport)(status, 0);
        return status;
    }

    // flash cmd successfully sent, now poll status
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: File transfer finished");

    uint32_t timeEnd = BTAgetTickCount() + 5 * 60 * 1000; // 5 minutes timeout
    while (1) {
        BTAmsleep(1000);
        if (BTAgetTickCount() > timeEnd) {
            if (progressReport) (*progressReport)(BTA_StatusTimeOut, 80);
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusTimeOut, "BTAflashUpdate: Timeout");
            return BTA_StatusTimeOut;
        }
        uint32_t fileUpdateStatus;
        status = readRegister(winst, 0x01d1, &fileUpdateStatus, 0, timeoutHuge);
        if (status == BTA_StatusOk) {
            uint8_t finished = 0;
            status = BTAhandleFileUpdateStatus(fileUpdateStatus, progressReport, winst->infoEventInst, &finished);
            if (finished) {
                return status;
            }
        }
    }
}



BTA_Status BTAUSBflashRead(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *config, FN_BTA_ProgressReport progressReport, uint8_t quiet) {
    if (!winst) {
        if (progressReport) (*progressReport)(BTA_StatusInvalidParameter, 0);
        return BTA_StatusInvalidParameter;
    }
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst->inst;
    if (!inst) {
        if (progressReport) (*progressReport)(BTA_StatusInvalidParameter, 0);
        return BTA_StatusInvalidParameter;
    }

    BTA_EthCommand cmd = BTA_EthCommandNone;
    BTA_EthSubCommand subCmd = BTA_EthSubCommandNone;
    BTAgetFlashCommand(config->target, config->flashId, &cmd, &subCmd);
    if (cmd == BTA_EthCommandNone || cmd == BTA_EthCommandNone) {
        if (progressReport) (*progressReport)(BTA_StatusInvalidParameter, 0);
        if (!quiet) BTAinfoEventHelperI(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAflashRead: FlashTarget or FlashId not supported %d", (int)config->target);
        return BTA_StatusInvalidParameter;
    }
    // change the flash command to a read command
    cmd = (BTA_EthCommand)((int)cmd + 100);

    uint8_t *sendBuffer;
    uint32_t sendBufferLen;
    BTA_Status status = BTAtoByteStream(cmd, subCmd, config->address, 0, 0, inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen, 0, 0, 0, 0, 0, 0, 0);
    if (status != BTA_StatusOk) {
        if (progressReport) (*progressReport)(status, 0);
        return status;
    }

    BTAlockMutex(inst->controlMutex);
    status = transmit(winst, sendBuffer, sendBufferLen, timeoutDefault);
    if (status != BTA_StatusOk) {
        free(sendBuffer);
        sendBuffer = 0;
        BTAunlockMutex(inst->controlMutex);
        if (progressReport) (*progressReport)(status, 0);
        return status;
    }
    if (!quiet) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashRead: Receiving requested data");
    uint8_t *dataBuffer;
    uint32_t dataBufferLen;
    if (quiet) status = receiveControlResponse(winst, sendBuffer, &dataBuffer, &dataBufferLen, timeoutHuge, progressReport, 0);
    else status = receiveControlResponse(winst, sendBuffer, &dataBuffer, &dataBufferLen, timeoutHuge, progressReport, winst->infoEventInst);
    BTAunlockMutex(inst->controlMutex);
    free(sendBuffer);
    sendBuffer = 0;
    if (status != BTA_StatusOk) {
        if (progressReport) (*progressReport)(status, 0);
        return status;
    }
    if (!quiet) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashRead: Transmission successful");
    if (dataBufferLen > config->dataLen) {
        free(dataBuffer);
        dataBuffer = 0;
        config->dataLen = 0;
        if (progressReport) (*progressReport)(BTA_StatusOutOfMemory, 0);
        if (!quiet) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "BTAflashRead: Provided buffer too small");
        return BTA_StatusOutOfMemory;
    }
    config->dataLen = dataBufferLen;
    memcpy(config->data, dataBuffer, dataBufferLen);
    if (progressReport) (*progressReport)(BTA_StatusOk, 100);
    return BTA_StatusOk;
}



BTA_Status BTAUSBstartDiscovery(BTA_DiscoveryConfig *discoveryConfig, FN_BTA_DeviceFound deviceFound, FN_BTA_InfoEvent infoEvent, BTA_Handle *handle) {
    if (!discoveryConfig || !handle) {
        return BTA_StatusInvalidParameter;
    }
    *handle = 0;
    BTA_DiscoveryInst *inst = (BTA_DiscoveryInst *)calloc(1, sizeof(BTA_DiscoveryInst));
    if (!inst) {
        return BTA_StatusOutOfMemory;
    }
    inst->infoEventInst = (BTA_InfoEventInst *)calloc(1, sizeof(BTA_InfoEventInst));
    inst->infoEventInst->infoEvent = infoEvent;
    inst->infoEventInst->verbosity = UINT8_MAX;

    inst->deviceFound = deviceFound;

    BTA_Status status = BTAcreateThread(&inst->discoveryThread, (void* (*)(void*))&discoveryRunFunction, inst, 0);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, status, "Discovery: Could not start discoveryThread");
        return status;
    }
    *handle = inst;
    return BTA_StatusOk;
}


BTA_Status BTAUSBstopDiscovery(BTA_Handle *handle) {
    if (!handle) {
        return BTA_StatusInvalidParameter;
    }
    BTA_DiscoveryInst *inst = (BTA_DiscoveryInst *)*handle;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    inst->abortDiscovery = 1;
    BTA_Status status = BTAjoinThread(inst->discoveryThread);
    if (status != BTA_StatusOk) {
        return status;
    }
    free(inst->infoEventInst);
    inst->infoEventInst = 0;
    free(inst);
    *handle = 0;
    return BTA_StatusOk;
}


static void *discoveryRunFunction(BTA_DiscoveryInst *inst) {
    int err = libusb_init(NULL);
    if (err < 0) {
        BTAinfoEventHelperS(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Discovery: cannot init usb library, error: %d", (uint8_t *)libusb_error_name(err));
        return 0;
    }

    const uint16_t handlesLen = 100;
    BTA_WrapperInst *winsts[handlesLen];
    int k = 0;
    while (!inst->abortDiscovery) {
        if (k >= handlesLen) {
            // fount 100 devices. that's enough. be idle
            BTAinfoEventHelperI(inst->infoEventInst, VERBOSE_INFO, BTA_StatusWarning, "Discovery thread: Found %d devices. Call BTAstopDiscovery now", handlesLen);
            BTAmsleep(500);
            continue;
        }
        winsts[k] = (BTA_WrapperInst *)calloc(1, sizeof(BTA_WrapperInst));
        if (!winsts[k]) {
            BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "Discovery thread: can't allocate");
            break;
        }
        // let's use empty instances for a quiet connection attempt
        winsts[k]->infoEventInst = (BTA_InfoEventInst *)calloc(1, sizeof(BTA_InfoEventInst));
        winsts[k]->frameArrivedInst = (BTA_FrameArrivedInst *)calloc(1, sizeof(BTA_FrameArrivedInst));
        BTA_Config config;
        BTAinitConfig(&config);
        BTA_Status status = BTAUSBopen(&config, winsts[k]);
        if (status != BTA_StatusOk) {
            continue;
        }
        BTA_DeviceInfo *deviceInfo;
        status = BTAUSBgetDeviceInfo(winsts[k], &deviceInfo);
        if (status != BTA_StatusOk) {
            BTAUSBclose(winsts[k]);
            continue;
        }
        if (inst->deviceType == 0 || inst->deviceType == BTA_DeviceTypeGenericUsb || inst->deviceType == deviceInfo->deviceType) {
            if (inst->deviceFound) {
                (*inst->deviceFound)(inst, deviceInfo);
            }
        }
        BTAfreeDeviceInfo(deviceInfo);
        k++;
    }

    while (--k >= 0) {
        BTAUSBclose(winsts[k]);
        if (winsts[k]) {
            free(winsts[k]->infoEventInst);
            winsts[k]->infoEventInst = 0;
            free(winsts[k]->frameArrivedInst);
            winsts[k]->frameArrivedInst = 0;
        }
    }
    return 0;
}


static BTA_Status receiveControlResponse(BTA_WrapperInst *winst, uint8_t *request, uint8_t **data, uint32_t *dataLen, uint32_t timeout, FN_BTA_ProgressReport progressReport, BTA_InfoEventInst *infoEventInst) {
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst->inst;
    struct libusb_device_handle *usbHandle = inst->usbHandle;
    struct libusb_device *usbDevice = inst->usbDevice;
    uint8_t header[BTA_ETH_HEADER_SIZE];
    uint32_t headerLen;
    uint32_t flags;
    uint32_t dataCrc32;
    BTA_Status status;
    if (data) {
        *data = 0;
    }
    if (!usbHandle || !usbDevice) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "USB receiveControlResponse: Handle or device missing");
        return BTA_StatusInvalidParameter;
    }

    headerLen = sizeof(header);
    status = receive(header, &headerLen, usbHandle, usbDevice, timeout, infoEventInst);
    if (status != BTA_StatusOk) {
        return status;
    }
    uint32_t dataLenTemp;
    status = BTAparseControlHeader(request, header, &dataLenTemp, &flags, &dataCrc32, infoEventInst);
    if (status != BTA_StatusOk) {
        return status;
    }
    if (dataLenTemp > 0) {
        if (!data || !dataLen) {
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "USB receiveControlResponse: data or dataLength missing");
            return BTA_StatusInvalidParameter;
        }
        *dataLen = dataLenTemp;
        *data = (uint8_t *)malloc(*dataLen);
        if (!*data) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "USB receiveControlResponse: Cannot allocate");
            return BTA_StatusOutOfMemory;
        }
        if (progressReport) {
            // read data in 10 parts and report progress
            uint32_t dataReceivedLen = 0;
            uint32_t dataPartLen = *dataLen / 10;
            uint32_t dataPartLenTemp;
            (*progressReport)(BTA_StatusOk, 0);
            do {
                dataPartLenTemp = dataPartLen;
                status = receive((*data) + dataReceivedLen, &dataPartLenTemp, usbHandle, usbDevice, timeout, infoEventInst);
                if (status != BTA_StatusOk) {
                    BTAinfoEventHelperI(infoEventInst, VERBOSE_ERROR, status, "USB receiveControlResponse: Failed! %d", dataPartLenTemp);
                }
                if (status == BTA_StatusOk) {
                    dataReceivedLen += dataPartLen;
                    if (dataReceivedLen + dataPartLen > *dataLen) {
                        dataPartLen = *dataLen - dataReceivedLen;
                    }
                    // report progress (use 90 of the 100% for the transmission
                    (*progressReport)(status, (uint8_t)(90 * dataReceivedLen / *dataLen));
                }
            } while (dataReceivedLen < *dataLen && status == BTA_StatusOk);
            if (status != BTA_StatusOk) {
                free(*data);
                *data = 0;
                (*progressReport)(status, 0);
                return status;
            }
        }
        else {
            // read data at once
            status = receive(*data, dataLen, usbHandle, usbDevice, timeout, infoEventInst);
            if (status != BTA_StatusOk) {
                free(*data);
                *data = 0;
                return status;
            }
        }
    }

    if (data) {
        if (*data) {
            if (!(flags & 1)) {
                uint32_t dataCrc32Calced = (uint32_t)CRC32ccitt(*data, *dataLen);
                if (dataCrc32 != dataCrc32Calced) {
                    free(*data);
                    *data = 0;
                    BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusCrcError, "USB receiveControlResponse: Data CRC mismatch");
                    return BTA_StatusCrcError;
                }
            }
        }
    }
    return BTA_StatusOk;
}


static BTA_Status receive(uint8_t *data, uint32_t *length, struct libusb_device_handle *usbHandle, struct libusb_device *usbDevice, uint32_t timeout, BTA_InfoEventInst *infoEventInst) {
    if (!usbHandle || !usbDevice || !data || !length) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "USB receive: Handle, device, data or length missing");
        return BTA_StatusInvalidParameter;
    }
    uint32_t bytesReadTotal = 0;
    while (bytesReadTotal < *length) {
        int bytesRead;
        int err = libusb_bulk_transfer(usbHandle, BTA_USB_EP_CONTROL_READ, (unsigned char *)data, *length, &bytesRead, timeout);
        if (err != 0) {
            // TODO error handling and possibly retry
            BTAinfoEventHelperS(infoEventInst, VERBOSE_ERROR, BTA_StatusDeviceUnreachable, "USB receive error %s", (uint8_t *)libusb_error_name(err));
            // TODO close connection
            return BTA_StatusDeviceUnreachable;
        }
        bytesReadTotal += bytesRead;
    }
    *length = bytesReadTotal;
    return BTA_StatusOk;
}


static BTA_Status transmit(BTA_WrapperInst *winst, uint8_t *data, uint32_t length, uint32_t timeout) {
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst->inst;
    struct libusb_device_handle *usbHandle = inst->usbHandle;
    struct libusb_device *usbDevice = inst->usbDevice;
    BTA_InfoEventInst *infoEventInst = winst->infoEventInst;
    if (length == 0) {
        return BTA_StatusOk;
    }
    if (!usbHandle || !usbDevice || !data) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "USB transmit: Handle, device or data missing");
        return BTA_StatusInvalidParameter;
    }
    uint32_t bytesWrittenTotal = 0;
    while (bytesWrittenTotal < length) {
        int bytesWritten;
        int err = libusb_bulk_transfer(usbHandle, BTA_USB_EP_CONTROL_WRITE, (unsigned char *)data, length, &bytesWritten, timeout);
        if (err != 0) {
            // TODO error handling and possibly retry
            BTAinfoEventHelperS(infoEventInst, VERBOSE_ERROR, BTA_StatusDeviceUnreachable, "USB transmit error %s", (uint8_t *)libusb_error_name(err));
            // TODO close connection
            return BTA_StatusDeviceUnreachable;
        }
        bytesWrittenTotal += bytesWritten;
    }
    return BTA_StatusOk;
}

#endif
