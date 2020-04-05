#ifndef PING_H_INCLUDED
#define PING_H_INCLUDED

#include <stdint.h>

int ping(uint8_t *ipAddr, uint8_t ipAddrLen, int requiredSuccesses, int msecsTimeout);

#endif