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


struct BTA_UndistortInst;
struct BTA_JpgInst;
struct BVQ_QueueInst;



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


typedef struct BTA_DiscoveryInst {
    BTA_DeviceType deviceType;

    uint8_t *broadcastIpAddr;
    uint8_t broadcastIpAddrLen;
    uint16_t broadcastPort;
    uint8_t *callbackIpAddr;
    uint8_t callbackIpAddrLen;
    uint16_t callbackPort;

    int32_t uartBaudRate;
    uint8_t uartDataBits;
    uint8_t uartStopBits;
    uint8_t uartParity;
    uint8_t uartTransmitterAddress;
    uint8_t uartReceiverAddressStart;
    uint8_t uartReceiverAddressEnd;

    FN_BTA_DeviceFound deviceFound;
    uint8_t abortDiscovery;
    void *discoveryThread;

    BTA_InfoEventInst *infoEventInst;
} BTA_DiscoveryInst;


typedef struct BTA_WrapperInst {
    void *inst;
    BTA_InfoEventInst *infoEventInst;
    BTA_FrameArrivedInst *frameArrivedInst;

    BTA_GrabInst *grabInst;
    BFQ_FrameQueueHandle frameQueue;

    struct BTA_JpgInst *jpgInst;
    struct BTA_UndistortInst *undistortInst;

    BTA_Status (*close)(struct BTA_WrapperInst *winst);
    BTA_Status (*getDeviceInfo)(struct BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo);
    uint8_t (*isRunning)(struct BTA_WrapperInst *winst);
    uint8_t (*isConnected)(struct BTA_WrapperInst *winst);
    BTA_Status (*setFrameMode)(struct BTA_WrapperInst *winst, BTA_FrameMode frameMode);
    BTA_Status (*getFrameMode)(struct BTA_WrapperInst *winst, BTA_FrameMode *frameMode);
    BTA_Status(*getFrame)(struct BTA_WrapperInst *winst, BTA_Frame **frame, uint32_t millisecondsTimeout);
    BTA_Status(*flushFrameQueue)(struct BTA_WrapperInst *winst);
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
    BTA_Status (*startGrabbing)(struct BTA_WrapperInst *winst, uint8_t *libNameVer, BTA_DeviceInfo *deviceInfo, BTA_GrabbingConfig *config);
    BTA_Status (*flashUpdate)(struct BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport);
    BTA_Status (*flashRead)(struct BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport, uint8_t quiet);
    BTA_Status (*writeCurrentConfigToNvm)(struct BTA_WrapperInst *winst);
    BTA_Status (*restoreDefaultConfig)(struct BTA_WrapperInst *winst);

    // LibParams
    uint16_t lpTestPatternEnabled;

    float lpDataStreamReadFailedCount;
    float lpDataStreamBytesReceivedCount;
    float lpDataStreamPacketsReceivedCount;
    float lpDataStreamPacketsMissedCount;
    float lpDataStreamPacketsToParse;
    float lpDataStreamParseFrameDuration;
    float lpDataStreamFrameCounterGapsCount;
    float lpDataStreamFramesParsedCount;

    BVQ_QueueHandle lpDataStreamFramesParsedPerSecFrametimes;
    uint32_t lpDataStreamFramesParsedPerSecUpdated;

    uint8_t lpPauseCaptureThread;

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
    uint64_t timestamp;             // BTAinitFrameToParse sets it and BTAparseFrame resets it. This way we distinguish between initialized or not.
    uint16_t frameCounter;          // this is the only way to distinguish between frames
    uint32_t frameLen;              // length of the frame
    uint8_t *frame;                 // storage for a frame. packets are memcpied directly to this buffer
    uint16_t packetCountGot;        // counter for keeping track if the frame is complete
    uint16_t packetCountNda;        // counter for keeping track if the frame is complete
    uint16_t packetCountTotal;      // number of packets for the complete frame
    uint32_t *packetStartAddr;      // to remember the packet's position
    uint16_t *packetSize;           // to remember the packet's size (if == 0 then the packet is missing if == UINT16_MAX then the packet cannot be requested to be resent)
    //uint16_t packetCounterLast;     // to remember which packet was received last. Gaps provoke retransmission requests
    uint64_t timeLastPacket;        // to remember when we last received a packet
    uint64_t retryTime;             // the time when a retransmission request is done earliest
    uint16_t retryCount;            // counter for keeping track how many times a retransmission request was sent (only counting complete requests, not gap requests)
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

const uint16_t btaData4DescriptorTypeEof = 0xfffe;

const uint16_t btaData4DescriptorTypeFrameInfoV1 = 0x0001;
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


const uint16_t btaData4DescriptorTypeTofV1 = 0x0002;
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


const uint16_t btaData4DescriptorTypeTofWithMetadataV1 = 0x0003;
typedef struct BTA_Data4DescTofWithMetadataV1 {
    uint16_t descriptorType;
    uint16_t descriptorLen;
    uint32_t dataLen;
} BTA_Data4DescTofWithMetadataV1;


const uint16_t btaData4DescriptorTypeColorV1 = 0x0004;
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
    BTA_EthCommandFlashAmpCompensation = 23,      // obsolete
    BTA_EthCommandFlashGeometricModelParameters = 24,
    BTA_EthCommandFlashOverlayCalibration = 25,
    BTA_EthCommandFlashFactoryConfig = 31,
    BTA_EthCommandFlashPredefinedConfig = 32,
    BTA_EthCommandFlashIntrinsicTof = 41,      // obsolete
    BTA_EthCommandFlashIntrinsicColor = 42,      // obsolete
    BTA_EthCommandFlashIntrinsicStereo = 43,      // obsolete

    BTA_EthCommandReadBootloader = 111,
    BTA_EthCommandReadApplication = 112,
    BTA_EthCommandReadGeneric = 113,
    BTA_EthCommandReadLensCalib = 121,
    BTA_EthCommandReadWigglingCalib = 122,
    BTA_EthCommandReadAmpCompensation = 123,      // obsolete
    BTA_EthCommandReadGeometricModelParameters = 124,
    BTA_EthCommandReadOverlayCalibration = 125,
    BTA_EthCommandReadFactoryConfig = 131,
    BTA_EthCommandReadIntrinsicTof = 141,      // obsolete
    BTA_EthCommandReadIntrinsicColor = 142,      // obsolete
    BTA_EthCommandReadIntrinsicStereo = 143,      // obsolete

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
    BTA_EthImgModeIntensity = 20,
    BTA_EthImgModeDistAmpConfColor = 21,
    BTA_EthImgModeColor = 22,
    BTA_EthImgModeDistAmpBalance = 23,
    BTA_EthImgModeRawPhases = 24,
    BTA_EthImgModeRawQI = 25,
    BTA_EthImgModeDistConfExt = 26,
    BTA_EthImgModeAmp = 27,
    BTA_EthImgModeXYZConfColor = 28,
    BTA_EthImgModeXYZAmpColorOverlay = 29,
} BTA_EthImgMode;


