#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <bta.h>
#include <bta_helper.h>
#include <bta_oshelper.h>
#include <timing_helper.h>
#include <pthread_helper.h>
#include "bta_p100.h"
#include "bta_p100_helper.h"
#include "configuration.h"



////////// Local prototypes
static void *captureRunFunction(void *handle);
static BTA_Status getFrameInstant(BTA_WrapperInst *winst, BTA_Frame **framePtr);


static uint32_t ponCodes[BTA_P100_DEVICE_TYPES_LEN] = { BTA_P100_PON_CODES };
static uint32_t deviceTypes[BTA_P100_DEVICE_TYPES_LEN] = { BTA_P100_DEVICE_TYPES };
static char const *productOrderNumbers[BTA_P100_DEVICE_TYPES_LEN] = { BTA_P100_PRODUCT_ORDER_NUMBERS };
static char const *productOrderNumberSuffixes[BTA_P100_DEVICE_TYPES_LEN] = { BTA_P100_PRODUCT_ORDER_NUMBER_SUFFIXES };
static BTA_Status restoreZFactors(BTA_WrapperInst *winst, const uint8_t *filename, float **zFactors, uint16_t *xRes, uint16_t *yRes);


BTA_Status BTAP100open(BTA_Config *config, BTA_WrapperInst *winst) {
    uint16_t *calibFileData;
    unsigned int cnt;
    unsigned int global_cnt;
    uint32_t regData;
    int x, y, i;
    int serialCode;
    BTA_Status status;

    if (!config || !winst) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen P100: config or handle missing");
        return BTA_StatusInvalidParameter;
    }

    winst->inst = calloc(1, sizeof(BTA_P100LibInst));
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusOutOfMemory;
    }
    inst->deviceIndex = -1;

    if (config->averageWindowLength > 12) {
        // averageWindowLength > 12 not allowed
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen P100: averageWindowLength too high. Set to its maximum of 12");
        config->averageWindowLength = 12;
    }

    if (!config->serialNumber && (!config->pon || config->pon[0] == 0)) {
        int result = openDevice(&inst->deviceIndex, 0, 0, OPEN_SERIAL_ANY);
        if (result != P100_OKAY) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusDeviceUnreachable, "BTAopen P100: No valid usb device found");
            BTAP100close(winst);
            return BTA_StatusDeviceUnreachable;
        }
    }
    else {
        serialCode = 0;
        if (config->pon) {
            for (i = 0; i < BTA_P100_DEVICE_TYPES_LEN; i++) {
                uint8_t *productOrderNumber = (uint8_t *)calloc(1, strlen(productOrderNumbers[i]) + strlen(productOrderNumberSuffixes[i]) + 1);
                if (!productOrderNumber) {
                    BTAP100close(winst);
                    return BTA_StatusOutOfMemory;
                }
                sprintf((char *)productOrderNumber, "%s%s", productOrderNumbers[i], productOrderNumberSuffixes[i]);
                if (strcmp((char *)config->pon, (char *)productOrderNumber) == 0) {
                    free(productOrderNumber);
                    productOrderNumber = 0;
                    break;
                }
                else {
                    free(productOrderNumber);
                    productOrderNumber = 0;
                }
            }
            if (i >= BTA_P100_DEVICE_TYPES_LEN) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen P100: Unrecognized PON");
                BTAP100close(winst);
                return BTA_StatusDeviceUnreachable;
            }
            serialCode = ponCodes[i];
        }

        int result = openDevice(&inst->deviceIndex, serialCode, config->serialNumber, OPEN_SERIAL_SPECIFIED);
        if (result != P100_OKAY) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen P100: P100 with specified PON and/or serial number not found");
            BTAP100close(winst);
            return BTA_StatusDeviceUnreachable;
        }
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInformation, "BTAopen P100: Connected");

    status = BTAinitMutex(&inst->handleMutex);
    if (status != BTA_StatusOk || !inst->handleMutex) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen P100: Cannot init handleMutex");
        BTAP100close(winst);
        return BTA_StatusRuntimeError;
    }

    if (config->frameMode == BTA_FrameModeCurrentConfig) {
        // defaults BTA_FrameModeDistAmp if not set by the user
        config->frameMode = BTA_FrameModeDistAmp;
    }
    status = BTAP100setFrameMode(winst, config->frameMode);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen P100: Failed at BTAsetFrameMode");
    }

    if (config->calibFileName && strlen((const char *)config->calibFileName) > 0) {
        inst->dx_cust_values = (float*)malloc(P100_WIDTH*P100_HEIGHT*sizeof(float));
        if (!inst->dx_cust_values) {
            BTAP100close(winst);
            return BTA_StatusOutOfMemory;
        }
        inst->dy_cust_values = (float*)malloc(P100_WIDTH*P100_HEIGHT*sizeof(float));
        if (!inst->dy_cust_values) {
            BTAP100close(winst);
            return BTA_StatusOutOfMemory;
        }
        inst->dz_cust_values = (float*)malloc(P100_WIDTH*P100_HEIGHT*sizeof(float));
        if (!inst->dz_cust_values) {
            BTAP100close(winst);
            return BTA_StatusOutOfMemory;
        }

        void *calibFP;
        status = BTAfopen((char *)config->calibFileName, "rb", &calibFP);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen P100: Could not open calibration file!");
            BTAP100close(winst);
            return BTA_StatusInvalidParameter;
        }

        //parse file and save to the three arrays above
        status = BTAfseek(calibFP, 0, BTA_SeekOriginEnd, 0);
        if (status != BTA_StatusOk) {
            BTAfclose(calibFP);
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen P100: Could not seek in calibration file!");
            BTAP100close(winst);
            return status;
        }
        uint32_t filesize;
        status = BTAftell(calibFP, &filesize);
        if (status != BTA_StatusOk) {
            BTAfclose(calibFP);
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen P100: Could not ftell calibration file!");
            BTAP100close(winst);
            return status;
        }
        status = BTAfseek(calibFP, 0, BTA_SeekOriginBeginning, 0);
        if (status != BTA_StatusOk) {
            BTAfclose(calibFP);
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen P100: Could not seek in calibration file!");
            BTAP100close(winst);
            return status;
        }
        calibFileData = (uint16_t *)malloc(filesize);
        if (!calibFileData) {
            BTAfclose(calibFP);
            BTAP100close(winst);
            return BTA_StatusOutOfMemory;
        }
        uint32_t fileSizeRead;
        status = BTAfread(calibFP, calibFileData, filesize, &fileSizeRead);
        if (status != BTA_StatusOk) {
            free(calibFileData);
            calibFileData = 0;
            BTAfclose(calibFP);
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen P100: Error reading calibration file %d", fileSizeRead);
            BTAP100close(winst);
            return status;
        }
        status = BTAfclose(calibFP);
        if (status != BTA_StatusOk) {
            free(calibFileData);
            calibFileData = 0;
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen P100: Error reading calibration file %d", fileSizeRead);
            BTAP100close(winst);
            return status;
        }
        //check header of calibFileData, transform, de-interleave and identify x,y,z
        if (!(calibFileData[0] == 0xF011 && calibFileData[1] == 0xB1AD) || calibFileData[2] != 1 || calibFileData[5] != P100_WIDTH || calibFileData[6] != P100_HEIGHT) {
            free(calibFileData);
            calibFileData = 0;
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen P100: Invalid calibration file");
            BTAP100close(winst);
            return BTA_StatusInvalidParameter;
        }

        //coordinate data starts at index 25
        global_cnt = 25;
        cnt = 0;
        for (y = 0; y < P100_HEIGHT; y++) {
            for (x = 0; x < P100_WIDTH; x++) {
                inst->dx_cust_values[cnt] = (float)((int16_t)calibFileData[global_cnt+0]) / (float)calibFileData[8];
                inst->dy_cust_values[cnt] = (float)((int16_t)calibFileData[global_cnt+1]) / (float)calibFileData[8];
                inst->dz_cust_values[cnt] = (float)((int16_t)calibFileData[global_cnt+2]) / (float)calibFileData[8];
                global_cnt += 3;
                cnt++;
            }
        }

        //assign in lower level
        int result2 = set3DCalibArrays(inst->deviceIndex, inst->dx_cust_values, inst->dy_cust_values, inst->dz_cust_values);
        if (result2 != P100_OKAY) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusRuntimeError, "BTAopen P100: Cannot set calibration data");
        }
        else {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInformation, "BTAopen P100: custom lens calibration file applied");
        }
        free(calibFileData);
        calibFileData = 0;
    }

    //-------Wiggling-------
    if (config->wigglingFileName && strlen((const char *)config->wigglingFileName) > 0) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "WARNING! BTAopen P100: parameter wigglingFilename no longer supported. Please use BTAwigglingUpdate()");
    }
    //----------------------

    status = BTAP100readRegister(winst, 0x38, &regData, 0);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen P100: Reading version failed");
        BTAP100close(winst);
        return status;
    }
    inst->version = regData;

    inst->imgProcConfig = 0; // bilateral filter off
    setBilateralStatus(inst->deviceIndex, (inst->imgProcConfig & 0x8) > 0);
    inst->filterBilateralConfig2 = 3; //3x3
    setBilateralWindow(inst->deviceIndex, inst->filterBilateralConfig2);

    if (config->zFactorsFileName && strlen((const char *)config->zFactorsFileName) > 0) {
        float* zFactors;
        uint16_t xRes, yRes;
        status = restoreZFactors(winst, config->zFactorsFileName, &zFactors, &xRes, &yRes);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen P100: Error zFactors not loaded");
            BTAP100close(winst);
            return status;
        }
        // TODO
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen P100: zFactors currently not supported");
        BTAP100close(winst);
        return BTA_StatusNotSupported;
    }

    #if DEVEL_DEBUG
    printf("BTAopen P100: device index %u - handle %lu\n", inst->deviceIndex, (BTA_Handle)inst);
    #endif



    if (config->frameQueueLength && (config->frameQueueMode == BTA_QueueModeDropCurrent || config->frameQueueMode == BTA_QueueModeDropOldest)) {
        status = BFQinit(config->frameQueueLength, config->frameQueueMode, &inst->frameQueueInst);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen P100: Error initializing frameQueue");
            BTAP100close(winst);
            return status;
        }
    }

    if (config->averageWindowLength > 0) {
        regData = (uint32_t)config->averageWindowLength;
        status = BTAP100writeRegister(winst, P100_REG_SEQ_LENGTH, &regData, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen P100: Error in BTAwriteRegister SequenceLength");
            BTAP100close(winst);
            return status;
        }

        if (config->averageWindowLength > 1) {
            status = BTAP100getIntegrationTime(winst, &regData);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen P100: Error in BTAgetIntegrationTime");
                BTAP100close(winst);
                return status;
            }
            // We want to make sure that all used sequences have the same integration time (of sequence 0)
            status = BTAP100setIntegrationTime(winst, regData);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen P100: Error in BTAsetIntegrationTime. Please make sure that all sequences have the desired integration time:");
            }

            status = BTAP100getModulationFrequency(winst, &regData);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen P100: Error in BTAgetModulationFrequency");
                BTAP100close(winst);
                return status;
            }
            // We want to make sure that all used sequences have the same modulation frequency time (of sequence 0)
            status = BTAP100setModulationFrequency(winst, regData);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen P100: Error in BTAsetModulationFrequency. Please make sure that all sequences have the desired modulation frequency:");
            }

            if (config->averageWindowLength > 1) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusNotSupported, "BTAopen P100: averageWindowLength currently not supported");
                BTAP100close(winst);
                return BTA_StatusNotSupported;
            }
        }
    }

    if ((winst->frameArrivedInst && (winst->frameArrivedInst->frameArrived || winst->frameArrivedInst->frameArrivedEx || winst->frameArrivedInst->frameArrivedEx2)) || inst->frameQueueInst) {
        // Proactive frame capturing is desired implicitly by the callback and/or queueing
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAopen P100: Starting capture thread. Polling BTAgetFrame()");
        status = BTAcreateThread(&(inst->captureThread), &captureRunFunction, (void *)winst, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen P100: Could not start captureThread");
            BTAP100close(winst);
            return status;
         }
    }

    return BTA_StatusOk;
}


