#ifndef BTA_WO_ETH

#include <bta.h>
#include "bta_helper.h"
#include <bta_flash_update.h>
#include <bta_frame.h>
#include <bta_status.h>
#include <mth_math.h>
#include "configuration.h"

#include "bta_eth.h"
#include "bta_eth_helper.h"
#include <bta_oshelper.h>
#include <timing_helper.h>
#include <pthread_helper.h>
#include <sockets_helper.h>
#include <utils.h>

#ifdef PLAT_WINDOWS
#elif defined PLAT_LINUX
#   include <sys/time.h>
#   include <netdb.h>
#   include <netinet/in.h>
#   include <ifaddrs.h>
#   define u_long uint32_t
#elif defined PLAT_APPLE
#   include <sys/select.h>
#   include <sys/time.h>
#   include <sys/types.h>
#   include <netdb.h>
#   include <stdbool.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <crc16.h>
#include <crc32.h>


//////////////////////////////////////////////////////////////////////////////////
// Local prototypes
static BTA_Status writeOrCheckDeviceUdpDataSettings(BTA_WrapperInst *winst);

static void *udpReadRunFunction(void *handle);
static void *parseFramesRunFunction(void *handle);
static void *shmReadRunFunction(void *handle);

static BTA_FrameToParse *getNewer(BTA_FrameToParse **framesToParse, int framesToParseLen, BTA_FrameToParse *ftpRef);
static BTA_FrameToParse *getOldest(BTA_FrameToParse **framesToParse, int framesToParseLen);
static BTA_FrameToParse *getOlderThan(BTA_FrameToParse **framesToParse, int framesToParseLen, uint64_t olderEqualThan);
static BTA_Status sendRetrReq(BTA_WrapperInst *winst, uint16_t frameCounter, uint16_t *packetCounters, int packetCountersLen);
static BTA_Status sendRetrReqComplete(BTA_WrapperInst *winst, BTA_FrameToParse *frameToParse);
static BTA_Status sendRetrReqGap(BTA_WrapperInst *winst, BTA_FrameToParse *frameToParse, uint16_t pcGapBeg, uint16_t pcGapEnd);

static void *connectionMonitorRunFunction(void *handle);
static BTA_Status sendKeepAliveMsg(BTA_WrapperInst *inst);

static BTA_Status readRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount, uint32_t timeout);

static SOCKET openDiscoveryRcvSocket(uint16_t *callbackPort, BTA_InfoEventInst *infoEventInst);
static void fillDiscoveryBuffer(uint8_t  buffer[64], uint16_t deviceType, uint8_t callbackIpAddr[4], uint16_t callbackPort);
static BTA_Status openDiscoverySendSockets(uint16_t broadcastPort, SOCKET *sockets, int *socketsCount, BTA_InfoEventInst *infoEventInst);
static BTA_Status sendDiscoveryMessage(SOCKET *sockets, int socketsCount, uint8_t *buffer, uint32_t bufferLen, uint8_t *broadcastIpAddr, uint16_t broadcastPort, BTA_InfoEventInst *infoEventInst);
static BTA_Status closeSockets(SOCKET *sockets, int socketsCount);
static BTA_DeviceInfo *parseDiscoveryResponse(uint8_t *responsePayload, uint32_t responseLen, void* deviceListMutex, BTA_DeviceInfo **deviceList, uint16_t deviceListCount, BTA_InfoEventInst *infoEventInst);

static BTA_Status receiveControlResponse(BTA_WrapperInst *inst, uint8_t *request, uint8_t **data, uint32_t *dataLen, uint32_t timeout, FN_BTA_ProgressReport progressReport);
// receiveControlResponse_2 is only needed in discovery where we don't have an instance. could be worked around..
static BTA_Status receiveControlResponse_2(uint8_t *request, uint8_t **data, uint32_t *dataLen, uint32_t timeout, FN_BTA_ProgressReport progressReport,
                                           uint8_t *udpControlConnectionStatus, SOCKET *udpControlSocket, uint8_t *udpControlCallbackIpAddr, uint16_t udpControlCallbackPort, uint8_t *tcpControlConnectionStatus, SOCKET *tcpControlSocket,
                                           uint32_t *keepAliveMsgTimestamp, float lpKeepAliveMsgInterval, BTA_InfoEventInst *infoEventInst);

static BTA_Status receive(uint8_t *data, uint32_t *length,
                          uint8_t *udpControlConnectionStatus, SOCKET *udpControlSocket, uint8_t *udpControlCallbackIpAddr, uint16_t udpControlCallbackPort, uint8_t *tcpControlConnectionStatus, SOCKET *tcpControlSocket,
                          uint32_t timeout, uint32_t *keepAliveMsgTimestamp, float lpKeepAliveMsgInterval, BTA_InfoEventInst *infoEventInst);

static BTA_Status transmit(BTA_WrapperInst *inst, uint8_t *data, uint32_t length, uint32_t timeout);
// transmit_2 is only needed in discovery where we don't have an instance. could be worked around..
static BTA_Status transmit_2(uint8_t *data, uint32_t length, uint32_t timeout,
                             uint8_t *udpControlConnectionStatus, SOCKET *udpControlSocket, uint8_t *udpControlDeviceIpAddr, uint16_t udpControlPort, uint8_t *tcpControlConnectionStatus, SOCKET *tcpControlSocket,
                             BTA_InfoEventInst *infoEventInst);


static const uint8_t ipAddrLenToVer[BTA_ETH_IP_ADDR_LEN_TO_VER_LEN] = { BTA_ETH_IP_ADDR_LEN_TO_VER };

static const uint32_t timeoutDefault = 4000;
static const uint32_t timeoutHuge = 120000;
static const uint32_t timeoutBigger = 30000;
//static const uint32_t timeoutBig = 15000;
//static const uint32_t timeoutSmall = 1000;
static const uint32_t timeoutTiny = 50;

#define RETR_REQ_PACKET_COUNT_MAX 128
static const int retrReqPacketCountMax = RETR_REQ_PACKET_COUNT_MAX;
static uint16_t retrReqPacketCounters[RETR_REQ_PACKET_COUNT_MAX];


static const uint16_t udpPacketLenMax = 0xffff;
static const int udpDataQueueLen = 5000;
static const int udpDataQueueLenPrealloc = 500;

#ifdef PLAT_WINDOWS
#   define ERROR_TRY_AGAIN WSAETIMEDOUT
#   define ERROR_IN_PROGRESS WSAEWOULDBLOCK
#elif defined PLAT_LINUX || defined PLAT_APPLE
#   define closesocket(x) close(x)
#   define ioctlsocket ioctl
#   define ERROR_TRY_AGAIN EAGAIN
#   define ERROR_IN_PROGRESS EINPROGRESS
#endif



BTA_Status BTAETHopen(BTA_Config *config, BTA_WrapperInst *winst) {
    BTA_Status status;
    if (!config || !winst) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: config or handle missing");
        return BTA_StatusInvalidParameter;
    }

    int configTest = !!config->udpDataIpAddr + !!config->udpDataIpAddrLen + !!config->udpDataPort;
    if (configTest != 0 && configTest != 3) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: Please use all or none of udpDataIpAddr, udpDataIpAddrLen and udpDataPort");
        return BTA_StatusInvalidParameter;
    }
    if (config->udpDataIpAddr && config->udpDataAutoConfig) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: UDP data connection information is given, but udpDataAutoConfig is set");
        return BTA_StatusInvalidParameter;
    }
    configTest = !!config->tcpDeviceIpAddr + !!config->tcpDeviceIpAddrLen + !!config->tcpControlPort;
    if (configTest != 0 && configTest != 3) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: Please use all or none of tcpDeviceIpAddr, tcpDeviceIpAddrLen and tcpControlPort");
        return BTA_StatusInvalidParameter;
    }
    configTest = !!config->udpControlOutIpAddr + !!config->udpControlOutIpAddrLen + !!config->udpControlPort;
    if (configTest != 0 && configTest != 3) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: Please use all or none of udpControlOutIpAddr, udpControlOutIpAddrLen and udpControlPort");
        return BTA_StatusInvalidParameter;
    }

    if (!config->tcpDeviceIpAddr && !config->udpControlOutIpAddr && !config->udpDataIpAddr) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: Set at least one prameter out of tcpDeviceIpAddr, udpControlOutIpAddr and udpDataIpAddr");
        return BTA_StatusInvalidParameter;
    }

    winst->inst = calloc(1, sizeof(BTA_EthLibInst));
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusOutOfMemory;
    }

    // Initialize LibParams that are not defaultly 0
    inst->lpKeepAliveMsgInterval = 2;

    inst->lpDataStreamRetrReqMode = 1;
    inst->lpDataStreamPacketWaitTimeout = 50;
    // 100 is apparently too small, not enough time to get an answer with a switch on windows with 50 fps DistAmp
    // 150 seems to be a minimum. at high frame-rates a second request is futile. with big frames the answer won't come faster (sender is so busy with big frames)
    inst->lpRetrReqIntervalMin = 50;
    inst->lpDataStreamRetrReqMaxAttempts = 5;

    if (config->pon) {
        // TODO: support when merging USB with ETH
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusConfigParamError, "BTAopen Eth: Parameter pon ignored, not supported");
    }
    if (config->serialNumber) {
        // TODO: support when merging USB with ETH
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusConfigParamError, "BTAopen Eth: Parameter serialNumber ignored, not supported");
    }

    status = BTAinitMutex(&inst->controlMutex);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen Eth: Cannot init controlMutex");
        BTAETHclose(winst);
        return status;
    }

#   ifdef PLAT_WINDOWS
    WSADATA wsaData;
    int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (err != NO_ERROR) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "Error in WSAStartup, error: %d", err);
        BTAETHclose(winst);
        return BTA_StatusRuntimeError;
    }
#   endif

    uint8_t udpDataWanted = (config->udpDataIpAddr && config->udpDataPort) || config->udpDataAutoConfig;
    uint8_t shmDataWanted = !!config->shmDataEnabled;
    uint8_t udpControlWanted = config->udpControlOutIpAddr && config->udpControlPort;
    uint8_t tcpControlWanted = config->tcpDeviceIpAddr && config->tcpControlPort;
    if (!udpDataWanted && !udpControlWanted && !tcpControlWanted) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: No connection information. Aborting.");
        BTAETHclose(winst);
        return BTA_StatusInvalidParameter;
    }

#   ifndef PLAT_LINUX
    if (shmDataWanted) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: Shm data Only supported on linux systems");
        BTAETHclose(winst);
        return BTA_StatusNotSupported;
    }
#   endif
    if (shmDataWanted && udpDataWanted) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: Shm data and UDP data shouldn't be used simultaneously. Aborting.");
        BTAETHclose(winst);
        return BTA_StatusInvalidParameter;
    }
    if (shmDataWanted && !udpControlWanted && !tcpControlWanted) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: Shm relies on a control connection. Aborting.");
        BTAETHclose(winst);
        return BTA_StatusInvalidParameter;
    }
    if (shmDataWanted && config->frameQueueMode != BTA_QueueModeDoNotQueue) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: Shm data: Please use frameArrived. Queue is not supported yet. Aborting.");
        BTAETHclose(winst);
        return BTA_StatusInvalidParameter;
    }

    if (!config->frameArrived && !config->frameArrivedEx && !config->frameArrivedEx2 && config->frameQueueMode == BTA_QueueModeDoNotQueue && udpDataWanted) {
        // No way to get frames without queueing or callback
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: A data interface connection is given, but queueing and frameArrived callback are disabled");
        BTAETHclose(winst);
        return BTA_StatusInvalidParameter;
    }

    inst->udpDataAutoConfig = config->udpDataAutoConfig > 0 ? 1 : 0;
    if (config->udpDataIpAddr) {
        if (config->udpDataIpAddrLen != 4) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: Only IPv4 supported, set udpDataIpAddrLen = 4");
            BTAETHclose(winst);
            return BTA_StatusInvalidParameter;
        }        
        if (!BTAisLocalIpAddrOrMulticast(config->udpDataIpAddr, config->udpDataIpAddrLen)) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: udpDataIpAddr %d.%d.%d.%d cannot be used because it's not a local or multicast address",
                                   config->udpDataIpAddr[0], config->udpDataIpAddr[1], config->udpDataIpAddr[2], config->udpDataIpAddr[3]);
            BTAETHclose(winst);
            return BTA_StatusInvalidParameter;
        }
        inst->udpDataIpAddrLen = config->udpDataIpAddrLen;
        inst->udpDataIpAddr = (uint8_t*)malloc(inst->udpDataIpAddrLen);
        if (!inst->udpDataIpAddr) {
            BTAETHclose(winst);
            return BTA_StatusOutOfMemory;
        }
        memcpy(inst->udpDataIpAddr, config->udpDataIpAddr, inst->udpDataIpAddrLen);
        inst->udpDataIpAddrVer = ipAddrLenToVer[inst->udpDataIpAddrLen];
    }
    inst->udpDataPort = config->udpDataPort;

    if (config->udpControlOutIpAddr) {
        if (config->udpControlOutIpAddrLen != 4) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: Only IPv4 supported, set udpControlOutIpAddrLen = 4");
            BTAETHclose(winst);
            return BTA_StatusInvalidParameter;
        }
        status = BTAgetMatchingLocalAddress(config->udpControlOutIpAddr, config->udpControlOutIpAddrLen, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: udpControlOutIpAddr %d.%d.%d.%d cannot be used because it's not in my subnet",
                               config->udpControlOutIpAddr[0], config->udpControlOutIpAddr[1], config->udpControlOutIpAddr[2], config->udpControlOutIpAddr[3]);
            BTAETHclose(winst);
            return BTA_StatusInvalidParameter;
        }
        inst->udpControlDeviceIpAddrLen = config->udpControlOutIpAddrLen;
        inst->udpControlDeviceIpAddr = (uint8_t*)malloc(inst->udpControlDeviceIpAddrLen);
        if (!inst->udpControlDeviceIpAddr) {
            BTAETHclose(winst);
            return BTA_StatusOutOfMemory;
        }
        memcpy(inst->udpControlDeviceIpAddr, config->udpControlOutIpAddr, inst->udpControlDeviceIpAddrLen);
        inst->udpControlDeviceIpAddrVer = ipAddrLenToVer[inst->udpControlDeviceIpAddrLen];
    }
    inst->udpControlPort = config->udpControlPort;

    if (config->udpControlInIpAddr) {
        if (config->udpControlInIpAddrLen != 4) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: Only IPv4 supported, set udpControlInIpAddrLen = 4");
            BTAETHclose(winst);
            return BTA_StatusInvalidParameter;
        }
        inst->udpControlCallbackIpAddrLen = config->udpControlInIpAddrLen;
        inst->udpControlCallbackIpAddr = (uint8_t*)malloc(inst->udpControlCallbackIpAddrLen);
        if (!inst->udpControlCallbackIpAddr) {
            BTAETHclose(winst);
            return BTA_StatusOutOfMemory;
        }
        memcpy(inst->udpControlCallbackIpAddr, config->udpControlInIpAddr, inst->udpControlCallbackIpAddrLen);
        inst->udpControlCallbackIpAddrVer = ipAddrLenToVer[inst->udpControlCallbackIpAddrLen];
    }
    inst->udpControlCallbackPort = config->udpControlCallbackPort;

    if (config->tcpDeviceIpAddr) {
        if (config->tcpDeviceIpAddrLen != 4) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: Only IPv4 supported, set tcpDeviceIpAddrLen = 4");
            BTAETHclose(winst);
            return BTA_StatusInvalidParameter;
        }
        status = BTAgetMatchingLocalAddress(config->tcpDeviceIpAddr, config->tcpDeviceIpAddrLen, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "BTAopen Eth: udpControlOutIpAddr %d.%d.%d.%d cannot be used because it's not in my subnet",
                               config->tcpDeviceIpAddr[0], config->tcpDeviceIpAddr[1], config->tcpDeviceIpAddr[2], config->tcpDeviceIpAddr[3]);
            BTAETHclose(winst);
            return BTA_StatusInvalidParameter;
        }
        inst->tcpDeviceIpAddrLen = config->tcpDeviceIpAddrLen;
        inst->tcpDeviceIpAddr = (uint8_t*)malloc(inst->tcpDeviceIpAddrLen);
        if (!inst->tcpDeviceIpAddr) {
            BTAETHclose(winst);
            return BTA_StatusOutOfMemory;
        }
        memcpy(inst->tcpDeviceIpAddr, config->tcpDeviceIpAddr, inst->tcpDeviceIpAddrLen);
    }
    inst->tcpControlPort = config->tcpControlPort;

    inst->udpDataSocket = INVALID_SOCKET;
    inst->udpControlSocket = INVALID_SOCKET;
    inst->tcpControlSocket = INVALID_SOCKET;


    // Activate first control connection
    if (udpControlWanted) {
        inst->udpControlConnectionStatus = 1;
    }

    // Activate second control connection
    if (tcpControlWanted) {
        inst->tcpControlConnectionStatus = 1;
    }

    // Activate data connection
    if (udpDataWanted) {
        inst->udpDataConnectionStatus = 1;
    }

    // Start connection monitor thread
    status = BTAcreateThread(&(inst->connectionMonitorThread), &connectionMonitorRunFunction, (void *)winst, 0);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen Eth: Could not start connectionMonitorThread");
        BTAETHclose(winst);
        return status;
    }

    // Wait for connections to establish. the timeout shall depend on the number of active control interfaces plus margin
    uint32_t endTime = BTAgetTickCount() + (udpControlWanted * timeoutDefault + tcpControlWanted * timeoutDefault) + 1000;
    while (1) {
        BTAmsleep(250);
        // lock in order to see if the connection is really up (not only pending alive msg)
        BTAlockMutex(inst->controlMutex);
        uint8_t controlOk = (!udpControlWanted && !tcpControlWanted) || inst->udpControlConnectionStatus == 16 || inst->tcpControlConnectionStatus == 16;
        BTAunlockMutex(inst->controlMutex);
        uint8_t dataOk = !udpDataWanted || inst->udpDataConnectionStatus == 16;
        if (controlOk && dataOk) {
            break;
        }
        if (BTAgetTickCount() > endTime) {
            if (inst->udpDataAutoConfig != -1) {  // in case autoconfig is not supported, don't produce a second infoEvent
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusDeviceUnreachable, "BTAopen Eth: timeout connecting to the device, see log");
            }
            BTAETHclose(winst);
            return BTA_StatusDeviceUnreachable;
        }
    }

    if (inst->udpControlConnectionStatus == 16 || inst->tcpControlConnectionStatus == 16) {
        status = BTAETHsetFrameMode(winst, config->frameMode);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAopen Eth: Error in BTAsetFrameMode: %s", BTAstatusToString2(status));
        }
    }

    if (udpDataWanted) {
        status = BCBinit(udpDataQueueLen, &inst->packetsToParseQueue);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen Eth: Could not init packetsToParseQueue");
            BTAETHclose(winst);
            return status;
        }
        status = BCBinit(udpDataQueueLen, &inst->packetsToFillQueue);
        if (!inst->packetsToFillQueue) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen Eth: Could not init packetsToFillQueue");
            BTAETHclose(winst);
            return status;
        }

        for (int i = 0; i < udpDataQueueLenPrealloc; i++) {
            BTA_MemoryArea *udpPacket;
            status = BTAinitMemoryArea(&udpPacket, udpPacketLenMax);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusOutOfMemory, "BTAopen Eth: Can't allocate more packet buffers than %d", inst->udpDataQueueMallocCount);
                break;
            }
            status = BCBput(inst->packetsToFillQueue, (void *)udpPacket);
            assert(status == BTA_StatusOk); // There are max as many packets around as the queue is long
            inst->udpDataQueueMallocCount++;
        }

        status = BTAcreateThread(&(inst->udpReadThread), udpReadRunFunction, (void *)winst, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen Eth: Could not start udpReadThread");
            BTAETHclose(winst);
            return status;
        }

        status = BTAcreateThread(&(inst->parseFramesThread), parseFramesRunFunction, (void *)winst, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen Eth: Could not start parseFramesThread");
            BTAETHclose(winst);
            return status;
        }
        //status = BTAsetRealTimePriority(inst->udpReadThread);
    }

    if (shmDataWanted) {
        status = BTAcreateThread(&(inst->shmReadThread), shmReadRunFunction, (void *)winst, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "BTAopen Eth: Could not start shmReadThread");
            BTAETHclose(winst);
            return status;
        }
    }

    return BTA_StatusOk;
}


BTA_Status BTAETHclose(BTA_WrapperInst *winst) {
    if (!winst) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAclose Eth: winst missing!");
        return BTA_StatusInvalidParameter;
    }
    BTA_Status status;
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
    if (!inst) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAclose Eth: inst missing!");
        return BTA_StatusInvalidParameter;
    }
    inst->closing = 1;
    status = BTAjoinThread(inst->shmReadThread);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose Eth: Failed to join shmReadThread");
    }
    status = BTAjoinThread(inst->udpReadThread);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose Eth: Failed to join udpReadThread");
    }
    status = BTAjoinThread(inst->parseFramesThread);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose Eth: Failed to join parseFramesThread");
    }
    status = BCBfree(inst->packetsToParseQueue, (BTA_Status(*)(void **))&BTAfreeMemoryArea);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose Eth: Failed to close packetsToParseQueue");
    }
    status = BCBfree(inst->packetsToFillQueue, (BTA_Status(*)(void **))&BTAfreeMemoryArea);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose Eth: Failed to close packetsToFillQueue");
    }

    status = BTAjoinThread(inst->connectionMonitorThread);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose Eth: Failed to join connectionMonitorThread");
    }

    free(inst->tcpDeviceIpAddr);
    inst->tcpDeviceIpAddr = 0;
    free(inst->udpControlCallbackIpAddr);
    inst->udpControlCallbackIpAddr = 0;
    free(inst->udpControlDeviceIpAddr);
    inst->udpControlDeviceIpAddr = 0;
    free(inst->udpDataIpAddr);
    inst->udpDataIpAddr = 0;

    status = BTAcloseMutex(inst->controlMutex);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAclose: Failed to close controlMutex");
    }
    free(inst);
    winst->inst = 0;

