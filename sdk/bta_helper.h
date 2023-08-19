/**  @file bta_wrapper.h
*
*    @brief Header file for bta_wrapper.c
*
*    BLT_DISCLAIMER
*
*    @author Alex Falkensteiner
*
*    @cond svn
*
*    Information of last commit
*    $Rev::               $:  Revision of last commit
*    $Author::            $:  Author of last commit
*    $Date::              $:  Date of last commit
*
*    @endcond
*/

#ifndef BTA_HELPER_H_INCLUDED
#define BTA_HELPER_H_INCLUDED

#include <bta.h>

#include "bta_grabbing.h"
#include <bvq_queue.h>

#include "fifo.h"
#include <semaphore.h>
#if defined PLAT_LINUX || defined PLAT_APPLE
#include <sys/stat.h> // For mode constants
#include <fcntl.h> // For O_* constants
#include <sys/mman.h>
#include <unistd.h>
#endif

#if !defined PLAT_WINDOWS && !defined PLAT_LINUX && !defined PLAT_APPLE
#   error "Please define PLAT_WINDOWS, PLAT_LINUX or PLAT_APPLE in your makefile/project"
#endif


struct BTA_UndistortInst;
struct BTA_CalcXYZInst;
struct BTA_JpgInst;
struct BVQ_QueueInst;



typedef struct BTA_KeepAliveInst {
    uint64_t timeToProbe;
    uint64_t interval;
    uint8_t failcount;
} BTA_KeepAliveInst;


typedef struct BTA_InfoEventInst {
    BTA_Handle handle;
    FN_BTA_InfoEvent infoEvent;
    FN_BTA_InfoEventEx infoEventEx;
    FN_BTA_InfoEventEx2 infoEventEx2;
    uint8_t *infoEventFilename;
    uint8_t verbosity;
    void *userArg;
} BTA_InfoEventInst;


typedef struct BTA_FrameArrivedInst {
    BTA_Handle handle;
    FN_BTA_FrameArrived frameArrived;
    FN_BTA_FrameArrivedEx frameArrivedEx;
    FN_BTA_FrameArrivedEx2 frameArrivedEx2;
    void *userArg;
    BTA_FrameArrivedReturnOptions *frameArrivedReturnOptions;
} BTA_FrameArrivedInst;


