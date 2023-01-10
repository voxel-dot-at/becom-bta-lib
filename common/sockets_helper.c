#include "sockets_helper.h"

#include <string.h>
#include <stdlib.h>
#ifdef PLAT_WINDOWS
#   include <winsock2.h>
#   include <Ws2tcpip.h>
#   include <iphlpapi.h>
#elif defined PLAT_LINUX || defined PLAT_APPLE
#   include <errno.h>
#   include <netdb.h>
#   include <netinet/in.h>
#   include <ifaddrs.h>
#endif


int getLastSocketError() {
#   ifdef PLAT_WINDOWS
    return WSAGetLastError();
#   elif defined PLAT_LINUX || defined PLAT_APPLE
    return errno;
#   endif
}


static BTA_Status BTAlistLocalIpAddrs(uint32_t **ipAddrs, uint32_t **subnetMasks, uint32_t *ipAddrsLen) {
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
#   elif defined PLAT_LINUX || defined PLAT_APPLE
    struct ifaddrs *addresses, *address;
    int err = getifaddrs(&addresses);
    if (err == -1) {
        //BTAinfoEventHelper(infoEventInst, VERBOSE_CRITICAL, BTA_StatusRuntimeError, "UDP data auto config failed to get adapter addresses: %d", getLastSocketError());
        return BTA_StatusRuntimeError;
    }
    // count addresses
    *ipAddrsLen = 0;
    address = addresses;
    while (address && address->ifa_addr) {
        if (address->ifa_addr->sa_family == AF_INET) {
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
    while (address && address->ifa_addr) {
        if (address->ifa_addr->sa_family == AF_INET) {
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
    uint32_t ipAddrTemp = ipAddr[0] | (ipAddr[1] << 8) | (ipAddr[2] << 16) | (ipAddr[3] << 24);
    if ((ipAddrTemp & 0xe0) == 0xe0) {
        // This is a multicast address
        return 1;
    }
    uint32_t *ipAddrs;
    uint32_t ipAddrsLen;
    BTA_Status status = BTAlistLocalIpAddrs(&ipAddrs, 0, &ipAddrsLen);
    if (status != BTA_StatusOk) {
        return status;
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
    uint32_t deviceIpAddrL = deviceIpAddr[0] | (deviceIpAddr[1] << 8) | (deviceIpAddr[2] << 16) | (deviceIpAddr[3] << 24);
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
                (*matchingLocalpAddr)[0] =  ipAddrs[i] & 0xff;
                (*matchingLocalpAddr)[1] = (ipAddrs[i] >> 8) & 0xff;
                (*matchingLocalpAddr)[2] = (ipAddrs[i] >> 16) & 0xff;
                (*matchingLocalpAddr)[3] = (ipAddrs[i] >> 24) & 0xff;
            }
            
        }
    }
    free(ipAddrs);
    free(subnetMasks);
    if (matchingLocalpAddr && *matchingLocalpAddr) {
        return BTA_StatusOk;
    }
    //BTAinfoEventHelper(infoEventInst, VERBOSE_CRITICAL, BTA_StatusRuntimeError, "UDP data auto config failed, unique local address not found");
    return BTA_StatusRuntimeError;
}


void BTAfreeNetworkBroadcastAddrs(uint8_t ***localIpAddrs, uint8_t ***networkBroadcastAddrs, uint32_t networkBroadcastAddrsLen) {
    for (uint32_t i = 0; i < networkBroadcastAddrsLen; i++) {
        free((*localIpAddrs)[i]);
        (*localIpAddrs)[i] = 0;
        free((*networkBroadcastAddrs)[i]);
        (*networkBroadcastAddrs)[i] = 0;
    }
    free(*localIpAddrs);
    *localIpAddrs = 0;
    free(*networkBroadcastAddrs);
    *networkBroadcastAddrs = 0;
}


BTA_Status BTAgetNetworkBroadcastAddrs(uint8_t ***localIpAddrs, uint8_t ***networkBroadcastAddrs, uint32_t *networkBroadcastAddrsLen) {
    if (!localIpAddrs || !networkBroadcastAddrs || !networkBroadcastAddrsLen) {
        return BTA_StatusInvalidParameter;
    }
    uint32_t *ipAddrs, *subnetMasks;
    BTA_Status status = BTAlistLocalIpAddrs(&ipAddrs, &subnetMasks, networkBroadcastAddrsLen);
    if (status != BTA_StatusOk) {
        return status;
    }
    uint32_t len = *networkBroadcastAddrsLen;
    if (len) {
        uint8_t **resultA1 = *localIpAddrs = (uint8_t **)calloc(len, sizeof(uint8_t *));
        uint8_t **resultB1 = *networkBroadcastAddrs = (uint8_t **)calloc(len, sizeof(uint8_t *));
        if (!resultA1 || !resultB1) {
            BTAfreeNetworkBroadcastAddrs(localIpAddrs, networkBroadcastAddrs, len);
            free(ipAddrs);
            free(subnetMasks);
            return BTA_StatusOutOfMemory;
        }
        for (uint32_t i = 0; i < len; i++) {
            uint8_t *resultA2 = resultA1[i] = (uint8_t *)malloc(4 * sizeof(uint8_t));
            uint8_t *resultB2 = resultB1[i] = (uint8_t *)malloc(4 * sizeof(uint8_t));
            if (!resultA2 || !resultB2) {
                BTAfreeNetworkBroadcastAddrs(localIpAddrs, networkBroadcastAddrs, len);
                free(ipAddrs);
                free(subnetMasks);
                return BTA_StatusOutOfMemory;
            }
            uint32_t ipAddr = ipAddrs[i];
            resultA2[0] = ipAddr & 0xff;
            resultA2[1] = (ipAddr >> 8) & 0xff;
            resultA2[2] = (ipAddr >> 16) & 0xff;
            resultA2[3] = (ipAddr >> 24) & 0xff;
            uint32_t broadcast = ipAddrs[i] | ~subnetMasks[i];
            resultB2[0] = broadcast & 0xff;
            resultB2[1] = (broadcast >> 8) & 0xff;
            resultB2[2] = (broadcast >> 16) & 0xff;
            resultB2[3] = (broadcast >> 24) & 0xff;
        }
        free(ipAddrs);
        free(subnetMasks);
    }
    return BTA_StatusOk;
}