#   ifdef PLAT_WINDOWS
    WSACleanup();
#   endif
    return BTA_StatusOk;
}


BTA_Status BTAETHgetDeviceType(BTA_WrapperInst *winst, BTA_DeviceType *deviceType) {
    if (!deviceType) {
        return BTA_StatusInvalidParameter;
    }
    *deviceType = BTA_DeviceTypeEthernet;
    BTA_Status status;
    uint32_t reg;
    status = BTAETHreadRegister(winst, 0x0006, &reg, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    *deviceType = (BTA_DeviceType)reg;
    return BTA_StatusOk;
}


static BTA_Status BTAETHgetMacAddr(BTA_WrapperInst *winst, uint64_t *macAddr) {
    if (!macAddr) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t dataAddress = 0x0241;
    uint32_t data[3];
    uint32_t dataCount = 3;
    BTA_Status status = BTAETHreadRegister(winst, dataAddress, data, &dataCount);
    if (status != BTA_StatusOk) {
        return status;
    }
    *macAddr = ((uint64_t)data[0x0241 - dataAddress] << 32) | ((uint64_t)data[0x0242 - dataAddress] << 16) | ((uint64_t)data[0x0243 - dataAddress]);
    return BTA_StatusOk;
}


static BTA_Status getDeviceInfoSlow(BTA_WrapperInst *winst, BTA_DeviceInfo *info) {
    uint32_t reg;

    uint32_t ponPart1;
    BTA_Status status = BTAETHreadRegister(winst, 0x0570, &ponPart1, 0);
    if (status != BTA_StatusOk) {
        ponPart1 = 0;
    }
    uint32_t ponPart2 = 0;
    if (ponPart1) {
        status = BTAETHreadRegister(winst, 0x0571, &ponPart2, 0);
        if (status != BTA_StatusOk) {
            ponPart2 = 0;
        }
    }
    uint32_t deviceRevisionMajor = 0;
    if (ponPart1 && ponPart2) {
        status = BTAETHreadRegister(winst, 0x0572, &deviceRevisionMajor, 0);
        if (status != BTA_StatusOk) {
            deviceRevisionMajor = 0;
        }
    }
    if (ponPart1 && ponPart2) {
        info->productOrderNumber = (uint8_t *)calloc(1, 20);
        if (!info->productOrderNumber) {
            BTAfreeDeviceInfo(info);
            return BTA_StatusOutOfMemory;
        }
        sprintf((char *)info->productOrderNumber, "%03d-%04d-%d", ponPart1, ponPart2, deviceRevisionMajor);
    }

    status = BTAETHreadRegister(winst, 0x000c, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->serialNumber |= reg;
    status = BTAETHreadRegister(winst, 0x000d, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->serialNumber |= reg << 16;

    status = BTAETHreadRegister(winst, 0x0008, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->firmwareVersionMajor = (reg & 0xf800) >> 11;
    info->firmwareVersionMinor = (reg & 0x07c0) >> 6;
    info->firmwareVersionNonFunc = (reg & 0x003f);

    status = BTAETHreadRegister(winst, 0x0001, &info->mode0, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }

    status = BTAETHreadRegister(winst, 0x0003, &info->status, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }

    status = BTAETHreadRegister(winst, 0x0040, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->uptime = reg;
    status = BTAETHreadRegister(winst, 0x0041, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->uptime |= reg << 16;

    info->deviceMacAddrLen = 6;
    info->deviceMacAddr = (uint8_t *)calloc(1, info->deviceMacAddrLen);
    if (!info->deviceMacAddr) {
        BTAfreeDeviceInfo(info);
        return BTA_StatusOutOfMemory;
    }
    status = BTAETHreadRegister(winst, 0x0243, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->deviceMacAddr[5] = (uint8_t)reg;
    info->deviceMacAddr[4] = (uint8_t)(reg >> 8);
    status = BTAETHreadRegister(winst, 0x0242, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->deviceMacAddr[3] = (uint8_t)reg;
    info->deviceMacAddr[2] = (uint8_t)(reg >> 8);
    status = BTAETHreadRegister(winst, 0x0241, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->deviceMacAddr[1] = (uint8_t)reg;
    info->deviceMacAddr[0] = (uint8_t)(reg >> 8);

    info->deviceIpAddrLen = 4;
    info->deviceIpAddr = (uint8_t *)calloc(1, info->deviceIpAddrLen);
    if (!info->deviceIpAddr) {
        BTAfreeDeviceInfo(info);
        return BTA_StatusOutOfMemory;
    }
    status = BTAETHreadRegister(winst, 0x0244, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->deviceIpAddr[3] = (uint8_t)reg;
    info->deviceIpAddr[2] = (uint8_t)(reg >> 8);
    status = BTAETHreadRegister(winst, 0x0245, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->deviceIpAddr[1] = (uint8_t)reg;
    info->deviceIpAddr[0] = (uint8_t)(reg >> 8);

    info->subnetMaskLen = 4;
    info->subnetMask = (uint8_t *)calloc(1, info->subnetMaskLen);
    if (!info->subnetMask) {
        BTAfreeDeviceInfo(info);
        return BTA_StatusOutOfMemory;
    }
    status = BTAETHreadRegister(winst, 0x0246, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->subnetMask[3] = (uint8_t)reg;
    info->subnetMask[2] = (uint8_t)(reg >> 8);
    status = BTAETHreadRegister(winst, 0x0247, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->subnetMask[1] = (uint8_t)reg;
    info->subnetMask[0] = (uint8_t)(reg >> 8);

    info->gatewayIpAddrLen = 4;
    info->gatewayIpAddr = (uint8_t *)calloc(1, info->gatewayIpAddrLen);
    if (!info->gatewayIpAddr) {
        BTAfreeDeviceInfo(info);
        return BTA_StatusOutOfMemory;
    }
    status = BTAETHreadRegister(winst, 0x0248, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->gatewayIpAddr[3] = (uint8_t)reg;
    info->gatewayIpAddr[2] = (uint8_t)(reg >> 8);
    status = BTAETHreadRegister(winst, 0x0249, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->gatewayIpAddr[1] = (uint8_t)reg;
    info->gatewayIpAddr[0] = (uint8_t)(reg >> 8);

    info->udpDataIpAddrLen = 4;
    info->udpDataIpAddr = (uint8_t *)calloc(1, info->udpDataIpAddrLen);
    if (!info->udpDataIpAddr) {
        BTAfreeDeviceInfo(info);
        return BTA_StatusOutOfMemory;
    }
    status = BTAETHreadRegister(winst, 0x024c, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->udpDataIpAddr[3] = (uint8_t)reg;
    info->udpDataIpAddr[2] = (uint8_t)(reg >> 8);
    status = BTAETHreadRegister(winst, 0x024d, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->udpDataIpAddr[1] = (uint8_t)reg;
    info->udpDataIpAddr[0] = (uint8_t)(reg >> 8);

    status = BTAETHreadRegister(winst, 0x024e, &reg, 0);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->udpDataPort = reg;
    return BTA_StatusOk;
}


BTA_Status BTAETHgetDeviceInfo(BTA_WrapperInst *winst, BTA_DeviceInfo **deviceInfo) {
    if (!winst ||!deviceInfo) {
        return BTA_StatusInvalidParameter;
    }
    *deviceInfo = 0;
    BTA_Status status;

    BTA_DeviceInfo *info = (BTA_DeviceInfo *)calloc(1, sizeof(BTA_DeviceInfo));
    if (!info) {
        return BTA_StatusOutOfMemory;
    }

    BTA_DeviceType deviceType;
    status = BTAETHgetDeviceType(winst, &deviceType);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }

    // TimEth fw v1.7.5 gives wrong values for multi reads
    if (deviceType == TimUp19kS3Eth || deviceType == TimUp19kS3EthP) {
        info->deviceType = TimUp19kS3Eth;
        status = getDeviceInfoSlow(winst, info);
        *deviceInfo = info;
        return status;
    }

    uint32_t data[50];
    uint32_t dataAddress = 0x0001;
    uint32_t dataCount = 8;
    status = BTAETHreadRegister(winst, dataAddress, data, &dataCount);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->mode0 = data[0x0001 - dataAddress];
    info->status = data[0x0003 - dataAddress];
    info->deviceType = (BTA_DeviceType)data[0x0006 - dataAddress];
    if (info->deviceType == 0) {
        info->deviceType = BTA_DeviceTypeEthernet;
    }
    info->firmwareVersionMajor = (data[0x0008 - dataAddress] & 0xf800) >> 11;
    info->firmwareVersionMinor = (data[0x0008 - dataAddress] & 0x07c0) >> 6;
    info->firmwareVersionNonFunc = (data[0x0008 - dataAddress] & 0x003f);


    dataAddress = 0x000c;
    dataCount = 2;
    status = BTAETHreadRegister(winst, dataAddress, data, &dataCount);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->serialNumber = data[0x000c - dataAddress];
    info->serialNumber |= data[0x000d - dataAddress] << 16;

    dataAddress = 0x0040;
    dataCount = 2;
    status = BTAETHreadRegister(winst, dataAddress, data, &dataCount);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->uptime = data[0x0040 - dataAddress];
    info->uptime |= data[0x0041 - dataAddress] << 16;

    dataAddress = 0x0241;
    dataCount = 14;
    status = BTAETHreadRegister(winst, dataAddress, data, &dataCount);
    if (status != BTA_StatusOk) {
        BTAfreeDeviceInfo(info);
        return status;
    }
    info->deviceMacAddrLen = 6;
    info->deviceMacAddr = (uint8_t *)calloc(1, info->deviceMacAddrLen);
    if (!info->deviceMacAddr) {
        BTAfreeDeviceInfo(info);
        return BTA_StatusOutOfMemory;
    }
    info->deviceMacAddr[0] = (uint8_t)(data[0x0241 - dataAddress] >> 8);
    info->deviceMacAddr[1] = (uint8_t)data[0x0241 - dataAddress];
    info->deviceMacAddr[2] = (uint8_t)(data[0x0242 - dataAddress] >> 8);
    info->deviceMacAddr[3] = (uint8_t)data[0x0242 - dataAddress];
    info->deviceMacAddr[4] = (uint8_t)(data[0x0243 - dataAddress] >> 8);
    info->deviceMacAddr[5] = (uint8_t)data[0x0243 - dataAddress];
    info->deviceIpAddrLen = 4;
    info->deviceIpAddr = (uint8_t *)calloc(1, info->deviceIpAddrLen);
    if (!info->deviceIpAddr) {
        BTAfreeDeviceInfo(info);
        return BTA_StatusOutOfMemory;
    }
    info->deviceIpAddr[3] = (uint8_t)data[0x0244 - dataAddress];
    info->deviceIpAddr[2] = (uint8_t)(data[0x0244 - dataAddress] >> 8);
    info->deviceIpAddr[1] = (uint8_t)data[0x0245 - dataAddress];
    info->deviceIpAddr[0] = (uint8_t)(data[0x0245 - dataAddress] >> 8);
    info->subnetMaskLen = 4;
    info->subnetMask = (uint8_t *)calloc(1, info->subnetMaskLen);
    if (!info->subnetMask) {
        BTAfreeDeviceInfo(info);
        return BTA_StatusOutOfMemory;
    }
    info->subnetMask[3] = (uint8_t)data[0x0246 - dataAddress];
    info->subnetMask[2] = (uint8_t)(data[0x0246 - dataAddress] >> 8);
    info->subnetMask[1] = (uint8_t)data[0x0247 - dataAddress];
    info->subnetMask[0] = (uint8_t)(data[0x0247 - dataAddress] >> 8);
    info->gatewayIpAddrLen = 4;
    info->gatewayIpAddr = (uint8_t *)calloc(1, info->gatewayIpAddrLen);
    if (!info->gatewayIpAddr) {
        BTAfreeDeviceInfo(info);
        return BTA_StatusOutOfMemory;
    }
    info->gatewayIpAddr[3] = (uint8_t)data[0x0248 - dataAddress];
    info->gatewayIpAddr[2] = (uint8_t)(data[0x0248 - dataAddress] >> 8);
    info->gatewayIpAddr[1] = (uint8_t)data[0x0249 - dataAddress];
    info->gatewayIpAddr[0] = (uint8_t)(data[0x0249 - dataAddress] >> 8);
    info->tcpDataPort = data[0x024a - dataAddress];
    info->tcpControlPort = data[0x024b - dataAddress];
    info->udpDataIpAddrLen = 4;
    info->udpDataIpAddr = (uint8_t *)calloc(1, info->udpDataIpAddrLen);
    if (!info->udpDataIpAddr) {
        BTAfreeDeviceInfo(info);
        return BTA_StatusOutOfMemory;
    }
    info->udpDataIpAddr[3] = (uint8_t)data[0x024c - dataAddress];
    info->udpDataIpAddr[2] = (uint8_t)(data[0x024c - dataAddress] >> 8);
    info->udpDataIpAddr[1] = (uint8_t)data[0x024d - dataAddress];
    info->udpDataIpAddr[0] = (uint8_t)(data[0x024d - dataAddress] >> 8);
    info->udpDataPort = data[0x024e - dataAddress];

    dataAddress = 0x0570;
    dataCount = 3;
    status = BTAETHreadRegister(winst, dataAddress, data, &dataCount);
    // These registers don't always exist (for older devices, so ignore an error
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


uint8_t BTAETHisRunning(BTA_WrapperInst *winst) {
    if (!winst) {
        return 0;
    }
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
    if (!inst) {
        return 0;
    }

    if (!inst->connectionMonitorThread) {
        return 0;
    }
    return 1;
}


uint8_t BTAETHisConnected(BTA_WrapperInst *winst) {
    if (!winst) {
        return 0;
    }
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
    if (!inst) {
        return 0;
    }
    uint8_t udpDataOk, udpControlOk, tcpControlOk;
    udpDataOk = inst->udpDataConnectionStatus == 0 || inst->udpDataConnectionStatus == 16;
    udpControlOk = inst->udpControlConnectionStatus == 0 || inst->udpControlConnectionStatus == 16;
    tcpControlOk = inst->tcpControlConnectionStatus == 0 || inst->tcpControlConnectionStatus == 16;
    return udpControlOk && tcpControlOk && udpDataOk;
}


BTA_Status BTAETHsetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode frameMode) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    if (frameMode == BTA_FrameModeCurrentConfig) {
        return BTA_StatusOk;
    }

    uint32_t imgDataFormat;
    BTA_Status status = BTAETHreadRegister(winst, 4, &imgDataFormat, 0);
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

    status = BTAETHwriteRegister(winst, 0x0004, &imgDataFormat, 0);
    if (status != BTA_StatusOk) {
        return status;
    }

    // To be sure the register content is up to date, let's wait camera needs to apply new modus
    BTAmsleep(2000);

    // read back the imgDataFormat
    uint32_t imgDataFormatReadBack;
    status = BTAETHreadRegister(winst, 4, &imgDataFormatReadBack, 0);
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


BTA_Status BTAETHgetFrameMode(BTA_WrapperInst *winst, BTA_FrameMode *frameMode) {
    if (!winst || !frameMode) {
        return BTA_StatusInvalidParameter;
    }

    uint32_t imgDataFormat;
    BTA_Status status = BTAETHreadRegister(winst, 4, &imgDataFormat, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    BTA_EthImgMode imageMode = (BTA_EthImgMode)((imgDataFormat >> 3) & 0xff);
    *frameMode = BTAimageDataFormatToFrameMode(0, imageMode);
    return BTA_StatusOk;
}


static void *connectionMonitorRunFunction(void *handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return 0;
    }
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
    if (!inst) {
        return 0;
    }
    int err;
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "ConnectionMonitorThread started");

    uint8_t keepAliveFails = 0;
    while (!inst->closing) {
        uint8_t reconnected = 0;

        // UDP control connection
        if (inst->udpControlConnectionStatus > 0 && inst->udpControlSocket == INVALID_SOCKET) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInformation, "UDP control: Connecting...");
            BTAlockMutex(inst->controlMutex);
            inst->udpControlSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (inst->udpControlSocket != INVALID_SOCKET) {
                u_long yes = 1;
                err = setsockopt(inst->udpControlSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
                if (err == SOCKET_ERROR) {
                    err = getLastSocketError();
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "UDP control: Failed to allow local address reuse number, error: %d", err);
                }

                // let's bind the inbound connection
                struct sockaddr_in socketAddr = { 0 };
                socketAddr.sin_family = AF_INET;
                // For special circumstances (i.e. when the right interface for sending the data is not determinable),
                // we should test all interfaces like in discovery. Then reMEMBER the socket for further comm
                socketAddr.sin_addr.s_addr = INADDR_ANY;
                if (inst->udpControlCallbackIpAddr) {
                    socketAddr.sin_addr.s_addr = inst->udpControlCallbackIpAddr[0] | (inst->udpControlCallbackIpAddr[1] << 8) | (inst->udpControlCallbackIpAddr[2] << 16) | (inst->udpControlCallbackIpAddr[3] << 24);
                }
                socketAddr.sin_port = htons(inst->udpControlCallbackPort);
                err = bind(inst->udpControlSocket, (struct sockaddr *)&socketAddr, sizeof(socketAddr));
                if (err != SOCKET_ERROR) {
                    // get the port number in case of inst->udpControlCallbackPort == 0
                    socklen_t socketAddrLen = sizeof(socketAddr);
                    err = getsockname(inst->udpControlSocket, (struct sockaddr *)&socketAddr, &socketAddrLen);
                    if (err == 0) {
                        inst->udpControlCallbackPort = htons(socketAddr.sin_port);
#                       ifdef PLAT_WINDOWS
                            DWORD timeout = 500;
#                       else
                            struct timeval timeout;
                            timeout.tv_sec = 0;
                            timeout.tv_usec = 500000;
#                       endif
                        err = setsockopt(inst->udpControlSocket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
                        if (err == SOCKET_ERROR) {
                            err = getLastSocketError();
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "UDP control: Failed to set timeout, error: %d", err);
                        }
                        const DWORD bufferSizeControl = 2 * 1000 * 1000;
                        err = setsockopt(inst->udpControlSocket, SOL_SOCKET, SO_RCVBUF, (const char *)&bufferSizeControl, sizeof(bufferSizeControl));
                        if (err == SOCKET_ERROR) {
                            err = getLastSocketError();
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "UDP control: Failed to set buffer size, error: %d", err);
                        }
                        inst->udpControlConnectionStatus = 8;
                        // connection is up, issue a first keep-alive message
                        BTA_Status status = sendKeepAliveMsg(winst);
                        if (status == BTA_StatusOk) {
                            // udp control is connected -> tcp control not needed
                            inst->tcpControlConnectionStatus = 0;
                            reconnected = 1;
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInformation, "UDP control: Connection open");
                        }
                        else {
                            // inst->udpControlConnectionStatus is set to 3 in sendKeepAliveMsg, override with 1
                            inst->udpControlConnectionStatus = 1;
                            closesocket(inst->udpControlSocket);
                            inst->udpControlSocket = INVALID_SOCKET;
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusWarning, "UDP control: Connection FAILED (failed to send or receive alive message)");
                        }
                    }
                    else {
                        err = getLastSocketError();
                        closesocket(inst->udpControlSocket);
                        inst->udpControlSocket = INVALID_SOCKET;
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusWarning, "UDP control: Failed to get socket name, error: %d", err);
                    }
                }
                else {
                    err = getLastSocketError();
                    closesocket(inst->udpControlSocket);
                    inst->udpControlSocket = INVALID_SOCKET;
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusWarning, "UDP control: Failed to bind socket, error: %d", err);
                }
            }
            else {
                err = getLastSocketError();
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusWarning, "UDP control: Could not open connection, error: %d", err);
            }
            BTAunlockMutex(inst->controlMutex);
        }

        if (inst->closing) {
            break;
        }

        // TCP control connection
        if (inst->tcpControlConnectionStatus > 0 && inst->tcpControlSocket == INVALID_SOCKET) {
            if (inst->tcpControlConnectionStatus == 2) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusWarning, "TCP control: Connection lost!");
            }
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInformation, "TCP control: Connecting...");
            BTAlockMutex(inst->controlMutex);
            inst->tcpControlSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (inst->tcpControlSocket != INVALID_SOCKET) {
                // set the socket to non-blocking
                // can also be done, for better compatibility, via:
                //int flags = fcntl(fd, F_GETFL, 0);
                //fcntl(fd, F_SETFL, flags | O_NONBLOCK);
                u_long yes = 1;
                err = ioctlsocket(inst->tcpControlSocket, FIONBIO, &yes);
                if (err != SOCKET_ERROR) {
                    struct sockaddr_in socketAddr = { 0 };
                    socketAddr.sin_family = AF_INET;
                    socketAddr.sin_addr.s_addr = inst->tcpDeviceIpAddr[0] | (inst->tcpDeviceIpAddr[1] << 8) | (inst->tcpDeviceIpAddr[2] << 16) | (inst->tcpDeviceIpAddr[3] << 24);
                    socketAddr.sin_port = htons(inst->tcpControlPort);

                    // connect the socket (in background)
                    err = connect(inst->tcpControlSocket, (struct sockaddr*) &socketAddr, sizeof(socketAddr));
                    if (err == SOCKET_ERROR && getLastSocketError() == ERROR_IN_PROGRESS) {
                        fd_set fds;
                        FD_ZERO(&fds);
                        FD_SET(inst->tcpControlSocket, &fds);
                        struct timeval timeoutTv;
                        timeoutTv.tv_sec = 4;
                        timeoutTv.tv_usec = 0;
                        // select socket: look for writeable
                        err = select((int)inst->tcpControlSocket + 1, (fd_set *)0, &fds, (fd_set *)0, &timeoutTv);
                        if (err == 1) {
                            inst->tcpControlConnectionStatus = 8;
                            // connection is up, issue a first keep-alive message
                            BTA_Status status = sendKeepAliveMsg(winst);
                            if (status == BTA_StatusOk) {
                                // tcp control is connected -> udp control not needed
                                inst->udpControlConnectionStatus = 0;
                                // The callback information is used for BTAtoByteStream, so set it to 0
                                inst->udpControlCallbackIpAddrVer = 0;
                                free(inst->udpControlCallbackIpAddr);
                                inst->udpControlCallbackIpAddr = 0;
                                inst->udpControlCallbackIpAddrLen = 0;
                                inst->udpControlCallbackPort = 0;
                                reconnected = 1;
                                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInformation, "TCP control: Connection established");
                            }
                            else {
                                // inst->tcpControlConnectionStatus is set to 2 in sendKeepAliveMsg, override with 1
                                // socket is closed down down below sendKeepAliveMsg
                                inst->tcpControlConnectionStatus = 1;
                                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusDeviceUnreachable, "TCP control: Connection FAILED");
                            }
                        }
                        else if (err == SOCKET_ERROR) {
                            err = getLastSocketError();
                            closesocket(inst->tcpControlSocket);
                            inst->tcpControlSocket = INVALID_SOCKET;
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusWarning, "TCP control: Connecting failed (select write), error: %d", err);
                        }
                        else {
                            err = getLastSocketError();
                            closesocket(inst->tcpControlSocket);
                            inst->tcpControlSocket = INVALID_SOCKET;
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusWarning, "TCP control: Connection timeout (select write), error: %d", err);
                        }
                    }
                    else {
                        err = getLastSocketError();
                        closesocket(inst->tcpControlSocket);
                        inst->tcpControlSocket = INVALID_SOCKET;
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusWarning, "TCP control: Could not establish connection, error: %d", err);
                    }
                }
                else {
                    err = getLastSocketError();
                    closesocket(inst->tcpControlSocket);
                    inst->tcpControlSocket = INVALID_SOCKET;
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusWarning, "TCP control: Could not set connection to non blocking, error: %d", err);
                }
            }
            else {
                err = getLastSocketError();
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusWarning, "TCP control: Could not create socket for connection, error: %d", err);
            }
            BTAunlockMutex(inst->controlMutex);
        }

        if (inst->closing) {
            break;
        }

        // udp data autoconfig
        if ((inst->udpControlConnectionStatus == 16 || inst->tcpControlConnectionStatus == 16) && inst->udpDataAutoConfig > 0 && inst->udpDataConnectionStatus > 0 && !inst->udpDataIpAddr) {
            // user wants udp data autoconfig
            // If allowed by device type, get local address and choose standard port
            // This happens only once successfully
            BTA_DeviceType deviceType;
            BTA_Status status = BTAETHgetDeviceType(winst, &deviceType);
            if (status == BTA_StatusOk) {
                if (deviceType == Argos3dP310 ||
                    deviceType == Sentis3dM100 ||
                    deviceType == TimUp19kS3Eth ||
                    deviceType == TimUp19kS3EthP) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusConfigParamError, "UDP data auto config not supported for this device");
                    inst->udpDataAutoConfig = -1;
                }
            }
            else {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusConfigParamError, "UDP data auto config automatically disabled because failed to read device type");
                inst->udpDataAutoConfig = 0;
            }

            if (inst->udpDataAutoConfig > 0) {
                uint8_t *udpDataIpAddr = 0;
                status = BTAgetMatchingLocalAddress(inst->tcpDeviceIpAddr, inst->tcpDeviceIpAddrLen, &udpDataIpAddr);
                if (status == BTA_StatusOk) {
                    inst->udpDataIpAddr = udpDataIpAddr;
                    inst->udpDataIpAddrLen = 4;
                    inst->udpDataIpAddrVer = ipAddrLenToVer[inst->udpDataIpAddrLen];
                    // The Internet Assigned Numbers Authority(IANA) suggests the range 49152 to 65535 for dynamic or private ports.
                    uint64_t macAddr;
                    status = BTAETHgetMacAddr(winst, &macAddr);
                    if (status == BTA_StatusOk) {
                        inst->udpDataPort = 0xc000 + ((macAddr & 0xffffff) % 0x200);
                    }
                    else {
                        inst->udpDataPort = 0xc000;
                    }
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, status, "UDP data auto config set up for %d.%d.%d.%d:%d", inst->udpDataIpAddr[0], inst->udpDataIpAddr[1], inst->udpDataIpAddr[2], inst->udpDataIpAddr[3], inst->udpDataPort);
                }
                else {
                    inst->udpDataAutoConfig = 0;
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusConfigParamError, "UDP data auto config automatically disabled because there is no (unique) local network matching the device ip address");
                }
            }
        }

        if (inst->closing) {
            break;
        }

        // UDP data connection
        if (inst->udpDataConnectionStatus > 0 && inst->udpDataSocket == INVALID_SOCKET && inst->udpDataIpAddr) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInformation, "UDP data: Connecting...");
            inst->udpDataSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (inst->udpDataSocket != INVALID_SOCKET) {
                struct sockaddr_in socketAddr = { 0 };
                socketAddr.sin_family = AF_INET;
                socketAddr.sin_port = htons(inst->udpDataPort);
                if (inst->udpDataIpAddr[0] == 224) {
                    // multicast address, retrieve all local network interfaces, then join them
                    struct addrinfo hints;
                    memset(&hints, 0, sizeof(hints));
                    hints.ai_family = AF_INET;
                    hints.ai_socktype = SOCK_DGRAM;
                    struct addrinfo *addrInfo = 0;
#                   ifdef PLAT_WINDOWS
                    err = getaddrinfo("", "", &hints, &addrInfo);
#                   elif defined PLAT_LINUX || defined PLAT_APPLE
                    char buf[10];
                    sprintf(buf, "%d", inst->udpDataPort);
                    hints.ai_flags = AI_PASSIVE;
                    err = getaddrinfo(0, (char*)buf, &hints, &addrInfo);
#                   endif
                    if (err == 0) {
                        struct addrinfo *addrInfoTemp = addrInfo;
                        while (!inst->closing && addrInfoTemp) {
                            struct ip_mreq mreq;
                            memset(&mreq, 0, sizeof(mreq));
                            mreq.imr_multiaddr.s_addr = inst->udpDataIpAddr[0] | (inst->udpDataIpAddr[1] << 8) | (inst->udpDataIpAddr[2] << 16) | (inst->udpDataIpAddr[3] << 24);
                            mreq.imr_interface.s_addr = ((struct sockaddr_in *)addrInfoTemp->ai_addr)->sin_addr.s_addr;
                            /* use setsockopt() to request that the kernel join a multicast group */
                            err = setsockopt(inst->udpDataSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq));
                            if (err == SOCKET_ERROR) {
                                err = getLastSocketError();
                                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "UDP data: Failed to join the multicast group, error: %d", err);
                            }
                            //else {
                            //    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "UDP data: Joined %.%.%.% to the multicast group", mreq.imr_interface.S_un.S_un_b.s_b1, mreq.imr_interface.S_un.S_un_b.s_b2, mreq.imr_interface.S_un.S_un_b.s_b3, mreq.imr_interface.S_un.S_un_b.s_b4);
                            //}
                            addrInfoTemp = addrInfoTemp->ai_next;
                        }
                        freeaddrinfo(addrInfo);

                        socketAddr.sin_addr.s_addr = INADDR_ANY;
                        //? socketAddr.sin_addr.s_addr = inst->udpDataIpAddr[0] | (inst->udpDataIpAddr[1] << 8) | (inst->udpDataIpAddr[2] << 16) | (inst->udpDataIpAddr[3] << 24);

                        u_long yes = 1;
                        err = setsockopt(inst->udpDataSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
                        if (err == SOCKET_ERROR) {
                            err = getLastSocketError();
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "UDP data: Failed to allow local address reuse , error: %d", err);
                        }
                    }
                    else {
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusWarning, "UDP data: Could not get address info, error: %d", err);
                        socketAddr.sin_addr.s_addr = INADDR_ANY;
                    }
                }
                else {
                    socketAddr.sin_addr.s_addr = inst->udpDataIpAddr[0] | (inst->udpDataIpAddr[1] << 8) | (inst->udpDataIpAddr[2] << 16) | (inst->udpDataIpAddr[3] << 24);
                }

                err = bind(inst->udpDataSocket, (struct sockaddr*)&socketAddr, sizeof(socketAddr));
                if (err != SOCKET_ERROR) {
                    // SO_RCVTIMEO is set in udpDataRunFunction
                    // SO_RCVTIMEO and SO_RCVBUF can be modified via LibParams
                    inst->udpDataConnectionStatus = 16;
                    reconnected = 1;
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInformation, "UDP data: Connection open");
                }
                else {
                    if (inst->udpDataAutoConfig > 0) {
                        err = getLastSocketError();
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "UDP data: Failed to bind socket on %d.%d.%d.%d:%d, error: %d, trying next port", inst->udpDataIpAddr[0], inst->udpDataIpAddr[1], inst->udpDataIpAddr[2], inst->udpDataIpAddr[3], inst->udpDataPort, err);
                        inst->udpDataPort++;
                        if (inst->udpDataPort >= 0xc200) {
                            inst->udpDataPort = 0xc000;
                        }
                        closesocket(inst->udpDataSocket);
                        inst->udpDataSocket = INVALID_SOCKET;
                    }
                    else {
                        err = getLastSocketError();
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusWarning, "UDP data: Failed to bind socket, error: %d", err);
                        closesocket(inst->udpDataSocket);
                        inst->udpDataSocket = INVALID_SOCKET;
                        BTAmsleep(840);
                    }
                }
            }
            else {
                err = getLastSocketError();
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusWarning, "UDP data: Could not open connection, error: %d", err);
            }
        }

        if (inst->closing) {
            break;
        }

        if (reconnected) {
            writeOrCheckDeviceUdpDataSettings(winst);
            winst->modFreqsReadFromDevice = 0;
        }

        // Keep-alive message
        if ((inst->tcpControlConnectionStatus == 16 || inst->udpControlConnectionStatus == 16 || inst->udpControlConnectionStatus == 3) && inst->lpKeepAliveMsgInterval > 0 && BTAgetTickCount() > inst->keepAliveMsgTimestamp) {
            BTAlockMutex(inst->controlMutex);
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
                    if (inst->udpControlConnectionStatus == 16) {
                        // the keepalive message failed, but udp connection is still open (protocol version-, crc- or such error). trigger reconnect
                        inst->udpControlConnectionStatus = 3;
                    }
                    else if (inst->tcpControlConnectionStatus == 16) {
                        // the keepalive message failed, but the tcp connection is still up (protocol version-, crc- or such error). trigger reconnect
                        BTAlockMutex(inst->controlMutex);
                        int err = closesocket(inst->tcpControlSocket);
                        if (err == SOCKET_ERROR) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "connectionMonitor: Failed to close socket, error: %d", err);
                        inst->tcpControlSocket = INVALID_SOCKET;
                        inst->tcpControlConnectionStatus = 2;
                        BTAunlockMutex(inst->controlMutex);
                    }
                }
            }
        }
        
        BTAmsleep(250);
    }


    // Thread was asked to stop
    if (inst->udpControlSocket != INVALID_SOCKET) {
        BTAlockMutex(inst->controlMutex);
        err = closesocket(inst->udpControlSocket);
        inst->udpControlSocket = INVALID_SOCKET;
        inst->udpControlConnectionStatus = 255;
        BTAunlockMutex(inst->controlMutex);
        if (err != SOCKET_ERROR) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "UDP control: Connection closed");
        }
        else {
            err = getLastSocketError();
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "UDP control: Connection close FAILED, error: %d", err);
        }
    }

    if (inst->tcpControlSocket != INVALID_SOCKET) {
        BTAlockMutex(inst->controlMutex);
        // (previously we did a shutdown interface here)
        err = closesocket(inst->tcpControlSocket);
        inst->tcpControlSocket = INVALID_SOCKET;
        inst->tcpControlConnectionStatus = 255;
        BTAunlockMutex(inst->controlMutex);
        if (err != SOCKET_ERROR) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "TCP control: Connection closed");
        }
        else {
            err = getLastSocketError();
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "TCP control: Connection close FAILED, error: %d", err);
        }
    }

    if (inst->udpDataSocket != INVALID_SOCKET) {
        err = closesocket(inst->udpDataSocket);
        inst->udpDataSocket = INVALID_SOCKET;
        if (err != SOCKET_ERROR) {
            inst->udpDataConnectionStatus = 255;
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "UDP data: Connection closed");
        }
        else {
            err = getLastSocketError();
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "UDP data: Connection close FAILED, error: %d", err);
        }
    }

    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "ConnectionMonitorThread terminated");
    return 0;
}


