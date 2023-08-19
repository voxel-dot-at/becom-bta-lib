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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <crc16.h>
#include <crc32.h>


//////////////////////////////////////////////////////////////////////////////////
// Local prototypes

static void *readFramesRunFunction(void *handle);
static void *connectionMonitorRunFunction(void *handle);
static BTA_Status sendKeepAliveMsg(BTA_WrapperInst *inst);

static BTA_Status readRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount, uint32_t timeout);

static BTA_Status receiveControlResponse(BTA_UsbLibInst *inst, uint8_t *request, uint8_t **data, uint32_t *dataLen, uint32_t timeout, FN_BTA_ProgressReport progressReport, BTA_InfoEventInst *infoEventInst);
static BTA_Status transmit(BTA_UsbLibInst *inst, uint8_t *data, uint32_t length, uint32_t timeout, BTA_InfoEventInst *infoEventInst);
static BTA_Status receive(BTA_UsbLibInst *inst, uint8_t *data, uint32_t *length, uint32_t timeout, BTA_InfoEventInst *infoEventInst);


static const uint32_t timeoutTiny = 250;
static const uint32_t timeoutDefault = 4000;
static const uint32_t timeoutDouble = 2 * timeoutDefault;
static const uint32_t timeoutBig = 30000;
static const uint32_t timeoutHuge = 120000;



BTA_Status BTAUSBopen(BTA_Config *config, BTA_WrapperInst *winst) {
    if (!config || !winst) {
        return BTA_StatusInvalidParameter;
    }

    winst->inst = calloc(1, sizeof(BTA_UsbLibInst));
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusOutOfMemory;
    }

    // Initialize LibParams that are not defaultly 0
    inst->lpKeepAliveMsgInterval = 2;

    BTA_Status status;

    if (config->pon) {
        inst->pon = (uint8_t *)calloc(1, strlen((char *)config->pon) + 1);
        if (!inst->pon) {
            free(inst);
            return BTA_StatusOutOfMemory;
        }
        strcpy((char *)inst->pon, (char *)config->pon);
    }
    inst->serialNumber = config->serialNumber;

    status = BTAinitMutex(&inst->dataMutex);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen USB: cannot init handleMutex");
        BTAUSBclose(winst);
        return status;
    }
    status = BTAinitMutex(&inst->controlMutex);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen USB: cannot init controlMutex");
        BTAUSBclose(winst);
        return status;
    }

    status = BTAinitSemaphore(&inst->semConnectionEstablishment, 0, 0);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen USB: Cannot not init semaphore!");
        BTAUSBclose(winst);
        return status;
    }

    // Start connection monitor thread
    status = BTAcreateThread(&(inst->connectionMonitorThread), &connectionMonitorRunFunction, (void *)winst);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen USB: Could not start connectionMonitorThread");
        BTAUSBclose(winst);
        return status;
    }

    // Wait for connections to establish
    BTAwaitSemaphore(inst->semConnectionEstablishment);
    // lock one mutex when reading usbHandle
    BTAlockMutex(inst->controlMutex);
    uint8_t ok = inst->usbHandle > 0;
    BTAunlockMutex(inst->controlMutex);
    if (!ok) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusDeviceUnreachable, "BTAopen USB: timeout connecting to the device, see log");
        BTAUSBclose(winst);
        return BTA_StatusDeviceUnreachable;
    }

    status = BTAcreateThread(&(inst->readFramesThread), &readFramesRunFunction, (void *)winst);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen USB: Could not start readFramesThread");
        BTAUSBclose(winst);
        return status;
    }

    return BTA_StatusOk;
}


BTA_Status BTAUSBclose(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst->inst;
    if (!inst) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAclose USB: inst missing!");
        return BTA_StatusInvalidParameter;
    }
    inst->closing = 1;

    BTA_Status status;
    status = BTAjoinThread(inst->readFramesThread);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose USB: Failed to join readFramesThread");
    }

    status = BTAjoinThread(inst->connectionMonitorThread);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose USB: Failed to join connectionMonitorThread");
    }

    status = BTAcloseMutex(inst->controlMutex);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose USB: Failed to close controlMutex");
    }
    status = BTAcloseMutex(inst->dataMutex);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose USB: Failed to close handleMutex");
    }
    free(inst->pon);
    free(inst);
    winst->inst = 0;
    return BTA_StatusOk;
}


