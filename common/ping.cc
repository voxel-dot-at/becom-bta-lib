
#include "ping.h"

#if defined PLAT_WINDOWS || defined PLAT_APPLE
#elif defined PLAT_LINUX
#    include <memory>
#    include <stdexcept>
#    include <string.h>
#    include <iostream>

extern "C" {
#    include <timing_helper.h>
}
#   include <stdio.h>
#   define _popen popen
#   define _pclose pclose
#else
#   error "unsupported platform ping.cpp"
#endif

#if defined PLAT_LINUX

std::string exec(const char* cmd) {
    const int bufferSize = 128;
    char buffer[bufferSize];
    std::string result = "";
    FILE* pipe = _popen(cmd, "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    try {
        while (fgets(buffer, sizeof buffer, pipe) != NULL) {
            result += buffer;
        }
    }
    catch (...) {
        _pclose(pipe);
        throw;
    }
    _pclose(pipe);
    return result;
    //std::string result;
    //std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
    //if (!pipe) {
    //    throw std::runtime_error("popen() failed!");
    //}
    //while (fgets(buffer, bufferSize, pipe.get()) != nullptr) {
    //    //sprintf(result + strlen(result), "%s", buffer);
    //    result += buffer;
    //}
    return result;
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
#   elif defined PLAT_LINUX
    sprintf(cmd + strlen(cmd), " -c 1 -w 1000");
#   endif

    uint32_t timeEnd = BTAgetTickCount() + msecsTimeout;
    int successes = 0;
    while (1) {
        std::string output = exec(cmd);
        //std::cout << output << std::endl;
#       if defined PLAT_WINDOWS
        if (output.find("Packets: Sent = 1, Received = 1, Lost = 0 (0% loss)", 0) != std::string::npos) {
#       elif defined PLAT_LINUX
        if (output.find("1 packets transmitted, 1 received, 0% packet loss", 0) != std::string::npos) {
#       endif
            successes++;
        }
        else successes = 0;
        if (successes == requiredSuccesses) return 1;
        if (BTAgetTickCount() > timeEnd) return 0;
        BTAmsleep(500);
    }
    return 0;
}

#endif // PLAT_LINUX