static BTA_Status writeOrCheckDeviceUdpDataSettings(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }

    if (inst->udpDataConnectionStatus == 16 && (inst->udpControlConnectionStatus == 16 || inst->tcpControlConnectionStatus == 16)) {
        // if connection fully open
        if (inst->udpDataAutoConfig > 0) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInformation, "UDP data: Configuring device to stream UDP data to %d.%d.%d.%d:%d", inst->udpDataIpAddr[0], inst->udpDataIpAddr[1], inst->udpDataIpAddr[2], inst->udpDataIpAddr[3], inst->udpDataPort);
            //Eth0UdpStreamIp0
            uint32_t v = inst->udpDataIpAddr[3] | (inst->udpDataIpAddr[2] << 8);
            BTA_Status status = BTAETHwriteRegister(winst, 588, &v, 0);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "UDP data: Device configuration failed! (ip0)");
                return status;
            }
            //Eth0UdpStreamIp1
            v = inst->udpDataIpAddr[1] | (inst->udpDataIpAddr[0] << 8);
            status = BTAETHwriteRegister(winst, 589, &v, 0);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "UDP data: Device configuration failed! (ip1)");
                return status;
            }
            //Eth0UdpStreamPort
            v = inst->udpDataPort;
            status = BTAETHwriteRegister(winst, 590, &v, 0);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "UDP data: Device configuration failed! (port)");
                return status;
            }
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusInformation, "UDP data: Device configuration ok");
        }
        else {
            //Eth0UdpStreamIp0, Eth0UdpStreamIp1 (no multi read in regard of TimEth which does not support it)
            uint32_t ipAddr[2];
            BTA_Status status = BTAETHreadRegister(winst, 588, ipAddr, 0);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusWarning, "UDP data: Cannot check stream ip");
                return status;
            }
            status = BTAETHreadRegister(winst, 589, &(ipAddr[1]), 0);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusWarning, "UDP data: Cannot check stream ip");
                return status;
            }

            uint32_t ipAddrTemp1 = inst->udpDataIpAddr[0] | (inst->udpDataIpAddr[1] << 8) | (inst->udpDataIpAddr[2] << 16) | (inst->udpDataIpAddr[3] << 24);
            uint32_t ipAddrTemp2 = ((ipAddr[1] & 0xff00) >> 8) | ((ipAddr[1] & 0xff) << 8) | ((ipAddr[0] & 0xff00) << 8) | ((ipAddr[0] & 0xff) << 24);
            uint8_t ipAddrTemp3[4] = { (ipAddr[1] >> 8) & 0xff, ipAddr[1] & 0xff, (ipAddr[0] >> 8) & 0xff, ipAddr[0] & 0xff };
            uint8_t listenMulti = (ipAddrTemp1 & 0xe0) == 0xe0;
            uint8_t cameraToLocalOrMulti = BTAisLocalIpAddrOrMulticast(ipAddrTemp3, 4);
            uint8_t ipsAreEqual = ipAddrTemp1 == ipAddrTemp2;
            if (!ipsAreEqual && !(listenMulti && cameraToLocalOrMulti)) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "UDP data: The device streams to %d.%d.%d.%d, but the BltTofApi is listening at %d.%d.%d.%d", (ipAddr[1] >> 8) & 0xff, ipAddr[1] & 0xff, (ipAddr[0] >> 8) & 0xff, ipAddr[0] & 0xff, inst->udpDataIpAddr[0], inst->udpDataIpAddr[1], inst->udpDataIpAddr[2], inst->udpDataIpAddr[3], inst->udpDataPort);
            }
            else {
                //Eth0UdpStreamPort
                uint32_t port;
                status = BTAETHreadRegister(winst, 590, &port, 0);
                if (status != BTA_StatusOk) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusWarning, "UDP data: Cannot check streaming port");
                }
                else {
                    if (port != inst->udpDataPort) {
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusConfigParamError, "UDP data: The device streams to port %d, but the BltTofApi is listening at port %d", port, inst->udpDataPort);
                    }
                }
            }
        }
    }
    return BTA_StatusOk;
}


BTA_Status BTAETHsendReset(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }

    uint8_t *sendBuffer;
    uint32_t sendBufferLen;
    BTA_Status status = BTAtoByteStream(BTA_EthCommandReset, BTA_EthSubCommandNone, (uint32_t)0, (uint8_t *)0, (uint32_t)0, inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen,
                               inst->udpControlCallbackIpAddrVer, inst->udpControlCallbackIpAddr, inst->udpControlCallbackIpAddrLen, inst->udpControlCallbackPort, 0, 0, 0);
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
    status = receiveControlResponse(winst, sendBuffer, 0, 0, timeoutDefault, 0);
    free(sendBuffer);
    sendBuffer = 0;
    BTAunlockMutex(inst->controlMutex);
    return status;
}


/*  @brief Simply requests and gets an alive msg
*   @pre lock the control MUTEX             */
static BTA_Status sendKeepAliveMsg(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_EthLibInst * inst = (BTA_EthLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }

    // The keepAliveMsg is the first message to test the connection, so we have to decide, if we add UDP callback information.
    // Once the connection is established, the connectionMonitorThread clears the callback information from inst
    uint8_t *sendBuffer = 0;
    uint32_t sendBufferLen = 0;
    BTA_Status status;
    if (inst->udpControlConnectionStatus == 16 || inst->udpControlConnectionStatus == 3 || inst->udpControlConnectionStatus == 8) {
        status = BTAtoByteStream(BTA_EthCommandKeepAliveMsg, BTA_EthSubCommandNone, 0, 0, 0, inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen,
                                   inst->udpControlCallbackIpAddrVer, inst->udpControlCallbackIpAddr, inst->udpControlCallbackIpAddrLen, inst->udpControlCallbackPort, 0, 0, 0);
    }
    else if (inst->tcpControlConnectionStatus == 16 || inst->tcpControlConnectionStatus == 8) {
        status = BTAtoByteStream(BTA_EthCommandKeepAliveMsg, BTA_EthSubCommandNone, 0, 0, 0, inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen, 0, 0, 0, 0, 0, 0, 0);
    }
    else {
        status = BTA_StatusNotConnected;
    }
    if (status != BTA_StatusOk) {
        return status;
    }
    status = transmit(winst, sendBuffer, sendBufferLen, timeoutDefault);
    if (status != BTA_StatusOk) {
        free(sendBuffer);
        sendBuffer = 0;
        return status;
    }
    status = receiveControlResponse(winst, sendBuffer, 0, 0, timeoutDefault, 0);
    free(sendBuffer);
    sendBuffer = 0;
    return status;
}


static void *udpReadRunFunction(void *handle) {
    int err;

//#   define DEBUGUDPREAD
    //char dm[2000] = { 0 };

    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return 0;
    }
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
    if (!inst) {
        return 0;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "UdpReadThread started");

    struct sockaddr_in socketAddr = { 0 };
    socketAddr.sin_family = AF_INET;
    socketAddr.sin_addr.s_addr = inst->udpDataIpAddr[0] | (inst->udpDataIpAddr[1] << 8) | (inst->udpDataIpAddr[2] << 16) | (inst->udpDataIpAddr[3] << 24);
    socketAddr.sin_port = htons(inst->udpDataPort);
    const int socketAddrLen = sizeof(struct sockaddr_in);

    if (inst->udpDataSocket == INVALID_SOCKET) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "UdpReadThread: Socket is invalid");
        return 0;
    }

    // set default socket timeout now, later LibParam can change it directly
#   ifdef PLAT_WINDOWS
        DWORD timeout = 250;
#   else
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 250000;
#   endif
    err = setsockopt(inst->udpDataSocket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    if (err == SOCKET_ERROR) {
        err = getLastSocketError();
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "UdpReadThread: Failed to set timeout, error: %d", err);
    }

    // set default socket buffer size now, later LibParam can change it directly
    uint32_t bufferSize = (uint32_t)100 * 1024 * 1024;
    err = setsockopt(inst->udpDataSocket, SOL_SOCKET, SO_RCVBUF, (char *)&bufferSize, sizeof(bufferSize));
    if (err == SOCKET_ERROR) {
        err = getLastSocketError();
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "UdpReadThread: Failed to set buffer size, error: %d", err);
    }

    BTA_MemoryArea *udpPacket = 0;

#   if defined(DEBUGUDPREAD)
    int frameCounterCurr = -1;
    int packetCount = 0;
    uint16_t packetCountTotal = 0;
    int frameCount = 0;
#   endif

    const int errorOnGapAfterMs = 7000;
    uint64_t timeLastPacketReceived = BTAgetTickCount64();
    while (!inst->closing) {

        if (winst->lpPauseCaptureThread) {
            BTAmsleep(50);
            continue;
        }

        if (!udpPacket) {
#           if defined(DEBUGUDPREAD)
            uint64_t time06 = BTAgetTickCountNano() / 1000;
#           endif
            BTA_Status status = BCBget(inst->packetsToFillQueue, (void **)&udpPacket);
            if (status != BTA_StatusOk) {
                if (inst->udpDataQueueMallocCount < udpDataQueueLen) {
                    status = BTAinitMemoryArea(&udpPacket, udpPacketLenMax);
                    if (status != BTA_StatusOk) {
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "UdpReadThread Eth: Can't allocate packet buffer");
                        udpPacket = 0;
                        continue;
                    }
                    inst->udpDataQueueMallocCount++;
                }
                else {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, status, "UdpReadThread: No packet buffer available queueLength=%d queueLengthMax=%d", BCBgetSize(inst->packetsToFillQueue), udpDataQueueLen);
                    BTAmsleep(50);
                    continue;
                }
            }