static void *connectionMonitorRunFunction(void *handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return 0;
    }
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst->inst;

    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "ConnectionMonitorThread started");
    uint8_t filterIsOn = inst->pon || inst->serialNumber;

    int err = libusb_init(&inst->context);
    if (err < 0) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusRuntimeError, "BTAopen USB: cannot init usb library (%s)", libusb_error_name(err));
        return 0;
    }

    uint8_t firstCycle = 1;
    uint8_t keepAliveFails = 0;
    while (!inst->closing) {

        BTAlockMutex(inst->controlMutex);
        if (!inst->usbHandle) {
            winst->modFreqsReadFromDevice = 0;
            libusb_device **devs = 0;
            int cnt = (int)libusb_get_device_list(inst->context, &devs);
            if (cnt < 0) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusDeviceUnreachable, "USB: cannot get usb devices (%d)", cnt);
                BTAmsleep(500);
            }
            else if (cnt == 0) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusDeviceUnreachable, "USB: No usb device found");
            }
            else {
                //BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInformation, "USB: Found %d USB devices...", handlesCount);
                for (int i = 0; i < cnt && devs[i]; i++) {
                    if (inst->closing) {
                        // connection established or aborted
                        break;
                    }
                    libusb_device *dev = devs[i];

                    struct libusb_device_descriptor desc;
                    err = libusb_get_device_descriptor(dev, &desc);
                    if (err < 0 || desc.idVendor != BTA_USB_VID || desc.idProduct != BTA_USB_PID) {
                        continue;
                    }
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusInformation, "USB: found a device (serial %d)", desc.iSerialNumber);
                    libusb_device_handle *usbHandle;
                    err = libusb_open(dev, &usbHandle);
                    if (err < 0) {
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusWarning, "USB: cannot open usb device, error: (%s)", libusb_error_name(err));
                    }
                    else {
                        if (desc.bNumConfigurations == 0) {
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusWarning, "USB: no configurations for this device");
                        }
                        else {
                            struct libusb_config_descriptor *conf_desc;
                            err = libusb_get_config_descriptor(dev, 0, &conf_desc);
                            if (err < 0) {
                                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusWarning, "USB: cannot get config descriptor, error: %s", libusb_error_name(err));
                            }
                            else {
                                // this acts like a light-weight reset
                                err = libusb_set_configuration(usbHandle, conf_desc->bConfigurationValue);
                                if (err < 0) {
                                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusWarning, "USB: cannot set configuration, error: %s", libusb_error_name(err));
                                }
                                else {
                                    int interfaceNumber = conf_desc->interface->altsetting[0].bInterfaceNumber;
                                    err = libusb_claim_interface(usbHandle, interfaceNumber);
                                    if (err < 0) {
                                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusWarning, "USB: cannot claim interface, error: %s", libusb_error_name(err));
                                    }
                                    else {

#                                       ifndef PLAT_WINDOWS
                                        //### DOES NOT WORK ON WINDOWS!!! ###
                                        err = libusb_reset_device(usbHandle);
                                        if (err < 0) {
                                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusWarning, "USB: libusb_reset failed %d", err);
                                        }
#                                       endif

                                        uint8_t match = 0;
                                        if (!filterIsOn) {
                                            match = 1;
                                        }
                                        else {
                                            BTA_WrapperInst *winstTemp = (BTA_WrapperInst *)calloc(1, sizeof(BTA_WrapperInst));
                                            if (winstTemp) {
                                                winstTemp->getDeviceInfo = BTAUSBgetDeviceInfo;
                                                BTA_UsbLibInst *instTemp = (BTA_UsbLibInst *)calloc(1, sizeof(BTA_UsbLibInst));
                                                if (instTemp) {
                                                    instTemp->usbHandle = usbHandle;
                                                    instTemp->interfaceNumber = interfaceNumber;
                                                    winstTemp->inst = instTemp;
                                                    BTA_DeviceInfo *deviceInfo;
                                                    BTA_Status status = BTAgetDeviceInfo(winstTemp, &deviceInfo);
                                                    if (status == BTA_StatusOk) {
                                                        uint8_t matchTemp = 1;
                                                        if (inst->pon) {
                                                            if (!deviceInfo->productOrderNumber) {
                                                                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInformation, "USB: Won't connect to device %d because PON cannot be read", i + 1);
                                                                matchTemp = 0;
                                                            }
                                                            else if (strcmp((char *)deviceInfo->productOrderNumber, (char *)inst->pon)) {
                                                                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInformation, "USB: Won't connect to device %d because PON does not match", i + 1);
                                                                matchTemp = 0;
                                                            }
                                                        }
                                                        if (matchTemp && inst->serialNumber) {
                                                            if (deviceInfo->serialNumber != inst->serialNumber) {
                                                                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInformation, "USB: Won't connect to device %d because serial number does not match", i + 1);
                                                                matchTemp = 0;
                                                            }
                                                        }
                                                        if (matchTemp) {
                                                            match = 1;
                                                        }

                                                        BTAfreeDeviceInfo(deviceInfo);
                                                    }
                                                    free(instTemp);
                                                }
                                                free(winstTemp);
                                            }
                                        }

                                        if (match) {
                                            BTAlockMutex(inst->dataMutex);
                                            inst->usbHandle = usbHandle;
                                            inst->interfaceNumber = interfaceNumber;
                                            BTAunlockMutex(inst->dataMutex);
                                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "USB: Connection established");
                                            break;
                                        }

                                        err = libusb_release_interface(usbHandle, interfaceNumber);
                                        if (err < 0) {
                                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "USB: Failed to release usb handle, error: %s", libusb_error_name(err));
                                        }
                                    }
                                }
                            }
                        }

                        libusb_close(usbHandle);
                    }
                }
            }
            libusb_free_device_list(devs, 1);
        }
        BTAunlockMutex(inst->controlMutex);

        if (inst->closing) {
            break;
        }

        // Keep-alive message
        if (inst->lpKeepAliveMsgInterval > 0 && BTAgetTickCount64() > inst->keepAliveMsgTimestamp) {
            BTAlockMutex(inst->controlMutex);
            if (inst->usbHandle) {
                BTA_Status status = sendKeepAliveMsg(winst);
                BTAunlockMutex(inst->controlMutex);
                if (status == BTA_StatusOk) {
                    keepAliveFails = 0;
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_READ_OP, BTA_StatusAlive, "Alive");
                }
                else {
                    keepAliveFails++;
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "Failed to send keep-alive-message, error: %d. ", status);
                    if (keepAliveFails >= 2) {
                        // the keepalive message failed, but the usb connection is still up (protocol version-, crc- or such error). trigger reconnect
                        BTAlockMutex(inst->controlMutex);
                        BTAlockMutex(inst->dataMutex);
                        err = libusb_release_interface(inst->usbHandle, inst->interfaceNumber);
                        if (err < 0) {
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "USB: Failed to release usb handle, error: %s", libusb_error_name(err));
                        }
                        libusb_close(inst->usbHandle);
                        inst->usbHandle = 0;
                        BTAunlockMutex(inst->dataMutex);
                        BTAunlockMutex(inst->controlMutex);
                    }
                }
            }
            else {
                BTAunlockMutex(inst->controlMutex);
            }
        }

        if (firstCycle) {
            BTApostSemaphore(inst->semConnectionEstablishment);
            firstCycle = 0;
        }

        BTAmsleep(250);
    }
    if (firstCycle) {
        BTApostSemaphore(inst->semConnectionEstablishment);
        firstCycle = 0;
    }


    // Thread was asked to stop
    BTAlockMutex(inst->controlMutex);
    BTAlockMutex(inst->dataMutex);
    if (inst->usbHandle) {
        err = libusb_release_interface(inst->usbHandle, inst->interfaceNumber);
        if (err < 0) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "USB: Failed to release usb handle, error: %s", libusb_error_name(err));
        }
        libusb_close(inst->usbHandle);
        inst->usbHandle = 0;
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "USB: Connection closed");
    }
    BTAunlockMutex(inst->dataMutex);
    BTAunlockMutex(inst->controlMutex);

    libusb_exit(inst->context);

    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "ConnectionMonitorThread terminated");
    return 0;
}