//----------------------------------------------------------------------------


BTA_Status BTAP100close(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }

    BTA_Status status;
    inst->closing = 1;
    if (inst->handleMutex) {
        BTAlockMutex(inst->handleMutex);
        BTAunlockMutex(inst->handleMutex);
        status = BTAcloseMutex(inst->handleMutex);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAclose: Failed to close handleMutex");
            return status;
        }
    }

    #if DEVEL_DEBUG
    printf("BTAclose: device index %u - handle %lu\n",inst->deviceIndex, *handle);
    #endif

    if (inst->captureThread) {
        inst->abortCaptureThread = 1;
        status = BTAjoinThread(inst->captureThread);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAclose: Failed to join captureThread");
            return status;
        }
    }

    status = BGRBclose(&(inst->grabInst));
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAclose: Failed to close grabber");
        return status;
    }

    status = BFQclose(&inst->frameQueueInst);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAclose: Failed to close frameQueue");
        return status;
    }

    free(inst->dx_cust_values);
    inst->dx_cust_values = 0;
    free(inst->dy_cust_values);
    inst->dy_cust_values = 0;
    free(inst->dz_cust_values);
    inst->dz_cust_values = 0;

    if (inst->deviceIndex >= 0) {
        int res = closeDevice(inst->deviceIndex);
        if (res != P100_OKAY) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusRuntimeError, "BTAclose: Failed to close device");
            return BTA_StatusRuntimeError;
        }
    }
    free(inst);
    inst = 0;
    winst->inst = 0;
    return BTA_StatusOk;
}


//----------------------------------------------------------------------------

uint8_t BTAP100isRunning(BTA_WrapperInst *winst) {
    if (!winst) {
        return 0;
    }
    if (winst->inst) {
        return 1;
    }
    else {
        return 0;
    }
}

uint8_t BTAP100isConnected(BTA_WrapperInst *winst) {
    if (!winst) {
        return 0;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return 0;
    }
    //BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_EventIdInformation, "BTAisConnected");
    //return BTAP100isRunning(handle);
    uint32_t dummy;
    return (getRegister(inst->deviceIndex, 0, &dummy) == P100_OKAY);
}


BTA_Status BTAP100sendReset(BTA_WrapperInst *winst) {
    BTA_Status status;
    BTA_DeviceInfo *info;
    uint32_t regData;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    status = BTAP100getDeviceInfo(winst, &info);
    if (status != BTA_StatusOk) {
        return status;
    }
    if (info->firmwareVersionMajor >= 2 && info->firmwareVersionMinor >= 1) {
        BTAfreeDeviceInfo(info);
        regData = 0xf0000000;
        return BTAP100writeRegister(winst, P100_REG_ADVANCED_FUNCTION, &regData, 0);
    }
    BTAfreeDeviceInfo(info);
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAsendReset: Not supported, firmware too old");
    return BTA_StatusNotSupported;
}


BTA_Status BTAP100getDeviceType(BTA_WrapperInst *winst, BTA_DeviceType *deviceType) {
    if (!deviceType) {
        return BTA_StatusInvalidParameter;
    }
    BTA_DeviceInfo *deviceInfo;
    BTA_Status status = BTAP100getDeviceInfo(winst, &deviceInfo);
    if (status != BTA_StatusOk) {
        return status;
    }
    *deviceType = deviceInfo->deviceType;
    BTAfreeDeviceInfo(deviceInfo);
    return BTA_StatusOk;
}


BTA_Status BTAP100getDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo){
    BTA_Status status;
    BTA_DeviceInfo *info;
    uint32_t code;
    uint32_t serial;
    uint32_t firmwareDate;
    uint32_t version;
    int i;

    *deviceInfo = 0;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }

    info = (BTA_DeviceInfo *)calloc(1, sizeof(BTA_DeviceInfo));
    if (!info) {
        return BTA_StatusOutOfMemory;
    }

    //read status
    status = BTAP100readRegister(winst, 0x00, &(info->status), 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    //read serial
    status = BTAP100readRegister(winst, 0x01, &serial, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    //read release
    status = BTAP100readRegister(winst, 0x02, &firmwareDate, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }

    info->serialNumber = serial & 0xfffff;
    code = serial >> 20;

    for (i = 0; i < BTA_P100_DEVICE_TYPES_LEN; i++) {
        if (ponCodes[i] == code) {
            info->deviceType = (BTA_DeviceType)deviceTypes[i];
            if (strlen(productOrderNumbers[i]) + strlen(productOrderNumberSuffixes[i]) > 0) {
                info->productOrderNumber = (uint8_t *)calloc(1, strlen(productOrderNumbers[i]) + strlen(productOrderNumberSuffixes[i]) + 1);
                if (!info->productOrderNumber) {
                    BTAfreeDeviceInfo(info);
                    return BTA_StatusOutOfMemory;
                }
                sprintf((char *)info->productOrderNumber, "%s%s", productOrderNumbers[i], productOrderNumberSuffixes[i]);
            }
            break;
        }
    }
    if (info->deviceType == 0) {
        info->deviceType = BTA_DeviceTypeUsb;
    }

    firmwareDate = ((firmwareDate & 0xFFFF) << 16) | ((firmwareDate & 0xFF0000) >> 8) | ((firmwareDate & 0xFF000000) >> 24);
    if (firmwareDate < BTA_P100_GLOBALOFFSET_FIRMWARE_OLD) {
        info->firmwareVersionMajor = 1;
        info->firmwareVersionMinor = 0;
        info->firmwareVersionNonFunc = 0;
    }
    else {
        status = BTAP100readRegister(winst, 0x38, &version, 0);
        if (status != BTA_StatusOk) {
            BTAfreeDeviceInfo(info);
            return status;
        }
        info->firmwareVersionMajor = (version & 0xF800) >> 11;
        info->firmwareVersionMinor = (version & 0x07C0) >> 6;
        info->firmwareVersionNonFunc = version & 0x003F;
    }
    *deviceInfo = info;
    return BTA_StatusOk;
}


BTA_Status BTAP100readRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount) {
    if (!winst || !data) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    int32_t my_reg_res;
    uint32_t my_reg;
    uint32_t i;
    uint32_t  lenToRead = 1;
    if (address > 0xffff || !data) {
        if (registerCount) {
            *registerCount = 0;
        }
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAreadRegister: Address overflow");
        return BTA_StatusInvalidParameter;
    }
    if (registerCount) {
        if (address + *registerCount > 0x10000) {
            *registerCount = 0;
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAreadRegister: Address overflow");
            return BTA_StatusInvalidParameter;
        }
        lenToRead = *registerCount;
        *registerCount = 0;
    }
    if (lenToRead == 0) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAreadRegister: Length is 0");
        return BTA_StatusInvalidParameter;
    }

    //virtual register range
    if (address >= P100_REG_ADDR_LIMIT) {
        switch (address) {
        case P100_IMG_PROC_CONFIG:
            if (lenToRead != 1) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAreadRegister: Register imgProcConfig does not support multiread");
                return BTA_StatusInvalidParameter;
            }
            *data = inst->imgProcConfig;
            if (registerCount) {
                *registerCount = 1;
            }
            return BTA_StatusOk;
        case P100_FILTER_BILATERAL_CONFIG_2:
            if (lenToRead != 1) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAreadRegister: Register filterBilateralConfig2 does not support multiread");
                return BTA_StatusInvalidParameter;
            }
            *data = inst->filterBilateralConfig2;
            if (registerCount) {
                *registerCount = 1;
            }
            return BTA_StatusOk;
        default:
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAreadRegister: Not a register %d", address);
            return BTA_StatusInvalidParameter;
        }
    }

    for (i = 0; i < lenToRead; i += 1) {
        my_reg_res = getRegister(inst->deviceIndex, address, &my_reg);
        if (my_reg_res == P100_OKAY) {
            data[i] = my_reg;
            address++;
            if (registerCount) {
                (*registerCount)++;
            }
        }
        else {
            BTAinfoEventHelper(winst->infoEventInst, 5, BTA_StatusDeviceUnreachable, "BTAreadRegister: Internal error %d", my_reg_res);
            #if USB_COMM_ERR_DEBUG
                printf("%s: Error from getRegister() [%d] [%u]\n", __func__, my_reg_res, __LINE__);
            #endif
            return BTA_StatusDeviceUnreachable;
        }
    }
    return BTA_StatusOk;
}