#           if defined(DEBUGUDPREAD)
            uint64_t dur06 = BTAgetTickCountNano() / 1000 - time06;
            winst->lpDebugValue06 = (float)MTHmax(dur06, (uint64_t)winst->lpDebugValue06);
#           endif
        }


#       if defined(DEBUGUDPREAD)
        uint64_t time07 = BTAgetTickCountNano() / 1000;
#       endif

#       ifdef PLAT_APPLE
            ssize_t readCount;
#       else
            int readCount;
#       endif
        readCount = recvfrom(inst->udpDataSocket, (char *)udpPacket->p, udpPacketLenMax, 0, (struct sockaddr *)&socketAddr, (socklen_t *)&socketAddrLen);

#       if defined(DEBUGUDPREAD)
        uint64_t dur07 = BTAgetTickCountNano() / 1000 - time07;
        winst->lpDebugValue07 = (float)MTHmax(dur07, (uint64_t)winst->lpDebugValue07);
#       endif

        if (readCount < 0) {
            //sprintf(dm + strlen(dm), " X");
            err = getLastSocketError();
            if (err == ERROR_TRY_AGAIN) {
                if (BTAgetTickCount64() - timeLastPacketReceived > errorOnGapAfterMs) {
                    timeLastPacketReceived = BTAgetTickCount64();
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "UdpReadThread: No data on UDP data stream socket %d", err);
                }
            }
            else {
                winst->lpDataStreamReadFailedCount += 1;
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "UdpReadThread: Error in recvfrom() %d", err);
            }
        }
        else {
            timeLastPacketReceived = BTAgetTickCount64();

#           if defined(DEBUGUDPREAD)
            winst->lpDebugValue04++;

            if (frameCounterCurr != -1) {
                packetCount++;
            }
            uint16_t frameCounter = 0;
            if (((uint16_t *)udpPacket->p)[0] == 0x0100) {
                BTA_UdpPackHead2 *udpPackHead2 = (BTA_UdpPackHead2 *)udpPacket->p;
                frameCounter = (uint16_t)(udpPackHead2->frameCounter << 8) | (uint16_t)(udpPackHead2->frameCounter >> 8);
                packetCountTotal = MTHmax(packetCountTotal, (uint16_t)(udpPackHead2->packetCounter << 8) | (uint16_t)(udpPackHead2->packetCounter >> 8));
            }
            else if (((uint16_t *)udpPacket->p)[0] == 0x0200) {
                BTA_UdpPackHead2 *udpPackHead2 = (BTA_UdpPackHead2 *)udpPacket->p;
                frameCounter = udpPackHead2->frameCounter;
                packetCountTotal = udpPackHead2->packetCountTotal;
            }
            else assert(0);
            if (frameCounter != frameCounterCurr)
            {
                if (frameCounterCurr != -1) {
                    if (frameCounter != (uint16_t)(frameCounterCurr + 1) && frameCount > 5) {
                        winst->lpDebugValue03++;
                    }
                    frameCount++;
                    if (packetCount < packetCountTotal && frameCount > 5) {
                        winst->lpDebugValue02 += packetCountTotal - packetCount;
                    }
                }
                packetCount = 0;
                winst->lpDebugValue01 = frameCounter;
                frameCounterCurr = frameCounter;
            }

            uint64_t time08 = BTAgetTickCountNano() / 1000;
#           endif

            udpPacket->l = (uint32_t)readCount;
            BCBput(inst->packetsToParseQueue, udpPacket); // There are max as many packets around as the queue is long -> no error checking
            udpPacket = 0;

            winst->lpDataStreamBytesReceivedCount += readCount;

#           if defined(DEBUGUDPREAD)
            uint64_t dur08 = BTAgetTickCountNano() / 1000 - time08;
            winst->lpDebugValue08 = (float)MTHmax(dur08, (uint64_t)winst->lpDebugValue08);
#           endif
        }
    }
    if (udpPacket) {
        BTAfreeMemoryArea(&udpPacket);
    }

    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "UdpReadThread thread terminated");
    return 0;
}


static void *parseFramesRunFunction(void *handle) {
    BTA_Status status;

    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return 0;
    }
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
    if (!inst) {
        return 0;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "ParseFramesThread started");

    while (!inst->closing) {

        // +++ helper variables for v1 ++++++++++++++++++++++++++++++
        uint8_t *completeFrame = 0;
        uint32_t completeFrameLen = 0;

        uint16_t framePacketsLen = 512;

        BTA_MemoryArea **framePacketsA = 0, **framePacketsB = 0, **framePacketsC = 0;
        int32_t frameCounterA = -1, frameCounterB = -1, frameCounterC = -1;
        uint32_t frameBytesReceivedA = 0, frameBytesReceivedB = 0, frameBytesReceivedC = 0;

        int frameCounterGotLast = -1;
        int packetCounterGotLast = 0;

        int packetCountMax = 0;
        // --- helper variables for v1 ------------------------------

        // +++ helper variables for v2 ++++++++++++++++++++++++++++++
        //BTA_FrameToParse *frameToParse;
        //BTAcreateFrameToParse(&frameToParse);
        const uint8_t framesToParseLen = 4;
        BTA_FrameToParse *framesToParse[framesToParseLen];
        for (uint8_t ftpInd = 0; ftpInd < framesToParseLen; ftpInd++) {
            BTAcreateFrameToParse(&framesToParse[ftpInd]);
        }
        // --- helper variables for v2 ------------------------------

#       if defined(DEBUG)
        uint64_t time09Prev = BTAgetTickCountNano() / 1000;
        int loopCount = 0;
#       endif
        uint8_t retransmissionSupport = 0;
        BTA_MemoryArea *packet = 0;
        while (!inst->closing) {

#           if defined(DEBUG)
            loopCount++;
            uint64_t dur09 = BTAgetTickCountNano() / 1000 - time09Prev;
            winst->lpDebugValue09 = (float)MTHmax(dur09, (uint64_t)winst->lpDebugValue09);
            time09Prev = BTAgetTickCountNano() / 1000;
#           endif

			// v1: init arrays of packets (on first loop and when framePacketsLen increases
            if (!framePacketsA || !framePacketsB || !framePacketsC) {
                framePacketsA = (BTA_MemoryArea **)calloc(framePacketsLen, sizeof(BTA_MemoryArea *));
                framePacketsB = (BTA_MemoryArea **)calloc(framePacketsLen, sizeof(BTA_MemoryArea *));
                framePacketsC = (BTA_MemoryArea **)calloc(framePacketsLen, sizeof(BTA_MemoryArea *));
                if (!framePacketsA || !framePacketsB || !framePacketsC) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "ParseFramesThread: Could not allocate");
                    return 0;
                }
            }

            // check timeouts and send retransmission request or parse
            uint64_t time = BTAgetTickCount64();
            for (uint8_t ftpInd = 0; ftpInd < framesToParseLen; ftpInd++) {
                BTA_FrameToParse *ftpTemp = framesToParse[ftpInd];
                if (ftpTemp->timestamp && time > ftpTemp->timeLastPacket + inst->lpDataStreamPacketWaitTimeout) {
                    if (retransmissionSupport) {
                        if (time >= ftpTemp->retryTime) {
                            if (ftpTemp->retryCount >= inst->lpDataStreamRetrReqMaxAttempts) {
                                // maximum attempts reached, give up (parse older unfinished frames first, then this)
                                while (1) {
                                    BTA_FrameToParse *ftpTemp2 = getOlderThan(framesToParse, framesToParseLen, ftpTemp->timestamp);
                                    if (!ftpTemp2) {
                                        break;
                                    }
                                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_DEBUG, BTA_StatusWarning, "parsing (%d) (%d/%d received) max attempts reached", ftpTemp2->frameCounter, ftpTemp2->packetCountGot, ftpTemp2->packetCountTotal);
                                    BTAparsePostprocessGrabCallbackEnqueue(winst, ftpTemp2);
                                }
                            }
                            else {
                                status = sendRetrReqComplete(winst, ftpTemp);
                                if (status == BTA_StatusOk) {
                                    ftpTemp->retryCount++;
                                }
                            }
                        }
                    }
                    else {
                        // frame is not expected to be complete -> parse (parse older unfinished frames first)
                        while (1) {
                            BTA_FrameToParse *ftpTemp2 = getOlderThan(framesToParse, framesToParseLen, ftpTemp->timestamp);
                            if (!ftpTemp2) {
                                break;
                            }
                            BTAparsePostprocessGrabCallbackEnqueue(winst, ftpTemp2);
                        }
                    }
                }
            }

            // This is for statistics
            //int count = BVQgetCount(inst->packetsToParseQueue);
            int count = BCBgetSize(inst->packetsToParseQueue);
            winst->lpDataStreamPacketsToParse = (float)MTHmax(count, (int)winst->lpDataStreamPacketsToParse);


            //printf("mallocCount %d  packetsToParse %d\n", inst->udpDataQueueMallocCount, count);

            // lpDataStreamPacketWaitTimeout is the time that has to pass (no packet received for a certain frame during this time) before any action is taken
            // ..so I figured we listen to Shannon and loop for checks at intervals of half that time
            //dequeStatus = BVQdequeue(inst->packetsToParseQueue, (void **)&packet, (uint32_t)inst->lpDataStreamPacketWaitTimeout / 2);
            uint64_t timeEnd = BTAgetTickCount64() + (uint64_t)(inst->lpDataStreamPacketWaitTimeout / 2);
            while (1) {
                status = BCBget(inst->packetsToParseQueue, (void **)&packet);
                if (status == BTA_StatusOk) {
                    break;
                }
                if (BTAgetTickCount64() > timeEnd) {
                    packet = 0;
                    break;
                }
                BTAmsleep(1 + (uint32_t)(inst->lpDataStreamPacketWaitTimeout / 20));
            }
            if (!packet) {
                continue;
            }

            if (packet->l < BTA_ETH_PACKET_HEADER_SIZE) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ParseFramesThread: Datagram too small %d", packet->l);
                continue;
            }

            BTA_FrameToParse *ftp = 0;
            uint16_t packetCounter = UINT16_MAX;
            uint8_t *packetBuf = (uint8_t *)packet->p;
            uint16_t packetLen = (uint16_t)packet->l;
            uint16_t headerVersion = (int)(((uint16_t)packetBuf[0] << 8) | (uint16_t)packetBuf[1]);
            switch (headerVersion)
            {
                case 1: {
                    int i = 2;
                    uint16_t frameCounter = (int)(((uint16_t)packetBuf[i] << 8) | (uint16_t)packetBuf[i + 1]);
                    i += 2;
                    uint16_t packetCounter = (int)(((uint16_t)packetBuf[i] << 8) | (uint16_t)packetBuf[i + 1]);
                    i += 2;
                    uint16_t payloadSize = (int)(((uint16_t)packetBuf[i] << 8) | (uint16_t)packetBuf[i + 1]);
                    i += 2;
                    if (payloadSize + BTA_ETH_PACKET_HEADER_SIZE != packetLen) {
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ParseFramesThread: Did not read expected amount of data", packetLen);
                        break;
                    }
                    uint32_t totalLength = (int)(((uint32_t)packetBuf[i] << 24) | ((uint32_t)packetBuf[i + 1] << 16) | ((uint32_t)packetBuf[i + 2] << 8) | (uint32_t)packetBuf[i + 3]);
                    i += 4;
                    uint32_t crc32 = (uint32_t)(((uint32_t)packetBuf[i] << 24) | ((uint32_t)packetBuf[i + 1] << 16) | ((uint32_t)packetBuf[i + 2] << 8) | (uint32_t)packetBuf[i + 3]);
                    i += 4;
                    uint32_t flags = (uint32_t)(((uint32_t)packetBuf[i] << 24) | ((uint32_t)packetBuf[i + 1] << 16) | ((uint32_t)packetBuf[i + 2] << 8) | (uint32_t)packetBuf[i + 3]);
                    i += 4;
                    if ((flags & 0x01) == 0) {
                        packetBuf[i] = packetBuf[i + 1] = packetBuf[i + 2] = packetBuf[i + 3] = 0; // these bytes have to be 0 for crc calculation
                        uint32_t crc32calc = (uint32_t)CRC32ccitt(packetBuf, packetLen);
                        if ((crc32 != crc32calc))
                        {
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ParseFramesThread: CRC32 mismatch");
                            break;
                        }
                    }

                    // count missing packets
                    winst->lpDataStreamPacketsReceivedCount += 1;
                    packetCountMax = MTHmax(packetCountMax, packetCounter);
                    if (frameCounterGotLast == -1) {
                        frameCounterGotLast = frameCounter;
                        packetCounterGotLast = packetCounter;
                    }

                    int frameCounterExpected = frameCounterGotLast;
                    int packetCounterExpected = packetCounterGotLast + 1;
                    if (packetCounterExpected > packetCountMax) {
                        if (++frameCounterExpected > 0xffff) frameCounterExpected = 0;
                        packetCounterExpected = 0;
                    }

                    if (frameCounter != frameCounterExpected || packetCounter != packetCounterExpected) {
                        int frameCounterMissedFirst = frameCounterGotLast;
                        int packetCounterMissedFirst = packetCounterGotLast + 1;
                        if (packetCounterMissedFirst > packetCountMax) {
                            if (++frameCounterMissedFirst > 0xffff) frameCounterMissedFirst = 0;
                            packetCounterMissedFirst = 0;
                        }
                        int frameCounterMissedLast = frameCounter;
                        int packetCounterMissedLast = packetCounter - 1;
                        if (packetCounterMissedLast < 0) {
                            if (--frameCounterMissedLast < 0) frameCounterMissedLast = 0xffff;
                            packetCounterMissedLast = packetCountMax;
                        }
                        winst->lpDataStreamPacketsMissedCount += (frameCounterMissedLast - frameCounterMissedFirst) * packetCountMax + packetCounterMissedLast - packetCounterMissedFirst + 1;
                    }
                    frameCounterGotLast = frameCounter;
                    packetCounterGotLast = packetCounter;

                    if (frameCounter != frameCounterA && frameCounter != frameCounterB && frameCounter != frameCounterC) {
                        uint16_t packetsDroppedCount = 0;
                        uint32_t bytesDropped = 0;
                        uint16_t frameCounterDropped;
                        // haven't seen this frame before
                        if ((MTHabs((long)frameCounterA - (long)frameCounter) >= MTHabs((long)frameCounterB - (long)frameCounter)) && (MTHabs((long)frameCounterA - (long)frameCounter) >= MTHabs((long)frameCounterC - (long)frameCounter))) {
                            // the above if is robust against overflows: it finds the bigger difference to the current frameCounter
                            frameCounterDropped = frameCounterA;
                            frameCounterA = frameCounter;
                            frameBytesReceivedA = 0;
                            for (i = 0; i < framePacketsLen; i++) {
                                if (framePacketsA[i]) {
                                    packetsDroppedCount++;
                                    bytesDropped += framePacketsA[i]->l;
                                    BCBput(inst->packetsToFillQueue, (void **)framePacketsA[i]);
                                    framePacketsA[i] = 0;
                                }
                            }
                        }
                        else if ((MTHabs((long)frameCounterB - (long)frameCounter) >= MTHabs((long)frameCounterA - (long)frameCounter)) && (MTHabs((long)frameCounterB - (long)frameCounter) >= MTHabs((long)frameCounterC - (long)frameCounter))) {
                            frameCounterDropped = frameCounterB;
                            frameCounterB = frameCounter;
                            frameBytesReceivedB = 0;
                            for (i = 0; i < framePacketsLen; i++) {
                                if (framePacketsB[i]) {
                                    packetsDroppedCount++;
                                    bytesDropped += framePacketsB[i]->l;
                                    BCBput(inst->packetsToFillQueue, (void **)framePacketsB[i]);
                                    framePacketsB[i] = 0;
                                }
                            }
                        }
                        else {
                            frameCounterDropped = frameCounterC;
                            frameCounterC = frameCounter;
                            frameBytesReceivedC = 0;
                            for (i = 0; i < framePacketsLen; i++) {
                                if (framePacketsC[i]) {
                                    packetsDroppedCount++;
                                    bytesDropped += framePacketsC[i]->l;
                                    BCBput(inst->packetsToFillQueue, (void **)framePacketsC[i]);
                                    framePacketsC[i] = 0;
                                }
                            }
                        }
                        if (packetsDroppedCount) {
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "ParseFramesThread: Frame %d incomplete: %d packets (%d bytes) dropped", frameCounterDropped, packetsDroppedCount, bytesDropped);
                        }
                    }

                    // help myself with pointers pointing to the right slot
                    BTA_MemoryArea **framePackets;
                    uint32_t *frameBytesReceived;
                    if (frameCounter == frameCounterA) {
                        framePackets = framePacketsA;
                        frameBytesReceived = &frameBytesReceivedA;
                    }
                    else if (frameCounter == frameCounterB) {
                        framePackets = framePacketsB;
                        frameBytesReceived = &frameBytesReceivedB;
                    }
                    else if (frameCounter == frameCounterC) {
                        framePackets = framePacketsC;
                        frameBytesReceived = &frameBytesReceivedC;
                    }
                    else {
                        assert(0);
                        break;
                    }

                    if (packetCounter >= framePacketsLen) {
                        // overflow! drop everything
                        for (i = 0; i < framePacketsLen; i++) {
                            if (framePacketsA[i]) {
                                BCBput(inst->packetsToFillQueue, (void **)framePacketsA[i]);
                                framePacketsA[i] = 0;
                            }
                            if (framePacketsB[i]) {
                                BCBput(inst->packetsToFillQueue, (void **)framePacketsB[i]);
                                framePacketsB[i] = 0;
                            }
                            if (framePacketsC[i]) {
                                BCBput(inst->packetsToFillQueue, (void **)framePacketsC[i]);
                                framePacketsC[i] = 0;
                            }
                        }
                        framePacketsLen *= 2;
                        free(framePacketsA);
                        framePacketsA = 0;
                        free(framePacketsB);
                        framePacketsB = 0;
                        free(framePacketsC);
                        framePacketsC = 0;
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusWarning, "ParseFramesThread: Too many packets for one frame. Dropping and reallocating");
                        break;
                    }

                    if (framePackets[packetCounter]) {
                        // already got this packet -> discard old packet and use new packet
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ParseFramesThread: Packet received twice %d", packetCounter);
                        BCBput(inst->packetsToFillQueue, (void **)framePackets[packetCounter]);
                        framePackets[packetCounter] = 0;
                        *frameBytesReceived -= payloadSize;
                    }
                    framePackets[packetCounter] = packet;
                    packet = 0; // set packet to null in order to signal that we are still using this MemoryArea
                    *frameBytesReceived += payloadSize;

                    if (*frameBytesReceived == totalLength) {
                        completeFrameLen = totalLength;
                        completeFrame = (uint8_t *)malloc(completeFrameLen);
                        if (!completeFrame) {
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "ParseFramesThread: Could not allocate");
                            break;
                        }
                        uint32_t completeFrameOffset = 0;
                        for (i = 0; i < framePacketsLen; i++) {
                            if (!framePackets[i]) {
                                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ParseFramesThread: Packet gap even though the total size of the frame is correct %d", i);
                                continue;
                            }
                            uint8_t *payload = &((uint8_t *)framePackets[i]->p)[BTA_ETH_PACKET_HEADER_SIZE];
                            int payloadLen = framePackets[i]->l - BTA_ETH_PACKET_HEADER_SIZE;
                            if (completeFrameOffset + payloadLen <= completeFrameLen) {
                                memcpy(completeFrame + completeFrameOffset, payload, payloadLen);
                                completeFrameOffset += payloadLen;
                            }
                            BCBput(inst->packetsToFillQueue, (void **)framePackets[i]);
                            framePackets[i] = 0;
                            // check if the frame is fully memcopied (all packets should've been given back to packetsToFillQueue)
                            if (completeFrameOffset == completeFrameLen) break;
                        }
                        if (completeFrameOffset != completeFrameLen) {
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ParseFramesThread: Frame length mismatch. Accumulated: %d, should be: ", completeFrameOffset, completeFrameLen);
                            free(completeFrame);
                            completeFrame = 0;
                            completeFrameLen = 0;
                            break;
                        }

                        BTA_FrameToParse *frameToParse;
                        status = BTAcreateFrameToParse(&frameToParse);
                        if (status != BTA_StatusOk) {
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "UDP data: BTAcreateFrameToParse: Could not create FrameToParse");
                            completeFrameLen = 0;
                            free(completeFrame);
                            completeFrame = 0;
                        }
                        frameToParse->timestamp = BTAgetTickCount64();
                        frameToParse->frameCounter = frameCounter;
                        frameToParse->frameLen = completeFrameLen;
                        frameToParse->frame = completeFrame;
                        completeFrameLen = 0;
                        completeFrame = 0;
                        BTAparsePostprocessGrabCallbackEnqueue(winst, frameToParse);
                        BTAfreeFrameToParse(&frameToParse); // with protocol v3 the frameToParse is not re-used!
                    }
                    break;
                }

                case 2: {
                    BTA_UdpPackHead2 *packHead = (BTA_UdpPackHead2 *)packet->p;

                    retransmissionSupport = packHead->flags & 0x04;
                    if (packHead->flags & 0x02) {
                        uint16_t crc16 = packHead->crc16;
                        packHead->crc16 = 0;
                        uint16_t crc16calc = crc16_ccitt(packet->p, packet->l);
                        if ((crc16 != crc16calc))
                        {
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ParseFramesThread v2: CRC16 of packet mismatch");
                            break;
                        }
                    }
                    else if (packHead->flags & 0x01) {
                        uint16_t crc16 = packHead->crc16;
                        packHead->crc16 = 0;
                        uint16_t crc16calc = crc16_ccitt(packet->p, BTA_ETH_PACKET_HEADER_SIZE);
                        if ((crc16 != crc16calc))
                        {
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ParseFramesThread v2: CRC16 of header mismatch");
                            break;
                        }
                    }

                    // length check
                    if (!packHead->packetDataLen) {
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ParseFramesThread v2: unexpected packet data size %d", packHead->packetDataLen);
                        break;
                    }
                    if ((uint32_t)(packHead->packetDataLen + BTA_ETH_PACKET_HEADER_SIZE) != packet->l) {
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ParseFramesThread v2: unexpected packet size", packHead->packetDataLen + BTA_ETH_PACKET_HEADER_SIZE, packet->l);
                        break;
                    }

                    packetCounter = packHead->packetCounter;
                    if (packHead->flags & 0x08 && packetCounter != UINT16_MAX) inst->lpDataStreamRetrPacketsCount++;
                    if (packHead->flags & 0x08 && packetCounter == UINT16_MAX) inst->lpDataStreamNdasReceived++;

                    //if (packHead->flags & 0x08 && packetCounter != UINT16_MAX) BTAinfoEventHelper(winst->infoEventInst, IMPORTANCE_MOST, BTA_StatusDebug, "got retr %d", packetCounter);

                    // check if this is an NDA packet
                    if (packetCounter == UINT16_MAX) {
                        for (uint8_t ftpInd = 0; ftpInd < framesToParseLen; ftpInd++) {
                            BTA_FrameToParse *ftpTemp = framesToParse[ftpInd];
                            if (ftpTemp->timestamp && ftpTemp->frameCounter == packHead->frameCounter) {
                                uint16_t *packetCounters = (uint16_t *)((uint8_t *)packet->p + BTA_ETH_PACKET_HEADER_SIZE);
#if defined(DEBUG)
                                char msg[5000] = { 0 };
                                sprintf(msg + strlen(msg), "got NDA (%%d)");
                                for (int pcInd = 0; pcInd < packHead->packetDataLen / 2; pcInd++) {
                                    sprintf(msg + strlen(msg), "%4d", *packetCounters++);
                                    if (strlen(msg) > 100) {
                                        sprintf(msg + strlen(msg), "...");
                                        break;
                                    }
                                }
                                if (!*((uint16_t *)((uint8_t *)packet->p + BTA_ETH_PACKET_HEADER_SIZE))) sprintf(msg + strlen(msg), "first packet missing -> discard");
                                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusDebug, msg, packHead->frameCounter);
                                packetCounters = (uint16_t *)((uint8_t *)packet->p + BTA_ETH_PACKET_HEADER_SIZE);
#endif
                                if (!*packetCounters) {
                                    // first packet is missing, don't bother any further..
                                    ftpTemp->timestamp = 0;
                                    winst->lpDataStreamPacketsReceivedCount += ftpTemp->packetCountGot;
                                    winst->lpDataStreamPacketsMissedCount += ftpTemp->packetCountTotal - ftpTemp->packetCountGot;
                                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ParseFramesThread v2: first packet is missing, discard");
                                    break;
                                }
                                for (int pcInd = 0; pcInd < packHead->packetDataLen / 2; pcInd++) {
                                    if (*packetCounters >= ftpTemp->packetCountTotal) {
                                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidData, "Got an NDA with packet counter %d, but total packet count is %d", *packetCounters, ftpTemp->packetCountTotal);
                                    }
                                    else {
                                        ftpTemp->packetCountNda++;
                                        ftpTemp->packetSize[*packetCounters] = UINT16_MAX;
                                        packetCounters++;
                                    }
                                }
                                ftp = ftpTemp;
                                break;
                            }
                        }
                        break;
                    }
                    // packet length check
                    if (packHead->packetPosition + packHead->packetDataLen > packHead->frameLen) {
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ParseFramesThread v2: Packet position (%d) and packet size (%d) exceed frame size (%d)", packHead->packetPosition, packHead->packetDataLen, packHead->frameLen);
                        break;
                    }
                    // packet count check
                    if (packetCounter >= packHead->packetCountTotal) {
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ParseFramesThread v2: wrong packetCounter", packetCounter, packHead->packetCountTotal);
                        break;
                    }

                    // Find corresponding ftp
                    BTA_FrameToParse *ftpFree = 0;
                    for (uint8_t ftpInd = 0; ftpInd < framesToParseLen; ftpInd++) {
                        BTA_FrameToParse *ftpTemp = framesToParse[ftpInd];
                        if (ftpTemp->timestamp) {
                            if (ftpTemp->frameCounter == packHead->frameCounter) {
                                ftp = ftpTemp;
                                break;
                            }
                        }
                        else {
                            ftpFree = ftpTemp;
                        }
                    }
                    if (!ftp) {
                        if (packHead->flags & 0x08) {
                            // this is a late arriving retransmission, frame must have been parsed already, discard packet
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "UDP data v2: got late retransmission (%d) %d", packHead->frameCounter, packHead->packetCounter);
                            break;
                        }
                        // No corresponding ftp found, use a free slot
                        ftp = ftpFree;
                        if (!ftp) {
                            // No free slots left, find oldest ftp and use it
                            ftp = getOldest(framesToParse, framesToParseLen);
                        }
                        if (!ftp) {
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, status, "UDP data v2: Init a new ftp: No oldest ftp found!");
                            break;
                        }
                        status = BTAinitFrameToParse(&ftp, BTAgetTickCount64(), packHead->frameCounter, packHead->frameLen, packHead->packetCountTotal);
                        if (status != BTA_StatusOk) {
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, status, "UDP data v2: Init a new ftp: Could not init FrameToParse!");
                            ftp = 0;
                            break;
                        }
                    }

                    if (packHead->frameLen != ftp->frameLen) {
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ParseFramesThread v2: wrong frameLen", packHead->frameLen, ftp->frameLen);
                        ftp->timestamp = 0;
                        ftp = 0;
                        break;
                    }
                    if (packHead->packetCountTotal != ftp->packetCountTotal) {
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ParseFramesThread v2: wrong packetCountTotal", packHead->packetCountTotal, ftp->packetCountTotal);
                        ftp->timestamp = 0;
                        ftp = 0;
                        break;
                    }

                    if (ftp->packetSize[packetCounter]) {
                        if (ftp->packetSize[packetCounter] == UINT16_MAX) {
                            // NDA for this packet already received (unreachable)
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusWarning, "ParseFramesThread v2: Packet received when we already got the NDA frame %d pack %d", ftp->frameCounter, packetCounter);
                            ftp = 0;
                            break;
                        }
                        else {
                            // This packet was already received -> discard old packet and use new packet
                            ftp->packetCountGot--;
                            inst->lpDataStreamRedundantPacketCount++;
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusWarning, "ParseFramesThread v2: Packet received twice frame %d pack %d", ftp->frameCounter, packetCounter);
                        }
                    }

                    // All prepared, now copy packet into frameToParse
                    memcpy(ftp->frame + packHead->packetPosition, (char *)packet->p + BTA_ETH_PACKET_HEADER_SIZE, packHead->packetDataLen);
                    ftp->packetStartAddr[packetCounter] = packHead->packetPosition;
                    ftp->packetSize[packetCounter] = packHead->packetDataLen;
                    ftp->packetCountGot++;
                    ftp->timeLastPacket = BTAgetTickCount64();
                    break;
                }

                default: {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusInvalidVersion, "ParseFramesThread: Unknown header version %d", headerVersion);
                    break;
                }
            }

            if (packet) {
                status = BCBput(inst->packetsToFillQueue, (void **)packet);
                assert(status == BTA_StatusOk); // There are max as many packets around as the queue is long
                packet = 0;
            }

            if (ftp) {
                // A packet or an NDA was processed
                if (!inst->lpDataStreamRetrReqMode || !retransmissionSupport) {
                    // no retransmission. see if current frame is complete and parse
                    if (ftp->packetCountGot + ftp->packetCountNda == ftp->packetCountTotal) {
                        BTAparsePostprocessGrabCallbackEnqueue(winst, ftp);
                    }
                }
                else if (inst->lpDataStreamRetrReqMode == 1) {
                    if (ftp->packetCountGot + ftp->packetCountNda == ftp->packetCountTotal) {
                        // current frame is complete -> parse all frames from oldest to newest
                        while (1) {
                            BTA_FrameToParse *ftpTemp2 = getOlderThan(framesToParse, framesToParseLen, ftp->timestamp);
                            if (!ftpTemp2) {
                                break;
                            }
                            //BTAinfoEventHelper(winst->infoEventInst, IMPORTANCE_INFO, BTA_StatusInformation, "parsing (%d) (%d pLost)  packetCount reached", ftpTemp2->frameCounter, ftpTemp2->packetCountGot - ftpTemp2->packetCountTotal);
                            BTAparsePostprocessGrabCallbackEnqueue(winst, ftpTemp2);
                        }
                    }
                    else if (packetCounter != UINT16_MAX) {
                        // don't do this if we just received an NDA
                        // current frame isn't complete -> if there is a gap directly before the just received packet, immediately request retransmission
                        int pcGapEnd = packetCounter - 1;
                        int pcGapBeg = UINT16_MAX;
                        if (pcGapEnd >= 0 && !ftp->packetSize[pcGapEnd]) {
                            // gap detected! (packet size is 0, not packet or nda received) go back to see what's missing
                            for (pcGapBeg = pcGapEnd; ; pcGapBeg--) {
                                if (!pcGapBeg || ftp->packetSize[pcGapBeg - 1]) {
                                    // reached packet 0 or a valid packet size (also including nda)
                                    break;
                                }
                            }
                        }
                        BTA_FrameToParse *ftpPrev = 0;
                        int pcGapEndPrev = pcGapEnd;
                        int pcGapBegPrev = UINT16_MAX;
                        if (!pcGapBeg || !packetCounter) {
                            // the gap begins at the start of the frame or we just got the first packet of the frame
                            // -> also check last frame (second newest) if it has a gap at the end
                            ftpPrev = getNewer(framesToParse, framesToParseLen, ftp);
                            if (ftpPrev && !ftpPrev->packetSize[ftp->packetCountTotal - 1]) {
                                // it's missing its last packet -> it does have a gap at the end
                                pcGapEndPrev = ftpPrev->packetCountTotal - 1;
                                for (pcGapBegPrev = pcGapEndPrev; ; pcGapBegPrev--) {
                                    if (!pcGapBegPrev || ftpPrev->packetSize[pcGapBegPrev - 1]) {
                                        break;
                                    }
                                }
                            }
                        }
                        if (pcGapBegPrev != UINT16_MAX) {
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "gap opened frame %d in previous frame %d packet %d", ftpPrev->frameCounter, ftp->frameCounter, packetCounter);
                            sendRetrReqGap(winst, ftpPrev, pcGapBegPrev, pcGapEndPrev);
                        }
                        if (pcGapBeg != UINT16_MAX) {
                            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "gap opened frame %d packet %d", ftp->frameCounter, packetCounter);
                            sendRetrReqGap(winst, ftp, pcGapBeg, pcGapEnd);
                        }
                    }
                }
                else if (inst->lpDataStreamRetrReqMode == 2) {
                    //uint8_t olderFrameStillInQueue = 0;
                    // ....
                }
            }

        }

        // +++ clean up variables for v1 ++++++++++++++++++++++++++++
        for (int i = 0; i < framePacketsLen; i++) {
            BCBput(inst->packetsToFillQueue, (void **)framePacketsA[i]);
            BCBput(inst->packetsToFillQueue, (void **)framePacketsB[i]);
            BCBput(inst->packetsToFillQueue, (void **)framePacketsC[i]);
        }
        free(framePacketsA);
        framePacketsA = 0;
        free(framePacketsB);
        framePacketsB = 0;
        free(framePacketsC);
        framePacketsC = 0;
        // --- clean up variables for v1 ----------------------------

        // +++ clean up variables for v2 ++++++++++++++++++++++++++++
        //BTAfreeFrameToParse(&frameToParse);
        for (uint8_t ftpInd = 0; ftpInd < framesToParseLen; ftpInd++) {
            BTAfreeFrameToParse(&framesToParse[ftpInd]);
        }
        // --- clean up variables for v2 ----------------------------
    }

    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "ParseFramesThread thread terminated");
    return 0;
}


