#ifndef BTA_P100_H_INCLUDED
#define BTA_P100_H_INCLUDED

#include <bta.h>
#include <bta_helper.h>
#include <bta_discovery_helper.h>
#include "bta_grabbing.h"
#include <pthread.h>




#define BTA_P100_GLOBALOFFSET_FIRMWARE_OLD   0x20140716


#define DIST_AMP_FLAG_CALC_MODE             0x2069F7DF

#define BILAT_MAX_WINDOWSIZE                7 //7x7




// TODO change to LibParam
//----------- P100 Virtual Registers --------------
//start address is 0x100

#define P100_REG_ADDR_LIMIT                  0x100

// TODO change to LibParam
//#define P100_WRITE_WIGGLING_DATA_TO_FLASH   0x101 //write-only register; a read always yields 0 //write any value != 0 to save
#define P100_IMG_PROC_CONFIG                0x1E0
#define P100_FILTER_BILATERAL_CONFIG_2      0x1E6
//-------------------------------------------------
// TODO change to LibParam




typedef struct BTA_P100LibInst {
    int deviceIndex;

    void *handleMutex;
    uint8_t closing;
    uint8_t disableDataScaling;

    void *captureThread;
    uint8_t abortCaptureThread;

    float *dx_cust_values;
    float *dy_cust_values;
    float *dz_cust_values;

    BTA_FrameMode frameMode;
    uint32_t rawDataSize;

    void *frameQueueInst;

    //virtual register memory
    uint32_t imgProcConfig; //bit 3: 1..bilateral filter enabled
    uint8_t filterBilateralConfig2; //window size

    uint32_t version;

    BTA_GrabInst *grabInst;
} BTA_P100LibInst;


BTA_Status BTAP100open(BTA_Config *config, BTA_WrapperInst *winst);
BTA_Status BTAP100close(BTA_WrapperInst *winst);
BTA_Status BTAP100getDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo);
BTA_Status BTAP100getDeviceType(BTA_WrapperInst *winst, BTA_DeviceType *deviceType);
uint8_t BTAP100isRunning(BTA_WrapperInst *winst);
uint8_t BTAP100isConnected(BTA_WrapperInst *winst);
BTA_Status BTAP100setFrameMode(BTA_WrapperInst *winst, BTA_FrameMode frameMode);
BTA_Status BTAP100getFrameMode(BTA_WrapperInst *winst, BTA_FrameMode *frameMode);
BTA_Status BTAP100setIntegrationTime(BTA_WrapperInst *winst, uint32_t integrationTime);
BTA_Status BTAP100getIntegrationTime(BTA_WrapperInst *winst, uint32_t *integrationTime);
BTA_Status BTAP100setFrameRate(BTA_WrapperInst *winst, float frameRate);
BTA_Status BTAP100getFrameRate(BTA_WrapperInst *winst, float *frameRate);
BTA_Status BTAP100setModulationFrequency(BTA_WrapperInst *winst, uint32_t modulationFrequency);
BTA_Status BTAP100getModulationFrequency(BTA_WrapperInst *winst, uint32_t *modulationFrequency);
BTA_Status BTAP100setGlobalOffset(BTA_WrapperInst *winst, float globalOffset);
BTA_Status BTAP100getGlobalOffset(BTA_WrapperInst *winst, float *globalOffset);
BTA_Status BTAP100readRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTAP100writeRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTAP100setLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float value);
BTA_Status BTAP100getLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float *value);
BTA_Status BTAP100sendReset(BTA_WrapperInst *winst);
BTA_Status BTAP100flashUpdate(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport);
BTA_Status BTAP100flashRead(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport, uint8_t quiet);
BTA_Status BTAP100writeCurrentConfigToNvm(BTA_WrapperInst *winst);
BTA_Status BTAP100restoreDefaultConfig(BTA_WrapperInst *winst);


#endif