BTA_Status BTAP100writeRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount) {
    if (!winst || !data) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    int res;
    unsigned int i;
    unsigned int value;
    uint32_t lenToWrite = 1;
    if (address > 0xffff) {
        if (registerCount) {
            *registerCount = 0;
        }
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAwriteRegister: Address overflow");
        return BTA_StatusInvalidParameter;
    }
    if (registerCount) {
        if (address + *registerCount > 0x10000) {
            *registerCount = 0;
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAwriteRegister: Address overflow");
            return BTA_StatusInvalidParameter;
        }
        lenToWrite = *registerCount;
        *registerCount = 0;
    }
    if (lenToWrite == 0) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAwriteRegister: Length is 0");
        return BTA_StatusInvalidParameter;
    }

    //virtual register range
    if (address >= P100_REG_ADDR_LIMIT) {
        switch (address) {
            case P100_IMG_PROC_CONFIG:
                if (lenToWrite != 1) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAwriteRegister: Register imgProcConfig does not support multiwrite");
                    return BTA_StatusInvalidParameter;
                }
                inst->imgProcConfig = *data;
                setBilateralStatus(inst->deviceIndex, (inst->imgProcConfig & 0x8) > 0);
                if (registerCount) {
                    *registerCount = 1;
                }
                return BTA_StatusOk;
            case P100_FILTER_BILATERAL_CONFIG_2:
                if (lenToWrite != 1) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAwriteRegister: Register filterBilateralConfig2 does not support multiwrite");
                    return BTA_StatusInvalidParameter;
                }
                if ((*data >= 3) && (*data <= BILAT_MAX_WINDOWSIZE) && (((*data) & 0x01) == 1)) {
                    inst->filterBilateralConfig2 = *((uint32_t *)data);
                    setBilateralWindow(inst->deviceIndex, data[0]);
                    if (registerCount) {
                        *registerCount = 1;
                    }
                    return BTA_StatusOk;
                }
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAwriteRegister: Value invalid %d", *data);
                return BTA_StatusInvalidParameter;
            default:
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAwriteRegister: Not a register %d", address);
                return BTA_StatusInvalidParameter;
        }
    }

    for (i = 0; i < lenToWrite; i += 1) {
        value = data[i];
        res = setRegister(inst->deviceIndex, address, value);
        if (res == P100_OKAY) {
            if (address == P100_REG_TRIGGER_MODE && value != 0 && inst->captureThread) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInformation, "BTAP100writeRegister: Be aware, captureThread is capturing frames and 'free run mode' is off -> timeouts occur and delay communications");
            }
            address += 1;
            if (registerCount) {
                (*registerCount)++;
            }
        }
        else {
            switch (res) {
                case P100_ACK_ERROR_REG_WRITE_PROTECTED:
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusIllegalOperation, "BTAwriteRegister: Register is read only %d", res);
                    return BTA_StatusIllegalOperation;
                case P100_ACK_ERROR_FPS_TOO_HIGH:
                case P100_ACK_ERROR_INDEX_OUT_OF_RANGE:
                case P100_ACK_ERROR_FRAME_TIME_TOO_HIGH:
                case P100_ACK_ERROR_FREQUENCY_NOT_SUPPORTED:
                case P100_INVALID_VALUE:
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAwriteRegister: Internal error %d", res);
                    return BTA_StatusInvalidParameter;
                //case P100_USB_ERROR:
                default:
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusUnknown, "BTAwriteRegister: Unrecognized error %d", res);
                    return BTA_StatusUnknown;
            }
        }
    }
    return BTA_StatusOk;
}


BTA_Status BTAP100setLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float value) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    switch (libParam) {
    case BTA_LibParamDisableDataScaling:
        inst->disableDataScaling = value > 0 ? 1 : 0;
        return BTA_StatusOk;
    default:
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAsetLibParam: LibParam %s not supported", BTAlibParamToString(libParam));
        return BTA_StatusNotSupported;
    }
}


BTA_Status BTAP100getLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float *value) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    switch (libParam) {
    case BTA_LibParamDisableDataScaling:
        *value = (float)inst->disableDataScaling;
        return BTA_StatusOk;
    default:
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAgetLibParam: LibParam not supported", BTAlibParamToString(libParam));
        return BTA_StatusNotSupported;
    }
}


//----------------------------------------------------------------------------