static void *shmReadRunFunction(void *handle) {
    BTA_WrapperInst *winst = (BTA_WrapperInst *)handle;
    if (!winst) {
        return 0;
    }
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
    if (!inst) {
        return 0;
    }
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "ShmReadThread started");
    BTA_Status status;

    // turn off streaming until it's off, the turn streaming on clean
    while (!inst->closing) {
        uint32_t interfaceConfig = 0xffff;
        status = BTAwriteRegister(winst, 0xfa, &interfaceConfig, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "SHM data: Could not set interface config to off. Continuing");
        }
        BTAmsleep(250);
        uint32_t shmKeyNum;
        status = BTAreadRegister(winst, 0x5c1, &shmKeyNum, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "SHM data: Could not read shmKey\n");
            BTAmsleep(500);
            continue;
        }
        if (shmKeyNum) {
            continue;
        }
        interfaceConfig = 2;
        status = BTAwriteRegister(winst, 0xfa, &interfaceConfig, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "SHM data: Could not set interface config to Shm. Continuing");
        }
        break;
    }

    while (!inst->closing) {
        uint32_t shmVersion;
        status = BTAreadRegister(winst, 0x5c0, &shmVersion, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "SHM data: Could not read shmVersion\n");
            BTAmsleep(500);
            continue;
        }
        if (shmVersion != SHM_PROTOCOL_VERSION) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, status, "SHM data: Unsupported Shm version\n");
            BTAmsleep(500);
            continue;
        }
        uint32_t shmKeyNum;
        status = BTAreadRegister(winst, 0x5c1, &shmKeyNum, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "SHM data: Could not read shmKey\n");
            BTAmsleep(500);
            continue;
        }
        if (!shmKeyNum) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, status, "SHM data: No Shm key\n");
            BTAmsleep(500);
            continue;
        }
        uint32_t shmSizeLw;
        status = BTAreadRegister(winst, 0x5c2, &shmSizeLw, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "SHM data: Could not read shmSizeLw\n");
            BTAmsleep(500);
            continue;
        }
        uint32_t shmSizeHw;
        status = BTAreadRegister(winst, 0x5c3, &shmSizeHw, 0);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "SHM data: Could not read shmSizeHw\n");
            BTAmsleep(500);
            continue;
        }
        uint32_t shmSize = (shmSizeHw << 16) | shmSizeLw;
        if (!shmSize) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, status, "SHM data: No Shm size\n");
            BTAmsleep(500);
            continue;
        }

        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, status, "SHM data: Connecting to shared memory (key: %d, size: %d)...\n", shmKeyNum, shmSize);
        int32_t shmFd;
        uint8_t *bufShmBase;
        sem_t *semFullWrite, *semFullRead, *semEmptyWrite, *semEmptyRead;
        fifo_t *fifoFull, *fifoEmpty;
        uint8_t *bufDataBase;
        if (!initShm(shmKeyNum, shmSize, &shmFd, &bufShmBase, &semFullWrite, &semFullRead, &semEmptyWrite, &semEmptyRead, &fifoFull, &fifoEmpty, &bufDataBase, winst->infoEventInst)) {
            BTAsleep(222);
            continue;
        }
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, status, "SHM data: Connection established\n");

        while (!inst->closing) {
            status = BTAwaitSemaphoreTimed(semFullRead, 2000);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "SHM data: Timeout on read, reconnecting\n");
                break;
            }
            BTAlockMutex(&(fifoFull->mutex));
            uint32_t offset;
            fifo_get_buffer(fifoFull, &offset);
            BTAunlockMutex(&(fifoFull->mutex));
            BTApostSemaphore(semFullWrite);
            BTA_Frame *frame;
            status = BTAparseFrameFromShm(winst, bufDataBase + offset, &frame);
            if (status == BTA_StatusOk) {
                if (frame) {
                    // Not an alive message frame
                    BTAgrabCallbackEnqueueFromShm(winst, frame);
                }
            }
            else {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "SHM data: Error parsing frame");
            }
            status = BTAwaitSemaphoreTimed(semEmptyWrite, 2000);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "SHM data: Timeout on write, reconnecting\n");
                break;
            }
            BTAlockMutex(&(fifoEmpty->mutex));
            fifo_push_buffer(fifoEmpty, offset);
            BTAunlockMutex(&(fifoEmpty->mutex));
            BTApostSemaphore(semEmptyRead);
        }
        closeShm(&semEmptyRead, &semEmptyWrite, &semFullRead, &semFullWrite, &bufShmBase, shmSize, &shmFd, shmKeyNum, winst->infoEventInst);
    }

    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "ShmReadThread thread terminated");
    return 0;
}


static BTA_FrameToParse *getNewer(BTA_FrameToParse **framesToParse, int framesToParseLen, BTA_FrameToParse *ftpRef) {
    uint64_t newest = 0;
    BTA_FrameToParse *ftp = 0;
    for (uint8_t ftpInd = 0; ftpInd < framesToParseLen; ftpInd++) {
        BTA_FrameToParse *ftpTemp = framesToParse[ftpInd];
        if (ftpTemp->timestamp > newest && ftpTemp->timestamp < ftpRef->timestamp) {
            // newer than others and older than ftpRef
            newest = ftpTemp->timestamp;
            ftp = framesToParse[ftpInd];
        }
    }
    return ftp;
}


static BTA_FrameToParse *getOldest(BTA_FrameToParse **framesToParse, int framesToParseLen) {
    uint64_t oldest = UINT64_MAX;
    BTA_FrameToParse *ftp = 0;
    for (uint8_t ftpInd = 0; ftpInd < framesToParseLen; ftpInd++) {
        BTA_FrameToParse *ftpTemp = framesToParse[ftpInd];
        if (ftpTemp->timestamp && ftpTemp->timestamp < oldest) {
            oldest = ftpTemp->timestamp;
            ftp = framesToParse[ftpInd];
        }
    }
    return ftp;
}


static BTA_FrameToParse *getOlderThan(BTA_FrameToParse **framesToParse, int framesToParseLen, uint64_t olderEqualThan) {
    uint64_t oldest = olderEqualThan;
    BTA_FrameToParse *ftp = 0;
    for (uint8_t ftpInd = 0; ftpInd < framesToParseLen; ftpInd++) {
        BTA_FrameToParse *ftpTemp = framesToParse[ftpInd];
        if (ftpTemp->timestamp && ftpTemp->timestamp <= oldest) {
            oldest = ftpTemp->timestamp;
            ftp = framesToParse[ftpInd];
        }
    }
    return ftp;
}


static BTA_Status sendRetrReq(BTA_WrapperInst *winst, uint16_t frameCounter, uint16_t *packetCounters, int packetCountersLen) {
    assert(winst);
    BTA_EthLibInst * inst = (BTA_EthLibInst *)winst->inst;
    assert(inst);

    inst->lpDataStreamRetrReqsCount += packetCountersLen;


    uint8_t *sendBuffer;
    uint32_t sendBufferLen;
    BTA_Status status = BTAtoByteStream(BTA_EthCommandRetransmissionRequest, BTA_EthSubCommandNone, (uint32_t)frameCounter, (uint8_t *)packetCounters, packetCountersLen * sizeof(uint16_t), inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen,
                                        inst->udpDataIpAddrVer, inst->udpDataIpAddr, inst->udpDataIpAddrLen, inst->udpDataPort, 0, 0, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    BTAlockMutex(inst->controlMutex);
    status = transmit(winst, sendBuffer, sendBufferLen, timeoutTiny);
    free(sendBuffer);
    sendBuffer = 0;
    BTAunlockMutex(inst->controlMutex);
    return status;
}


/*  @brief Sends a retransmission request for all missing packets up until maxPacketCounter  */
static BTA_Status sendRetrReqComplete(BTA_WrapperInst *winst, BTA_FrameToParse *frameToParse) {
    assert(winst);
    BTA_EthLibInst * inst = (BTA_EthLibInst *)winst->inst;
    assert(inst);

    BTA_Status status;
    int packetCountersLen = 0;
    for (int pInd = 0; pInd < frameToParse->packetCountTotal; pInd++) {
        if (!frameToParse->packetSize[pInd]) {
            retrReqPacketCounters[packetCountersLen++] = pInd;
            if (packetCountersLen == retrReqPacketCountMax) {
#if defined(DEBUG)
                char msg[5000] = { 0 };
                sprintf(msg + strlen(msg), "retrReq complete (%%d)");
                for (int i = 0; i < packetCountersLen; i++) {
                    //if (i == 0 || pack i + 1)
                    sprintf(msg + strlen(msg), "%4d", retrReqPacketCounters[i]);
                    if (strlen(msg) > 100) {
                        sprintf(msg + strlen(msg), "...");
                        break;
                    }
                }
                sprintf(msg + strlen(msg), " (%d)", packetCountersLen);
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusDebug, msg, frameToParse->frameCounter);
#endif
                // reached limit -> request packets gathered so far
                status = sendRetrReq(winst, frameToParse->frameCounter, retrReqPacketCounters, packetCountersLen);
                if (status != BTA_StatusOk) {
                    return status;
                }
                packetCountersLen = 0;
            }
        }
    }

    if (packetCountersLen > 0) {
#if defined(DEBUG)
        char msg[5000] = { 0 };
        sprintf(msg + strlen(msg), "retrReq complete (%%d)");
        for (int i = 0; i < packetCountersLen; i++) {
            //if (i == 0 || pack i + 1)
            sprintf(msg + strlen(msg), "%4d", retrReqPacketCounters[i]);
            if (strlen(msg) > 100) {
                sprintf(msg + strlen(msg), "...");
                break;
            }
        }
        sprintf(msg + strlen(msg), " (%d)", packetCountersLen);
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusDebug, msg, frameToParse->frameCounter);
#endif
        status = sendRetrReq(winst, frameToParse->frameCounter, retrReqPacketCounters, packetCountersLen);
        if (status != BTA_StatusOk) {
            return status;
        }
        packetCountersLen = 0;
    }

    frameToParse->retryTime = BTAgetTickCount64() + (uint64_t)inst->lpRetrReqIntervalMin;
    return BTA_StatusOk;
}


