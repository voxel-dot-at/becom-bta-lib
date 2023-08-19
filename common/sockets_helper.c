#include "sockets_helper.h"

#include <string.h>
#include <stdlib.h>
#include <mth_math.h>
#include <stdio.h>
#include <bitconverter.h>
#include <utils.h>


int getLastSocketError() {
#   ifdef PLAT_WINDOWS
    return WSAGetLastError();
#   else
    return errno;
#   endif
}


#ifdef PLAT_WINDOWS
static char errorMsg[456];
#endif
const char *errorToString(int err) {
#   ifdef PLAT_WINDOWS
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorMsg, sizeof(errorMsg), NULL);
    UTILremoveChar(errorMsg, '\n');
    return errorMsg;
#   else
    return strerror(MTHabs(err));
#   endif
}


//uint32_t convertByteWiseToInt(uint8_t *ipAddr, uint8_t ipAddrLen) {
//    uint32_t ipAddrL = 0;
//    for (int i = 0; i < ipAddrLen; i++) {
//        ipAddrL |= ipAddr[i] << (i * 8);
//    }
//    return ipAddrL;
//}
//
//
//void convertIntToByteWise(uint32_t ipAddrL, uint8_t *ipAddr, uint8_t ipAddrLen) {
//    for (int i = 0; i < ipAddrLen; i++) {
//        ipAddr[i] = (ipAddrL >> (i * 8)) & 0xff;
//    }
//}


//void printAddrinfo(struct addrinfo *ai) {
//    //printf("ai_family: %d\n", ai->ai_family);
//    //printf("ai_socktype: %d\n", ai->ai_socktype);
//    //printf("ai_protocol: %d\n", ai->ai_protocol);
//    //printf("ai_addrlen: %d\n", ai->ai_addrlen);
//    //printf("ai_canonname: %s\n", ai->ai_canonname);
//    //if (ai->ai_family == AF_INET) {
//    //    struct sockaddr_in *ipv4 = (struct sockaddr_in *)ai->ai_addr;
//    //    printf("ai_addr: %s\n", inet_ntoa(ipv4->sin_addr));
//    //}
//    //else if (ai->ai_family == AF_INET6) {
//    //    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)ai->ai_addr;
//    //    printf("ai_addr: %s\n", inet_ntop(ai->ai_family, &ipv6->sin6_addr, NULL, 0));
//    //}
//    char host[NI_MAXHOST], port[NI_MAXSERV];
//    int ret;
//
//    // Get the human-readable address and port
//    ret = getnameinfo(ai->ai_addr, ai->ai_addrlen, host, NI_MAXHOST, port, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);
//    if (ret != 0) {
//        printf("getnameinfo: %s\n", gai_strerror(ret));
//        return;
//    }
//
//    // Print the address information
//    printf("Family: %s, Socket Type: %s, Protocol: %s, Address: %s, Port: %s\n",
//        ai->ai_family == AF_INET ? "IPv4" : "IPv6",
//        ai->ai_socktype == SOCK_STREAM ? "TCP" : "UDP",
//        ai->ai_protocol == IPPROTO_TCP ? "TCP" : "UDP", host, port);
//    if (ai->ai_next) {
//        ret = getnameinfo(ai->ai_next->ai_addr, ai->ai_next->ai_addrlen, host, NI_MAXHOST, port, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);
//        printf("Next is %s:%s\n", host, port);
//    }
//    else {
//        printf("There is no 'next'\n");
//    }
//}