typedef enum BTA_EthSubCommand { // now also for USB
    BTA_EthSubCommandNone = 0,
    BTA_EthSubCommandSpiFlash = 0,
    BTA_EthSubCommandParallelFlash = 1,
    BTA_EthSubCommandOtpFlash = 2
} BTA_EthSubCommand;


// now also for USB
#define BTA_ETH_CONTROL_PROTOCOL_VERSION    3
#define BTA_ETH_PREAMBLE_0                  0xa1
#define BTA_ETH_PREAMBLE_1                  0xec
#define BTA_ETH_HEADER_SIZE                 64

#define BTA_ETH_PACKET_HEADER_SIZE          32
#define BTA_ETH_FRAME_DATA_HEADER_SIZE      64


void BTAinfoEventHelperIIIIIIII(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, int par2, int par3, int par4, int par5, int par6, int par7, int par8);
void BTAinfoEventHelperISIIIII(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, uint8_t *par2, int par3, int par4, int par5, int par6, int par7);
void BTAinfoEventHelperIIIVI(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, int par2, int par3, void *par4, int par5);
void BTAinfoEventHelperIIIII(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, int par2, int par3, int par4, int par5);
void BTAinfoEventHelperIIII(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, int par2, int par3, int par4);
void BTAinfoEventHelperIII(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, int par2, int par3);
void BTAinfoEventHelperISF(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, uint8_t *par2, float par3);
void BTAinfoEventHelperSF(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, uint8_t *par1, float par2);
void BTAinfoEventHelperSV(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, uint8_t *par1, void *par2);
void BTAinfoEventHelperSI(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, uint8_t *par1, int par2);
void BTAinfoEventHelperIF(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, float par2);
void BTAinfoEventHelperII(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, int par2);
void BTAinfoEventHelperIS(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1, uint8_t *par2);
void BTAinfoEventHelperS(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, uint8_t *par1);
void BTAinfoEventHelperF(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, float par1);
void BTAinfoEventHelperI(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg, int par1);
void BTAinfoEventHelper(BTA_InfoEventInst *infoEventInst, uint8_t importance, BTA_Status status, const char *msg);


