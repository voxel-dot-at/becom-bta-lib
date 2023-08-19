#ifndef BTA_WO_USB

#ifndef BTA_USB_H_INCLUDED
#define BTA_USB_H_INCLUDED

#ifdef PLAT_WINDOWS
#   include "libusb.h"
#else
#   include "libusb-1.0/libusb.h"
#   include <time.h>
#endif


#include <bta.h>
#include <bta_helper.h>
#include <bta_discovery_helper.h>
#include <bta_oshelper.h>

#if !defined PLAT_WINDOWS && !defined PLAT_LINUX && !defined PLAT_APPLE
#   error "Please define PLAT_WINDOWS, PLAT_LINUX or PLAT_APPLE in your makefile/project"
#endif


#define BTA_USB_VID                  0x2398
#define BTA_USB_PID                  0x1002
#define BTA_USB_EP_DATA_STREAM       0x81
#define BTA_USB_EP_CONTROL_WRITE     0x01
#define BTA_USB_EP_CONTROL_READ      0x82



typedef struct BTA_UsbLibInst {
    uint8_t closing;

    uint8_t *pon;
    uint32_t serialNumber;

    libusb_context *context;
    libusb_device_handle *usbHandle;
    int interfaceNumber;

    void *dataMutex;
    void *controlMutex;
    void *semConnectionEstablishment;

    void *readFramesThread;
    void *connectionMonitorThread;

    uint64_t keepAliveMsgTimestamp;

    uint8_t lpControlCrcEnabled;
    float lpKeepAliveMsgInterval;
} BTA_UsbLibInst;


void *BTAUSBdiscoveryRunFunction(BTA_DiscoveryInst *inst);
BTA_Status BTAUSBopen(BTA_Config *config, BTA_WrapperInst *winst);
BTA_Status BTAUSBclose(BTA_WrapperInst *winst);
BTA_Status BTAUSBgetDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo);
BTA_Status BTAUSBgetDeviceType(BTA_WrapperInst *winst, BTA_DeviceType *deviceType);
uint8_t BTAUSBisRunning(BTA_WrapperInst *winst);
uint8_t BTAUSBisConnected(BTA_WrapperInst *winst);
BTA_Status BTAUSBsetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode frameMode);
BTA_Status BTAUSBgetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode *frameMode);
BTA_Status BTAUSBsetIntegrationTime(BTA_WrapperInst *winst, uint32_t integrationTime);
BTA_Status BTAUSBgetIntegrationTime(BTA_WrapperInst *winst, uint32_t *integrationTime);
BTA_Status BTAUSBsetFrameRate(BTA_WrapperInst *winst, float frameRate);
BTA_Status BTAUSBgetFrameRate(BTA_WrapperInst *winst, float *frameRate);
BTA_Status BTAUSBsetModulationFrequency(BTA_WrapperInst *winst, uint32_t modulationFrequency);
BTA_Status BTAUSBgetModulationFrequency(BTA_WrapperInst *winst, uint32_t *modulationFrequency);
BTA_Status BTAUSBsetGlobalOffset(BTA_WrapperInst *winst, float globalOffset);
BTA_Status BTAUSBgetGlobalOffset(BTA_WrapperInst *winst, float *globalOffset);
BTA_Status BTAUSBreadRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTAUSBwriteRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTAUSBsetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float value);
BTA_Status BTAUSBgetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float *value);
BTA_Status BTAUSBsendReset(BTA_WrapperInst *winst);
BTA_Status BTAUSBflashUpdate(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport);
BTA_Status BTAUSBflashRead(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport, uint8_t quiet);
BTA_Status BTAUSBwriteCurrentConfigToNvm(BTA_WrapperInst *winst);
BTA_Status BTAUSBrestoreDefaultConfig(BTA_WrapperInst *winst);





#endif

#endif