BTA_Status BTAlistLocalIpAddrs(uint32_t **ipAddrs, uint32_t **subnetMasks, uint32_t *ipAddrsLen) {
    if (!ipAddrsLen) {
        return BTA_StatusInvalidParameter;
    }
#   ifdef PLAT_WINDOWS
    PIP_ADAPTER_ADDRESSES pAddresses = 0;
    ULONG outBufLen = 15000;
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;
    DWORD dwRetVal;
    int iterations = 0;
    do {
        pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
        if (!pAddresses) {
            return BTA_StatusOutOfMemory;
        }

        dwRetVal = GetAdaptersAddresses(AF_INET, flags, NULL, pAddresses, &outBufLen);
        if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
            free(pAddresses);
            pAddresses = NULL;
        }
        else {
            break;
        }
        iterations++;
    } while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (iterations < 3));
    if (dwRetVal != NO_ERROR) {
        free(pAddresses);
        //BTAinfoEventHelper(infoEventInst, VERBOSE_CRITICAL, BTA_StatusRuntimeError, "Failed to get adapter addresses: %d", dwRetVal);
        return BTA_StatusRuntimeError;
    }
    // count addresses
    *ipAddrsLen = 0;
    PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses;
    while (pCurrAddresses) {
        PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
        while (pUnicast) {
            uint32_t addr = (uint32_t)((struct sockaddr_in *)pUnicast->Address.lpSockaddr)->sin_addr.S_un.S_addr;
            if (addr != 0 && addr != 0x100007f) {
                *ipAddrsLen = *ipAddrsLen + 1;
            }
            pUnicast = pUnicast->Next;
        }
        pCurrAddresses = pCurrAddresses->Next;
    }
    if (!*ipAddrsLen) {
        free(pAddresses);
        if (ipAddrs) *ipAddrs = 0;
        if (subnetMasks) *subnetMasks = 0;
        return BTA_StatusOk;
    }
    if (ipAddrs) *ipAddrs = (uint32_t *)malloc(*ipAddrsLen * sizeof(uint32_t));
    if (subnetMasks) *subnetMasks = (uint32_t *)malloc(*ipAddrsLen * sizeof(uint32_t));
    if ((ipAddrs && !*ipAddrs) || (subnetMasks && !*subnetMasks)) {
        free(pAddresses);
        return BTA_StatusOutOfMemory;
    }
    // copy to result
    int i = 0;
    pCurrAddresses = pAddresses;
    while (pCurrAddresses) {
        PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
        while (pUnicast) {
            uint32_t addr = (uint32_t)((struct sockaddr_in *)pUnicast->Address.lpSockaddr)->sin_addr.S_un.S_addr;
            if (addr != 0 && addr != 0x100007f) {
                ULONG mask;
                ConvertLengthToIpv4Mask(pUnicast->OnLinkPrefixLength, &mask);
                if (ipAddrs) (*ipAddrs)[i] = addr;
                if (subnetMasks) (*subnetMasks)[i] = mask;
                i++;
            }
            pUnicast = pUnicast->Next;
        }
        pCurrAddresses = pCurrAddresses->Next;
    }
    free(pAddresses);
    return BTA_StatusOk;
#   else
    struct ifaddrs *addresses, *address;
    int err = getifaddrs(&addresses);
    if (err == -1) {
        //BTAinfoEventHelper(infoEventInst, VERBOSE_CRITICAL, BTA_StatusRuntimeError, "UDP data auto config failed to get adapter addresses: %d", getLastSocketError());
        return BTA_StatusRuntimeError;
    }
    // count addresses
    *ipAddrsLen = 0;
    address = addresses;
    while (address) {
        if (address->ifa_addr && address->ifa_addr->sa_family == AF_INET) {
            uint32_t addr = ((struct sockaddr_in *)(address->ifa_addr))->sin_addr.s_addr;
            if (addr != 0 && addr != 0x100007f) {
                *ipAddrsLen = *ipAddrsLen + 1;
            }
        }
        address = address->ifa_next;
    }
    if (ipAddrs) *ipAddrs = (uint32_t *)malloc(*ipAddrsLen * sizeof(uint32_t));
    if (subnetMasks) *subnetMasks = (uint32_t *)malloc(*ipAddrsLen * sizeof(uint32_t));
    if ((ipAddrs && !*ipAddrs) || (subnetMasks && !*subnetMasks)) {
        freeifaddrs(addresses);
        return BTA_StatusOutOfMemory;
    }
    // copy to result
    int i = 0;
    address = addresses;
    while (address) {
        if (address->ifa_addr && address->ifa_addr->sa_family == AF_INET) {
            uint32_t addr = ((struct sockaddr_in *)(address->ifa_addr))->sin_addr.s_addr;
            uint32_t mask = ((struct sockaddr_in *)(address->ifa_netmask))->sin_addr.s_addr;
            if (addr != 0 && addr != 0x100007f) {
                if (ipAddrs) (*ipAddrs)[i] = addr;
                if (subnetMasks) (*subnetMasks)[i] = mask;
                i++;
            }
        }
        address = address->ifa_next;
    }
    freeifaddrs(addresses);
    return BTA_StatusOk;
