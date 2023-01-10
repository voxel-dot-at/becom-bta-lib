#pragma once

#include <stdint.h>
#include <bta.h>

int getLastSocketError(void);

uint8_t BTAisLocalIpAddrOrMulticast(uint8_t *ipAddr, uint8_t ipAddrLen);
BTA_Status BTAgetMatchingLocalAddress(uint8_t *deviceIpAddr, uint8_t ipAddrLen, uint8_t **matchingLocalpAddr);