static BTA_Status getFrameInstant(BTA_WrapperInst *winst, BTA_Frame **framePtr) {
    if (!winst || !framePtr) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_Status status;
    BTA_FrameMode frameMode;
    uint32_t firmwareDate;
    uint32_t tempTemp;
    uint32_t rawDataSize;
    uint8_t *rawData;
    int result;
    int i;
    BTA_Frame *frame;

    *framePtr = 0;

    if (inst->closing) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusIllegalOperation, "getFrameInstant: Instance is closing");
        return BTA_StatusIllegalOperation;
    }

    BTAlockMutex(inst->handleMutex);

    frameMode =  inst->frameMode;
    rawDataSize = inst->rawDataSize;

    rawData = (uint8_t *)malloc(rawDataSize);
    if (!rawData) {
        BTAunlockMutex(inst->handleMutex);
        return BTA_StatusOutOfMemory;
    }
    result = readFrame(inst->deviceIndex, rawData, rawDataSize);
    if (result != P100_OKAY) {
        free(rawData);
        rawData = 0;
        if (result == P100_GET_DATA_ERROR) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusTimeOut, "BTAgetFrameInstant: Timeout getting frame %d", result);
            BTAunlockMutex(inst->handleMutex);
            return BTA_StatusTimeOut;
        }
        else {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusDeviceUnreachable, "BTAgetFrameInstant: Error getting frame %d", result);
            BTAunlockMutex(inst->handleMutex);
            return BTA_StatusDeviceUnreachable;
        }
    }

    frame = (BTA_Frame *)calloc(1, sizeof(BTA_Frame));
    if (!frame) {
        free(rawData);
        rawData = 0;
        BTAunlockMutex(inst->handleMutex);
        return BTA_StatusOutOfMemory;
    }
    firmwareDate = src_data_container_header_pos_ntohs(rawData, 0, 2);
    firmwareDate = ((firmwareDate & 0xFFFF) << 16) | ((firmwareDate & 0xFF0000) >> 8) | ((firmwareDate & 0xFF000000) >> 24);
    if (firmwareDate < BTA_P100_GLOBALOFFSET_FIRMWARE_OLD) {
        frame->firmwareVersionMajor = 1;
        frame->firmwareVersionMinor = 0;
        frame->firmwareVersionNonFunc = 0;
    }
    else {
        frame->firmwareVersionMajor = (inst->version & 0xF800) >> 11;
        frame->firmwareVersionMinor = (inst->version & 0x07C0) >> 6;
        frame->firmwareVersionNonFunc = inst->version & 0x003F;
    }
    tempTemp = src_data_container_header_pos_ntohs(rawData, 0, IMG_HEADER_TEMERATURE_ILL);
    if (tempTemp & 0x1000) {
        tempTemp |= 0xfffff000;
    }
    frame->ledTemp = (float)((int32_t)tempTemp) / (float)16.0;
    tempTemp = src_data_container_header_pos_ntohs(rawData, 0, IMG_HEADER_TEMPERATURE);
    if (tempTemp & 0x1000) {
        tempTemp |= 0xfffff000;
    }
    frame->mainTemp = (float)((int32_t)tempTemp) / (float)16.0;
    tempTemp = src_data_container_header_pos_ntohs(rawData, 0, IMG_HEADER_TEMPERATURE_CUST);
    if (tempTemp & 0x1000) {
        tempTemp |= 0xfffff000;
    }
    frame->genericTemp = (float)((int32_t)tempTemp) / (float)16.0;
    frame->frameCounter = src_data_container_header_pos_ntohs(rawData, 0, IMG_HEADER_FRAME_COUNTER);
    frame->timeStamp = 1000*src_data_container_header_pos_ntohs(rawData, 0, IMG_HEADER_TIME_STAMP);
    frame->sequenceCounter = src_data_container_header_pos_ntohs(rawData, 0, IMG_HEADER_SEQ_LENGTH);
    frame->channelsLen = 0;
    frame->channels = 0;

    int distPos = -1;
    int ampPos = -1;
    int flagPos = -1;
    int intensPos = -1;
    int rawPos = -1;
    for (i = 0; i < (int)rawDataSize/(int)SIZEOF_1_CONTAINER; i++) {
        uint32_t channelId = src_data_container_header_pos_ntohs(rawData, i, IMG_HEADER_OUTPUTMODE);
        switch (channelId) {
        case IMG_HEADER_DIST_VALUES:
            distPos = i;
            break;
        case IMG_HEADER_AMP_VALUES:
            ampPos = i;
            break;
        case IMG_HEADER_FLAG_VALUES:
            flagPos = i;
            break;
        case IMG_HEADER_INTENS_VALUES:
            intensPos = i;
            break;
        case IMG_HEADER_PHASE_VALUES:
            rawPos = i;
            break;
        }
        if (winst->lpTestPatternEnabled) {
            if (channelId == IMG_HEADER_DIST_VALUES) {
                // Set the modulation frequency to the value where the unambiguous range is 65,535m
                uint32_t *modFreq = (((unsigned int *)(rawData + (P100_WIDTH*P100_HEIGHT*P100_BYTES_PER_PIXEL*i + P100_IMG_HEADER_SIZE*i))) + IMG_HEADER_MODULATIONFREQUENCY);
                // Set to 2287269Hz == 0x0022e6a5 reversed:
                *modFreq = 0xa5e62200;
            }
            uint32_t x, y, val = 0;
            uint16_t *temp = (uint16_t *)(rawData + P100_IMG_HEADER_SIZE*(i + 1) + P100_WIDTH*P100_HEIGHT*P100_BYTES_PER_PIXEL*i);
            for (y = 0; y < P100_HEIGHT; y++) {
                for (x = 0; x < P100_WIDTH; x++) {
                    temp[P100_WIDTH - 1 - x + y*P100_WIDTH] = val++;
                }
            }
        }
    }

    status = BTA_StatusOk;
    if (distPos >= 0 && (frameMode == BTA_FrameModeXYZ || frameMode == BTA_FrameModeXYZAmp || frameMode == BTA_FrameModeXYZAmpFlags)) {
        uint16_t xRes = src_data_container_header_pos_ntohs(rawData, distPos, 5);
        uint16_t yRes = src_data_container_header_pos_ntohs(rawData, distPos, 4);
        uint32_t integrationTime = src_data_container_header_pos_ntohs(rawData, distPos, IMG_HEADER_INTEGRATIONTIME);
        uint32_t modulationFrequency = src_data_container_header_pos_ntohs(rawData, distPos, IMG_HEADER_MODULATIONFREQUENCY);
        uint32_t dataLen = P100_WIDTH*P100_HEIGHT*sizeof(float);
        float *dataX = (float *)malloc(dataLen);
        float *dataY = (float *)malloc(dataLen);
        float *dataZ = (float *)malloc(dataLen);
        if (dataX && dataY && dataZ) {
            result = calc3Dcoordinates(inst->deviceIndex, rawData, rawDataSize, dataX, dataY, dataZ, FLAGS_NONE, 0);
            if (result == P100_OKAY) {
                status = BTAinsertChannelIntoFrame2(frame, BTA_ChannelIdX, xRes, yRes, BTA_DataFormatFloat32, BTA_UnitMillimeter, integrationTime, modulationFrequency, (uint8_t *)dataX, dataLen);
                if (status != BTA_StatusOk) {
                    free(dataX);
                    dataX = 0;
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAgetFrameInstant: Error adding channel (X)");
                }
                status = BTAinsertChannelIntoFrame2(frame, BTA_ChannelIdY, xRes, yRes, BTA_DataFormatFloat32, BTA_UnitMillimeter, integrationTime, modulationFrequency, (uint8_t *)dataY, dataLen);
                if (status != BTA_StatusOk) {
                    free(dataY);
                    dataY = 0;
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAgetFrameInstant: Error adding channel (Y)");
                }
                status = BTAinsertChannelIntoFrame2(frame, BTA_ChannelIdZ, xRes, yRes, BTA_DataFormatFloat32, BTA_UnitMillimeter, integrationTime, modulationFrequency, (uint8_t *)dataZ, dataLen);
                if (status != BTA_StatusOk) {
                    free(dataZ);
                    dataZ = 0;
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAgetFrameInstant: Error adding channel (Z)");
                }
            }
            else {
                free(dataX);
                dataX = 0;
                free(dataY);
                dataY = 0;
                free(dataZ);
                dataZ = 0;
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAgetFrameInstant: Error creating frame (XYZ) %d", result);
            }
        }
        else {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "BTAgetFrameInstant: Error allocating for channels (XYZ)");
        }
    }

    if (distPos >= 0 && (frameMode == BTA_FrameModeCurrentConfig || frameMode == BTA_FrameModeDistAmp || frameMode == BTA_FrameModeDistAmpColor || frameMode == BTA_FrameModeDistAmpFlags)) {
        uint16_t xRes = src_data_container_header_pos_ntohs(rawData, distPos, 5);
        uint16_t yRes = src_data_container_header_pos_ntohs(rawData, distPos, 4);
        uint32_t integrationTime = src_data_container_header_pos_ntohs(rawData, distPos, IMG_HEADER_INTEGRATIONTIME);
        uint32_t modulationFrequency = src_data_container_header_pos_ntohs(rawData, distPos, IMG_HEADER_MODULATIONFREQUENCY);
        uint32_t dataLen = P100_WIDTH*P100_HEIGHT*sizeof(float);
        unsigned int flags = 0;
        BTA_ChannelId id;
        BTA_Unit unit;
        if (inst->disableDataScaling) {
            flags |= DIST_FLOAT_PHASE;
            id = BTA_ChannelIdRawDist;
            unit = BTA_UnitUnitLess;
        }
        else {
            flags |= DIST_FLOAT_METER;
            id = BTA_ChannelIdDistance;
            unit = BTA_UnitMillimeter;
        }
        float *data = (float *)malloc(dataLen);
        if (data) {
            result = calcDistances(inst->deviceIndex, rawData, rawDataSize, data, dataLen, flags, 0);
            if (result == P100_OKAY) {
                status = BTAinsertChannelIntoFrame2(frame, id, xRes, yRes, BTA_DataFormatFloat32, unit, integrationTime, modulationFrequency, (uint8_t *)data, dataLen);
                if (status != BTA_StatusOk) {
                    free(data);
                    data = 0;
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAgetFrameInstant: Error adding channel (dist)");
                }
            }
            else {
                free(data);
                data = 0;
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAgetFrameInstant: Error creating frame (dist) %d", result);
            }
        }
        else {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "BTAgetFrameInstant: Error allocating for channel (dist)");
        }
    }

    if (ampPos >= 0 && (frameMode == BTA_FrameModeCurrentConfig || frameMode == BTA_FrameModeDistAmp || frameMode == BTA_FrameModeDistAmpColor || frameMode == BTA_FrameModeDistAmpFlags || frameMode == BTA_FrameModeXYZAmp || frameMode == BTA_FrameModeXYZAmpFlags)) {
        uint16_t xRes = src_data_container_header_pos_ntohs(rawData, ampPos, 5);
        uint16_t yRes = src_data_container_header_pos_ntohs(rawData, ampPos, 4);
        uint32_t integrationTime = src_data_container_header_pos_ntohs(rawData, ampPos, IMG_HEADER_INTEGRATIONTIME);
        uint32_t modulationFrequency = src_data_container_header_pos_ntohs(rawData, ampPos, IMG_HEADER_MODULATIONFREQUENCY);
        uint32_t dataLen = P100_WIDTH*P100_HEIGHT*sizeof(float);
        float *data = (float *)malloc(dataLen);
        if (data) {
            result = calcAmplitudes(rawData, rawDataSize, data, dataLen, FLAGS_NONE, 0);
            status = BTA_StatusOk;
            if (result == P100_OKAY) {
                status = BTAinsertChannelIntoFrame2(frame, BTA_ChannelIdAmplitude, xRes, yRes, BTA_DataFormatFloat32, BTA_UnitUnitLess, integrationTime, modulationFrequency, (uint8_t *)data, dataLen);
                if (status != BTA_StatusOk) {
                    free(data);
                    data = 0;
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAgetFrameInstant: Error creating frame (amp)");
                }
            }
            else {
                free(data);
                data = 0;
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAgetFrameInstant: Error creating frame (amp) %d", result);
            }
        }
        else {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "BTAgetFrameInstant: Error allocating for channel (amp)");
        }
    }

    if (flagPos >= 0 && (frameMode == BTA_FrameModeCurrentConfig || frameMode == BTA_FrameModeDistAmpFlags || frameMode == BTA_FrameModeXYZAmpFlags)) {
        uint16_t xRes = src_data_container_header_pos_ntohs(rawData, flagPos, 5);
        uint16_t yRes = src_data_container_header_pos_ntohs(rawData, flagPos, 4);
        uint32_t integrationTime = src_data_container_header_pos_ntohs(rawData, flagPos, IMG_HEADER_INTEGRATIONTIME);
        uint32_t modulationFrequency = src_data_container_header_pos_ntohs(rawData, flagPos, IMG_HEADER_MODULATIONFREQUENCY);
        uint32_t dataLen = P100_WIDTH*P100_HEIGHT*sizeof(uint32_t);
        uint32_t *data = (uint32_t *)malloc(dataLen);
        if (data) {
            result = calcFlags(rawData, rawDataSize, data, dataLen, FLAGS_NONE, 0);
            if (result == P100_OKAY) {
                status = BTAinsertChannelIntoFrame2(frame, BTA_ChannelIdFlags, xRes, yRes, BTA_DataFormatUInt32, BTA_UnitUnitLess, integrationTime, modulationFrequency, (uint8_t *)data, dataLen);
                if (status != BTA_StatusOk) {
                    free(data);
                    data = 0;
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAgetFrameInstant: Error creating frame (flags)");
                }
            }
            else {
                free(data);
                data = 0;
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAgetFrameInstant: Error creating frame (flags) %d", result);
            }
        }
        else {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "BTAgetFrameInstant: Error allocating for channel (flags)");
        }
    }

    if (intensPos >= 0 && (frameMode == BTA_FrameModeCurrentConfig || frameMode == BTA_FrameModeIntensities)) {
        uint16_t xRes = src_data_container_header_pos_ntohs(rawData, intensPos, 5);
        uint16_t yRes = src_data_container_header_pos_ntohs(rawData, intensPos, 4);
        uint32_t integrationTime = src_data_container_header_pos_ntohs(rawData, intensPos, IMG_HEADER_INTEGRATIONTIME);
        uint32_t modulationFrequency = src_data_container_header_pos_ntohs(rawData, intensPos, IMG_HEADER_MODULATIONFREQUENCY);
        uint32_t dataLen = P100_WIDTH*P100_HEIGHT*sizeof(uint16_t);
        uint16_t *data = (uint16_t *)malloc(dataLen);
        if (data) {
            result = calc_intensities(rawData, rawDataSize, data, dataLen);
            if (result == P100_OKAY) {
                status = BTAinsertChannelIntoFrame2(frame, BTA_ChannelIdGrayScale, xRes, yRes, BTA_DataFormatUInt16, BTA_UnitUnitLess, integrationTime, modulationFrequency, (uint8_t *)data, dataLen);
                if (status != BTA_StatusOk) {
                    free(data);
                    data = 0;
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAgetFrameInstant: Error creating frame (intens)");
                }
            }
            else {
                free(data);
                data = 0;
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAgetFrameInstant: Error creating frame (intens) %d", result);
            }
        }
        else {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "BTAgetFrameInstant: Error allocating for channel (intens)");
        }
    }

    if (rawPos >= 0 && (frameMode == BTA_FrameModeCurrentConfig || frameMode == BTA_FrameModeRawPhases)) {
        uint16_t xRes = src_data_container_header_pos_ntohs(rawData, 0, 5);
        uint16_t yRes = src_data_container_header_pos_ntohs(rawData, 0, 4);
        uint32_t integrationTime = src_data_container_header_pos_ntohs(rawData, 0, IMG_HEADER_INTEGRATIONTIME);
        uint32_t modulationFrequency = src_data_container_header_pos_ntohs(rawData, 0, IMG_HEADER_MODULATIONFREQUENCY);
        uint32_t dataLen = P100_WIDTH*P100_HEIGHT*sizeof(uint16_t);
        BTA_ChannelId ids[4] = { BTA_ChannelIdPhase0, BTA_ChannelIdPhase90, BTA_ChannelIdPhase180, BTA_ChannelIdPhase270 };
        for (i = 0; i < 4; i++) {
            uint16_t *data = (uint16_t *)malloc(dataLen);
            if (data) {
                result = calc_phases(rawData, rawDataSize, data, dataLen, i);
                if (result == P100_OKAY) {
                    status = BTAinsertChannelIntoFrame2(frame, ids[i], xRes, yRes, BTA_DataFormatSInt16, BTA_UnitUnitLess, integrationTime, modulationFrequency, (uint8_t *)data, dataLen);
                    if (status != BTA_StatusOk) {
                        free(data);
                        data = 0;
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAgetFrameInstant: Error creating frame (raw) %d", i);
                    }
                }
                else {
                    free(data);
                    data = 0;
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAgetFrameInstant: Error creating frame (raw) %d", result);
                }
            }
            else {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "BTAgetFrameInstant: Error allocating for channel (raw) %d", i);
            }
        }
    }
    *framePtr = frame;
    free(rawData);
    rawData = 0;
    BTAunlockMutex(inst->handleMutex);
    return BTA_StatusOk;
}