/*  @brief Sends a retransmission request for all packets between and including pcGapBeg and pcGapEnd
           Does not consider frameToParse->retryTime, instead it always sends*/
static BTA_Status sendRetrReqGap(BTA_WrapperInst *winst, BTA_FrameToParse *frameToParse, uint16_t pcGapBeg, uint16_t pcGapEnd) {
    assert(winst);
    BTA_EthLibInst * inst = (BTA_EthLibInst *)winst->inst;
    assert(inst);
    assert(pcGapBeg <= pcGapEnd);
    if (pcGapEnd == UINT16_MAX) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "sendRetrReqGap: INVALID pcIndGapEnd!");
        return BTA_StatusInvalidParameter;
    }

    BTA_Status status;
    int packetCountersLen = 0;
    for (int pInd = pcGapBeg; pInd <= pcGapEnd; pInd++) {
        retrReqPacketCounters[packetCountersLen++] = pInd;
        if (packetCountersLen == retrReqPacketCountMax) {
#if defined(DEBUG)
            char msg[5000] = { 0 };
            sprintf(msg + strlen(msg), "retrReq gap (%%d)");
            for (int i = 0; i < packetCountersLen; i++) {
                //if (i == 0 || pack i + 1)
                sprintf(msg + strlen(msg), "%4d", retrReqPacketCounters[i]);
                if (strlen(msg) > 100) {
                    sprintf(msg + strlen(msg), "...");
                    break;
                }
            }
            sprintf(msg + strlen(msg), " (%d)", packetCountersLen);
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusDebug, msg, frameToParse->frameCounter);
#endif
            // reached limit -> request packets gathered so far
            status = sendRetrReq(winst, frameToParse->frameCounter, retrReqPacketCounters, packetCountersLen);
            if (status != BTA_StatusOk) {
                return status;
            }
            packetCountersLen = 0;
        }
    }

    if (packetCountersLen > 0) {
#if defined(DEBUG)
        char msg[5000] = { 0 };
        sprintf(msg + strlen(msg), "retrReq gap (%%d)");
        for (int i = 0; i < packetCountersLen; i++) {
            //if (i == 0 || pack i + 1)
            sprintf(msg + strlen(msg), "%4d", retrReqPacketCounters[i]);
            if (strlen(msg) > 100) {
                sprintf(msg + strlen(msg), "...");
                break;
            }
        }
        sprintf(msg + strlen(msg), " (%d)", packetCountersLen);
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_CRITICAL, BTA_StatusDebug, msg, frameToParse->frameCounter);
#endif
        status = sendRetrReq(winst, frameToParse->frameCounter, retrReqPacketCounters, packetCountersLen);
        if (status != BTA_StatusOk) {
            return status;
        }
        packetCountersLen = 0;
    }

    frameToParse->retryTime = BTAgetTickCount64() + (uint64_t)inst->lpRetrReqIntervalMin;
    return BTA_StatusOk;
}


BTA_Status BTAETHgetIntegrationTime(BTA_WrapperInst *winst, uint32_t *integrationTime) {
    return BTAETHreadRegister(winst, 0x0005, integrationTime, 0);
}


BTA_Status BTAETHsetIntegrationTime(BTA_WrapperInst *winst, uint32_t integrationTime) {
    if (integrationTime < 1 || integrationTime > 0xffff) {
        return BTA_StatusInvalidParameter;
    }
    return BTAETHwriteRegister(winst, 0x0005, &integrationTime, 0);
}


BTA_Status BTAETHgetFrameRate(BTA_WrapperInst *winst, float *frameRate) {
    if (!frameRate) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t frameRate32;
    BTA_Status status = BTAETHreadRegister(winst, 0x000a, &frameRate32, 0);
    if (status == BTA_StatusOk) {
        *frameRate = (float)frameRate32;
    }
    return status;
}


BTA_Status BTAETHsetFrameRate(BTA_WrapperInst *winst, float frameRate) {
    uint32_t frameRate32 = (uint32_t)frameRate;
    if (frameRate < 1 || frameRate > 0xffff) {
        return BTA_StatusInvalidParameter;
    }
    return BTAETHwriteRegister(winst, 0x000a, &frameRate32, 0);
}


BTA_Status BTAETHgetModulationFrequency(BTA_WrapperInst *winst, uint32_t *modulationFrequency) {
    if (!modulationFrequency) {
        return BTA_StatusInvalidParameter;
    }
    BTA_Status status;
    status = BTAETHreadRegister(winst, 0x0009, modulationFrequency, 0);
    if (status != BTA_StatusOk) {
        *modulationFrequency = 0;
        return status;
    }
    *modulationFrequency *= 10000;
    return BTA_StatusOk;
}


BTA_Status BTAETHsetModulationFrequency(BTA_WrapperInst *winst, uint32_t modulationFrequency) {
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
    return BTAETHwriteRegister(winst, 0x0009, &modFreq, 0);
}


BTA_Status BTAETHgetGlobalOffset(BTA_WrapperInst *winst, float *offset) {
    if (!winst || !offset) {
        return BTA_StatusInvalidParameter;
    }
    *offset = 0;
    uint32_t modFreq;
    BTA_Status status;
    status = BTAETHreadRegister(winst, 9, &modFreq, 0);
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
    status = BTAETHreadRegister(winst, 0x00c1 + modFreqIndex, &offset32, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    *offset = (int16_t)offset32;
    return BTA_StatusOk;
}


BTA_Status BTAETHsetGlobalOffset(BTA_WrapperInst *winst, float offset) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_Status status;
    if (offset < -32768 || offset > 32767) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t modFreq;
    status = BTAETHreadRegister(winst, 9, &modFreq, 0);
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
    status = BTAETHwriteRegister(winst, 0x00c1 + modFreqIndex, (uint32_t *)&offset32, 0);
    return status;
}


BTA_Status BTAETHwriteCurrentConfigToNvm(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t password = 0x4877;
    BTA_Status status = BTAETHwriteRegister(winst, 0x0022, &password, 0);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAwriteCurrentConfigToNvm() failed (a)");
        return status;
    }
    BTAmsleep(200);
    uint32_t command = 0xdd9e;
    status = BTAETHwriteRegister(winst, 0x0033, &command, 0);
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
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTAwriteCurrentConfigToNvm() failed, the device said %d", result);
                return BTA_StatusRuntimeError;
            }
            return BTA_StatusOk;
        }
    } while (BTAgetTickCount() < endTime);
    return BTA_StatusTimeOut;
}


BTA_Status BTAETHrestoreDefaultConfig(BTA_WrapperInst *winst) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t password = 0x4877;
    BTA_Status status = BTAETHwriteRegister(winst, 0x0022, &password, 0);
    if (status != BTA_StatusOk) {
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTArestoreDefaultConfig() failed (a)");
        return status;
    }
    BTAmsleep(200);
    uint32_t command = 0xc2ae;
    status = BTAETHwriteRegister(winst, 0x0033, &command, 0);
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
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "BTArestoreDefaultConfig() failed, the device said %d", result);
                return BTA_StatusRuntimeError;
            }
            return BTA_StatusOk;
        }
    } while (BTAgetTickCount() < endTime);
    return BTA_StatusTimeOut;
}


BTA_Status BTAETHreadRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount) {
    return readRegister(winst, address, data, registerCount, timeoutDefault);
}


static BTA_Status readRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount, uint32_t timeout) {
    if (!winst || !data) {
        return BTA_StatusInvalidParameter;
    }
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
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
    BTA_Status status = BTAtoByteStream(BTA_EthCommandRead, BTA_EthSubCommandNone, address, 0, lenToRead, inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen,
                               inst->udpControlCallbackIpAddrVer, inst->udpControlCallbackIpAddr, inst->udpControlCallbackIpAddrLen, inst->udpControlCallbackPort, 0, 0, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    // If control connection is UDP, try 3 times
    int attempts = 3;
    if (inst->tcpControlConnectionStatus == 16 || inst->tcpControlConnectionStatus == 8) {
        attempts = 1;
    }
    uint8_t *dataBuffer;
    uint32_t dataBufferLen;
    while (1) {
        if (!attempts--) {
            free(sendBuffer);
            sendBuffer = 0;
            return status;
        }
        BTAlockMutex(inst->controlMutex);
        status = transmit(winst, sendBuffer, sendBufferLen, timeout);
        if (status == BTA_StatusDeviceUnreachable) {
            // retry
            BTAunlockMutex(inst->controlMutex);
            continue;
        }
        if (status != BTA_StatusOk) {
            free(sendBuffer);
            sendBuffer = 0;
            BTAunlockMutex(inst->controlMutex);
            return status;
        }
        status = receiveControlResponse(winst, sendBuffer, &dataBuffer, &dataBufferLen, timeout, 0);
        BTAunlockMutex(inst->controlMutex);
        if (status == BTA_StatusDeviceUnreachable) {
            // retry
            continue;
        }
        if (status != BTA_StatusOk) {
            free(sendBuffer);
            sendBuffer = 0;
            return status;
        }
        // success
        free(sendBuffer);
        sendBuffer = 0;
        break;
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


BTA_Status BTAETHwriteRegister(BTA_WrapperInst *winst, uint32_t address, uint32_t *data, uint32_t *registerCount) {
    if (!winst || !data) {
        return BTA_StatusInvalidParameter;
    }
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
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
    BTA_Status status = BTAtoByteStream(BTA_EthCommandWrite, BTA_EthSubCommandNone, address, (void *)data, lenToWrite, inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen,
                                        inst->udpControlCallbackIpAddrVer, inst->udpControlCallbackIpAddr, inst->udpControlCallbackIpAddrLen, inst->udpControlCallbackPort, 0, 0, 0);
    if (status != BTA_StatusOk) {
        return status;
    }
    // If control connection is UDP, try 3 times
    int attempts = 3;
    if (inst->tcpControlConnectionStatus == 16 || inst->tcpControlConnectionStatus == 8) {
        attempts = 1;
    }
    while (1) {
        if (!attempts--) {
            free(sendBuffer);
            sendBuffer = 0;
            return status;
        }
        BTAlockMutex(inst->controlMutex);
        status = transmit(winst, sendBuffer, sendBufferLen, timeoutDefault);
        if (status == BTA_StatusDeviceUnreachable) {
            // retry
            BTAunlockMutex(inst->controlMutex);
            continue;
        }
        if (status != BTA_StatusOk) {
            free(sendBuffer);
            sendBuffer = 0;
            BTAunlockMutex(inst->controlMutex);
            return status;
        }
        uint32_t dataLen = 0;
        status = receiveControlResponse(winst, sendBuffer, 0, &dataLen, timeoutDefault, 0);
        BTAunlockMutex(inst->controlMutex);
        if (status == BTA_StatusDeviceUnreachable) {
            // retry
            continue;
        }
        if (status != BTA_StatusOk) {
            free(sendBuffer);
            sendBuffer = 0;
            return status;
        }
        // success
        free(sendBuffer);
        sendBuffer = 0;
        break;
    }
    if (registerCount) {
        *registerCount = lenToWrite / 2;
    }
    return status;
}


BTA_Status BTAETHsetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float value) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    switch (libParam) {
    case BTA_LibParamKeepAliveMsgInterval:
        if (value < 0) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAsetLibParam: Interval must be >= 0");
            return BTA_StatusInvalidParameter;
        }
        inst->lpKeepAliveMsgInterval = value;
        return BTA_StatusOk;
    case BTA_LibParamCrcControlEnabled:
        inst->lpControlCrcEnabled = (uint8_t)(value != 0);
        return BTA_StatusOk;

    case BTA_LibParamDataStreamRetrReqMode:
        inst->lpDataStreamRetrReqMode = (uint8_t)value;
        return BTA_StatusOk;
    case BTA_LibParamDataStreamPacketWaitTimeout:
        inst->lpDataStreamPacketWaitTimeout = value;
        return BTA_StatusOk;
    case BTA_LibParamDataStreamRetrReqIntervalMin:
        inst->lpRetrReqIntervalMin = value;
        return BTA_StatusOk;

    case BTA_LibParamDataStreamRetrReqMaxAttempts:
        inst->lpDataStreamRetrReqMaxAttempts = value;
        return BTA_StatusOk;
    case BTA_LibParamDataStreamRetrReqsCount:
    case BTA_LibParamDataStreamRetransPacketsCount:
    case BTA_LibParamDataStreamNdasReceived:
    case BTA_LibParamDataStreamRedundantPacketCount:
        return BTA_StatusIllegalOperation;

    case BTA_LibParamDataSockOptRcvtimeo: {
#       ifdef PLAT_WINDOWS
            DWORD timeout = (DWORD)value;
#       else
            struct timeval timeout;
            timeout.tv_sec = (int)(value / 1000);
            timeout.tv_usec = ((int)(value * 1000.0f)) % 1000000;
#       endif
        int err = setsockopt(inst->udpDataSocket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
        if (err) {
            return (BTA_Status)-getLastSocketError();
        }
        return BTA_StatusOk;
    }

    case BTA_LibParamDataSockOptRcvbuf: {
        int optLen = 4;
        uint32_t optVal = (uint32_t)value;
        int err = setsockopt(inst->udpDataSocket, SOL_SOCKET, SO_RCVBUF, (char *)&optVal, optLen);
        if (err) {
            return (BTA_Status)-getLastSocketError();
        }
        return BTA_StatusOk;
    }

    default:
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAsetLibParam: LibParam not supported %d", libParam);
        return BTA_StatusNotSupported;
    }
}


BTA_Status BTAETHgetLibParam(BTA_WrapperInst *winst, BTA_LibParam libParam, float *value) {
    if (!winst) {
        return BTA_StatusInvalidParameter;
    }
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
    if (!inst) {
        return BTA_StatusInvalidParameter;
    }
    switch (libParam) {
    case BTA_LibParamKeepAliveMsgInterval:
        *value = inst->lpKeepAliveMsgInterval;
        return BTA_StatusOk;
    case BTA_LibParamCrcControlEnabled:
        *value = (float)inst->lpControlCrcEnabled;
        return BTA_StatusOk;

    case BTA_LibParamDataStreamRetrReqMode:
        *value = (float)inst->lpDataStreamRetrReqMode;
        return BTA_StatusOk;
    case BTA_LibParamDataStreamPacketWaitTimeout:
        *value = (float)inst->lpDataStreamPacketWaitTimeout;
        return BTA_StatusOk;
    case BTA_LibParamDataStreamRetrReqIntervalMin:
        *value = (float)inst->lpRetrReqIntervalMin;
        return BTA_StatusOk;
    case BTA_LibParamDataStreamRetrReqMaxAttempts:
        *value = inst->lpDataStreamRetrReqMaxAttempts;
        return BTA_StatusOk;
    case BTA_LibParamDataStreamRetrReqsCount:
        *value = (float)inst->lpDataStreamRetrReqsCount;
        inst->lpDataStreamRetrReqsCount = 0;
        return BTA_StatusOk;
    case BTA_LibParamDataStreamRetransPacketsCount:
        *value = inst->lpDataStreamRetrPacketsCount;
        inst->lpDataStreamRetrPacketsCount = 0;
        return BTA_StatusOk;
    case BTA_LibParamDataStreamNdasReceived:
        *value = inst->lpDataStreamNdasReceived;
        inst->lpDataStreamNdasReceived = 0;
        return BTA_StatusOk;
    case BTA_LibParamDataStreamRedundantPacketCount:
        *value = inst->lpDataStreamRedundantPacketCount;
        inst->lpDataStreamRedundantPacketCount = 0;
        return BTA_StatusOk;

    case BTA_LibParamDataSockOptRcvtimeo: {
#       ifdef PLAT_WINDOWS
        DWORD timeout;
#       else
        struct timeval timeout = { 0 };
#       endif
        socklen_t optLen = sizeof(timeout);
        int err = getsockopt(inst->udpDataSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, &optLen);
        if (err) {
            return (BTA_Status)-getLastSocketError();
        }
#       ifdef PLAT_WINDOWS
        *value = (float)timeout;
#       else
        *value = timeout.tv_sec * 1000.0f + timeout.tv_usec / 1000.0f;
#       endif
        return BTA_StatusOk;
    }

    case BTA_LibParamDataSockOptRcvbuf: {
        uint32_t optVal = 0;
        socklen_t optLen = sizeof(optVal);
        int err = getsockopt(inst->udpDataSocket, SOL_SOCKET, SO_RCVBUF, (char *)&optVal, &optLen);
        if (err) {
            return (BTA_Status)-getLastSocketError();
        }
        *value = (float)optVal;
        return BTA_StatusOk;
    }

    default:
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotSupported, "BTAgetLibParam: LibParam not supported");
        return BTA_StatusNotSupported;
    }
}




BTA_Status BTAETHflashUpdate(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport) {
    if (!winst) {
        if (progressReport) (*progressReport)(BTA_StatusInvalidParameter, 0);
        return BTA_StatusInvalidParameter;
    }
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
    if (!inst) {
        if (progressReport) (*progressReport)(BTA_StatusInvalidParameter, 0);
        return BTA_StatusInvalidParameter;
    }

    BTA_EthCommand cmd = BTA_EthCommandNone;
    BTA_EthSubCommand subCmd = BTA_EthSubCommandNone;
    BTAgetFlashCommand(flashUpdateConfig->target, flashUpdateConfig->flashId, &cmd, &subCmd);
    if (cmd == BTA_EthCommandNone) {
        if (progressReport) (*progressReport)(BTA_StatusInvalidParameter, 0);
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAflashUpdate: FlashTarget %d or FlashId %d not supported", flashUpdateConfig->target, flashUpdateConfig->flashId);
        return BTA_StatusInvalidParameter;
    }

    BTA_Status status;
    uint8_t *sendBuffer;
    uint32_t sendBufferLen;
    if (inst->udpControlConnectionStatus == 16 || inst->udpControlConnectionStatus == 3 || inst->udpControlConnectionStatus == 8) {
        uint8_t *sendBufferPart = flashUpdateConfig->data;
        uint32_t sendBufferPartLen = MTHmin(768, flashUpdateConfig->dataLen);
        uint32_t sendBufferLenSent = 0;
        uint32_t packetNumber = 0;
        uint32_t fileCrc32 = (uint32_t)CRC32ccitt(flashUpdateConfig->data, flashUpdateConfig->dataLen);
        BTAlockMutex(inst->controlMutex);
        if (progressReport) (*progressReport)(BTA_StatusOk, 0);
        do {
            packetNumber++;
            status = BTAtoByteStream(cmd, subCmd, flashUpdateConfig->address, sendBufferPart, sendBufferPartLen, inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen,
                                                inst->udpControlCallbackIpAddrVer, inst->udpControlCallbackIpAddr, inst->udpControlCallbackIpAddrLen, inst->udpControlCallbackPort,
                                                packetNumber, flashUpdateConfig->dataLen, fileCrc32);
            if (status != BTA_StatusOk) {
                if (progressReport) (*progressReport)(status, 0);
                return status;
            }
            uint8_t attempts = 0;
            do {
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: Transmitting packet #%d", packetNumber);
                status = transmit(winst, sendBuffer, sendBufferLen, timeoutDefault);
                if (status == BTA_StatusOk) {
                    status = receiveControlResponse(winst, sendBuffer, 0, 0, timeoutDefault, 0);
                    if (status != BTA_StatusOk) {
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "BTAflashUpdate: Erroneous response for packet #%d (attempt %d)", packetNumber, attempts + 1);
                        // wait between attempts
                        BTAmsleep(1000);
                    }
                }
                else {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "BTAflashUpdate: Failed to send packet #%d (attempt %d)", packetNumber, attempts + 1);
                    // wait between attempts
                    BTAmsleep(1000);
                }
            } while (attempts++ < 5 && status != BTA_StatusOk);
            free(sendBuffer);
            sendBuffer = 0;
            if (status != BTA_StatusOk) {
                BTAunlockMutex(inst->controlMutex);
                BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAflashUpdate: Failed to send packet #%d", packetNumber);
                if (progressReport) (*progressReport)(status, 0);
                return status;
            }
            sendBufferLenSent += sendBufferPartLen;
            // report progress (use 50 of the 100% for the transmission
            if (progressReport) (*progressReport)(status, (byte)(50.0 * sendBufferLenSent / flashUpdateConfig->dataLen));
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: Sent #%d", packetNumber);
            sendBufferPart += sendBufferPartLen;
            if (sendBufferLenSent + sendBufferPartLen > flashUpdateConfig->dataLen) {
                sendBufferPartLen = flashUpdateConfig->dataLen - sendBufferLenSent;
            }
            // wait between packets
            BTAmsleep(22);
        } while (sendBufferLenSent < flashUpdateConfig->dataLen);
        BTAunlockMutex(inst->controlMutex);
    }

    else if (inst->tcpControlConnectionStatus == 16 || inst->tcpControlConnectionStatus == 8) {
        status = BTAtoByteStream(cmd, subCmd, flashUpdateConfig->address, flashUpdateConfig->data, flashUpdateConfig->dataLen, inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen,
                                 inst->udpControlCallbackIpAddrVer, inst->udpControlCallbackIpAddr, inst->udpControlCallbackIpAddrLen, inst->udpControlCallbackPort, 0, 0, 0);
        if (status != BTA_StatusOk) {
            if (progressReport) (*progressReport)(status, 0);
            return status;
        }
        BTAlockMutex(inst->controlMutex);
        uint8_t *sendBufferPart = sendBuffer;
        uint32_t sendBufferPartLen = sendBufferLen > 500000 ? sendBufferLen / 10 : sendBufferLen;
        uint32_t sendBufferLenSent = 0;
        if (progressReport) (*progressReport)(BTA_StatusOk, 0);
        do {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: Transmitting %d bytes of data (already sent %d bytes)", sendBufferLen, sendBufferLenSent);
            uint8_t attempts = 0;
            while (1) {
                status = transmit(winst, sendBufferPart, sendBufferPartLen, timeoutHuge);
                if (status != BTA_StatusOk) {
                    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_WARNING, status, "BTAflashUpdate: Failed to send! (attempt %d)", attempts + 1);
                    attempts++;
                    if (attempts == 3) {
                        BTAunlockMutex(inst->controlMutex);
                        free(sendBuffer);
                        sendBuffer = 0;
                        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAflashUpdate: Failed to send!");
                        if (progressReport) (*progressReport)(status, 0);
                        return status;
                    }
                    BTAmsleep(1000);
                    continue;
                }
                break;
            }
            sendBufferPart += sendBufferPartLen;
            sendBufferLenSent += sendBufferPartLen;
            if (sendBufferLenSent + sendBufferPartLen > sendBufferLen) {
                sendBufferPartLen = sendBufferLen - sendBufferLenSent;
            }
            // report progress (use 50 of the 100% for the transmission
            if (progressReport) (*progressReport)(status, (byte)(50.0 * sendBufferLenSent / sendBufferLen));
            BTAmsleep(22);
        } while (sendBufferLenSent < sendBufferLen);
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: %d bytes sent", sendBufferLenSent);
        status = receiveControlResponse(winst, sendBuffer, 0, 0, timeoutHuge, 0);
        free(sendBuffer);
        sendBuffer = 0;
        BTAunlockMutex(inst->controlMutex);
        if (status != BTA_StatusOk) {
            BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, status, "BTAflashUpdate: Erroneous response!");
            if (progressReport) (*progressReport)(status, 0);
            return status;
        }
    }
    else {
        if (progressReport) (*progressReport)(BTA_StatusNotConnected, 0);
        BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusNotConnected, "BTAflashUpdate: Not connected");
        return BTA_StatusNotConnected;
    }

    // flash cmd successfully sent, now poll status
    BTAinfoEventHelper(winst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "BTAflashUpdate: File transfer finished successfully");

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