#   endif
}


uint8_t BTAisLocalIpAddrOrMulticast(uint8_t *ipAddr, uint8_t ipAddrLen) {
    if (ipAddrLen != 4) {
        return 0;
    }
    uint32_t ipAddrTemp = BTAbitConverterToUInt32(ipAddr, 0);
    if ((ipAddrTemp & 0xe0) == 0xe0) {
        // This is a multicast address
        return 1;
    }
    uint32_t *ipAddrs;
    uint32_t ipAddrsLen;
    BTA_Status status = BTAlistLocalIpAddrs(&ipAddrs, 0, &ipAddrsLen);
    if (status != BTA_StatusOk) {
        return 0;
    }
    for (uint32_t i = 0; i < ipAddrsLen; i++) {
        if (ipAddrs[i] == ipAddrTemp) {
            free(ipAddrs);
            return 1;
        }
    }
    free(ipAddrs);
    return 0;
}


BTA_Status BTAgetMatchingLocalAddress(uint8_t *deviceIpAddr, uint8_t ipAddrLen, uint8_t **matchingLocalpAddr) {
    if (ipAddrLen != 4) {
        return BTA_StatusInvalidParameter;
    }
    if (matchingLocalpAddr) {
        // Set to zero so we can detect if a match was found in the loop
        *matchingLocalpAddr = 0;
    }
    uint32_t deviceIpAddrL = BTAbitConverterToUInt32(deviceIpAddr, 0);
    uint32_t *ipAddrs, *subnetMasks;
    uint32_t ipAddrsLen;
    BTA_Status status = BTAlistLocalIpAddrs(&ipAddrs, &subnetMasks, &ipAddrsLen);
    if (status != BTA_StatusOk) {
        return status;
    }
    for (uint32_t i = 0; i < ipAddrsLen; i++) {
        if ((ipAddrs[i] & subnetMasks[i]) == (deviceIpAddrL & subnetMasks[i])) {
            if (!matchingLocalpAddr) {
                // Exact address is not requested -> return ok when a match is found
                free(ipAddrs);
                free(subnetMasks);
                return BTA_StatusOk;
            }
            else {
                if (*matchingLocalpAddr) {
                    // Already found a matching address before. No unique match possible. Error
                    free(*matchingLocalpAddr);
                    *matchingLocalpAddr = 0;
                    free(ipAddrs);
                    free(subnetMasks);
                    return BTA_StatusIllegalOperation;
                }
                // First match found
                *matchingLocalpAddr = (uint8_t*)malloc(4);
                if (!*matchingLocalpAddr) {
                    free(ipAddrs);
                    free(subnetMasks);
                    return BTA_StatusOutOfMemory;
                }
                BTAbitConverterFromUInt32(ipAddrs[i], *matchingLocalpAddr, 0);
            }            
        }
    }
    free(ipAddrs);
    free(subnetMasks);
    if (matchingLocalpAddr && *matchingLocalpAddr) {
        // We searched all interfaces and found exactly one match
        return BTA_StatusOk;
    }
    //BTAinfoEventHelper(infoEventInst, VERBOSE_CRITICAL, BTA_StatusRuntimeError, "UDP data auto config failed, unique local address not found");
    return BTA_StatusRuntimeError;
}