BTA_Status BTAP100flushFrameQueue(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }

    if (!inst->frameQueueInst) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusIllegalOperation, "BTAgetFrame: Frame queueing must be enabled in BTAopen");
        return BTA_StatusIllegalOperation;
    }

    return BFQclear(inst->frameQueueInst);
}


BTA_Status BTAP100setFrameMode(BTA_WrapperInst *winst, BTA_FrameMode frameMode) {
    if (!winst || frameMode == BTA_FrameModeCurrentConfig) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }

    BTA_Status status;
    // This measure helps us get the lock sooner
    uint8_t pause = winst->lpPauseCaptureThread;
    winst->lpPauseCaptureThread = 1;
    if (inst->closing) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusIllegalOperation, "BTAsetFrameMode: Instance is closing");
        return BTA_StatusIllegalOperation;
    }
    BTAlockMutex(inst->handleMutex);

    uint32_t grayscaleShiftSeq0, numPhasesSeq0, calcMode;
    status = BTAP100readRegister(winst, P100_REG_CAL_MODE, &calcMode, 0);
    if (status != BTA_StatusOk) {
        winst->lpPauseCaptureThread = pause;
        BTAunlockMutex(inst->handleMutex);
        return status;
    }

    // Clear all bits that we want to control
    calcMode &= ~((1 << P100_REG_CALC_MODE_DOUT0) | (1 << P100_REG_CALC_MODE_DOUT1) | (1 << P100_REG_CALC_MODE_DOUT2) | (1 << P100_REG_CALC_MODE_DOUT3));
    calcMode &= ~((1 << P100_REG_CALC_MODE_DOUT0_DATA0) | (1 << P100_REG_CALC_MODE_DOUT0_DATA1) |
                    (1 << P100_REG_CALC_MODE_DOUT1_DATA0) | (1 << P100_REG_CALC_MODE_DOUT1_DATA1) |
                    (1 << P100_REG_CALC_MODE_DOUT2_DATA0) | (1 << P100_REG_CALC_MODE_DOUT2_DATA1) |
                    (1 << P100_REG_CALC_MODE_DOUT3_DATA0) | (1 << P100_REG_CALC_MODE_DOUT3_DATA1));

    switch (frameMode) {
    case BTA_FrameModeXYZ:
        grayscaleShiftSeq0 = 0x0123;
        numPhasesSeq0 = 0x4;
        // DOUT1: on
        calcMode |= (1 << P100_REG_CALC_MODE_DOUT1);
        // DOUT1: phase_plaus
        calcMode |= (1 << P100_REG_CALC_MODE_DOUT1_DATA0);
        break;

    case BTA_FrameModeXYZAmp:
    case BTA_FrameModeDistAmp:
        grayscaleShiftSeq0 = 0x0123;
        numPhasesSeq0 = 0x4;
        // DOUT1: on, DOUT2: on
        calcMode |= (1 << P100_REG_CALC_MODE_DOUT1) | (1 << P100_REG_CALC_MODE_DOUT2);
        // DOUT1: phase_plaus, DOUT2: amplitude
        calcMode |= (1 << P100_REG_CALC_MODE_DOUT1_DATA0);
        break;

    case BTA_FrameModeDistAmpFlags:
    case BTA_FrameModeXYZAmpFlags:
        grayscaleShiftSeq0 = 0x0123;
        numPhasesSeq0 = 0x4;
        // DOUT1: on, DOUT2: on, DOUT3: on
        calcMode |= (1 << P100_REG_CALC_MODE_DOUT1) | (1 << P100_REG_CALC_MODE_DOUT2) | (1 << P100_REG_CALC_MODE_DOUT3);
        // DOUT1: phase_plaus, DOUT2: amplitude, DOUT3: plausibility
        calcMode |= (1 << P100_REG_CALC_MODE_DOUT1_DATA0);
        break;

    case BTA_FrameModeRawPhases:
        grayscaleShiftSeq0 = 0x0123;
        numPhasesSeq0 = 0x4;
        // DOUT0: on, DOUT1: on, DOUT2: on, DOUT3: on
        calcMode |= (1 << P100_REG_CALC_MODE_DOUT0) | (1 << P100_REG_CALC_MODE_DOUT1) | (1 << P100_REG_CALC_MODE_DOUT2) | (1 << P100_REG_CALC_MODE_DOUT3);
        // DOUT0: diff0, DOUT1: diff1, DOUT2: diff2, DOUT3: diff3
        calcMode |= (1 << P100_REG_CALC_MODE_DOUT0_DATA0) | (1 << P100_REG_CALC_MODE_DOUT0_DATA1) |
                    (1 << P100_REG_CALC_MODE_DOUT1_DATA0) | (1 << P100_REG_CALC_MODE_DOUT1_DATA1) |
                    (1 << P100_REG_CALC_MODE_DOUT2_DATA0) | (1 << P100_REG_CALC_MODE_DOUT2_DATA1) |
                    (1 << P100_REG_CALC_MODE_DOUT3_DATA0) | (1 << P100_REG_CALC_MODE_DOUT3_DATA1);
        break;

    case BTA_FrameModeIntensities:
        grayscaleShiftSeq0 = 0x10123; // grayscaleShiftSeq0 = 0x1000 //not working
        numPhasesSeq0 = 0x1;
        // DOUT0: on
        calcMode |= (1 << P100_REG_CALC_MODE_DOUT0);
        // DOUT0: intensities
        break;

    default:
        winst->lpPauseCaptureThread = pause;
        BTAunlockMutex(inst->handleMutex);
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAsetFrameMode: frameMode not supported %s", BTAframeModeToString(frameMode));
        return BTA_StatusNotSupported;
    }

    status = BTAP100writeRegister(winst, 0x84, &grayscaleShiftSeq0, 0);
    if (status != BTA_StatusOk) {
        winst->lpPauseCaptureThread = pause;
        BTAunlockMutex(inst->handleMutex);
        return status;
    }
    status = BTAP100writeRegister(winst, 0x85, &numPhasesSeq0, 0);
    if (status != BTA_StatusOk) {
        winst->lpPauseCaptureThread = pause;
        BTAunlockMutex(inst->handleMutex);
        return status;
    }
    calcMode |= (1 << P100_REG_CALC_MODE_CALC_PHASE) | (1 << P100_REG_CALC_MODE_CALC_INTENSITY) | (1 << P100_REG_CALC_MODE_CALC_AMP) | (1 << P100_REG_CALC_MODE_CALC_PLAUS_ACTIVE);
    status = BTAP100writeRegister(winst, P100_REG_CAL_MODE, &calcMode, 0);
    if (status != BTA_StatusOk) {
        winst->lpPauseCaptureThread = pause;
        BTAunlockMutex(inst->handleMutex);
        return status;
    }
    BTAmsleep(1000);

    // Remember frameMode for getFrame
    inst->frameMode = frameMode;
    // Remember rawDataSize for getFrame
    status = BTAP100readRegister(winst, P100_REG_RAW_DATA_SIZE, &inst->rawDataSize, 0);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAsetFrameMode: Cannot read rawDataSize");
        winst->lpPauseCaptureThread = pause;
        BTAunlockMutex(inst->handleMutex);
        return status;
    }
    winst->lpPauseCaptureThread = pause;
    BTAunlockMutex(inst->handleMutex);
    return BTA_StatusOk;
}


