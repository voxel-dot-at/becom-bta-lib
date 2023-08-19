#include "ping.h"
#include <string.h>
#include <timing_helper.h>
#include <bta_oshelper.h>


uint8_t ping(uint8_t *ipAddr, uint8_t ipAddrLen, int requiredSuccesses, int intervalMillis, int timeoutMillis) {
    char cmd[400];
    sprintf(cmd, "ping ");
    for (int i = 0; i < ipAddrLen; i++) {
        if (i != 0) sprintf(cmd + strlen(cmd), ".");
        sprintf(cmd + strlen(cmd), "%d", ipAddr[i]);
    }
#   if defined PLAT_WINDOWS
    sprintf(cmd + strlen(cmd), " -n 1 -w %d", timeoutMillis);
#   elif defined PLAT_LINUX || defined PLAT_APPLE
    if (timeoutMillis < 1000) timeoutMillis = 1000;
    sprintf(cmd + strlen(cmd), " -c 1 -w %d", timeoutMillis / 1000);
#   endif

    uint64_t timeEnd = BTAgetTickCount64() + timeoutMillis;
    uint64_t timeToProbe = BTAgetTickCount64();
    int successes = 0;
    char output[5000];
    while (1) {
        uint64_t ticks = BTAgetTickCount64();
        if (ticks < timeToProbe) {
            BTAmsleep((uint32_t)(timeToProbe - ticks));
        }
        timeToProbe += intervalMillis;
        BTAexec(cmd, output);
        //std::cout << output << std::endl;
#       if defined PLAT_WINDOWS
        char *txt = "Packets: Sent = 1, Received = 1, Lost = 0 (0% loss)";
#       elif defined PLAT_LINUX || defined PLAT_APPLE
        char *txt = "1 packets transmitted, 1 received, 0% packet loss";
#       endif
        if (strstr(output, txt)) {
            successes++;
        }
        else {
            successes = 0;
        }
        if (successes == requiredSuccesses) {
            return 1;
        }
        if (BTAgetTickCount64() > timeEnd) {
            return 0;
        }
    }
    return 0;
}
