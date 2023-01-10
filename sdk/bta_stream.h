#ifndef BTA_WO_STREAM

#ifndef BTA_STREAM_H_INCLUDED
#define BTA_STREAM_H_INCLUDED

#ifdef PLAT_WINDOWS
#else
    #include <unistd.h>
    #include <time.h>
#endif


#include <bta.h>
#include <bta_helper.h>
#include <bta_helper.h>
#include <bvq_queue.h>
#include <bta_oshelper.h>
#include "bta_grabbing.h"

// externals
extern const char *btaGrabVersionKey;
extern const char *btaGrabPonKey;
extern const char *btaGrabDeviceTypeKey;
extern const char *btaGrabSerialNumberKey;
extern const char *btaGrabFirmwareVersionKey;
extern const char *btaGrabTotalFrameCountKey;
extern const char *btaGrabSeparator;

typedef struct BTA_StreamLibInst {

    void *parseThread;
    uint8_t closing;

    BVQ_QueueHandle frameAndIndexQueueInst;

    uint32_t fileFormatVersion;
    BTA_DeviceType deviceType;
    uint8_t *pon;
    uint32_t serialNumber;
    uint32_t firmwareVersionMajor;
    uint32_t firmwareVersionMinor;
    uint32_t firmwareVersionNonFunc;
    uint32_t totalFrameCount;

    float autoPlaybackSpeed;
    int32_t frameIndex;         ///< LibParam. Can always be read but only be written when realTimePlayback != 0
    int32_t frameIndexToSeek;   ///< LibParam. Applies only if >= 0 and realTimePlayback != 0
    void *bufferThread;
    uint8_t abortBufferThread;

    void *file;
    uint64_t filePos;
    uint64_t filePosMin;
    uint64_t filePosMax;
    int32_t frameIndexAtFilePos;


    uint8_t endReached;

    uint8_t *inputFilename;
} BTA_StreamLibInst;


//void *BTASTREAMdiscoveryRunFunction(BTA_DiscoveryInst *inst);
BTA_Status BTASTREAMopen(BTA_Config *config, BTA_WrapperInst *winst);
BTA_Status BTASTREAMclose(BTA_WrapperInst *winst);
BTA_Status BTASTREAMgetDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo);
BTA_Status BTASTREAMgetDeviceType(BTA_WrapperInst *winst, BTA_DeviceType *deviceType);
uint8_t BTASTREAMisRunning(BTA_WrapperInst *winst);
uint8_t BTASTREAMisConnected(BTA_WrapperInst *winst);
BTA_Status BTASTREAMsetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode frameMode);
BTA_Status BTASTREAMgetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode *frameMode);
BTA_Status BTASTREAMsetIntegrationTime(BTA_WrapperInst *winst, uint32_t integrationTime);
BTA_Status BTASTREAMgetIntegrationTime(BTA_WrapperInst *winst, uint32_t *integrationTime);
BTA_Status BTASTREAMsetFrameRate(BTA_WrapperInst *winst, float frameRate);
BTA_Status BTASTREAMgetFrameRate(BTA_WrapperInst *winst, float *frameRate);
BTA_Status BTASTREAMsetModulationFrequency(BTA_WrapperInst *winst, uint32_t modulationFrequency);
BTA_Status BTASTREAMgetModulationFrequency(BTA_WrapperInst *winst, uint32_t *modulationFrequency);
BTA_Status BTASTREAMsetGlobalOffset(BTA_WrapperInst *winst, float globalOffset);
BTA_Status BTASTREAMgetGlobalOffset(BTA_WrapperInst *winst, float *globalOffset);
BTA_Status BTASTREAMreadRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTASTREAMwriteRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTASTREAMsetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float value);
BTA_Status BTASTREAMgetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float *value);
BTA_Status BTASTREAMsendReset(BTA_WrapperInst *winst);
BTA_Status BTASTREAMflashUpdate(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport);
BTA_Status BTASTREAMflashRead(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport, uint8_t quiet);
BTA_Status BTASTREAMwriteCurrentConfigToNvm(BTA_WrapperInst *winst);
BTA_Status BTASTREAMrestoreDefaultConfig(BTA_WrapperInst *winst);

#endif

#endif