BTA_Status BTAETHflashRead(BTA_WrapperInst *winst, BTA_FlashUpdateConfig *flashUpdateConfig, FN_BTA_ProgressReport progressReport, uint8_t quiet) {
    if (!winst) {
        if (progressReport) (*progressReport)(BTA_StatusInvalidParameter, 0);
        return BTA_StatusInvalidParameter;
    }
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
    if (!inst) {
        if (progressReport) (*progressReport)(BTA_StatusInvalidParameter, 0);
        return BTA_StatusInvalidParameter;
    }

    BTA_EthCommand cmd;
    BTA_EthSubCommand subCmd;
    BTAgetFlashCommand(flashUpdateConfig->target, flashUpdateConfig->flashId, &cmd, &subCmd);
    if (cmd == BTA_EthCommandNone) {
        if (progressReport) (*progressReport)(BTA_StatusInvalidParameter, 0);
        if (!quiet) BTAinfoEventHelper(winst->infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "BTAflashRead: FlashTarget %d or FlashId %d not supported", flashUpdateConfig->target, flashUpdateConfig->flashId);
        return BTA_StatusInvalidParameter;
    }
    // change the flash command to a read command
    cmd = (BTA_EthCommand)((int)cmd + 100);

    uint8_t *sendBuffer;
    uint32_t sendBufferLen;
    BTA_Status status = BTAtoByteStream(cmd, subCmd, flashUpdateConfig->address, 0, 0, inst->lpControlCrcEnabled, &sendBuffer, &sendBufferLen,
                                        inst->udpControlCallbackIpAddrVer, inst->udpControlCallbackIpAddr, inst->udpControlCallbackIpAddrLen, inst->udpControlCallbackPort, 0, 0, 0);
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
    if (quiet) status = receiveControlResponse_2(sendBuffer, &dataBuffer, &dataBufferLen, timeoutHuge, progressReport,
                                                 &inst->udpControlConnectionStatus, &inst->udpControlSocket, inst->udpControlCallbackIpAddr, inst->udpControlCallbackPort, &inst->tcpControlConnectionStatus, &inst->tcpControlSocket,
                                                 &inst->keepAliveMsgTimestamp, inst->lpKeepAliveMsgInterval, 0);
    else status = receiveControlResponse(winst, sendBuffer, &dataBuffer, &dataBufferLen, timeoutHuge, progressReport);
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
    free(dataBuffer);
    dataBuffer = 0;
    if (progressReport) (*progressReport)(BTA_StatusOk, 100);
    return BTA_StatusOk;
}


void *BTAETHdiscoveryRunFunction(BTA_DiscoveryInst *inst) {
    if (!inst) {
        return 0;
    }

    BTAinfoEventHelper(inst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Discovery: started");

#   ifdef PLAT_WINDOWS
    int err;
    WSADATA wsaData;
    do {
        err = WSAStartup(MAKEWORD(2, 2), &wsaData);
        BTAmsleep(220);
        if (inst->abortDiscovery) {
            return 0;
        }
    } while (err != NO_ERROR);
#   endif

    SOCKET udpRcvSocket = openDiscoveryRcvSocket(&(inst->callbackPort), inst->infoEventInst);
    if (!udpRcvSocket) {
#       ifdef PLAT_WINDOWS
        WSACleanup();
#       endif
        return 0;
    }

    // build packet for discovery transmission
    uint8_t buffer[64] = { 0 };
    fillDiscoveryBuffer(buffer, inst->deviceType, inst->callbackIpAddr, inst->callbackPort);


    uint16_t broadcastPortDefault = 11003;
    uint16_t broadcastPort = inst->broadcastPort;
    if (!broadcastPort) {
        broadcastPort = broadcastPortDefault;
    }
    uint8_t broadcastIpAddrDefault[4] = { 255, 255, 255, 255 };
    uint8_t *broadcastIpAddr = inst->broadcastIpAddr;
    if (!broadcastIpAddr) {
        broadcastIpAddr = broadcastIpAddrDefault;
    }

    BTA_Status status;
    SOCKET sockets[50];
    int socketsCount;
    status = openDiscoverySendSockets(broadcastPort, sockets, &socketsCount, inst->infoEventInst);
    if (status != BTA_StatusOk) {
        closesocket(udpRcvSocket);
#       ifdef PLAT_WINDOWS
        WSACleanup();
#       endif
        return 0;
    }

    uint32_t ticksSendMessage = BTAgetTickCount();
    while (!inst->abortDiscovery) {

        if (BTAgetTickCount() >= ticksSendMessage) {
            ticksSendMessage += 3000;
            sendDiscoveryMessage(sockets, socketsCount, buffer, sizeof(buffer), broadcastIpAddr, broadcastPort, inst->infoEventInst);
        }

        uint8_t *responsePayload;
        uint32_t responseLen = 0;
        uint8_t udpControlConnectionStatus = 16;
        uint8_t tcpControlConnectionStatus = 0;
        status = receiveControlResponse_2(buffer, &responsePayload, &responseLen, timeoutTiny, 0,
                                          &udpControlConnectionStatus, &udpRcvSocket, 0, inst->callbackPort, &tcpControlConnectionStatus, 0, 0, 0, 0);
        if (status == BTA_StatusDeviceUnreachable) {
            BTAmsleep(100);
        }
        else if (status != BTA_StatusOk) {
            BTAinfoEventHelper(inst->infoEventInst, VERBOSE_WARNING, status, "Discovery: Error receiving message");
        }
        else {
            BTA_DeviceInfo *deviceInfo = parseDiscoveryResponse(responsePayload, responseLen, inst->deviceListMutex, inst->deviceList, inst->deviceListCount, inst->infoEventInst);
            if (deviceInfo) {
                BTAinfoEventHelper(inst->infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Discovery: received an answer from %d.%d.%d.%d", deviceInfo->deviceIpAddr[0], deviceInfo->deviceIpAddr[1], deviceInfo->deviceIpAddr[2], deviceInfo->deviceIpAddr[3]);
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
                    deviceInfo = 0;
                }
            }
            free(responsePayload);
            responsePayload = 0;
        }
    }

    BTAinfoEventHelper(inst->infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "Discovery: stopped");

    closeSockets(sockets, socketsCount);
    closesocket(udpRcvSocket);
#   ifdef PLAT_WINDOWS
    WSACleanup();
#   endif
    return 0;
}


static SOCKET openDiscoveryRcvSocket(uint16_t *callbackPort, BTA_InfoEventInst *infoEventInst) {
    int err;
    // bind a socket for listening
    SOCKET udpRcvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpRcvSocket == INVALID_SOCKET) {
        err = getLastSocketError();
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Discovery: Error creating socket, error: %d", err);
#       ifdef PLAT_WINDOWS
        WSACleanup();
#       endif
        return 0;
    }
    u_long yes = 1;
    err = setsockopt(udpRcvSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
    if (err == SOCKET_ERROR) {
        err = getLastSocketError();
        closesocket(udpRcvSocket);
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Discovery: Error setting SO_REUSEADDR, error: %d", err);
#       ifdef PLAT_WINDOWS
        WSACleanup();
#       endif
        return 0;
    }
    yes = 1;
    err = setsockopt(udpRcvSocket, SOL_SOCKET, SO_BROADCAST, (const char *)&yes, sizeof(yes));
    if (err == SOCKET_ERROR) {
        err = getLastSocketError();
        closesocket(udpRcvSocket);
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Discovery: Error setting SO_BROADCAST, error: %d", err);
#       ifdef PLAT_WINDOWS
        WSACleanup();
#       endif
        return 0;
    }
    struct sockaddr_in socketAddr = { 0 };
    socketAddr.sin_family = AF_INET;
    socketAddr.sin_addr.s_addr = INADDR_ANY;
    socketAddr.sin_port = htons(*callbackPort);
    err = bind(udpRcvSocket, (struct sockaddr *)&socketAddr, sizeof(socketAddr));
    if (err == SOCKET_ERROR) {
        err = getLastSocketError();
        closesocket(udpRcvSocket);
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Discovery: Error binding socket, error: %d", err);
#       ifdef PLAT_WINDOWS
        WSACleanup();
#       endif
        return 0;
    }

    // get the port number in case of inst->udpControlCallbackPort == 0
    socklen_t socketAddrLen = sizeof(socketAddr);
    err = getsockname(udpRcvSocket, (struct sockaddr *)&socketAddr, &socketAddrLen);
    if (err != 0) {
        err = getLastSocketError();
        closesocket(udpRcvSocket);
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Discovery: Error in getsockname, error: %d", err);
#       ifdef PLAT_WINDOWS
        WSACleanup();
#       endif
        return 0;
    }
    *callbackPort = htons(socketAddr.sin_port);

    DWORD bufferSizeControl = 1000000;
    err = setsockopt(udpRcvSocket, SOL_SOCKET, SO_RCVBUF, (const char *)&bufferSizeControl, sizeof(bufferSizeControl));
    if (err == SOCKET_ERROR) {
        err = getLastSocketError();
        closesocket(udpRcvSocket);
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Discovery: Error setting SO_RCVBUF, error: %d", err);
#       ifdef PLAT_WINDOWS
        WSACleanup();
#       endif
        return 0;
    }
    BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Discovery: Receive socket open");
    return udpRcvSocket;
}


static void fillDiscoveryBuffer(uint8_t buffer[64], uint16_t deviceType, uint8_t callbackIpAddr[4], uint16_t callbackPort) {
    memset(buffer, 0, 64);
    buffer[0] = BTA_ETH_PREAMBLE_0;
    buffer[1] = BTA_ETH_PREAMBLE_1;
    buffer[2] = 3;
    buffer[3] = BTA_EthCommandDiscovery;
    //buffer[6] = 1; // flags
    //buffer[7] = 1; // flags
    if (deviceType == BTA_DeviceTypeEthernet) {
        deviceType = BTA_DeviceTypeAny;
    }
    buffer[12] = (uint8_t)(deviceType >> 8);
    buffer[13] = (uint8_t)deviceType;
    buffer[16] = 4;
    if (callbackIpAddr) {
        buffer[17] = callbackIpAddr[0];
        buffer[18] = callbackIpAddr[1];
        buffer[19] = callbackIpAddr[2];
        buffer[20] = callbackIpAddr[3];
    }
    buffer[21] = (uint8_t)(callbackPort >> 8);
    buffer[22] = (uint8_t)callbackPort;
    uint16_t crc16 = crc16_ccitt(buffer + 2, 60);
    buffer[62] = (uint8_t)(crc16 >> 8);
    buffer[63] = (uint8_t)crc16;
}


static BTA_Status openDiscoverySendSockets(uint16_t broadcastPort, SOCKET *sockets, int *socketsCount, BTA_InfoEventInst *infoEventInst) {
    // retrieve all local network interfaces and bind
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo *addrInfo = 0;
    struct addrinfo *addrInfoTemp;
    char buf[10];
    sprintf(buf, "%d", broadcastPort);
#   ifdef PLAT_WINDOWS
    int err = getaddrinfo("", "" /*buf untested*/, &hints, &addrInfo);
#   elif defined PLAT_LINUX || PLAT_APPLE
    hints.ai_flags = AI_PASSIVE;
    int err = getaddrinfo(0, (char*)buf, &hints, &addrInfo);
#   endif
    if (err) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Discovery: Error getting addrInfo struct, error: %d", err);
    }
    *socketsCount = 0;
    addrInfoTemp = addrInfo;
    while (addrInfoTemp != 0) {
        SOCKET udpTransmitSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udpTransmitSocket == INVALID_SOCKET) {
            err = getLastSocketError();
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Discovery: Error creating socket, error: %d", err);
            return BTA_StatusRuntimeError;
        }
        u_long yes = 1;
        err = setsockopt(udpTransmitSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
        if (err == SOCKET_ERROR) {
            err = getLastSocketError();
            closesocket(udpTransmitSocket);
            freeaddrinfo(addrInfo);
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Discovery: Error setting SO_REUSEADDR, error: %d", err);
            return BTA_StatusRuntimeError;
        }
        yes = 1;
        err = setsockopt(udpTransmitSocket, SOL_SOCKET, SO_BROADCAST, (const char *)&yes, sizeof(yes));
        if (err == SOCKET_ERROR) {
            err = getLastSocketError();
            closesocket(udpTransmitSocket);
            freeaddrinfo(addrInfo);
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Discovery: Error setting SO_BROADCAST, error: %d", err);
            return BTA_StatusRuntimeError;
        }

        struct sockaddr_in socketAddr = { 0 };
        socketAddr.sin_family = AF_INET;
        socketAddr.sin_addr.s_addr = ((struct sockaddr_in *)addrInfoTemp->ai_addr)->sin_addr.s_addr;
        err = bind(udpTransmitSocket, (struct sockaddr *)&socketAddr, sizeof(socketAddr));
        if (err == SOCKET_ERROR) {
            err = getLastSocketError();
            closesocket(udpTransmitSocket);
            freeaddrinfo(addrInfo);
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Discovery: Error binding socket, error: %d", err);
            return BTA_StatusRuntimeError;
        }

        sockets[*socketsCount] = udpTransmitSocket;
        *socketsCount = *socketsCount + 1;

        addrInfoTemp = addrInfoTemp->ai_next;
    }
    freeaddrinfo(addrInfo);
    BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Discovery: %d send socket(s) open", *socketsCount);
    return BTA_StatusOk;
}


static BTA_Status sendDiscoveryMessage(SOCKET *sockets, int socketsCount, uint8_t *buffer, uint32_t bufferLen, uint8_t *broadcastIpAddr, uint16_t broadcastPort, BTA_InfoEventInst *infoEventInst) {
    for (int i = 0; i < socketsCount; i++) {
        uint8_t udpControlConnectionStatus = 16;
        uint8_t tcpControlConnectionStatus = 0;
        BTA_Status status = transmit_2(buffer, bufferLen, timeoutDefault, &udpControlConnectionStatus, &(sockets[i]), broadcastIpAddr, broadcastPort, &tcpControlConnectionStatus, 0, 0);
        if (status != BTA_StatusOk) {
            int err = getLastSocketError();
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, status, "Discovery: Error in transmit to IP %d.%d.%d.%d:%d, error: %d", broadcastIpAddr[0], broadcastIpAddr[1], broadcastIpAddr[2], broadcastIpAddr[3], broadcastPort, err);
            return BTA_StatusRuntimeError;
        }
    }
    BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "Discovery: message sent");
    return BTA_StatusOk;
}


static BTA_DeviceInfo *parseDiscoveryResponse(uint8_t *responsePayload, uint32_t responseLen, void* deviceListMutex, BTA_DeviceInfo **deviceList, uint16_t deviceListCount, BTA_InfoEventInst *infoEventInst) {
    if (responseLen == 48) {
        BTA_DeviceInfo *deviceInfo = (BTA_DeviceInfo *)calloc(1, sizeof(BTA_DeviceInfo));
        if (deviceInfo) {
            deviceInfo->deviceMacAddr = (uint8_t *)malloc(6);
            if (deviceInfo->deviceMacAddr) {
                memcpy(deviceInfo->deviceMacAddr, &responsePayload[0x40 - 64], 6);
                deviceInfo->deviceMacAddrLen = 6;
            }
            deviceInfo->deviceIpAddr = (uint8_t *)malloc(4);
            if (deviceInfo->deviceIpAddr) {
                memcpy(deviceInfo->deviceIpAddr, &responsePayload[0x47 - 64], 4);
                deviceInfo->deviceIpAddrLen = 4;
            }
            deviceInfo->subnetMask = (uint8_t *)malloc(4);
            if (deviceInfo->subnetMask) {
                memcpy(deviceInfo->subnetMask, &responsePayload[0x4b - 64], 4);
                deviceInfo->subnetMaskLen = 4;
            }
            deviceInfo->gatewayIpAddr = (uint8_t *)malloc(4);
            if (deviceInfo->gatewayIpAddr) {
                memcpy(deviceInfo->gatewayIpAddr, &responsePayload[0x4f - 64], 4);
                deviceInfo->gatewayIpAddrLen = 4;
            }
            deviceInfo->udpDataIpAddr = (uint8_t *)malloc(4);
            if (deviceInfo->udpDataIpAddr) {
                memcpy(deviceInfo->udpDataIpAddr, &responsePayload[0x54 - 64], 4);
                deviceInfo->udpDataIpAddrLen = 4;
            }
            deviceInfo->udpDataPort = (responsePayload[0x58 - 64] << 8) | responsePayload[0x59 - 64];
            deviceInfo->udpControlPort = (responsePayload[0x5a - 64] << 8) | responsePayload[0x5b - 64];
            deviceInfo->tcpDataPort = (responsePayload[0x5c - 64] << 8) | responsePayload[0x5d - 64];
            deviceInfo->tcpControlPort = (responsePayload[0x5e - 64] << 8) | responsePayload[0x5f - 64];
            deviceInfo->deviceType = (BTA_DeviceType)((responsePayload[0x60 - 64] << 8) | responsePayload[0x61 - 64]);
            deviceInfo->serialNumber = (responsePayload[0x62 - 64] << 24) | (responsePayload[0x63 - 64] << 16) | (responsePayload[0x64 - 64] << 8) | responsePayload[0x65 - 64];
            deviceInfo->uptime = (responsePayload[0x66 - 64] << 24) | (responsePayload[0x67 - 64] << 16) | (responsePayload[0x68 - 64] << 8) | responsePayload[0x69 - 64];
            deviceInfo->mode0 = (responsePayload[0x6a - 64] << 8) | responsePayload[0x6b - 64];
            deviceInfo->status = (responsePayload[0x6c - 64] << 8) | responsePayload[0x6d - 64];
            uint16_t fwv = (responsePayload[0x6e - 64] << 8) | responsePayload[0x6f - 64];
            deviceInfo->firmwareVersionMajor = (fwv & 0xf800) >> 11;
            deviceInfo->firmwareVersionMinor = (fwv & 0x07c0) >> 6;
            deviceInfo->firmwareVersionNonFunc = (fwv & 0x003f);

            // HACK because PON not in discovery protocol
            if (BTAisInDiscoveredListIgnorePon(deviceListMutex, deviceList, deviceListCount, deviceInfo)) {
                // if this device was already discovered, do NOT connect and read PON again!
                BTAfreeDeviceInfo(deviceInfo);
                return 0;
            }
            // Establish a connection and read PON directly
            BTA_Config config;
            BTA_Status status = BTAinitConfig(&config);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, status, "Discovery: Read PON: Error in BTAinitConfig");
                return deviceInfo;
            }

            config.deviceType = BTA_DeviceTypeEthernet;
            if (deviceInfo->tcpControlPort) {
                config.tcpDeviceIpAddr = deviceInfo->deviceIpAddr;
                config.tcpDeviceIpAddrLen = deviceInfo->deviceIpAddrLen;
                config.tcpControlPort = deviceInfo->tcpControlPort;
            }
            if (deviceInfo->udpControlPort != 0) {
                config.udpControlOutIpAddr = deviceInfo->deviceIpAddr;
                config.udpControlOutIpAddrLen = deviceInfo->deviceIpAddrLen;
                config.udpControlPort = deviceInfo->udpControlPort;
            }
            BTA_Handle btaHandle;
            status = BTAopen(&config, &btaHandle);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, status, "Discovery: Read PON: Error in BTAopen");
                return deviceInfo;
            }

            //ArticleNrPart1, ArticleNrPart2, DeviceRevisionMajor (no multi read in regard of TimEth which does not support it)
            uint32_t ponPart1;
            uint32_t ponPart2;
            uint32_t deviceRevisionMajor;
            status = BTAreadRegister(btaHandle, 0x0570, &ponPart1, 0);
            if (status != BTA_StatusOk) {
                BTAclose(&btaHandle);
                BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, status, "Discovery: Read PON: Error in BTAreadRegister");
                return deviceInfo;
            }
            status = BTAreadRegister(btaHandle, 0x0571, &ponPart2, 0);
            if (status != BTA_StatusOk) {
                BTAclose(&btaHandle);
                BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, status, "Discovery: Read PON: Error in BTAreadRegister");
                return deviceInfo;
            }
            status = BTAreadRegister(btaHandle, 0x0572, &deviceRevisionMajor, 0);
            if (status != BTA_StatusOk) {
                BTAclose(&btaHandle);
                BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, status, "Discovery: Read PON: Error in BTAreadRegister");
                return deviceInfo;
            }
            if (!ponPart1 || !ponPart2) {
                BTAclose(&btaHandle);
                BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, BTA_StatusOutOfMemory, "Discovery: Read PON: PON related registers are empty!");
                return deviceInfo;
            }
            free(deviceInfo->productOrderNumber);
            deviceInfo->productOrderNumber = (uint8_t *)calloc(1, 20);
            if (deviceInfo->productOrderNumber) {
                sprintf((char *)deviceInfo->productOrderNumber, "%03d-%04d-%d", ponPart1, ponPart2, deviceRevisionMajor);
            }
            BTAclose(&btaHandle);
            return deviceInfo;
        }
    }
    return 0;
}