BTA_Status BTAP100getFrameMode(BTA_WrapperInst *winst, BTA_FrameMode *frameMode) {
    if (!winst || !frameMode) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }

    *frameMode = inst->frameMode;
    if (*frameMode != BTA_FrameModeCurrentConfig) {
        return BTA_StatusOk;
    }

    //BTA_Status status;
    //uint32_t grayscaleShiftSeq0, numPhasesSeq0,

    //status = BTAP100readRegister(winst, 0x84, &grayscaleShiftSeq0, 0);
    //if (status != BTA_StatusOk) {
    //    return status;
    //}
    //status = BTAP100readRegister(winst, 0x85, &numPhasesSeq0, 0);
    //if (status != BTA_StatusOk) {
    //    return status;
    //}

    //uint32_t calcMode;
    //status = BTAP100readRegister(winst, P100_REG_CAL_MODE, &calcMode, 0);
    //if (status != BTA_StatusOk) {
    //    return status;
    //}
    //BTA_ChannelId ids0[4] = { BTA_ChannelIdGrayScale, BTA_ChannelIdDistance, BTA_ChannelIdAmplitude, BTA_ChannelIdPhase0 };
    //BTA_ChannelId ids1[4] = { BTA_ChannelIdDistance, BTA_ChannelIdDistance, BTA_ChannelIdFlags, BTA_ChannelIdPhase90 };
    //BTA_ChannelId ids2[4] = { BTA_ChannelIdAmplitude, BTA_ChannelIdAmplitude, BTA_ChannelIdUnknown, BTA_ChannelIdPhase180 };
    //BTA_ChannelId ids3[4] = { BTA_ChannelIdFlags, BTA_ChannelIdDistance, BTA_ChannelIdUnknown, BTA_ChannelIdPhase270 };
    //BTA_ChannelId ids[4] = { BTA_ChannelIdUnknown, BTA_ChannelIdUnknown, BTA_ChannelIdUnknown, BTA_ChannelIdUnknown };
    //if (calcMode & (1 << P100_REG_CALC_MODE_DOUT0)) {
    //    ids[0] = ids0[(calcMode >> P100_REG_CALC_MODE_DOUT0_DATA0) & 0x3];
    //}
    //if (calcMode & (1 << P100_REG_CALC_MODE_DOUT1)) {
    //    ids[0] = ids1[(calcMode >> P100_REG_CALC_MODE_DOUT1_DATA0) & 0x3];
    //}
    //if (calcMode & (1 << P100_REG_CALC_MODE_DOUT2)) {
    //    ids[0] = ids2[(calcMode >> P100_REG_CALC_MODE_DOUT2_DATA0) & 0x3];
    //}
    //if (calcMode & (1 << P100_REG_CALC_MODE_DOUT3)) {
    //    ids[0] = ids3[(calcMode >> P100_REG_CALC_MODE_DOUT3_DATA0) & 0x3];
    //}

    //........... TODO.........
    //calcMode &= 0x1FE001E0;

    //if (grayscaleShiftSeq0 == 0x00000123 && numPhasesSeq0 == 0x00000004) {
    //    if (calcMode == 0x6000C0) {
    //        *frameMode = BTA_FrameModeDistAmp;
    //        return BTA_StatusOk;
    //    }
    //    if (calcMode == 0x6001C0) {
    //        *frameMode = BTA_FrameModeDistAmpFlags;
    //        return BTA_StatusOk;
    //    }
    //    if (calcMode == 0xE00040) {
    //        *frameMode = BTA_FrameModeXYZ;
    //        return BTA_StatusOk;
    //    }
    //    if (calcMode == 0xE000C0) {
    //        *frameMode = BTA_FrameModeXYZAmp;
    //        return BTA_StatusOk;
    //    }
    //    if (calcMode == 0xE001C0) {
    //        *frameMode = BTA_FrameModeXYZAmpFlags;
    //        return BTA_StatusOk;
    //    }
    //    if (calcMode == 0x1FE001E0) {
    //        *frameMode = BTA_FrameModeRawPhases;
    //        return BTA_StatusOk;
    //    }
    //}
    //else if (grayscaleShiftSeq0 == 0x00010123 && numPhasesSeq0 == 0x00000001) {
    //    if (calcMode == 0x9fe3f) {
    //        *frameMode = BTA_FrameModeIntensities;
    //        return BTA_StatusOk;
    //    }
    //}

    *frameMode = BTA_FrameModeCurrentConfig;
    return BTA_StatusOk;
}


BTA_Status BTAP100setIntegrationTime(BTA_WrapperInst *winst, uint32_t integrationTime) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t regData;
    BTA_Status status = BTAP100readRegister(winst, P100_REG_SEQ_LENGTH, &regData, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    uint32_t i;
    for (i = 0; i < regData; i++) {
        // We are using x sequences, so set them all
        int res = setIntegrationTime(inst->deviceIndex, integrationTime, i);
        if (res != P100_OKAY) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusDeviceUnreachable, "BTAsetIntegrationTime: Internal error %d", res);
            return BTA_StatusDeviceUnreachable;
        }
    }
    return BTA_StatusOk;
}


BTA_Status BTAP100getIntegrationTime(BTA_WrapperInst *winst, uint32_t *integrationTime){
    if (!winst || !integrationTime) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    // always return integration time of sequence 0
    int res = getIntegrationTime(inst->deviceIndex, integrationTime, 0);
    if (res != P100_OKAY) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusDeviceUnreachable, "BTAsetIntegrationTime: Internal error %d", res);
        return BTA_StatusDeviceUnreachable;
    }
    return BTA_StatusOk;
}


BTA_Status BTAP100setFrameRate(BTA_WrapperInst *winst, float frameRate){
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;

    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    //get firmware date
    uint32_t firmwareDate;
    BTA_Status status = BTAP100readRegister(winst, P100_REG_RELEASE, &firmwareDate, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    firmwareDate = ((firmwareDate & 0xFFFF) << 16) | ((firmwareDate & 0xFF0000) >> 8) | ((firmwareDate & 0xFF000000) >> 24);
    if (firmwareDate < BTA_P100_GLOBALOFFSET_FIRMWARE_OLD) {
        //########### OLD FIRMWARE ###############
        #if DEVEL_DEBUG
        printf(">>>>>>>>>> OLD FIRMWARE\n");
        #endif
        int res = setFPS(inst->deviceIndex, frameRate);
        if (res != P100_OKAY) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusDeviceUnreachable, "BTAsetFrameRate: Internal error %d", res);
            return BTA_StatusDeviceUnreachable;
        }
        return BTA_StatusOk;
    }
    else {
        //########### NEW FIRMWARE ###############
        #if DEVEL_DEBUG
        printf(">>>>>>>>>> NEW FIRMWARE\n");
        #endif
        uint32_t frameRateInt = (uint32_t)frameRate;
        status = BTAP100writeRegister(winst, P100_REG_FRAMES_PER_SECOND, &frameRateInt, 0);
        return status;
    }
}