BTA_Status BTAUSBgetDeviceType(BTA_WrapperInst *winst, BTA_DeviceType *deviceType) {
    if (!winst || !deviceType) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t dt;
    BTA_Status status = BTAUSBreadRegister(winst, 6, &dt, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    *deviceType = (BTA_DeviceType)dt;
    return BTA_StatusOk;
}


BTA_Status BTAUSBgetDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo) {
    if (!winst || !deviceInfo) {
        return BTA_StatusInvalidParameter;
    }
    *deviceInfo = 0;
    BTA_Status status;

    BTA_DeviceInfo *info = (BTA_DeviceInfo *)calloc(1, sizeof(BTA_DeviceInfo));
    if (!info) {
        return BTA_StatusOutOfMemory;
    }

    uint32_t data[50];
    uint32_t dataAddress = 0x0001;
    uint32_t dataCount = 8;
    status = BTAUSBreadRegister(winst, dataAddress, data, &dataCount);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->mode0 = data[0x0001 - dataAddress];
    info->status = data[0x0003 - dataAddress];

    info->deviceType = (BTA_DeviceType)data[0x0006 - dataAddress];
    if (info->deviceType == 0) {
        info->deviceType = BTA_DeviceTypeUsb;
    }

    info->firmwareVersionMajor = (data[0x0008 - dataAddress] & 0xf800) >> 11;
    info->firmwareVersionMinor = (data[0x0008 - dataAddress] & 0x07c0) >> 6;
    info->firmwareVersionNonFunc = (data[0x0008 - dataAddress] & 0x003f);

    dataAddress = 0x000c;
    dataCount = 2;
    status = BTAUSBreadRegister(winst, dataAddress, data, &dataCount);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->serialNumber = data[0x000c - dataAddress];
    info->serialNumber |= data[0x000d - dataAddress] << 16;

    dataAddress = 0x0040;
    dataCount = 2;
    status = BTAUSBreadRegister(winst, dataAddress, data, &dataCount);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->uptime = data[0x0040 - dataAddress];
    info->uptime |= data[0x0041 - dataAddress] << 16;

    dataAddress = 0x0570;
    dataCount = 3;
    status = BTAUSBreadRegister(winst, dataAddress, data, &dataCount);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
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
    if (!winst) {
        return 0;
    }
    BTA_UsbLibInst *inst = (BTA_UsbLibInst *)winst->inst;
    if (!inst) {
        return 0;
    }
    return inst->usbHandle > 0;
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
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAsetFrameMode: frameMode not supported: %s (%d)", BTAframeModeToString(frameMode), frameMode);
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
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAsetFrameMode: frameMode not supported: %s (%d), device silently refused", BTAframeModeToString(frameMode), frameMode);
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


/*  @brief Simply requests and gets an alive msg
*   @pre lock the control MUTEX             */
static BTA_Status sendKeepAliveMsg(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_UsbLibInst * inst = (BTA_UsbLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }

    // The keepAliveMsg is the first message to test the connection, so we have to decide, if we add UDP callback information.
    // Once the connection is established, the connectionMonitorThread clears the callback information from inst
    uint8_t *sendBuffer = 0;
    uint32_t sendBufferLen = 0;
    BTA_Status status = BTAtoByteStream(BTA_EthCommandKeepAliveMsg, BTA_EthSubCommandNone, 0, 0, 0, inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen, 0, 0, 0, 0, 0, 0, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    status = transmit(inst, sendBuffer, sendBufferLen, timeoutDouble, winst->infoEventInst);
    if (status != BTA_StatusOk) {
        free(sendBuffer);
        sendBuffer = 0;
        return status;
    }
    status = receiveControlResponse(inst, sendBuffer, 0, 0, timeoutDouble, 0, winst->infoEventInst);
    free(sendBuffer);
    sendBuffer = 0;
    return status;
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
    BTA_Status status = BTAtoByteStream(BTA_EthCommandReset, BTA_EthSubCommandNone, 0, 0, 0, inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen, 0, 0, 0, 0, 0, 0, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    BTAlockMutex(inst->controlMutex);
    status = transmit(inst, sendBuffer, sendBufferLen, timeoutDefault, winst->infoEventInst);
    if (status != BTA_StatusOk) {
        free(sendBuffer);
        sendBuffer = 0;
        BTAunlockMutex(inst->controlMutex);
        return status;
    }
    status = receiveControlResponse(inst, sendBuffer, 0, 0, timeoutDefault, 0, winst->infoEventInst);
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
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "ReadFramesThread: thread started");

    const uint8_t preamble3[4] = { 0xff, 0xff, 0x0, 0x3 };
    const uint8_t preamble4[4] = { 0xff, 0xff, 0x0, 0x4 };
    const int minTransferSize = 512;
    int lenBuffer = 640 * 480 * 2 * 4;  // Start with a somewhat reasonable size
    int lenDiscarded = 0;               // number of already processed bytes at the start of the buffer. Start parsing at buffer + lenDiscarded
    int lenCurrent = 0;                 // number of bytes of unprocessed data. At buffer + lenDiscarded there are lenCurrent bytes to be parsed
    int bytesToRead = 0;                // number of bytes additionally needed
    uint8_t *buffer = (uint8_t *)malloc(lenBuffer);
    uint64_t timeLastErrorMsg = BTAgetTickCount64();
    while (!inst->closing) {
        assert(lenBuffer >= lenDiscarded + lenCurrent);

        // If the buffer is empty we can use the beginnig of the buffer again
        if (lenCurrent == 0) {
            lenDiscarded = 0;
        }
        // Make sure we have 4 valid bytes that contain the preamble
        if (lenCurrent < 4) {
            bytesToRead = 4 - lenCurrent;
        }

        if (bytesToRead > 0) {
            // We need more data than we currently have -> read more bytes from interface
            
            // Always read at least minTransferSize
            bytesToRead = bytesToRead < minTransferSize ? minTransferSize : bytesToRead;

            if (lenBuffer < lenDiscarded + lenCurrent + bytesToRead) {
                // we don't have space at the end to read the needed data -> create new buffer with double the size
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "readFramesRunFunction: resizing buffer. %d bytes are not enough", lenBuffer);
                uint8_t *bufTemp = buffer;
                lenBuffer *= 2;
                buffer = (uint8_t *)malloc(lenBuffer);
                if (!buffer) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "readFramesRunFunction: can't allocate");
                    free(bufTemp);
                    bufTemp = 0;
                    return 0;
                }
                memcpy(buffer, bufTemp + lenDiscarded, lenCurrent);
                free(bufTemp);
                bufTemp = 0;
                lenDiscarded = 0;
            }

            // we have enough space in buffer and will now read from usb
            BTAlockMutex(inst->dataMutex);
            if (!inst->usbHandle) {
                BTAunlockMutex(inst->dataMutex);
                if (BTAgetTickCount64() > timeLastErrorMsg + 7000) {
                    timeLastErrorMsg = BTAgetTickCount64();
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ReadFramesThread: no connection");
                }
                BTAmsleep(2000);
                continue;
            }
            int bytesRead = 0;
            int err = libusb_bulk_transfer(inst->usbHandle, BTA_USB_EP_DATA_STREAM, (unsigned char *)buffer + lenDiscarded + lenCurrent, bytesToRead, &bytesRead, timeoutTiny);
            BTAunlockMutex(inst->dataMutex);
            if (err < 0 || bytesRead <= 0) {
                if (BTAgetTickCount64() > timeLastErrorMsg + 7000) {
                    timeLastErrorMsg = BTAgetTickCount64();
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ReadFramesThread: failed to read: %s, bytes read %d/%d", libusb_error_name(err), bytesRead, bytesToRead);
                }
                if (err != LIBUSB_ERROR_TIMEOUT) {
                    winst->lpDataStreamReadFailedCount += 1;
                    // dataReceiveTimeout does not always apply, so wait manually before retrying
                    BTAmsleep(2000);
                }
                continue;
            }
            lenCurrent += bytesRead;
            winst->lpDataStreamBytesReceivedCount += bytesRead;
            bytesToRead = 0;
        }

        // bufferStart marks the beginning of data to be processed and it is lenCurrent bytes long
        uint8_t *bufferStart = &buffer[lenDiscarded];
                
        if (memcmp(bufferStart, preamble3, 4) == 0) {
            // found v3
            // Make sure we have 64 valid bytes that contain the header (preamble is included in 64byte header)
            if (lenCurrent < BTA_ETH_FRAME_DATA_HEADER_SIZE) {
                bytesToRead = BTA_ETH_FRAME_DATA_HEADER_SIZE - lenCurrent;
                continue;
            }
            // header is completely read, now parse
            int xRes = bufferStart[4] << 8 | bufferStart[5];
            int yRes = bufferStart[6] << 8 | bufferStart[7];
            //int nofChannels = bufferStart[8];   unreliable, unused!
            //int bytesPerPixel = bufferStart[9]; unreliable, unused!
            BTA_EthImgMode imgMode = (BTA_EthImgMode)((bufferStart[10] << 8 | bufferStart[11]) >> 3);
            int dataLen = xRes * yRes * BTAgetBytesPerPixelSum(imgMode);
            assert(dataLen);
            uint16_t crc16 = bufferStart[BTA_ETH_FRAME_DATA_HEADER_SIZE - 2] << 8 | bufferStart[BTA_ETH_FRAME_DATA_HEADER_SIZE - 1];
            uint16_t crc16calc = crc16_ccitt(bufferStart + 2, BTA_ETH_FRAME_DATA_HEADER_SIZE - 4);
            if (crc16 != crc16calc) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ReadFramesThread: v3 CRC16 of header mismatch");
                // The preamble was found, but header crc wrong -> find next preamble
                lenDiscarded += 4;
                lenCurrent -= 4;
                continue;
            }
            //BTAinfoEventHelper(winst->infoEventInst, VERBOSE_DEBUG, BTA_StatusInformation, "ReadFramesThread: v3: %dx%d %dbytes, imgMode %d\n", xRes, yRes, dataLen, imgMode);
            // We now know the complete length
            if (lenCurrent < BTA_ETH_FRAME_DATA_HEADER_SIZE + dataLen) {
                bytesToRead = BTA_ETH_FRAME_DATA_HEADER_SIZE + dataLen - lenCurrent;
                continue;
            }
            if (lenDiscarded > 0) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "ReadFramesThread: v3 parsing frame after %d discarded bytes", lenDiscarded);
            }
            // prepare frameToParse, as it is what parseFrame expects
            BTA_FrameToParse *frameToParse = (BTA_FrameToParse *)calloc(1, sizeof(BTA_FrameToParse));
            if (!frameToParse) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "ReadFramesThread: v3 BTAcreateFrameToParse: Could not create FrameToParse");
                return 0;
            }
            frameToParse->timestamp = (bufferStart[12] << 24) | (bufferStart[13] << 16) | (bufferStart[14] << 8) | bufferStart[15];
            frameToParse->frameCounter = (bufferStart[16] << 8) | bufferStart[17];
            frameToParse->frameSize = BTA_ETH_FRAME_DATA_HEADER_SIZE + dataLen;
            frameToParse->frame = bufferStart;
            BTAparsePostprocessGrabCallbackEnqueue(winst, frameToParse);
            free(frameToParse);
            lenDiscarded += BTA_ETH_FRAME_DATA_HEADER_SIZE + dataLen;
            lenCurrent -= BTA_ETH_FRAME_DATA_HEADER_SIZE + dataLen;
            if (lenCurrent > 0) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "ReadFramesThread v3: %d bytes left after parsed frame", lenCurrent);
            }
        }
        else if (memcmp(bufferStart, preamble4, 4) == 0) {
            // found v4
            // Make sure we have 6 valid bytes that contain the preamble + headerLength
            if (lenCurrent < 6) {
                bytesToRead = 6 - lenCurrent;
                continue;
            }
            uint16_t headerLength = bufferStart[4] | (bufferStart[5] << 8);
            if (lenCurrent < headerLength) {
                bytesToRead = headerLength - lenCurrent;
                continue;
            }
            // header is completely read

            uint16_t crc16 = bufferStart[headerLength - 2] | bufferStart[headerLength - 1] << 8;
            uint16_t crc16calc = crc16_ccitt(bufferStart + 2, headerLength - 4);
            if (crc16 != crc16calc) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ReadFramesThread v4: CRC16 of header mismatch");
                // The preamble was found, but header crc wrong -> find next preamble
                lenDiscarded += 4;
                lenCurrent -= 4;
                continue;
            }

            // go through descriptors and calculate dataLen
            int dataLen = 0;
            uint16_t frameCounter = 0;
            uint32_t timestamp = 0;
            uint8_t *dataHeaderTemp = bufferStart + 6;
            while (1) {
                BTA_Data4DescBase *descBase = (BTA_Data4DescBase *)dataHeaderTemp;
                if (descBase->descriptorType == btaData4DescriptorTypeFrameInfoV1) {
                    BTA_Data4DescFrameInfoV1 *frameInfo = (BTA_Data4DescFrameInfoV1 *)&descBase;
                    frameCounter = frameInfo->frameCounter;
                    timestamp = frameInfo->timestamp;
                }
                if (descBase->descriptorType == btaData4DescriptorTypeEof) {
                    dataHeaderTemp += 2;
                    break;
                }
                dataLen += descBase->dataLen;
                dataHeaderTemp += descBase->descriptorLen;
            }

            // we now know the complete length
            if (lenCurrent < headerLength + dataLen) {
                bytesToRead = headerLength + dataLen - lenCurrent;
                continue;
            }

            // prepare frameToParse, as it is what parseFrame expects
            if (lenDiscarded > 0) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "ReadFramesThread v4: parsing frame after %d discarded bytes", lenDiscarded);
            }
            BTA_FrameToParse *frameToParse = (BTA_FrameToParse *)calloc(1, sizeof(BTA_FrameToParse));
            if (!frameToParse) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "ReadFramesThread v4: BTAcreateFrameToParse: Could not create FrameToParse");
                return 0;
            }
            frameToParse->timestamp = timestamp;
            frameToParse->frameCounter = frameCounter;
            frameToParse->frameSize = headerLength + dataLen;
            frameToParse->frame = bufferStart;
            BTAparsePostprocessGrabCallbackEnqueue(winst, frameToParse);
            free(frameToParse);
            lenDiscarded += BTA_ETH_FRAME_DATA_HEADER_SIZE + dataLen;
            lenCurrent -= BTA_ETH_FRAME_DATA_HEADER_SIZE + dataLen;
            if (lenCurrent > 0) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "ReadFramesThread v4: %d bytes left after parsed frame", lenCurrent);
            }
        }
        else {
            // no preamble found, advance in buffer
            lenDiscarded++;
            lenCurrent--;
        }
    }
    free(buffer);
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "ReadFramesThread terminated");
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
    uint32_t modFreq;
    status = BTAgetNextBestModulationFrequency(winst, modulationFrequency, &modFreq, 0);
    if (status != BTA_StatusOk) {
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
    int32_t modFreqIndex;
    status = BTAgetNextBestModulationFrequency(winst, modFreq, 0, &modFreqIndex);
    if (status != BTA_StatusOk) {
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
    int32_t modFreqIndex;
    status = BTAgetNextBestModulationFrequency(winst, modFreq, 0, &modFreqIndex);
    if (status != BTA_StatusOk) {
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
    uint64_t endTime = BTAgetTickCount64() + timeoutBig;
    do {
        BTAmsleep(500);
        uint32_t result = 0;
        status = readRegister(winst, 0x0034, &result, 0, timeoutBig);
        if (status == BTA_StatusOk) {
            if (result != 1) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAwriteCurrentConfigToNvm() failed, the device said %d", result);
                return BTA_StatusRuntimeError;
            }
            return BTA_StatusOk;
        }
    } while (BTAgetTickCount64() < endTime);
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
    uint64_t endTime = BTAgetTickCount64() + timeoutBig;
    do {
        BTAmsleep(500);
        uint32_t result = 0;
        status = readRegister(winst, 0x0034, &result, 0, timeoutBig);
        if (status == BTA_StatusOk) {
            if (result != 1) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTArestoreDefaultConfig() failed, the device said %d", result);
                return BTA_StatusRuntimeError;
            }
            return BTA_StatusOk;
        }
    } while (BTAgetTickCount64() < endTime);
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
    uint8_t *dataBuffer;
    uint32_t dataBufferLen = 0;
    BTAlockMutex(inst->controlMutex);
    status = transmit(inst, sendBuffer, sendBufferLen, timeout, winst->infoEventInst);
    if (status != BTA_StatusOk) {
        free(sendBuffer);
        sendBuffer = 0;
        BTAunlockMutex(inst->controlMutex);
        return status;
    }
    status = receiveControlResponse(inst, sendBuffer, &dataBuffer, &dataBufferLen, timeout, 0, winst->infoEventInst);
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
    status = transmit(inst, sendBuffer, BTA_ETH_HEADER_SIZE, timeoutDefault, winst->infoEventInst);
    if (status != BTA_StatusOk) {
        free(sendBuffer);
        sendBuffer = 0;
        BTAunlockMutex(inst->controlMutex);
        return status;
    }
    status = transmit(inst, sendBuffer + BTA_ETH_HEADER_SIZE, sendBufferLen - BTA_ETH_HEADER_SIZE, timeoutDefault, winst->infoEventInst);
    if (status != BTA_StatusOk) {
        free(sendBuffer);
        sendBuffer = 0;
        BTAunlockMutex(inst->controlMutex);
        return status;
    }
    uint32_t dataLen = 0;
    status = receiveControlResponse(inst, sendBuffer, 0, &dataLen, timeoutDefault, 0, winst->infoEventInst);
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
    case BTA_LibParamKeepAliveMsgInterval:
        if (value < 0) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAsetLibParam: Interval must be >= 0");
            return BTA_StatusInvalidParameter;
        }
        inst->lpKeepAliveMsgInterval = value;
        return BTA_StatusOk;
    default:
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAsetLibParam: LibParam not supported %d", libParam);
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
    case BTA_LibParamKeepAliveMsgInterval:
        *value = inst->lpKeepAliveMsgInterval;
        return BTA_StatusOk;
    default:
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAgetLibParam: LibParam not supported");
        return BTA_StatusNotSupported;
    }
}