static BTA_Status closeSockets(SOCKET *sockets, int socketsCount) {
    for (int i = 0; i < socketsCount; i++) {
        closesocket(sockets[i]);
    }
    return BTA_StatusOk;
}



static BTA_Status receiveControlResponse(BTA_WrapperInst *winst, uint8_t *request, uint8_t **data, uint32_t *dataLen, uint32_t timeout, FN_BTA_ProgressReport progressReport) {
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
    return receiveControlResponse_2(request, data, dataLen, timeout, progressReport,
                                    &inst->udpControlConnectionStatus, &inst->udpControlSocket, inst->udpControlCallbackIpAddr, inst->udpControlCallbackPort, &inst->tcpControlConnectionStatus, &inst->tcpControlSocket,
                                    &inst->keepAliveMsgTimestamp, inst->lpKeepAliveMsgInterval, winst->infoEventInst);
}



static BTA_Status receiveControlResponse_2(uint8_t *request, uint8_t **data, uint32_t *dataLen, uint32_t timeout, FN_BTA_ProgressReport progressReport,
                                           uint8_t *udpControlConnectionStatus, SOCKET *udpControlSocket, uint8_t *udpControlCallbackIpAddr, uint16_t udpControlCallbackPort, uint8_t *tcpControlConnectionStatus, SOCKET *tcpControlSocket,
                                           uint32_t *keepAliveMsgTimestamp, float lpKeepAliveMsgInterval, BTA_InfoEventInst *infoEventInst) {
    if (!udpControlConnectionStatus || !tcpControlConnectionStatus) {
        return BTA_StatusInvalidParameter;
    }
    uint8_t datagram[1500];
    uint8_t header[BTA_ETH_HEADER_SIZE];
    uint32_t headerLen;
    uint32_t flags;
    uint32_t dataCrc32;
    uint8_t parseError;
    uint32_t datagramLen;
    BTA_Status status;
    if (data) {
        *data = 0;
    }
    if (!udpControlConnectionStatus || !tcpControlConnectionStatus) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "UDP receiveControlResponse: parameters udpControlConnectionStatus or tcpControlConnectionStatus missing");
        return BTA_StatusInvalidParameter;
    }
    if (*udpControlConnectionStatus == 16 || *udpControlConnectionStatus == 3 || *udpControlConnectionStatus == 8) {
        datagramLen = sizeof(datagram);
        status = receive(datagram, &datagramLen,
                         udpControlConnectionStatus, udpControlSocket, udpControlCallbackIpAddr, udpControlCallbackPort, tcpControlConnectionStatus, 0,
                         timeout, keepAliveMsgTimestamp, lpKeepAliveMsgInterval, infoEventInst);
        if (status != BTA_StatusOk) {
            return status;
        }
        uint32_t dataLenTemp;
        status = BTAparseControlHeader(request, datagram, &dataLenTemp, &flags, &dataCrc32, &parseError, infoEventInst);
        if (status != BTA_StatusOk) {
            return status;
        }
        if (datagramLen != dataLenTemp + BTA_ETH_HEADER_SIZE) {
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "UDP receiveControlResponse: The datagram has the wrong size: %d", datagramLen);
            return BTA_StatusRuntimeError;
        }
        if (dataLenTemp > 0) {
            if (!data || !dataLen) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "UDP receiveControlResponse: data or dataLen missing");
                return BTA_StatusInvalidParameter;
            }
            *dataLen = dataLenTemp;
            *data = (uint8_t *)malloc(*dataLen);
            if (!*data) {
                return BTA_StatusOutOfMemory;
            }
            memcpy(*data, datagram + BTA_ETH_HEADER_SIZE, *dataLen);
        }
    }

    else if (*tcpControlConnectionStatus == 16 || *tcpControlConnectionStatus == 8) {
        // read byte by byte until preamble or timeout
        uint8_t preamble0 = 0;
        uint8_t preamble1 = 0;
        int bytesReadForPreambleCount = 0;
        while (1) {
            uint32_t len = 1;
            status = receive(&preamble1, &len, udpControlConnectionStatus, 0, 0, 0, tcpControlConnectionStatus, tcpControlSocket, timeout, keepAliveMsgTimestamp, lpKeepAliveMsgInterval, infoEventInst);
            if (status != BTA_StatusOk) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "TCP receiveControlResponse: could not find preamble in answer, %d bytes read", bytesReadForPreambleCount);
                return status;
            }
            bytesReadForPreambleCount++;
            if (preamble0 == BTA_ETH_PREAMBLE_0 && preamble1 == BTA_ETH_PREAMBLE_1) {
                break;
            }
            preamble0 = preamble1;
        }
        header[0] = BTA_ETH_PREAMBLE_0;
        header[1] = BTA_ETH_PREAMBLE_1;
        headerLen = sizeof(header) - 2;
        status = receive(header + 2, &headerLen, udpControlConnectionStatus, 0, 0, 0, tcpControlConnectionStatus, tcpControlSocket, timeout, keepAliveMsgTimestamp, lpKeepAliveMsgInterval, infoEventInst);
        if (status != BTA_StatusOk) {
            return status;
        }
        uint32_t dataLenTemp;
        status = BTAparseControlHeader(request, header, &dataLenTemp, &flags, &dataCrc32, &parseError, infoEventInst);
        if (parseError) {
            // we found a preamble, but parsing gave an error, try to read on, maybe the answer is yet to come (recursively ;)
            timeout = timeout > timeoutDefault ? timeoutDefault : timeout;
            return receiveControlResponse_2(request, data, dataLen, timeout, progressReport, udpControlConnectionStatus, udpControlSocket, udpControlCallbackIpAddr, udpControlCallbackPort,
                                            tcpControlConnectionStatus, tcpControlSocket, keepAliveMsgTimestamp, lpKeepAliveMsgInterval, infoEventInst);
        }
        if (status != BTA_StatusOk) {
            return status;
        }
        if (dataLenTemp > 0) {
            if (!data || !dataLen) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "TCP receiveControlResponse: parameters data or dataLen missing");
                return BTA_StatusInvalidParameter;
            }
            *dataLen = dataLenTemp;
            *data = (uint8_t *)malloc(*dataLen);
            if (!*data) {
                BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusOutOfMemory, "TCP receiveControlResponse: Cannot allocate");
                return BTA_StatusOutOfMemory;
            }
            if (progressReport) {
                // read data in 10 parts and report progress
                uint32_t dataReceivedLen = 0;
                uint32_t dataPartLen = *dataLen / 10;
                uint32_t dataPartLenTemp;
                (*progressReport)(BTA_StatusOk, 0);
                do {
                    BTAinfoEventHelper(infoEventInst, VERBOSE_INFO, BTA_StatusInformation, "TCP receiveControlResponse: receiving %d bytes", dataPartLen);
                    dataPartLenTemp = dataPartLen;
                    status = receive((*data) + dataReceivedLen, &dataPartLenTemp,
                                     udpControlConnectionStatus, 0, 0, 0, tcpControlConnectionStatus, tcpControlSocket,
                                     timeout, keepAliveMsgTimestamp, lpKeepAliveMsgInterval, infoEventInst);
                    if (status == BTA_StatusOk) {
                        dataReceivedLen += dataPartLen;
                        if (dataReceivedLen + dataPartLen > *dataLen) {
                            dataPartLen = *dataLen - dataReceivedLen;
                        }
                        // report progress (use 90 of the 100% for the transmission
                        (*progressReport)(status, (uint8_t)(90.0 * dataReceivedLen / *dataLen));
                    }
                    else {
                        BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, status, "TCP receiveControlResponse: Failed, error: %d", dataPartLenTemp);
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
                // read data in one piece
                status = receive(*data, dataLen,
                                 udpControlConnectionStatus, 0, 0, 0, tcpControlConnectionStatus, tcpControlSocket,
                                 timeout, keepAliveMsgTimestamp, lpKeepAliveMsgInterval, infoEventInst);
                if (status != BTA_StatusOk) {
                    free(*data);
                    *data = 0;
                    return status;
                }
            }
        }
    }

    else {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusNotConnected, "receiveControlResponse: Not connected");
        return BTA_StatusNotConnected;
    }

    if (data) {
        if (*data) {
            if (!(flags & 1)) {
                uint32_t dataCrc32Calced = (uint32_t)CRC32ccitt(*data, *dataLen);
                if (dataCrc32 != dataCrc32Calced) {
                    free(*data);
                    *data = 0;
                    BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusCrcError, "receiveControlResponse: Data CRC mismatch");
                    return BTA_StatusCrcError;
                }
            }
        }
    }
    return BTA_StatusOk;
}


static BTA_Status receive(uint8_t *data, uint32_t *length,
                          uint8_t *udpControlConnectionStatus, SOCKET *udpControlSocket, uint8_t *udpControlCallbackIpAddr, uint16_t udpControlCallbackPort, uint8_t *tcpControlConnectionStatus, SOCKET *tcpControlSocket,
                          uint32_t timeout, uint32_t *keepAliveMsgTimestamp, float lpKeepAliveMsgInterval, BTA_InfoEventInst *infoEventInst) {
    if (length && *length == 0) return BTA_StatusOk;
    if (!length || !udpControlConnectionStatus || !tcpControlConnectionStatus) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "UDP receive: parameters length, udpControlConnectionStatus or tcpControlConnectionStatus missing");
        return BTA_StatusInvalidParameter;
    }
    fd_set fds;
    struct timeval timeoutTv;
    if (*udpControlConnectionStatus == 16 || *udpControlConnectionStatus == 3 || *udpControlConnectionStatus == 8) {
        if (!udpControlSocket) {
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "UDP receive: parameter udpControlSocket missing");
            return BTA_StatusInvalidParameter;
        }
        // for datagrams the length parameter does not match the length to read. It is unknown -> one datagram is read
        struct sockaddr_in socketAddr = { 0 };
        socketAddr.sin_family = AF_INET;
        socketAddr.sin_addr.s_addr = INADDR_ANY;
        if (udpControlCallbackIpAddr) {
            socketAddr.sin_addr.s_addr = udpControlCallbackIpAddr[0] | (udpControlCallbackIpAddr[1] << 8) | (udpControlCallbackIpAddr[2] << 16) | (udpControlCallbackIpAddr[3] << 24);
        }
        socketAddr.sin_port = htons(udpControlCallbackPort);
        int socketAddrLen = sizeof(struct sockaddr_in);
        uint32_t endTime = BTAgetTickCount() + timeout;
        while (1) {
            if (BTAgetTickCount() > endTime) {
                if (*udpControlConnectionStatus == 16) {
                    *udpControlConnectionStatus = 3;
                }
                BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusDeviceUnreachable, "UDP control receive: Failed: timeout");
                return BTA_StatusDeviceUnreachable;
            }
            FD_ZERO(&fds);
            FD_SET(*udpControlSocket, &fds);
            timeoutTv.tv_sec = timeout / 1000;
            timeoutTv.tv_usec = (timeout % 1000) * 1000;
            int err = select((int)*udpControlSocket + 1, &fds, (fd_set *)0, (fd_set *)0, &timeoutTv);
            if (err == 1) {
                err = (int)recvfrom(*udpControlSocket, (char *)data, *length, 0, (struct sockaddr *)&socketAddr, (socklen_t *)&socketAddrLen);
                if (err == SOCKET_ERROR) {
                    err = getLastSocketError();
                    if (err == ERROR_TRY_AGAIN) {
                        BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "UDP control receive: recvfrom said to try again");
                        BTAmsleep(22);
                        continue;
                    }
                    BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusDeviceUnreachable, "UDP control receive: Error in recvfrom, error: %d", err);
                    return BTA_StatusDeviceUnreachable;
                }
                *length = err;
                *udpControlConnectionStatus = 16;
                if (keepAliveMsgTimestamp && lpKeepAliveMsgInterval > 0) {
                    *keepAliveMsgTimestamp = BTAgetTickCount() + (int)(1000 * lpKeepAliveMsgInterval);
                }
                return BTA_StatusOk;
            }
            BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "UDP control receive: Failed to select, returned: %d", err);
            return BTA_StatusDeviceUnreachable;
        }
    }

    else if (*tcpControlConnectionStatus == 16 || *tcpControlConnectionStatus == 8) {
        if (!tcpControlSocket) {
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "TCP control receive: parameter tcpControlSocket missing");
            return BTA_StatusInvalidParameter;
        }
        uint32_t nBytesReceived = 0;
        uint32_t endTime = BTAgetTickCount() + timeout;
        while (nBytesReceived < *length) {
            while (1) {
                if (BTAgetTickCount() > endTime) {
                    int err = closesocket(*tcpControlSocket);
                    if (err == SOCKET_ERROR) BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "TCP control receive: Failed to close socket, error: %d", err);
                    *tcpControlSocket = INVALID_SOCKET;
                    if (*tcpControlConnectionStatus == 16) {
                        *tcpControlConnectionStatus = 2;
                    }
                    BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusDeviceUnreachable, "TCP control receive: Failed: timeout");
                    return BTA_StatusDeviceUnreachable;
                }
                FD_ZERO(&fds);
                FD_SET(*tcpControlSocket, &fds);
                timeoutTv.tv_sec = timeout / 1000;
                timeoutTv.tv_usec = (timeout % 1000) * 1000;
                int err = select((int)*tcpControlSocket + 1, &fds, (fd_set *)0, (fd_set *)0, &timeoutTv);
                if (err == 1) {
                    err = (int)recv(*tcpControlSocket, (char *)&(data[nBytesReceived]), *length - nBytesReceived, 0);
                    if (err == SOCKET_ERROR) {
                        err = getLastSocketError();
                        if (err == ERROR_TRY_AGAIN) {
                            BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "TCP control receive: recv said to try again");
                            BTAmsleep(22);
                            continue;
                        }
                        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusDeviceUnreachable, "TCP control receive: Failed in recv, error: %d", err);
                        err = closesocket(*tcpControlSocket);
                        if (err == SOCKET_ERROR) BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "TCP control receive: Failed to close socket, error: %d", err);
                        *tcpControlSocket = INVALID_SOCKET;
                        if (*tcpControlConnectionStatus == 16) {
                            *tcpControlConnectionStatus = 2;
                        }
                        return BTA_StatusDeviceUnreachable;
                    }
                    nBytesReceived += err;
                    break;
                }
                BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "TCP control receive: Failed to select, returned: %d", err);
                if (*tcpControlConnectionStatus < 16) {
                    err = closesocket(*tcpControlSocket);
                    if (err == SOCKET_ERROR) BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "TCP control receive: Failed to close socket, error: %d", err);
                    *tcpControlSocket = INVALID_SOCKET;
                }
                return BTA_StatusDeviceUnreachable;
            }
        }
        *tcpControlConnectionStatus = 16;
        if (keepAliveMsgTimestamp && lpKeepAliveMsgInterval > 0) {
            *keepAliveMsgTimestamp = BTAgetTickCount() + (int)(1000 * lpKeepAliveMsgInterval);
        }
        return BTA_StatusOk;
    }
    else {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusNotConnected, "Not connected");
        return BTA_StatusNotConnected;
    }
}



static BTA_Status transmit(BTA_WrapperInst *winst, uint8_t *data, uint32_t length, uint32_t timeout) {
    BTA_EthLibInst *inst = (BTA_EthLibInst *)winst->inst;
    return transmit_2(data, length, timeout,
                      &inst->udpControlConnectionStatus, &inst->udpControlSocket, inst->udpControlDeviceIpAddr, inst->udpControlPort, &inst->tcpControlConnectionStatus, &inst->tcpControlSocket,
                      winst->infoEventInst);
}



static BTA_Status transmit_2(uint8_t *data, uint32_t length, uint32_t timeout,
                             uint8_t *udpControlConnectionStatus, SOCKET *udpControlSocket, uint8_t *udpControlDeviceIpAddr, uint16_t udpControlPort, uint8_t *tcpControlConnectionStatus, SOCKET *tcpControlSocket,
                             BTA_InfoEventInst *infoEventInst) {
    if (length == 0) return BTA_StatusOk;
    fd_set fds;
    struct timeval timeoutTv;
    if (!udpControlConnectionStatus || !tcpControlConnectionStatus || !data) {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "UDP/TCP transmit: parameters data, udpControlConnectionStatus or tcpControlConnectionStatus missing");
        return BTA_StatusInvalidParameter;
    }
    if (*udpControlConnectionStatus == 16 || *udpControlConnectionStatus == 3 || *udpControlConnectionStatus == 8) {
        if (!udpControlSocket) {
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "UDP transmit: parameters udpControlSocket missing");
            return BTA_StatusInvalidParameter;
        }
        struct sockaddr_in socketAddr = { 0 };
        socketAddr.sin_family = AF_INET;
        socketAddr.sin_addr.s_addr = udpControlDeviceIpAddr[0] | (udpControlDeviceIpAddr[1] << 8) | (udpControlDeviceIpAddr[2] << 16) | (udpControlDeviceIpAddr[3] << 24);
        socketAddr.sin_port = htons(udpControlPort);
        uint32_t endTime = BTAgetTickCount() + timeout;
        while (1) {
            if (BTAgetTickCount() > endTime) {
                if (*udpControlConnectionStatus == 16) {
                    *udpControlConnectionStatus = 3;
                }
                BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusDeviceUnreachable, "UDP control transmit: Failed: timeout");
                return BTA_StatusDeviceUnreachable;
            }
            FD_ZERO(&fds);
            FD_SET(*udpControlSocket, &fds);
            timeoutTv.tv_sec = timeout / 1000;
            timeoutTv.tv_usec = (timeout % 1000) * 1000;
            int err = select((int)*udpControlSocket + 1, (fd_set *)0, &fds, (fd_set *)0, &timeoutTv);
            if (err == 1) {
                err = (int)sendto(*udpControlSocket, (char *)data, (int)length, 0, (struct sockaddr *)&socketAddr, sizeof(socketAddr));
                if (err == SOCKET_ERROR) {
                    err = getLastSocketError();
                    if (err == ERROR_TRY_AGAIN) {
                        BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "UDP control transmit: send said to try again");
                        BTAmsleep(22);
                        continue;
                    }
                    BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusDeviceUnreachable, "UDP control transmit: Failed in sendto, error: %d", err);
                    return BTA_StatusDeviceUnreachable;
                }
                assert(err == (int)length);
                return BTA_StatusOk;
            }
            BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "UDP control transmit: Failed to select, returned: %d", err);
            return BTA_StatusDeviceUnreachable;
        }
    }

    else if (*tcpControlConnectionStatus == 16 || *tcpControlConnectionStatus == 8) {
        if (!tcpControlSocket) {
            BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusInvalidParameter, "TCP transmit: parameters tcpControlSocket missing");
            return BTA_StatusInvalidParameter;
        }
        // ### TCP control connection
        uint32_t nBytesWritten = 0;
        uint32_t endTime = BTAgetTickCount() + timeout;
        while (nBytesWritten < length) {
            while (1) {
                if (BTAgetTickCount() > endTime) {
                    int err = closesocket(*tcpControlSocket);
                    if (err == SOCKET_ERROR) BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "TCP transmit: Failed to close socket, error: %d", err);
                    *tcpControlSocket = INVALID_SOCKET;
                    if (*tcpControlConnectionStatus == 16) {
                        *tcpControlConnectionStatus = 2;
                    }
                    BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusDeviceUnreachable, "TCP control transmit: Failed: timeout");
                    return BTA_StatusDeviceUnreachable;
                }
                FD_ZERO(&fds);
                FD_SET(*tcpControlSocket, &fds);
                timeoutTv.tv_sec = timeout / 1000;
                timeoutTv.tv_usec = (timeout % 1000) * 1000;
                int err = select((int)*tcpControlSocket + 1, (fd_set *)0, &fds, (fd_set *)0, &timeoutTv);
                if (err == 1) {
                    err = (int)send(*tcpControlSocket, (const char *)&(data[nBytesWritten]), length - nBytesWritten, 0);
                    if (err == SOCKET_ERROR) {
                        err = getLastSocketError();
                        if (err == ERROR_TRY_AGAIN) {
                            BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "TCP control transmit: send said to try again");
                            BTAmsleep(22);
                            continue;
                        }
                        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusDeviceUnreachable, "TCP control transmit: Failed in send, error: %d", err);
                        err = closesocket(*tcpControlSocket);
                        if (err == SOCKET_ERROR) BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "TCP transmit: Failed to close socket, error: %d", err);
                        *tcpControlSocket = INVALID_SOCKET;
                        if (*tcpControlConnectionStatus == 16) {
                            *tcpControlConnectionStatus = 2;
                        }
                        return BTA_StatusDeviceUnreachable;
                    }
                    nBytesWritten += err;
                    break;
                }
                BTAinfoEventHelper(infoEventInst, VERBOSE_WARNING, BTA_StatusWarning, "TCP control transmit: Failed to select, returned: %d", err);
                if (*tcpControlConnectionStatus < 16) {
                    err = closesocket(*tcpControlSocket);
                    if (err == SOCKET_ERROR) BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusRuntimeError, "TCP transmit: Failed to close socket, error: %d", err);
                    *tcpControlSocket = INVALID_SOCKET;
                }
                return BTA_StatusDeviceUnreachable;
            }
        }
        return BTA_StatusOk;
    }

    else {
        BTAinfoEventHelper(infoEventInst, VERBOSE_ERROR, BTA_StatusNotConnected, "Not connected");
        return BTA_StatusNotConnected;
    }
}


#endif