BTA_Status BTAP100getFrameRate(BTA_WrapperInst *winst, float *frameRate) {
    unsigned int firmwareDate;
    int frameRateint;
    BTA_Status status;
    if (!winst || !frameRate) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    //get firmware date
    status = BTAP100readRegister(winst, P100_REG_RELEASE, &firmwareDate, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    firmwareDate = ((firmwareDate & 0xFFFF) << 16) | ((firmwareDate & 0xFF0000) >> 8) | ((firmwareDate & 0xFF000000) >> 24);
    if (firmwareDate < BTA_P100_GLOBALOFFSET_FIRMWARE_OLD) {
        //########### OLD FIRMWARE ###############
        #if DEVEL_DEBUG
        printf(">>>>>>>>>> OLD FIRMWARE\n");
        #endif
        int res = getFPS(inst->deviceIndex, frameRate);
        if (res != P100_OKAY) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusDeviceUnreachable, "BTAgetFrameRate: Internal error %d", res);
            return BTA_StatusDeviceUnreachable;
        }
    }
    else {
        //########### NEW FIRMWARE ###############
        #if DEVEL_DEBUG
        printf(">>>>>>>>>> NEW FIRMWARE\n");
        #endif
        status = BTAP100readRegister(winst, P100_REG_FRAMES_PER_SECOND, (unsigned int *)&frameRateint, 0);
        if (status != BTA_StatusOk) {
            return status;
        }
        *frameRate = (float)frameRateint;
    }
    return BTA_StatusOk;
}


BTA_Status BTAP100setModulationFrequency(BTA_WrapperInst *winst, uint32_t modulationFrequency) {
    BTA_Status status;
    unsigned int firmwareDate;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }

    //get firmware date
    status = BTAP100readRegister(winst, P100_REG_RELEASE, &firmwareDate, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    firmwareDate = ((firmwareDate & 0xFFFF) << 16) | ((firmwareDate & 0xFF0000) >> 8) | ((firmwareDate & 0xFF000000) >> 24);
    if (firmwareDate < BTA_P100_GLOBALOFFSET_FIRMWARE_OLD) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusNotSupported, "BTAsetModulationFrequency: Not supported below firmware v2.0.0");
        return BTA_StatusNotSupported;
    }
    else {
        //########### NEW FIRMWARE ###############
        BTA_DeviceType deviceType;
        status = BTAP100getDeviceType(winst, &deviceType);
        if (status != BTA_StatusOk) {
            return status;
        }
        uint32_t validModFreq;
        status = BTAgetNextBestModulationFrequency(winst, modulationFrequency, &validModFreq, 0);
        if (status != BTA_StatusOk) {
            if (status == BTA_StatusNotSupported) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAsetModulationFrequency: Not supported for this deviceType %d", deviceType);
            }
            return status;
        }
        uint32_t regData;
        BTA_Status status = BTAP100readRegister(winst, P100_REG_SEQ_LENGTH, &regData, 0);
        if (status != BTA_StatusOk) {
            return status;
        }
        uint32_t i;
        for (i = 0; i < regData; i++) {
            // We are using x sequences, so set them all
            uint32_t zero = 0;
            status = BTAP100writeRegister(winst, P100_REG_SEQ0_PLL_SELECT + 10*i, &zero, 0);
            if (status != BTA_StatusOk) {
                return status;
            }
        }

        //estimated to be on the safe side
        BTAmsleep(1000);
        //usleep(100000); seems to work as well, but wait longer to be on the safe side

        status = BTAP100writeRegister(winst, P100_REG_MODULATION_FREQUENCY_0, &modulationFrequency, 0);
        if (status != BTA_StatusOk) {
            return status;
        }
    }
    return BTA_StatusOk;
}


BTA_Status BTAP100getModulationFrequency(BTA_WrapperInst *winst, uint32_t *modulationFrequency) {
    BTA_Status status;
    if (!winst || !modulationFrequency) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t mf;
    status = BTAP100readRegister(winst, P100_REG_SEQ0_MOD_FREQ, &mf, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    *modulationFrequency = mf;
    return BTA_StatusOk;
}


BTA_Status BTAP100setGlobalOffset(BTA_WrapperInst *winst, float globalOffset) {
    BTA_Status status;
    unsigned int modulationFrequency;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    //get firmware date
    uint32_t firmwareDate;
    status = BTAP100readRegister(winst, P100_REG_RELEASE, &firmwareDate, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    firmwareDate = ((firmwareDate & 0xFFFF) << 16) | ((firmwareDate & 0xFF0000) >> 8) | ((firmwareDate & 0xFF000000) >> 24);
    if (firmwareDate < BTA_P100_GLOBALOFFSET_FIRMWARE_OLD) {
        //########### OLD FIRMWARE ###############
        #if DEVEL_DEBUG
        printf(">>>>>>>>>> OLD FIRMWARE\n");
        #endif
        status = BTAP100readRegister(winst, P100_REG_SEQ0_MOD_FREQ, &modulationFrequency, 0);
        if (status != BTA_StatusOk) {
            return status;
        }
        uint32_t offset = (uint32_t)globalOffset;
        if (globalOffset < 0) {
            float unambig = (float)1.0f / modulationFrequency * SPEED_OF_LIGHT * (float)0.5;
            unambig *= (float)1000.0f; // convert to [mm]
            offset = (uint32_t)(unambig + offset);
        }
        status = BTAP100writeRegister(winst, P100_REG_SEQ0_DIST_OFFSET, &offset, 0);
        if (status != BTA_StatusOk) {
            return status;
        }
    }
    else {
        //########### NEW FIRMWARE ###############
        #if DEVEL_DEBUG
        printf(">>>>>>>>>> NEW FIRMWARE\n");
        #endif
        status = BTAP100readRegister(winst, P100_REG_SEQ0_MOD_FREQ, &modulationFrequency, 0);
        if (status != BTA_StatusOk) {
            return status;
        }
        uint32_t regAddr = 0;
        switch (modulationFrequency) {
            case 5000000:
                regAddr = P100_REG_5MHz_OFFSET;
            break;
            case 7500000:
                regAddr = P100_REG_7MHz5_OFFSET;
            break;
            case 10000000:
                regAddr = P100_REG_10MHz_OFFSET;
            break;
            case 15000000:
                regAddr = P100_REG_15MHz_OFFSET;
            break;
            case 20000000:
                regAddr = P100_REG_20MHz_OFFSET;
            break;
            case 25000000:
                regAddr = P100_REG_25MHz_OFFSET;
            break;
            case 30000000:
                regAddr = P100_REG_30MHz_OFFSET;
            break;
            default:
                //should never be reached!
                return BTA_StatusRuntimeError;
            break;
        }
        #if DEVEL_DEBUG
        printf("matching offset register is: 0x%X - %u \n", regAddr, regAddr);
        printf("offset is in int: %i\n", globalOffset);
        #endif
        uint32_t offset = (int32_t)globalOffset;
        status = BTAP100writeRegister(winst, regAddr, &offset, 0);
        if (status != BTA_StatusOk) {
            return status;
        }
        //estimated to be on the safe side
        BTAmsleep(1000);
        //usleep(100000); seems to work as well, but wait longer to be on the safe side
        status = BTAP100writeRegister(winst, P100_REG_SEQ0_MOD_FREQ, &modulationFrequency, 0);
        if (status != BTA_StatusOk) {
            return status;
        }

        //workaround for firmware bug in v2.0.0
        //in case the offset=0, the offset will not be applied to the appropriate sequence offset register.
        //therefore we do it here
        if (offset == 0) {
            status = BTAP100writeRegister(winst, P100_REG_SEQ0_DIST_OFFSET, &offset, 0);
            if (status != BTA_StatusOk) {
                return status;
            }
        }
    }
    return BTA_StatusOk;
}


BTA_Status BTAP100getGlobalOffset(BTA_WrapperInst *winst, float *globalOffset) {
    BTA_Status status;
    if (!winst || !globalOffset) {
        return BTA_StatusInvalidParameter;
    }
    //get firmware date
    uint32_t firmwareDate;
    status = BTAP100readRegister(winst, P100_REG_RELEASE, &firmwareDate, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    // ALWAYS RETURNING SEQUENCE 0 PROPERTY
    firmwareDate = ((firmwareDate & 0xFFFF) << 16) | ((firmwareDate & 0xFF0000) >> 8) | ((firmwareDate & 0xFF000000) >> 24);
    if (firmwareDate <= BTA_P100_GLOBALOFFSET_FIRMWARE_OLD) {
        //########### OLD FIRMWARE ###############
        //get offset
        uint32_t temp_offset;
        status = BTAP100readRegister(winst, P100_REG_SEQ0_DIST_OFFSET, &temp_offset, 0);
        if (status != BTA_StatusOk) {
            return status;
        }
        uint32_t modulationFrequency;
        status = BTAP100readRegister(winst, P100_REG_SEQ0_MOD_FREQ, &modulationFrequency, 0);
        if (status != BTA_StatusOk) {
            return status;
        }
        float offset = (float)(temp_offset);
        if (offset != 0) {
            float unambig = 1.0f / modulationFrequency * SPEED_OF_LIGHT * 0.5f;
            unambig *= 1000.0f; // convert to [mm]
            offset = (offset - unambig);
        }
        *globalOffset = (float)((int32_t)offset);
    }
    else {
        //########### NEW FIRMWARE ###############
        uint32_t offset;
        status = BTAP100readRegister(winst, P100_REG_SEQ0_DIST_OFFSET, &offset, 0);
        if (status != BTA_StatusOk){
            return status;
        }
        *globalOffset = (float)((int32_t)offset);
    }
    return BTA_StatusOk;
}


//------------------------------------------------------------------------


BTA_Status BTAP100flashUpdate(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport) {
    if (!winst || !flashUpdateConfig) {
        if (progressReport) {
            progressReport(BTA_StatusInvalidParameter, 0);
        }
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        if (progressReport) {
            progressReport(BTA_StatusInvalidParameter, 0);
        }
        return BTA_StatusInvalidParameter;
    }

    // remember frame-rate and integration time and set it to 5 and 100
    float frameRateOriginal;
    BTA_Status status = BTAP100getFrameRate(winst, &frameRateOriginal);
    if (status != BTA_StatusOk) {
        if (progressReport) {
            progressReport(status, 0);
        }
        return status;
    }
    status = BTAP100setFrameRate(winst, 5);
    if (status != BTA_StatusOk) {
        if (progressReport) {
            progressReport(status, 0);
        }
        return status;
    }
    uint32_t integrationTimeOriginal;
    status = BTAP100getIntegrationTime(winst, &integrationTimeOriginal);
    if (status != BTA_StatusOk) {
        if (progressReport) {
            progressReport(status, 0);
        }
        return status;
    }
    status = BTAP100setIntegrationTime(winst, 100);
    if (status != BTA_StatusOk) {
        if (progressReport) {
            progressReport(status, 0);
        }
        return status;
    }

    if (progressReport) {
        progressReport(BTA_StatusOk, 0);
    }
    switch (flashUpdateConfig->target) {
        case BTA_FlashTargetApplication: {
            if (flashUpdateConfig->dataLen % 4) {
                if (progressReport) {
                    (*progressReport)(BTA_StatusInvalidParameter, 0);
                }
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "The firmware file size must be a multiple of 4 %d", flashUpdateConfig->dataLen);
                return BTA_StatusInvalidParameter;
            }
            int result = firmwareUpdate(inst->deviceIndex, flashUpdateConfig->data, flashUpdateConfig->dataLen);
            status = (result == P100_OKAY) ? BTA_StatusOk : BTA_StatusRuntimeError;
            break;
        }
        case BTA_FlashTargetWigglingCalibration: {
            if (flashUpdateConfig->dataLen != 4096) {
                if (progressReport) {
                    (*progressReport)(BTA_StatusInvalidParameter, 0);
                }
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "The wiggling file size must be exactly 4096 %d", flashUpdateConfig->dataLen);
                return BTA_StatusInvalidParameter;
            }
            int result = p100WriteToFlash(inst->deviceIndex, flashUpdateConfig->data, flashUpdateConfig->dataLen, CMD_WIGGLING);
            status = (result == P100_OKAY) ? BTA_StatusOk : BTA_StatusRuntimeError;
            break;
        }
        case BTA_FlashTargetFpn: {
            if (flashUpdateConfig->dataLen != 2 * P100_WIDTH * P100_HEIGHT) {
                if (progressReport) {
                    (*progressReport)(BTA_StatusInvalidParameter, 0);
                }
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "The FPN file size must be exactly 38400 %d", flashUpdateConfig->dataLen);
                return BTA_StatusInvalidParameter;
            }
            int result = p100WriteToFlash(inst->deviceIndex, flashUpdateConfig->data, flashUpdateConfig->dataLen, CMD_FPN);
            status = (result == P100_OKAY) ? BTA_StatusOk : BTA_StatusRuntimeError;
            break;
        }
        case BTA_FlashTargetFppn: {
            if (flashUpdateConfig->dataLen != 2 * P100_WIDTH * P100_HEIGHT) {
                if (progressReport) {
                    (*progressReport)(BTA_StatusInvalidParameter, 0);
                }
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "The FPPN file size must be exactly 38400 %d", flashUpdateConfig->dataLen);
                return BTA_StatusInvalidParameter;
            }
            int result = p100WriteToFlash(inst->deviceIndex, flashUpdateConfig->data, flashUpdateConfig->dataLen, CMD_FPPN);
            status = (result == P100_OKAY) ? BTA_StatusOk : BTA_StatusRuntimeError;
            break;
        }
        default: {
            status = BTA_StatusNotSupported;
            if (progressReport) {
                (*progressReport)(BTA_StatusInvalidParameter, 0);
            }
            return BTA_StatusNotSupported;
        }
    }

    // Restore original frame-rate and integration time
    if (flashUpdateConfig->target != BTA_FlashTargetApplication) {
        status = BTAP100setFrameRate(winst, frameRateOriginal);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "BTAflashUpdate: Cannot restore frame-rate");
        }
        status = BTAP100setIntegrationTime(winst, integrationTimeOriginal);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "BTAflashUpdate: Cannot restore integration time");
        }
    }

    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAflashUpdate: Failed to update");
        if (progressReport) {
            progressReport(BTA_StatusRuntimeError, 0);
        }
        return status;
    }

    if (progressReport) {
        progressReport(BTA_StatusOk, 100);
    }
    return BTA_StatusOk;
}