BTA_Status BTAparseGrabCallbackEnqueue(BTA_WrapperInst *winst, BTA_FrameToParse *frameToParse);
/*      @brief  Function that handles the image processing queue and consumes the frame, respectively frees it  */
BTA_Status BTAgrabCallbackEnqueue(BTA_WrapperInst *winst, BTA_Frame *frame);


void BTAgetFlashCommand(BTA_FlashTarget flashTarget, BTA_FlashId flashId, BTA_EthCommand *cmd, BTA_EthSubCommand *subCmd);
BTA_Status BTAhandleFileUpdateStatus(uint32_t fileUpdateStatus, FN_BTA_ProgressReport progressReport, BTA_InfoEventInst *infoEventInst, uint8_t *finished);


BTA_Status BTAreadMetrilusFromFlash(BTA_WrapperInst *winst, BTA_IntrinsicData *intData, uint8_t quiet);
BTA_Status BTAreadGeomModelFromFlash(BTA_WrapperInst *winst, BTA_IntrinsicData ***intrinsicData, uint16_t *intrinsicDataLen, BTA_ExtrinsicData ***extrinsicData, uint16_t *extrinsicDataLen, uint8_t quiet);
BTA_Status BTAfreeIntrinsicData(BTA_IntrinsicData ***intData, uint16_t intDataLen);
BTA_Status BTAfreeExtrinsicData(BTA_ExtrinsicData ***extData, uint16_t extDataLen);

BTA_Status flashUpdate(BTA_WrapperInst *winst, const uint8_t *filename, FN_BTA_ProgressReport progressReport, BTA_FlashTarget target);

BTA_Status BTAtoByteStream(BTA_EthCommand cmd, BTA_EthSubCommand subCmd, uint32_t addr, void *data, uint32_t length, uint8_t crcEnabled, uint8_t **result, uint32_t *resultLen, uint8_t callbackIpAddrVer, uint8_t *callbackIpAddr, uint8_t callbackIpAddrLen, uint16_t callbackPort, uint32_t packetNumber, uint32_t fileSize, uint32_t fileCrc32);
BTA_Status BTAparseControlHeader(uint8_t *request, uint8_t *data, uint32_t *payloadLength, uint32_t *flags, uint32_t *dataCrc32, BTA_InfoEventInst *infoEventInst);
BTA_Status BTAparseFrame(BTA_WrapperInst *winst, BTA_FrameToParse *frameToParse, BTA_Frame **framePtr);

#endif