BTA_Status BTAUSBflashUpdate(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport) {
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
    BTAgetFlashCommand(flashUpdateConfig, &cmd, &subCmd);
    if (cmd == BTA_EthCommandNone) {
        if (progressReport) (*progressReport)(BTA_StatusInvalidParameter, 0);
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAflashUpdate: FlashTarget %d or FlashId %d not supported", flashUpdateConfig->target, flashUpdateConfig->flashId);
        return BTA_StatusInvalidParameter;
    }

    BTA_Status status;
    uint8_t *sendBuffer;
    uint32_t sendBufferLen;
    status = BTAtoByteStream(cmd, subCmd, flashUpdateConfig->address, flashUpdateConfig->data, flashUpdateConfig->dataLen, inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen, 0, 0, 0, 0, 0, 0, 0);
    if (status != BTA_StatusOk) {
        if (progressReport) (*progressReport)(status, 0);
        return status;
    }
    // first send the header alone (must with usb)
    if (progressReport) (*progressReport)(BTA_StatusOk, 0);
    BTAlockMutex(inst->controlMutex);
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: Transmitting header (64 bytes)");
    status = transmit(inst, sendBuffer, BTA_ETH_HEADER_SIZE, timeoutHuge, winst->infoEventInst);
    // calculate size of data without header and if data large, divide into 10 parts
    uint8_t *sendBufferPart = sendBuffer + BTA_ETH_HEADER_SIZE;
    uint32_t sendBufferPartLen = sendBufferLen > 1048576 ? 1048576 : (sendBufferLen - BTA_ETH_HEADER_SIZE);
    uint32_t sendBufferLenSent = BTA_ETH_HEADER_SIZE;
    do {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: Transmitting %d bytes of data (already sent %d bytes)", sendBufferLen - BTA_ETH_HEADER_SIZE, sendBufferLenSent - BTA_ETH_HEADER_SIZE);
        status = transmit(inst, sendBufferPart, sendBufferPartLen, timeoutHuge, winst->infoEventInst);
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
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: %d bytes sent", sendBufferLenSent);
    status = receiveControlResponse(inst, sendBuffer, 0, 0, timeoutHuge, 0, winst->infoEventInst);
    free(sendBuffer);
    sendBuffer = 0;
    BTAunlockMutex(inst->controlMutex);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAflashUpdate: Erroneous response! %d", status);
        if (progressReport) (*progressReport)(status, 0);
        return status;
    }

    // flash cmd successfully sent, now poll status
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: File transfer finished");

    uint64_t timeEnd = BTAgetTickCount64() + 5 * 60 * 1000; // 5 minutes timeout
    while (1) {
        BTAmsleep(1000);
        if (BTAgetTickCount64() > timeEnd) {
            if (progressReport) (*progressReport)(BTA_StatusTimeOut, 80);
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusTimeOut, "BTAflashUpdate: Timeout");
            return BTA_StatusTimeOut;
        }
        uint32_t fileUpdateStatus = 0;
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


BTA_Status BTAUSBflashRead(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport, uint8_t quiet) {
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
    BTAgetFlashCommand(flashUpdateConfig, &cmd, &subCmd);
    if (cmd == BTA_EthCommandNone) {
        if (progressReport) (*progressReport)(BTA_StatusInvalidParameter, 0);
        if (!quiet) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAflashRead: FlashTarget %d or FlashId %d not supported", flashUpdateConfig->target, flashUpdateConfig->flashId);
        return BTA_StatusInvalidParameter;
    }
    // change the flash command to a read command
    cmd = (BTA_EthCommand)((int)cmd + 100);

    uint8_t *sendBuffer;
    uint32_t sendBufferLen;
    BTA_Status status = BTAtoByteStream(cmd, subCmd, flashUpdateConfig->address, 0, 0, inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen, 0, 0, 0, 0, 0, 0, 0);
    if (status != BTA_StatusOk) {
        if (progressReport) (*progressReport)(status, 0);
        return status;
    }

    BTAlockMutex(inst->controlMutex);
    status = transmit(inst, sendBuffer, sendBufferLen, timeoutDouble, winst->infoEventInst);
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
    if (quiet) status = receiveControlResponse(inst, sendBuffer, &dataBuffer, &dataBufferLen, timeoutHuge, progressReport, 0);
    else status = receiveControlResponse(inst, sendBuffer, &dataBuffer, &dataBufferLen, timeoutHuge, progressReport, winst->infoEventInst);
    BTAunlockMutex(inst->controlMutex);
    free(sendBuffer);
    sendBuffer = 0;
    if (status != BTA_StatusOk) {
        if (progressReport) (*progressReport)(status, 0);
        return status;
    }
    if (!quiet) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashRead: Transmission successful");
    if (!flashUpdateConfig->dataLen) {
        flashUpdateConfig->data = (uint8_t *)malloc(dataBufferLen);
        if (!flashUpdateConfig->data) {
            free(dataBuffer);
            dataBuffer = 0;
            if (progressReport) (*progressReport)(BTA_StatusOutOfMemory, 0);
            if (!quiet) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "BTAflashRead: Out of memory!");
            return BTA_StatusOutOfMemory;
        }
    }
    else if (dataBufferLen > flashUpdateConfig->dataLen) {
        free(dataBuffer);
        dataBuffer = 0;
        flashUpdateConfig->dataLen = 0;
        if (progressReport) (*progressReport)(BTA_StatusOutOfMemory, 0);
        if (!quiet) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "BTAflashRead: Provided buffer too small");
        return BTA_StatusOutOfMemory;
    }
    flashUpdateConfig->dataLen = dataBufferLen;
    memcpy(flashUpdateConfig->data, dataBuffer, dataBufferLen);
    if (progressReport) (*progressReport)(BTA_StatusOk, 100);
    return BTA_StatusOk;
}