typedef struct BTA_WrapperInst {
    void *inst;
    BTA_InfoEventInst *infoEventInst;
    BTA_FrameArrivedInst *frameArrivedInst;

    BTA_GrabInst *grabInst;
    BFQ_FrameQueueHandle frameQueue;

    struct BTA_JpgInst *jpgInst;
    struct BTA_UndistortInst *undistortInst;
    struct BTA_CalcXYZInst *calcXYZInst;

    uint32_t modFreqs[15];
    int modFreqsReadFromDevice;

    BTA_Status (*close)(struct BTA_WrapperInst *winst);
    BTA_Status (*getDeviceInfo)(struct BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo);
    BTA_Status(*getDeviceType)(struct BTA_WrapperInst *winst, BTA_DeviceType *deviceType);
    uint8_t (*isRunning)(struct BTA_WrapperInst *winst);
    uint8_t (*isConnected)(struct BTA_WrapperInst *winst);
    BTA_Status (*setFrameMode)(struct BTA_WrapperInst *winst, BTA_FrameMode frameMode);
    BTA_Status (*getFrameMode)(struct BTA_WrapperInst *winst, BTA_FrameMode *frameMode);
    BTA_Status (*setIntegrationTime)(struct BTA_WrapperInst *winst, uint32_t integrationTime);
    BTA_Status (*getIntegrationTime)(struct BTA_WrapperInst *winst, uint32_t *integrationTime);
    BTA_Status (*setFrameRate)(struct BTA_WrapperInst *winst, float frameRate);
    BTA_Status (*getFrameRate)(struct BTA_WrapperInst *winst, float *frameRate);
    BTA_Status (*setModulationFrequency)(struct BTA_WrapperInst *winst, uint32_t modulationFrequency);
    BTA_Status (*getModulationFrequency)(struct BTA_WrapperInst *winst, uint32_t *modulationFrequency);
    BTA_Status (*setGlobalOffset)(struct BTA_WrapperInst *winst, float globalOffset);
    BTA_Status (*getGlobalOffset)(struct BTA_WrapperInst *winst, float *globalOffset);
    BTA_Status (*readRegister)(struct BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
    BTA_Status (*writeRegister)(struct BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
    BTA_Status (*setLibParam)(struct BTA_WrapperInst *winst, BTA_LibParam libParam, float value);
    BTA_Status (*getLibParam)(struct BTA_WrapperInst *winst, BTA_LibParam libParam, float *value);
    BTA_Status (*sendReset)(struct BTA_WrapperInst *winst);
    BTA_Status (*flashUpdate)(struct BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport);
    BTA_Status (*flashRead)(struct BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport, uint8_t quiet);
    BTA_Status (*writeCurrentConfigToNvm)(struct BTA_WrapperInst *winst);
    BTA_Status (*restoreDefaultConfig)(struct BTA_WrapperInst *winst);


    // LibParams
    float lpDataStreamReadFailedCount;
    float lpDataStreamBytesReceivedCount;
    float lpDataStreamPacketsReceivedCount;
    float lpDataStreamPacketsMissedCount;
    float lpDataStreamPacketsToParse;
    float lpDataStreamParseFrameDuration;
    float lpDataStreamFrameCounterGap;
    float lpDataStreamFrameCounterGapsCount;
    float lpDataStreamFramesParsedCount;
    float lpAllowIncompleteFrames;

    uint32_t timeStampLast;
    uint32_t frameCounterLast;
    BVQ_QueueHandle lpDataStreamFramesParsedPerSecFrametimes;
    uint64_t lpDataStreamFramesParsedPerSecUpdated;

    uint8_t lpPauseCaptureThread;

    uint8_t lpBilateralFilterWindow;
    uint8_t lpCalcXyzEnabled;
    float lpCalcXyzOffset;
    uint8_t lpColorFromTofEnabled;
    uint8_t lpJpgDecodeEnabled;
    uint8_t lpUndistortRgbEnabled;

    uint32_t lpDebugFlags01;
    float lpDebugValue01;
    float lpDebugValue02;
    float lpDebugValue03;
    float lpDebugValue04;
    float lpDebugValue05;
    float lpDebugValue06;
    float lpDebugValue07;
    float lpDebugValue08;
    float lpDebugValue09;
    float lpDebugValue10;
} BTA_WrapperInst;




typedef struct BTA_FrameToParse {
    uint64_t timestamp;             ///< BTAinitFrameToParse sets it and BTAparseFrame resets it. This way we distinguish between initialized or not.
    uint16_t frameCounter;          ///< this is the only way to distinguish between frames
    uint32_t frameSize;             ///< length of the frame to be parsed
    uint32_t frameLen;              ///< length of the allocated buffer frame
    uint8_t *frame;                 ///< storage for a frame. packets are memcpied directly to this buffer
    uint16_t packetCountGot;        ///< counter for keeping track if the frame is complete
    uint16_t packetCountNda;        ///< counter for keeping track if the frame is complete
    uint16_t packetCountTotal;      ///< number of packets for the complete frame
    uint16_t packetStartAddrsLen;   ///< Size of allocated buffer packetStartAddrs
    uint32_t *packetStartAddrs;     ///< to remember the packet's position
    uint16_t packetSizesLen;        ///< Size of allocated buffer packetSizes
    uint16_t *packetSizes;          ///< to remember the packet's size (if == 0 then the packet is missing if == UINT16_MAX then the packet cannot be requested to be resent)
    //uint16_t packetCounterLast;     ///< to remember which packet was received last. Gaps provoke retransmission requests
    uint64_t timeLastPacket;        ///< to remember when we last received a packet
    uint64_t retryTime;             ///< the time when a retransmission request is done earliest
    uint16_t retryCount;            ///< counter for keeping track how many times a retransmission request was sent (only counting complete requests, not gap requests)

    uint32_t shmOffset;             ///< in case of shared memory, this is the 'id' that is returned to the camera's shared memory management
} BTA_FrameToParse;

BTA_Status BTAcreateFrameToParse(BTA_FrameToParse **frameToParse);
BTA_Status BTAinitFrameToParse(BTA_FrameToParse **frameToParse, uint64_t timestamp, uint16_t frameCounter, uint32_t frameLen, uint16_t packetCountTotal);
BTA_Status BTAfreeFrameToParse(BTA_FrameToParse **frameToParse);




// BltTofDataInterfaceProtocol descriptors

#pragma pack(push,1)
typedef struct BTA_UdpPackHead2 {
    uint16_t version;
    uint16_t frameCounter;
    uint16_t packetCounter;
    uint16_t packetDataLen;
    uint32_t frameLen;
    uint16_t crc16;
    uint8_t flags;
    uint8_t reserved;
    uint32_t packetPosition;
    uint16_t packetCountTotal;
} BTA_UdpPackHead2;


typedef struct BTA_Data4DescBase {
    uint16_t descriptorType;
    uint16_t descriptorLen;
    uint32_t dataLen;
} BTA_Data4DescBase;

#define btaData4DescriptorTypeEof               0xfffe
#define btaData4DescriptorTypeAliveMsgV1        0x0005

#define btaData4DescriptorTypeFrameInfoV1       0x0001
typedef struct BTA_Data4DescFrameInfoV1 {
    uint16_t descriptorType;
    uint16_t descriptorLen;
    uint32_t dataLen;
    uint16_t frameCounter;
    uint32_t timestamp;
    uint16_t mainTemp;
    uint16_t ledTemp;
    uint16_t genericTemp;
    uint16_t firmwareVersion;
} BTA_Data4DescFrameInfoV1;


#define btaData4DescriptorTypeTofV1             0x0002
typedef struct BTA_Data4DescTofV1 {
    uint16_t descriptorType;
    uint16_t descriptorLen;
    uint32_t dataLen;
    uint8_t lensIndex;
    uint8_t flags;
    uint32_t channelId;
    uint16_t width;
    uint16_t height;
    uint16_t dataFormat;
    uint8_t unit;
    uint8_t sequenceCounter;
    uint16_t integrationTime;
    uint16_t modulationFrequency;
} BTA_Data4DescTofV1;


#define btaData4DescriptorTypeTofWithMetadataV1 0x0003
typedef struct BTA_Data4DescTofWithMetadataV1 {
    uint16_t descriptorType;
    uint16_t descriptorLen;
    uint32_t dataLen;
} BTA_Data4DescTofWithMetadataV1;


#define btaData4DescriptorTypeColorV1           0x0004
typedef struct BTA_Data4DescColorV1 {
    uint16_t descriptorType;
    uint16_t descriptorLen;
    uint32_t dataLen;
    uint8_t lensIndex;
    uint8_t flags;
    uint16_t width;
    uint16_t height;
    uint16_t colorFormat;
    uint16_t gain;
    uint16_t integrationTime;
} BTA_Data4DescColorV1;


#define btaData4DescriptorTypeMetadataV1        0x0006
typedef struct BTA_Data4DescMetadataV1 {
    uint16_t descriptorType;
    uint16_t descriptorLen;
    uint32_t dataLen;
    uint8_t lensIndex;
    uint8_t flags;
    uint16_t reserved;
    uint32_t metadataId;
} BTA_Data4DescMetadataV1;
#pragma pack(pop)



typedef enum BTA_EthCommand { // now also for USB
    BTA_EthCommandNone = 0,
    BTA_EthCommandRead = 3,
    BTA_EthCommandWrite = 4,
    BTA_EthCommandReset = 7,
    BTA_EthCommandFlashBootloader = 11,
    BTA_EthCommandFlashApplication = 12,
    BTA_EthCommandFlashGeneric = 13,
    BTA_EthCommandFlashLensCalib = 21,
    BTA_EthCommandFlashWigglingCalib = 22,
    BTA_EthCommandFlashAmpCompensation = 23,            // obsolete
    BTA_EthCommandFlashGeometricModelParameters = 24,
    BTA_EthCommandFlashOverlayCalibration = 25,
    BTA_EthCommandFlashFPPN = 26,
    BTA_EthCommandFlashFPN = 27,
    BTA_EthCommandFlashDeadPixelList = 28,
    BTA_EthCommandFlashFactoryConfig = 31,
    BTA_EthCommandFlashPredefinedConfig = 32,
    BTA_EthCommandFlashXml = 33,
    // reserved = 34
    BTA_EthCommandFlashIntrinsicTof = 41,               // obsolete
    BTA_EthCommandFlashIntrinsicColor = 42,             // obsolete
    BTA_EthCommandFlashIntrinsicStereo = 43,            // obsolete

    BTA_EthCommandReadBootloader = 111,
    BTA_EthCommandReadApplication = 112,
    BTA_EthCommandReadGeneric = 113,
    BTA_EthCommandReadLensCalib = 121,
    BTA_EthCommandReadWigglingCalib = 122,
    BTA_EthCommandReadAmpCompensation = 123,            // obsolete
    BTA_EthCommandReadGeometricModelParameters = 124,
    BTA_EthCommandReadOverlayCalibration = 125,
    BTA_EthCommandReadFPPN = 126,
    BTA_EthCommandReadFPN = 127,
    BTA_EthCommandReadDeadPixelList = 128,
    BTA_EthCommandReadFactoryConfig = 131,
    // reserved = 132
    BTA_EthCommandReadXml = 133,
    BTA_EthCommandReadLogFiles = 134,
    BTA_EthCommandReadIntrinsicTof = 141,               // obsolete
    BTA_EthCommandReadIntrinsicColor = 142,             // obsolete
    BTA_EthCommandReadIntrinsicStereo = 143,            // obsolete

    BTA_EthCommandRetransmissionRequest = 241,
    BTA_EthCommandDiscovery = 253,
    BTA_EthCommandKeepAliveMsg = 254
} BTA_EthCommand;


typedef enum BTA_EthImgMode { // now also for USB
    BTA_EthImgModeNone = -1,
    BTA_EthImgModeDistAmp = 0,
    BTA_EthImgModeDistAmpConf = 1,
    BTA_EthImgModeDistAmpColor = 2,
    BTA_EthImgModeXYZ = 3,
    BTA_EthImgModeXYZAmp = 4,
    BTA_EthImgModeXYZColor = 5,
    BTA_EthImgModeDistColor = 6,
    BTA_EthImgModePhase0_90_180_270 = 7,
    BTA_EthImgModePhase270_180_90_0 = 8,
    BTA_EthImgModeDistXYZ = 9,
    BTA_EthImgModeXAmp = 10,
    BTA_EthImgModeTest = 11,
    BTA_EthImgModeDist = 12,
    BTA_EthImgModeRawdistAmp = 13,
    BTA_EthImgModePhase0_180 = 14,
    BTA_EthImgModePhase90_270 = 15,
    BTA_EthImgModePhase0 = 16,
    BTA_EthImgModePhase90 = 17,
    BTA_EthImgModePhase180 = 18,
    BTA_EthImgModePhase270 = 19,
    BTA_EthImgModeIntensities = 20,
    BTA_EthImgModeDistAmpConfColor = 21,
    BTA_EthImgModeColor = 22,
    BTA_EthImgModeDistAmpBalance = 23,
    BTA_EthImgModeRawPhases = 24,
    BTA_EthImgModeRawQI = 25,
    BTA_EthImgModeDistConfExt = 26,
    BTA_EthImgModeAmp = 27,
    BTA_EthImgModeXYZConfColor = 28,
    BTA_EthImgModeXYZAmpColorOverlay = 29,
    BTA_EthImgModeChannelSelection = 255,
} BTA_EthImgMode;


typedef enum BTA_EthSubCommand { // now also for USB
    BTA_EthSubCommandNone = 0,
    BTA_EthSubCommandSpiFlash = 0,
    BTA_EthSubCommandParallelFlash = 1,
    BTA_EthSubCommandOtpFlash = 2
} BTA_EthSubCommand;


typedef struct BTA_LenscalibHeader {
    uint16_t preamble0;
    uint16_t preamble1;
    uint16_t version;
    uint16_t hardwareConfig;
    uint16_t lensId;
    uint16_t xRes;
    uint16_t yRes;
    uint16_t bytesPerPixel;
    uint16_t expasionFactor;
    uint16_t coordSysId;
    uint16_t reserved[15];
} BTA_LenscalibHeader;


// now also for USB
#define BTA_ETH_CONTROL_PROTOCOL_VERSION    3
#define BTA_ETH_PREAMBLE_0                  0xa1
#define BTA_ETH_PREAMBLE_1                  0xec
#define BTA_ETH_HEADER_SIZE                 64

#define BTA_ETH_PACKET_HEADER_SIZE          32
#define BTA_ETH_FRAME_DATA_HEADER_SIZE      64


void BHLPzeroLogTimestamp();
void BTAinfoEventHelper(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, ...);

int BTAgetBytesPerPixelSum(BTA_EthImgMode imgMode);

BTA_Status BTAdeserializeFrameV1(BTA_Frame **frame, uint8_t *frameSerialized, uint32_t *frameSerializedLen);
BTA_Status BTAdeserializeFrameV2(BTA_Frame **frame, uint8_t *frameSerialized, uint32_t *frameSerializedLen);
BTA_Status BTAdeserializeFrameV3(BTA_Frame **frame, uint8_t *frameSerialized, uint32_t *frameSerializedLen);
BTA_Status BTAdeserializeFrameV4(BTA_Frame **frame, uint8_t *frameSerialized, uint32_t *frameSerializedLen);

void BTAgetFlashCommand(BTA_FlashUpdateConfig *flashUpdateConfig, BTA_EthCommand *cmd, BTA_EthSubCommand *subCmd);
BTA_Status BTAhandleFileUpdateStatus(uint32_t fileUpdateStatus, FN_BTA_ProgressReport progressReport, BTA_InfoEventInst *infoEventInst, uint8_t *finished);


BTA_Status BTAreadMetrilusFromFlash(BTA_WrapperInst *winst, BTA_IntrinsicData *intData, uint8_t quiet);
BTA_Status BTAreadGeomModelFromFlash(BTA_WrapperInst *winst, BTA_IntrinsicData ***intrinsicData, uint16_t *intrinsicDataLen, BTA_ExtrinsicData ***extrinsicData, uint16_t *extrinsicDataLen, uint8_t quiet);
BTA_Status BTAfreeIntrinsicData(BTA_IntrinsicData ***intData, uint16_t intDataLen);
BTA_Status BTAfreeExtrinsicData(BTA_ExtrinsicData ***extData, uint16_t extDataLen);

BTA_Status flashUpdate(BTA_WrapperInst *winst, const uint8_t *filename, FN_BTA_ProgressReport progressReport, BTA_FlashTarget target);

BTA_Status BTAtoByteStream(BTA_EthCommand cmd, BTA_EthSubCommand subCmd, uint32_t addr, void *data, uint32_t length, uint8_t crcEnabled, uint8_t **result, uint32_t *resultLen, uint8_t callbackIpAddrVer, uint8_t *callbackIpAddr, uint8_t callbackIpAddrLen, uint16_t callbackPort, uint32_t packetNumber, uint32_t fileSize, uint32_t fileCrc32);
BTA_Status BTAparseControlHeader(uint8_t *request, uint8_t *data, uint32_t *payloadLength, uint32_t *flags, uint32_t *dataCrc32, uint8_t *parseError, BTA_InfoEventInst *infoEventInst);
BTA_Status BTAparseFrame(BTA_WrapperInst *winst, BTA_FrameToParse *frameToParse, BTA_Frame **framePtr);

BTA_Status BTAparsePostprocessGrabCallbackEnqueue(BTA_WrapperInst *winst, BTA_FrameToParse *frameToParse);
/*      @brief  Function that handles the image processing queue and consumes the frame, respectively frees it  */
void BTApostprocessGrabCallbackEnqueue(BTA_WrapperInst *winst, BTA_Frame *frame);

BTA_Status BTAparseLenscalib(uint8_t* data, uint32_t dataLen, BTA_LensVectors** calcXYZVectors, BTA_InfoEventInst *infoEventInst);


int initShm(uint32_t shmKeyNum, uint32_t shmSize, int32_t *shmFd, uint8_t **bufShmBase, sem_t **semFullWrite, sem_t **semFullRead, sem_t **semEmptyWrite, sem_t **semEmptyRead, fifo_t **fifoFull, fifo_t **fifoEmpty, uint8_t **bufDataBase, BTA_InfoEventInst *infoEventInst);
void closeShm(sem_t **semEmptyRead, sem_t **semEmptyWrite, sem_t **semFullRead, sem_t **semFullWrite, uint8_t **bufShmBase, uint32_t shmSize, int32_t *shmFd, uint32_t shmKeyNum, BTA_InfoEventInst *infoEventInst);
BTA_Status BTAparseFrameFromShm(BTA_Handle handle, uint8_t *data, BTA_Frame **framePtr);
BTA_Status BTAgrabCallbackEnqueueFromShm(BTA_WrapperInst *winst, BTA_Frame *frame);

#endif
