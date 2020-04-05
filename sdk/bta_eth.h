#ifndef BTA_WO_ETH

#ifndef BTA_ETH_H_INCLUDED
#define BTA_ETH_H_INCLUDED

#ifdef PLAT_WINDOWS
#   include <winsock2.h>
#   include <Ws2tcpip.h>
#else
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <sys/ioctl.h>
#   include <errno.h>
#   include <unistd.h>
#   include <time.h>
#endif


#include <bta.h>
#include "bta_helper.h"
#include "bta_frame_queueing.h"
#include <bvq_queue.h>
#include <bta_oshelper.h>
#include "bta_grabbing.h"
#include <undistort.h>
#include <circular_buffer.h>

#define BTA_ETH_IP_ADDR_VER_TO_LEN_LEN      7
#define BTA_ETH_IP_ADDR_VER_TO_LEN          0, 0, 0, 0, 4, 0, 10
#define BTA_ETH_IP_ADDR_LEN_TO_VER_LEN      17
#define BTA_ETH_IP_ADDR_LEN_TO_VER          0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6



#ifdef PLAT_LINUX
#   define SOCKET              int
#   define DWORD               unsigned long
#   define byte                unsigned char

#   define NO_ERROR            0
#   define SOCKET_ERROR        -1

#   define INVALID_SOCKET      -100  //TODO (just arbitrary value for now)
#endif


// prototypes



typedef struct BTA_EthLibInst {
    uint8_t closing;

    SOCKET udpDataSocket;
    SOCKET tcpControlSocket;
    SOCKET udpControlSocket;
    void *controlMutex;

    void *udpReadThread;
    void *parseFramesThread;
    void *connectionMonitorThread;

    uint8_t *udpDataIpAddr;
    uint8_t udpDataIpAddrVer;
    uint8_t udpDataIpAddrLen;
    uint16_t udpDataPort;
    uint8_t *udpControlDeviceIpAddr;
    uint8_t udpControlDeviceIpAddrVer;
    uint8_t udpControlDeviceIpAddrLen;
    uint16_t udpControlPort;
    uint8_t *udpControlCallbackIpAddr;
    uint8_t udpControlCallbackIpAddrVer;
    uint8_t udpControlCallbackIpAddrLen;
    uint16_t udpControlCallbackPort;
    uint8_t *tcpDeviceIpAddr;
    uint8_t tcpDeviceIpAddrLen;
    uint16_t tcpControlPort;

    cbuf_handle_t packetsToFillQueue;
    int udpDataQueueMallocCount;
    cbuf_handle_t packetsToParseQueue;
    BVQ_QueueHandle framesToParseQueue;

    uint32_t keepAliveMsgTimestamp;

    // Satus:
    // 0: deactivated
    // 1: activated, connection attempt will be made
    // 2: tcp only: connection lost, reconnecting
    // 3: udp only: connection lost, sending keepAlive messages waiting for a response
    // 8: first connection attempt, requesting alive message
    // 16: connected
    // 255: disconnected, closing
    uint8_t udpDataConnectionStatus;
    uint8_t udpControlConnectionStatus;
    uint8_t tcpControlConnectionStatus;

    // LibParams

    float lpKeepAliveMsgInterval;
    uint8_t lpControlCrcEnabled;

    int lpDataStreamRetrReqMode;
    float lpDataStreamRetrReqsCount;
    float lpRetrReqIntervalMin;
    //int parseFrameThreadInterval;
    //float transmissionTimePerPacketMax;
    float lpDataStreamPacketWaitTimeout;
    float lpDataStreamRetrReqMaxAttempts;
    float lpDataStreamRetrPacketsCount;
    float lpDataStreamNdasReceived;
    float lpDataStreamRedundantPacketCount;
} BTA_EthLibInst;


BTA_Status BTAETHstartDiscovery(BTA_DiscoveryConfig *discoveryConfig, FN_BTA_DeviceFound deviceFound, FN_BTA_InfoEvent infoEvent, BTA_Handle *handle);
BTA_Status BTAETHstopDiscovery(BTA_Handle *handle);
BTA_Status BTAETHopen(BTA_Config *config, BTA_WrapperInst *wrapperInst);
BTA_Status BTAETHclose(BTA_WrapperInst *winst);
BTA_Status BTAETHgetDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo);
uint8_t BTAETHisRunning(BTA_WrapperInst *winst);
uint8_t BTAETHisConnected(BTA_WrapperInst *winst);
BTA_Status BTAETHsetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode frameMode);
BTA_Status BTAETHgetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode *frameMode);
BTA_Status BTAETHsetIntegrationTime(BTA_WrapperInst *winst, uint32_t integrationTime);
BTA_Status BTAETHgetIntegrationTime(BTA_WrapperInst *winst, uint32_t *integrationTime);
BTA_Status BTAETHsetFrameRate(BTA_WrapperInst *winst, float frameRate);
BTA_Status BTAETHgetFrameRate(BTA_WrapperInst *winst, float *frameRate);
BTA_Status BTAETHsetModulationFrequency(BTA_WrapperInst *winst, uint32_t modulationFrequency);
BTA_Status BTAETHgetModulationFrequency(BTA_WrapperInst *winst, uint32_t *modulationFrequency);
BTA_Status BTAETHsetGlobalOffset(BTA_WrapperInst *winst, float globalOffset);
BTA_Status BTAETHgetGlobalOffset(BTA_WrapperInst *winst, float *globalOffset);
BTA_Status BTAETHreadRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTAETHwriteRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount);
BTA_Status BTAETHsetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float value);
BTA_Status BTAETHgetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float *value);
BTA_Status BTAETHsendReset(BTA_WrapperInst *winst);
BTA_Status BTAETHflashUpdate(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport);
BTA_Status BTAETHflashRead(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport, uint8_t quiet);
BTA_Status BTAETHwriteCurrentConfigToNvm(BTA_WrapperInst *winst);
BTA_Status BTAETHrestoreDefaultConfig(BTA_WrapperInst *winst);



#endif

#endif