static void *doDiscovery(void *handle) {
    BTA_DiscoveryInst *inst = (BTA_DiscoveryInst *)handle;
    libusb_context *context;
    int err = libusb_init(&context);
    if (err < 0) {
        BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Discovery: cannot init usb library, error: %d", libusb_error_name(err));
        return 0;
    }
    libusb_device **devs = 0;
    ssize_t cnt = libusb_get_device_list(context, &devs);
    if (cnt > 0) {
        for (int i = 0; i < cnt && devs[i]; i++) {
            libusb_device *dev = devs[i];
            struct libusb_device_descriptor desc;
            err = libusb_get_device_descriptor(dev, &desc);
            if (err < 0 || desc.idVendor != BTA_USB_VID || desc.idProduct != BTA_USB_PID) {
                continue;
            }
            libusb_device_handle *usbHandle;
            err = libusb_open(dev, &usbHandle);
            if (err >= 0) {
                if (desc.bNumConfigurations > 0) {
                    struct libusb_config_descriptor *conf_desc;
                    err = libusb_get_config_descriptor(dev, 0, &conf_desc);
                    if (err >= 0) {
                        // this acts like a light-weight reset
                        err = libusb_set_configuration(usbHandle, conf_desc->bConfigurationValue);
                        if (err >= 0) {
                            int interfaceNumber = conf_desc->interface->altsetting[0].bInterfaceNumber;
                            err = libusb_claim_interface(usbHandle, interfaceNumber);
                            if (err >= 0) {

#                               ifndef PLAT_WINDOWS
                                //### DOES NOT WORK ON WINDOWS!!! ###
                                libusb_reset_device(usbHandle);
#                               endif
                                BTA_WrapperInst *winstTemp = (BTA_WrapperInst *)calloc(1, sizeof(BTA_WrapperInst));
                                if (winstTemp) {
                                    winstTemp->getDeviceInfo = BTAUSBgetDeviceInfo;
                                    BTA_UsbLibInst *instTemp = (BTA_UsbLibInst *)calloc(1, sizeof(BTA_UsbLibInst));
                                    if (instTemp) {
                                        instTemp->usbHandle = usbHandle;
                                        instTemp->interfaceNumber = interfaceNumber;
                                        winstTemp->inst = instTemp;
                                        BTA_DeviceInfo *deviceInfo;
                                        BTA_Status status = BTAgetDeviceInfo(winstTemp, &deviceInfo);
                                        if (status == BTA_StatusOk) {
                                            if (BTAaddToDiscoveredList(inst->deviceListMutex, inst->deviceList, &inst->deviceListCount, inst->deviceListCountMax, deviceInfo)) {
                                                if (inst->deviceFound) {
                                                    (*inst->deviceFound)(inst, deviceInfo);
                                                }
                                                if (inst->deviceFoundEx) {
                                                    (*inst->deviceFoundEx)(inst, deviceInfo, inst->userArg);
                                                }
                                            }
                                            else {
                                                BTAfreeDeviceInfo(deviceInfo);
                                            }
                                        }
                                        else {
                                            BTAinfoEventHelper(inst->infoEventInst, VERBOSE_WARNING, status, "Discovery: BTAgetDeviceInfo failed!");
                                        }
                                        free(instTemp);
                                    }
                                    free(winstTemp);
                                }

                                err = libusb_release_interface(usbHandle, interfaceNumber);
                                if (err < 0) {
                                    BTAinfoEventHelper(inst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "Discovery: Failed to release usb handle, error: %s", libusb_error_name(err));
                                }
                            }
                        }
                    }
                    libusb_close(usbHandle);
                }
            }
        }
    }
    libusb_free_device_list(devs, 1);
    libusb_exit(context);
    return 0;
}