BTA_Status BTAP100flashRead(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport, uint8_t quiet) {
    if (progressReport) {
        progressReport(BTA_StatusNotSupported, 0);
    }
    return BTA_StatusNotSupported;
}


BTA_Status BTAP100writeCurrentConfigToNvm(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    int result = saveConfig(inst->deviceIndex);
    if (result != P100_OKAY) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAflashUpdate: Internal error %d", result);
        return BTA_StatusRuntimeError;
    }
    return BTA_StatusOk;
}


BTA_Status BTAP100restoreDefaultConfig(BTA_WrapperInst *winst) {
    BTA_Status status;
    uint32_t regData;
    BTA_DeviceInfo *deviceInfo;
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    status = BTAP100getDeviceInfo(winst, &deviceInfo);
    if (status != BTA_StatusOk) {
        return status;
    }
    if (deviceInfo->firmwareVersionMajor >= 2 && deviceInfo->firmwareVersionMinor >= 1) {
        BTAfreeDeviceInfo(deviceInfo);
        regData = 0xf000000;
        return BTAP100writeRegister(winst, P100_REG_ADVANCED_FUNCTION, &regData, 0);
    }
    BTAfreeDeviceInfo(deviceInfo);
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTArestoreDefaultConfig: Not supported, firmware too old");
    return BTA_StatusNotSupported;
}


//NOT USED
//BTA_Status BTAwriteSerial(BTA_WrapperInst *winst, uint32_t serialNr){
//    BTA_P100LibInst *inst;
//    int res;
//    if (!handle) {
//        return BTA_StatusInvalidParameter;
//    }
//    inst = (BTA_P100LibInst *)handle;
//    res = saveSerial(inst->deviceIndex, serialNr);
//    if(res != P100_OKAY){
//        return BTA_StatusDeviceUnreachable;
//    }
//
//    return BTA_StatusOk;
//}



static void *captureRunFunction(void *handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    BTA_Frame *frame = 0;
    BTA_Status status;
    //uint64_t timeLastSuccess = BTAgetTickCount64();
    while (!inst->abortCaptureThread && !inst->closing) {
        if (winst->lpPauseCaptureThread) {
            BTAmsleep(250);
            continue;
        }
        status = getFrameInstant(winst, &frame);
        if (status == BTA_StatusOk && frame) {
            BTApostprocessGrabCallbackEnqueue(winst, frame);
            //timeLastSuccess = BTAgetTickCount64();
            continue;
        }
        else  {
            BTAmsleep(500);
            //if (BTAgetTickCount64() > timeLastSuccess + 7000) {
            //timeLastSuccess = BTAgetTickCount64();
            //BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, status, "USB captureThread: Got no frame");
        }
    }
    return 0;
}


static BTA_Status restoreZFactors(BTA_WrapperInst *winst, const uint8_t *filename, float **zFactors, uint16_t *xRes, uint16_t *yRes) {
    if (!winst || !filename || !zFactors || !xRes || !yRes) {
        return BTA_StatusInvalidParameter;
    }
    BTA_P100LibInst *inst = (BTA_P100LibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    *xRes = P100_WIDTH;
    *yRes = P100_HEIGHT;
    void *file;
    BTA_Status status = BTAfopen((char *)filename, "r", &file);
    if (status != BTA_StatusOk) {
        return status;
    }
    *zFactors = (float *)malloc(P100_WIDTH*P100_HEIGHT * sizeof(float));
    if (!*zFactors) {
        BTAfclose(file);
        return BTA_StatusOutOfMemory;
    }
    int i = 0;
    while (1) {
        char str[20];
        status = BTAfreadLine(file, str, sizeof(str));
        if (status != BTA_StatusOk) {
            BTAfclose(file);
            if (i == (P100_WIDTH*P100_HEIGHT)) {
                return BTA_StatusOk;
            }
            free(*zFactors);
            *zFactors = 0;
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInvalidParameter, "zFactors: End of file reached early %d", i);
            return BTA_StatusInvalidParameter;
        }
        if (str[0] == '#') {
            continue;
        }
        float value;
        int n = sscanf((const char *)str, "%f", &value);
        if (n != 1) {
            free(*zFactors);
            *zFactors = 0;
            BTAfclose(file);
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusRuntimeError, "zFactors: Parse error %d", i);
            return BTA_StatusRuntimeError;
        }
        if (i >= P100_WIDTH*P100_HEIGHT) {
            BTAfclose(file);
            free(*zFactors);
            *zFactors = 0;
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInvalidParameter, "zFactors: Too many values %d", i);
            return BTA_StatusInvalidParameter;
        }
        (*zFactors)[i++] = value;
    }
}


