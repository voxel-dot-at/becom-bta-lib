#include <string.h>
#include <timing_helper.h>

#include "ping.h"


#if defined PLAT_WINDOWS
#include <memory>
#include <stdexcept>
#include <iostream>
#elif defined PLAT_LINUX || defined PLAT_APPLE
#include <stdio.h>
#   define _popen popen
#   define _pclose pclose
#else
#   error "unsupported platform ping.c"
#endif


void exec(const char* cmd, char *result) {
    const int bufferSize = 128;
    char buffer[bufferSize];
    *result = 0;
    FILE* pipe = _popen(cmd, "r");
    if (!pipe) {
        return;
    }
    while (fgets(buffer, sizeof buffer, pipe)) {
        sprintf(result + strlen(result), "%s", buffer);
    }
    _pclose(pipe);
}


int ping(uint8_t *ipAddr, uint8_t ipAddrLen, int requiredSuccesses, int msecsTimeout) {
    char cmd[200];
    sprintf(cmd, "ping ");
    for (int i = 0; i < ipAddrLen; i++) {
        if (i != 0) sprintf(cmd + strlen(cmd), ".");
        sprintf(cmd + strlen(cmd), "%d", ipAddr[i]);
    }
#   if defined PLAT_WINDOWS
    sprintf(cmd + strlen(cmd), " -n 1 -w 1000");
#   elif defined PLAT_LINUX || defined PLAT_APPLE
    sprintf(cmd + strlen(cmd), " -c 1 -w 1");
#   endif

    uint32_t timeEnd = BTAgetTickCount() + msecsTimeout;
    int successes = 0;
    char output[1024];
    while (1) {
        exec(cmd, output);
        //std::cout << output << std::endl;
#       if defined PLAT_WINDOWS
        char *txt = "Packets: Sent = 1, Received = 1, Lost = 0 (0% loss)";
#       elif defined PLAT_LINUX || defined PLAT_APPLE
        char *txt = "1 packets transmitted, 1 received, 0% packet loss";
#       endif
        if (strstr(output, txt)) {
            successes++;
            BTAmsleep(100);
        }
        else successes = 0;
        if (successes == requiredSuccesses) return 1;
        if (BTAgetTickCount() > timeEnd) return 0;
        BTAmsleep(500);
    }
    return 0;
}