void *BTAUSBdiscoveryRunFunction(BTA_DiscoveryInst *inst) {
    while (!inst->abortDiscovery) {

        doDiscovery(inst);

        if (!inst->millisInterval) {
            // Only discover once. We are finished here
            return 0;
        }

        // Wait given interval, then loop
        for (uint32_t i = 0; i < inst->millisInterval / 10 && !inst->abortDiscovery; i++) {
            BTAmsleep(100);
        }
    }
    return 0;
}


static BTA_Status receiveControlResponse(BTA_UsbLibInst *inst, uint8_t *request, uint8_t **data, uint32_t *dataLen, uint32_t timeout, FN_BTA_ProgressReport progressReport, BTA_InfoEventInst *infoEventInst) {
    uint8_t header[BTA_ETH_HEADER_SIZE];
    uint32_t headerLen;
    uint32_t flags;
    uint32_t dataCrc32;
    BTA_Status status;
    if (data) {
        *data = 0;
    }

    headerLen = sizeof(header);
    status = receive(inst, header, &headerLen, timeout, infoEventInst);
    if (status != BTA_StatusOk) {
        return status;
    }
    uint32_t dataLenTemp;
    uint8_t parseError;
    status = BTAparseControlHeader(request, header, &dataLenTemp, &flags, &dataCrc32, &parseError, infoEventInst);
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
        uint32_t dataReceivedLen = 0;
        uint32_t dataPartLen = *dataLen > 1048576 ? 1048576 : *dataLen;
        uint32_t dataPartLenTemp;
        if (progressReport) (*progressReport)(BTA_StatusOk, 0);
        do {
            dataPartLenTemp = dataPartLen;
            status = receive(inst, *data + dataReceivedLen, &dataPartLenTemp, timeout, infoEventInst);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, status, "USB receiveControlResponse: Failed! %d", dataPartLenTemp);
            }
            if (status == BTA_StatusOk) {
                dataReceivedLen += dataPartLen;
                if (dataReceivedLen + dataPartLen > *dataLen) {
                    dataPartLen = *dataLen - dataReceivedLen;
                }
                // report progress (use 90 of the 100% for the transmission
                if (progressReport) (*progressReport)(status, (uint8_t)(90.0 * dataReceivedLen / *dataLen));
            }
        } while (dataReceivedLen < *dataLen && status == BTA_StatusOk);
        if (status != BTA_StatusOk) {
            free(*data);
            *data = 0;
            if (progressReport) (*progressReport)(status, 0);
            return status;
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


static BTA_Status receive(BTA_UsbLibInst *inst, uint8_t *data, uint32_t *length, uint32_t timeout, BTA_InfoEventInst *infoEventInst) {
    if (!inst->usbHandle) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusNotConnected, "Not connected");
        return BTA_StatusNotConnected;
    }
    uint32_t bytesReadTotal = 0;
    while (bytesReadTotal < *length) {
        int bytesRead;
        int err = libusb_bulk_transfer(inst->usbHandle, BTA_USB_EP_CONTROL_READ, (unsigned char *)data, *length, &bytesRead, timeout);
        if (err != 0) {
            // TODO error handling and possibly retry
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusDeviceUnreachable, "USB receive error %s", libusb_error_name(err));
            // lock both mutexes when writing usbHandle
            BTAlockMutex(inst->dataMutex);
            err = libusb_release_interface(inst->usbHandle, inst->interfaceNumber);
            if (err < 0) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "USB: Failed to release usb handle, error: %s", libusb_error_name(err));
            }
            libusb_close(inst->usbHandle);
            inst->usbHandle = 0;
            BTAunlockMutex(inst->dataMutex);
            BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "USB: Connection lost!");
            return BTA_StatusDeviceUnreachable;
        }
        bytesReadTotal += bytesRead;
    }
    *length = bytesReadTotal;
    if (inst->lpKeepAliveMsgInterval > 0) {
        inst->keepAliveMsgTimestamp = BTAgetTickCount64() + (int)(1000 * inst->lpKeepAliveMsgInterval);
    }
    return BTA_StatusOk;
}


