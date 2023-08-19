#pragma once

#include <stdint.h>
#include <bta_status.h>

#ifdef PLAT_WINDOWS
#   include <winsock2.h>
#   include <Ws2tcpip.h>
#   include <iphlpapi.h>
#else
#   include <errno.h>
#   include <netdb.h>
#   include <netinet/in.h>
#   include <ifaddrs.h>
#endif

#if !defined PLAT_WINDOWS && !defined PLAT_LINUX && !defined PLAT_APPLE
#   error "Please define PLAT_WINDOWS, PLAT_LINUX or PLAT_APPLE in your makefile/project"
#endif

int getLastSocketError(void);
const char *errorToString(int err);

//uint32_t convertByteWiseToInt(uint8_t *ipAddr, uint8_t ipAddrLen);
//void convertIntToByteWise(uint32_t ipAddrL, uint8_t *ipAddr, uint8_t ipAddrLen);
//void printAddrinfo(struct addrinfo *ai);

BTA_Status BTAlistLocalIpAddrs(uint32_t **ipAddrs, uint32_t **subnetMasks, uint32_t *ipAddrsLen);
uint8_t BTAisLocalIpAddrOrMulticast(uint8_t *ipAddr, uint8_t ipAddrLen);
BTA_Status BTAgetMatchingLocalAddress(uint8_t *deviceIpAddr, uint8_t ipAddrLen, uint8_t **matchingLocalpAddr);