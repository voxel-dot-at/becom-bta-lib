#ifndef PING_H_INCLUDED
#define PING_H_INCLUDED

#include <stdint.h>

uint8_t ping(uint8_t *ipAddr, uint8_t ipAddrLen, int requiredSuccesses, int intervalMillis, int timeoutMillis);

#endif