static BTA_Status transmit(BTA_UsbLibInst *inst, uint8_t *data, uint32_t length, uint32_t timeout, BTA_InfoEventInst *infoEventInst) {
    if (!inst->usbHandle) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusNotConnected, "Not connected");
        return BTA_StatusNotConnected;
    }
    if (!data) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "USB transmit: Handle, device or data missing");
        return BTA_StatusInvalidParameter;
    }
    uint32_t bytesWrittenTotal = 0;
    while (bytesWrittenTotal < length) {
        int bytesWritten;
        int err = libusb_bulk_transfer(inst->usbHandle, BTA_USB_EP_CONTROL_WRITE, (unsigned char *)data, length, &bytesWritten, timeout);
        if (err != 0) {
            // TODO error handling and possibly retry
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusDeviceUnreachable, "USB transmit error %s", libusb_error_name(err));
            // lock both mutexes when writing usbHandle
            BTAlockMutex(inst->dataMutex);
            err = libusb_release_interface(inst->usbHandle, inst->interfaceNumber);
            if (err < 0) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "USB: Failed to release usb handle, error: %s", libusb_error_name(err));
            }
            libusb_close(inst->usbHandle);
            inst->usbHandle = 0;
            BTAunlockMutex(inst->dataMutex);
            BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "USB: Connection lost!");
            return BTA_StatusDeviceUnreachable;
        }
        bytesWrittenTotal += bytesWritten;
    }
    return BTA_StatusOk;
}

#endif